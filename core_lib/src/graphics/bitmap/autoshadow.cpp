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

constexpr int ALPHA_MIN = 16; // α≥此值视为不透明内容

int clampInt(const int v, const int lo, const int hi)
{
    return std::min(hi, std::max(lo, v));
}

float smoothStep(const float e0, const float e1, const float x)
{
    if (e1 <= e0)
        return x < e0 ? 0.0f : 1.0f;
    const float t = std::min(1.0f, std::max(0.0f, (x - e0) / (e1 - e0)));
    return t * t * (3.0f - 2.0f * t);
}

/** 双线性采样（边界钳位） */
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

/** 单通道混合（预乘域）：mode 决定公式，返回混合后的预乘分量（浮点，外层统一加权后再取整） */
double blendChannel(const int premul, const int alpha, const int s, const AutoShadowBlendMode mode)
{
    switch (mode)
    {
    case AutoShadowBlendMode::Multiply:
        return premul * static_cast<double>(s) / 255.0;
    case AutoShadowBlendMode::Normal:
        return s * static_cast<double>(alpha) / 255.0;
    case AutoShadowBlendMode::LinearBurn:
    {
        if (alpha >= 255)
            return std::max(0, premul + s - 255);
        const int straight = std::min(255, (premul * 255 + alpha / 2) / alpha);
        return std::max(0, std::min(255, straight + s - 255)) * static_cast<double>(alpha) / 255.0;
    }
    }
    return premul;
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

    // 阈值清洗：递增且落在 1..100（乱序输入按就近可用值收敛，预览与实跑同规则）
    const int t1 = clampInt(params.thresholds[0], 1, 98);
    const int t2 = clampInt(params.thresholds[1], t1 + 1, 99);
    const int t3 = clampInt(params.thresholds[2], t2 + 1, 100);
    const double t1f = t1, t2f = t2, t3f = t3;

    const double angleRad = qDegreesToRadians(static_cast<double>(((params.lightAngle % 360) + 360) % 360));
    const double dirX = std::cos(angleRad);
    const double dirY = -std::sin(angleRad); // 屏幕 y 向下，数学角逆时针
    const double dist = std::max(50.0, static_cast<double>(params.lightDistance));
    const int displace = clampInt(params.displaceStrength, 0, 100);
    const float feather = std::max(0.0f, static_cast<float>(params.edgeFeather));

    // 色带（反转=镜像）
    AutoShadowLevel levels[4];
    for (int i = 0; i < 4; ++i)
    {
        const AutoShadowLevel& src = params.levels[params.invertLevels ? 3 - i : i];
        levels[i].color = src.color;
        levels[i].mode = src.mode;
    }

    const size_t count = static_cast<size_t>(w) * h;

    // α 场 + 亮度场 + 包围盒
    std::vector<uint8_t> contentMask(count, 0);
    std::vector<float> lumaF(count, 0.5f); // 置换贴图：直通亮度，透明处中性 0.5
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
            contentMask[row + x] = 1;
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

    // ── 场生成段：g = 到光源距离 − r0（内容最近点归零），置换后按内容最大值归一化到 0..100
    const double lightX = (minX + maxX) / 2.0 + dirX * dist;
    const double lightY = (minY + maxY) / 2.0 + dirY * dist;
    double minD2 = std::numeric_limits<double>::max();
    for (int y = minY; y <= maxY; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        const double dy = y - lightY;
        for (int x = minX; x <= maxX; ++x)
        {
            if (contentMask[row + x] == 0)
                continue;
            const double dx = x - lightX;
            const double d2 = dx * dx + dy * dy;
            if (d2 < minD2)
                minD2 = d2;
        }
    }
    const double r0 = std::sqrt(minD2);

    std::vector<float> field(count, 0.0f);
    double fieldMax = 0.0;
    for (int y = 0; y < h; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        const double dy = y - lightY;
        for (int x = 0; x < w; ++x)
        {
            const double dx = x - lightX;
            field[row + x] = static_cast<float>(std::max(0.0, std::sqrt(dx * dx + dy * dy) - r0));
        }
    }
    for (int y = minY; y <= maxY; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = minX; x <= maxX; ++x)
        {
            if (contentMask[row + x] != 0)
                fieldMax = std::max(fieldMax, static_cast<double>(field[row + x]));
        }
    }
    if (fieldMax <= 0.0)
        fieldMax = 1.0;
    const double normScale = 100.0 / fieldMax;

    // 置换（贴图=原图亮度，双轴等强度）→ 归一化场值 F∈[0,100]
    std::vector<float> fieldNorm(count, 0.0f);
    for (int y = 0; y < h; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            double g = field[row + x];
            if (displace > 0)
            {
                const double off = static_cast<double>(displace) * (lumaF[row + x] - 0.5) * 2.0;
                if (off != 0.0)
                    g = bilinearSample(field, w, h, x + off, y + off);
            }
            fieldNorm[row + x] = static_cast<float>(std::min(100.0, std::max(0.0, g * normScale)));
        }
    }

    // ── 映射段：权重（色调分离=smoothstep阶跃 / 平滑=分段线性）→ 各阶混合按权重加权
    const double levelS[4][3] = {
        { qRed(levels[0].color), qGreen(levels[0].color), qBlue(levels[0].color) },
        { qRed(levels[1].color), qGreen(levels[1].color), qBlue(levels[1].color) },
        { qRed(levels[2].color), qGreen(levels[2].color), qBlue(levels[2].color) },
        { qRed(levels[3].color), qGreen(levels[3].color), qBlue(levels[3].color) },
    };

    int changed = 0;
    for (int y = 0; y < h; ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            if (contentMask[row + x] == 0)
                continue;
            const float F = fieldNorm[row + x];
            const QRgb px = line[x];
            const int alpha = qAlpha(px);

            // 色阶权重 w0..w3（和恒为 1）：三条越界进度 s1/s2/s3 链式组合
            //   s1: 场值 0→t1（色阶1→2）、s2: t1→t2（2→3）、s3: t2→t3（3→4）
            //   色调分离 = smoothstep 过渡（feather=0 即硬阶跃）；平滑 = 线性坡道（连续梯度映射）
            double s1, s2, s3;
            const auto ramp = [](const double lo, const double hi, const double v) {
                return std::min(1.0, std::max(0.0, (v - lo) / (hi - lo)));
            };
            if (params.smooth)
            {
                s1 = ramp(0.0, t1f, F);
                s2 = ramp(t1f, t2f, F);
                s3 = ramp(t2f, t3f, F);
            }
            else
            {
                const float half = feather * 0.5f;
                s1 = smoothStep(t1f - half, t1f + half, F);
                s2 = smoothStep(t2f - half, t2f + half, F);
                s3 = smoothStep(t3f - half, t3f + half, F);
            }
            const double w[4] = {
                1.0 - s1,
                s1 * (1.0 - s2),
                s2 * (1.0 - s3),
                s3,
            };

            // 各阶独立混合后按权重加权（模式不同也能连续过渡），一次取整
            double outR = 0.0, outG = 0.0, outB = 0.0;
            for (int i = 0; i < 4; ++i)
            {
                if (w[i] <= 0.0)
                    continue;
                outR += w[i] * blendChannel(qRed(px), alpha, static_cast<int>(levelS[i][0]), levels[i].mode);
                outG += w[i] * blendChannel(qGreen(px), alpha, static_cast<int>(levelS[i][1]), levels[i].mode);
                outB += w[i] * blendChannel(qBlue(px), alpha, static_cast<int>(levelS[i][2]), levels[i].mode);
            }
            const QRgb out = qRgba(qRound(outR), qRound(outG), qRound(outB), alpha);
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
