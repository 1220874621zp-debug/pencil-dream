/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
#include "autoshadow.h"

#include <QImage>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{

constexpr int ALPHA_MIN = 16; // α≥此值视为不透明内容（掩膜/包围盒判定）

int clampInt(const int v, const int lo, const int hi)
{
    return std::min(hi, std::max(lo, v));
}

/** ── 效果2：高斯模糊 ── 3 次盒式模糊近似高斯（分离前缀和，O(N) 与半径无关） */

std::vector<int> boxesForGauss(const float sigma, const int boxCount)
{
    const float wIdeal = std::sqrt((12.0f * sigma * sigma / static_cast<float>(boxCount)) + 1.0f);
    int wl = static_cast<int>(std::floor(wIdeal));
    if (wl % 2 == 0)
        --wl;
    if (wl < 1)
        wl = 1;
    const int wu = wl + 2;
    const float mIdeal = (12.0f * sigma * sigma
                          - static_cast<float>(boxCount) * wl * wl
                          - static_cast<float>(boxCount) * wl
                          - static_cast<float>(boxCount) / 4.0f)
                         / static_cast<float>(-4 * wl - 4);
    const int m = static_cast<int>(std::round(mIdeal));
    std::vector<int> sizes;
    for (int i = 0; i < boxCount; ++i)
        sizes.push_back(i < m ? wl : wu);
    return sizes;
}

void boxBlurH(const std::vector<float>& src, std::vector<float>& dst, const int w, const int h, const int r)
{
    if (r <= 0)
    {
        dst = src;
        return;
    }
    const float inv = 1.0f / static_cast<float>(2 * r + 1);
    for (int y = 0; y < h; ++y)
    {
        const float* line = &src[static_cast<size_t>(y) * w];
        float* out = &dst[static_cast<size_t>(y) * w];
        float acc = line[0] * static_cast<float>(r + 1);
        for (int i = 1; i <= r; ++i)
            acc += line[std::min(i, w - 1)];
        for (int x = 0; x < w; ++x)
        {
            out[x] = acc * inv;
            acc += line[std::min(x + r + 1, w - 1)] - line[std::max(x - r, 0)];
        }
    }
}

void boxBlurV(const std::vector<float>& src, std::vector<float>& dst, const int w, const int h, const int r)
{
    if (r <= 0)
    {
        dst = src;
        return;
    }
    const float inv = 1.0f / static_cast<float>(2 * r + 1);
    for (int x = 0; x < w; ++x)
    {
        float acc = src[static_cast<size_t>(x)] * static_cast<float>(r + 1);
        for (int i = 1; i <= r; ++i)
            acc += src[static_cast<size_t>(std::min(i, h - 1)) * w + x];
        for (int y = 0; y < h; ++y)
        {
            dst[static_cast<size_t>(y) * w + x] = acc * inv;
            acc += src[static_cast<size_t>(std::min(y + r + 1, h - 1)) * w + x]
                 - src[static_cast<size_t>(std::max(y - r, 0)) * w + x];
        }
    }
}

void gaussBlurAlpha(std::vector<float>& buf, std::vector<float>& tmp, const int w, const int h, const float sigma)
{
    const std::vector<int> boxes = boxesForGauss(sigma, 3);
    for (const int size : boxes)
    {
        const int r = (size - 1) / 2;
        if (r <= 0)
            continue;
        boxBlurH(buf, tmp, w, h, r);
        boxBlurV(tmp, buf, w, h, r);
    }
}

/** ── 效果3：圆形遮罩 ── smoothstep 羽化；圆内=1（露出底下原图），圆外=0 */
float circleMask(const double dist, const double radius, const float feather)
{
    if (feather < 1.0f)
        return dist < radius ? 1.0f : 0.0f;
    const float t = std::min(1.0f, std::max(0.0f,
        static_cast<float>((dist - (radius - feather * 0.5)) / feather)));
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

/** ── 效果4：置换贴图 ── 双线性采样（边界钳位） */
float bilinearSample(const std::vector<float>& buf, const int w, const int h, double x, double y)
{
    x = std::min(static_cast<double>(w - 1), std::max(0.0, x));
    y = std::min(static_cast<double>(h - 1), std::max(0.0, y));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const double fx = x - x0;
    const double fy = y - y0;
    const float a = buf[static_cast<size_t>(y0) * w + x0];
    const float b = buf[static_cast<size_t>(y0) * w + x1];
    const float c = buf[static_cast<size_t>(y1) * w + x0];
    const float d = buf[static_cast<size_t>(y1) * w + x1];
    return static_cast<float>((a * (1.0 - fx) + b * fx) * (1.0 - fy) + (c * (1.0 - fx) + d * fx) * fy);
}

} // namespace

namespace AutoShadow
{

int apply(QImage& img, const AutoShadowParams& params)
{
    if (img.isNull() || img.format() != QImage::Format_ARGB32_Premultiplied)
        return 0;
    const int w = img.width();
    const int h = img.height();
    if (w <= 0 || h <= 0)
        return 0;

    const int range = clampInt(params.shadowRange, 2, 400);
    int second = params.secondLevelRange;
    if (second > 0)
        second = clampInt(second, range + 2, 800);
    const bool hasSecond = second > 0;
    const int blur = clampInt(params.blurRadius, 0, 200);
    const int displace = clampInt(params.displaceStrength, 0, 100);
    const double opacity = clampInt(params.shadowOpacity, 1, 100) / 100.0;

    const double angleRad = qDegreesToRadians(static_cast<double>(((params.lightAngle % 360) + 360) % 360));
    const double dirX = std::cos(angleRad);
    const double dirY = -std::sin(angleRad); // 屏幕 y 向下，数学角逆时针
    const double dist = std::max(50.0, static_cast<double>(params.lightDistance));

    const size_t count = static_cast<size_t>(w) * h;

    // ── 效果1：实体化「填充+降透明度」副本——颜色恒 S，只需求 α 场与亮度场
    std::vector<float> alphaF(count, 0.0f);   // 副本 α（原图 α）
    std::vector<float> lumaF(count, 0.5f);    // 置换贴图：原图直通亮度，透明处中性 0.5
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y)
    {
        const auto* line = reinterpret_cast<const QRgb*>(img.scanLine(y));
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a < ALPHA_MIN)
                continue;
            alphaF[row + x] = a / 255.0f;
            const auto lift = [a](const int premul) { return std::min(255, (premul * 255 + a / 2) / a); };
            lumaF[row + x] = (0.299f * lift(qRed(px)) + 0.587f * lift(qGreen(px)) + 0.114f * lift(qBlue(px))) / 255.0f;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX < 0)
        return 0;

    // 光源点 = 内容中心 + 极坐标(角度, 距离)；r0 = 内容上离光源最近点（遮罩半径基准）
    const double lightX = (minX + maxX) / 2.0 + dirX * dist;
    const double lightY = (minY + maxY) / 2.0 + dirY * dist;
    double minD2 = std::numeric_limits<double>::max();
    for (int y = minY; y <= maxY; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        const double dy = y - lightY;
        for (int x = minX; x <= maxX; ++x)
        {
            if (alphaF[row + x] <= 0.0f)
                continue;
            const double dx = x - lightX;
            const double d2 = dx * dx + dy * dy;
            if (d2 < minD2)
                minD2 = d2;
        }
    }
    const double r0 = std::sqrt(minD2);
    const double radius1 = r0 + range;
    const double radius2 = r0 + second;
    const float feather = static_cast<float>(blur);

    // ── 效果2：高斯模糊副本 α（羽化来源，也是置换采样的越界垫边）
    std::vector<float> blurAlpha = alphaF;
    if (blur > 0)
    {
        std::vector<float> tmp(count);
        gaussBlurAlpha(blurAlpha, tmp, w, h, std::max(1.0f, blur / 2.0f));
    }

    // ── 效果3+4：圆形遮罩（add 相交）→ 置换（贴图=原图亮度，双轴等强度）
    const auto buildMatte = [&](const double radius) {
        std::vector<float> matte(count, 0.0f);
        for (int y = minY; y <= maxY; ++y)
        {
            const size_t row = static_cast<size_t>(y) * w;
            const double dy = y - lightY;
            for (int x = minX; x <= maxX; ++x)
            {
                const float a = blurAlpha[row + x];
                if (a <= 0.0f)
                    continue;
                const double dx = x - lightX;
                matte[row + x] = a * circleMask(std::sqrt(dx * dx + dy * dy), radius, feather);
            }
        }
        if (displace > 0)
        {
            std::vector<float> displaced(count, 0.0f);
            const double strength = static_cast<double>(displace);
            for (int y = 0; y < h; ++y)
            {
                const size_t row = static_cast<size_t>(y) * w;
                for (int x = 0; x < w; ++x)
                {
                    const double off = strength * (lumaF[row + x] - 0.5) * 2.0;
                    if (off == 0.0)
                    {
                        displaced[row + x] = matte[row + x];
                        continue;
                    }
                    displaced[row + x] = bilinearSample(matte, w, h, x + off, y + off);
                }
            }
            matte.swap(displaced);
        }
        return matte;
    };

    const std::vector<float> matte1 = buildMatte(radius1);
    const std::vector<float> matte2 = hasSecond ? buildMatte(radius2) : std::vector<float>();

    // ── 效果5：轨道遮罩翻转 + 整合——α原×O×(1−matte)，双层同色叠加 a=1−(1−a1)(1−a2)
    const double sR = qRed(params.shadowColor);
    const double sG = qGreen(params.shadowColor);
    const double sB = qBlue(params.shadowColor);

    int changed = 0;
    for (int y = 0; y < h; ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            const float alpha = alphaF[row + x];
            if (alpha <= 0.0f)
                continue; // 副本 α 裁回内容：阴影不出轮廓
            const float m1 = matte1[row + x];
            double shadowA = alpha * opacity * (1.0 - m1);
            if (hasSecond)
            {
                const double a2 = alpha * opacity * (1.0 - matte2[row + x]);
                shadowA = 1.0 - (1.0 - shadowA) * (1.0 - a2);
            }
            if (shadowA <= 0.0)
                continue;

            const QRgb px = line[x];
            const double keep = 1.0 - shadowA;
            const int outA = qRound(shadowA * 255.0 + qAlpha(px) * keep);
            const QRgb out = qRgba(qRound(sR * shadowA + qRed(px) * keep),
                                   qRound(sG * shadowA + qGreen(px) * keep),
                                   qRound(sB * shadowA + qBlue(px) * keep),
                                   std::min(255, outA));
            if (out != px)
            {
                line[x] = out;
                ++changed;
            }
        }
    }
    return changed;
}

} // namespace AutoShadow
