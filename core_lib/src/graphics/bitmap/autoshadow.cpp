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

constexpr int ALPHA_MIN = 16; // α≥此值视为不透明内容（抗锯齿半透明边缘不参与）

int clampInt(const int v, const int lo, const int hi)
{
    return std::min(hi, std::max(lo, v));
}

/** 方形结构元的膨胀/腐蚀（0/1 缓冲，水平+垂直两趟分离实现，就地写回 buf） */
void boxMorph(std::vector<uint8_t>& buf, const int w, const int h, const int r, const bool dilate)
{
    if (r <= 0)
        return;
    std::vector<uint8_t> tmp(static_cast<size_t>(w) * h);
    const auto fold = dilate
        ? [](const uint8_t a, const uint8_t b) { return static_cast<uint8_t>(std::max(a, b)); }
        : [](const uint8_t a, const uint8_t b) { return static_cast<uint8_t>(std::min(a, b)); };

    // 水平趟：buf → tmp
    for (int y = 0; y < h; ++y)
    {
        const uint8_t* src = &buf[static_cast<size_t>(y) * w];
        uint8_t* dst = &tmp[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x)
        {
            uint8_t acc = dilate ? uint8_t(0) : uint8_t(1);
            const int x0 = std::max(0, x - r);
            const int x1 = std::min(w - 1, x + r);
            for (int i = x0; i <= x1; ++i)
                acc = fold(acc, src[i]);
            dst[x] = acc;
        }
    }
    // 垂直趟：tmp → buf
    for (int y = 0; y < h; ++y)
    {
        uint8_t* dst = &buf[static_cast<size_t>(y) * w];
        const int y0 = std::max(0, y - r);
        const int y1 = std::min(h - 1, y + r);
        for (int x = 0; x < w; ++x)
        {
            uint8_t acc = dilate ? uint8_t(0) : uint8_t(1);
            for (int i = y0; i <= y1; ++i)
                acc = fold(acc, tmp[static_cast<size_t>(i) * w + x]);
            dst[x] = acc;
        }
    }
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

    const double angleRad = qDegreesToRadians(static_cast<double>(((params.lightAngle % 360) + 360) % 360));
    const double dirX = std::cos(angleRad);
    const double dirY = -std::sin(angleRad); // 屏幕 y 向下，数学角逆时针
    const double dist = std::max(50.0, static_cast<double>(params.lightDistance));

    const double opacity = clampInt(params.shadowOpacity, 1, 100) / 100.0;
    const double opacity2 = std::min(1.0, opacity * 1.5);
    const int choke = clampInt(params.choke, 0, 8);

    // 1. 掩膜 + 内容包围盒
    std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y)
    {
        const auto* line = reinterpret_cast<const QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            if (qAlpha(line[x]) >= ALPHA_MIN)
            {
                mask[static_cast<size_t>(y) * w + x] = 1;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (maxX < 0)
        return 0;

    // 2. 光源点 = 内容中心 + 极坐标(角度, 距离)；近=圆形径向渐变，远=接近平行条带
    const double lightX = (minX + maxX) / 2.0 + dirX * dist;
    const double lightY = (minY + maxY) / 2.0 + dirY * dist;

    // 3. 径向渐变场：v(p) = p 到光源的距离；以内容上离光源最近的点为 0（近端全亮起步），
    //    只比较平方距离，阈值 = (r0 + T)² 与 d² 直接比，免开方
    double minD2 = std::numeric_limits<double>::max();
    for (int y = minY; y <= maxY; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        const double dy = y - lightY;
        for (int x = minX; x <= maxX; ++x)
        {
            if (mask[row + x] == 0)
                continue;
            const double dx = x - lightX;
            const double d2 = dx * dx + dy * dy;
            if (d2 < minD2)
                minD2 = d2;
        }
    }
    const double r0 = std::sqrt(minD2);
    const double t1Sq = (r0 + range) * (r0 + range);
    const double t2Sq = (r0 + second) * (r0 + second);

    // 4. 色调分离：d² ≥ (r0+range)² 一档；有第二档时 d² ≥ (r0+second)² 更暗
    std::vector<uint8_t> level1(static_cast<size_t>(w) * h, 0);
    std::vector<uint8_t> level2(static_cast<size_t>(w) * h, 0);
    for (int y = minY; y <= maxY; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        const double dy = y - lightY;
        for (int x = minX; x <= maxX; ++x)
        {
            if (mask[row + x] == 0)
                continue;
            const double dx = x - lightX;
            const double d2 = dx * dx + dy * dy;
            if (d2 >= t1Sq)
            {
                level1[row + x] = 1;
                if (hasSecond && d2 >= t2Sq)
                    level2[row + x] = 1;
            }
        }
    }

    // 5. 阻塞：两级掩膜各自膨胀 choke px 后裁回原内容边界（咬进线条、不出界）
    if (choke > 0)
    {
        std::vector<uint8_t> grown1 = level1;
        std::vector<uint8_t> grown2 = level2;
        boxMorph(grown1, w, h, choke, true);
        boxMorph(grown2, w, h, choke, true);
        for (size_t i = 0; i < level1.size(); ++i)
        {
            level1[i] = mask[i] != 0 ? grown1[i] : uint8_t(0);
            level2[i] = mask[i] != 0 ? grown2[i] : uint8_t(0);
        }
    }

    // 6. 上色：预乘域正片叠底 c' = c·((1-k)+k·s/255)，α 不变
    const double sR = qRed(params.shadowColor);
    const double sG = qGreen(params.shadowColor);
    const double sB = qBlue(params.shadowColor);
    const double fR1 = (1.0 - opacity) + opacity * sR / 255.0;
    const double fG1 = (1.0 - opacity) + opacity * sG / 255.0;
    const double fB1 = (1.0 - opacity) + opacity * sB / 255.0;
    const double fR2 = (1.0 - opacity2) + opacity2 * sR / 255.0;
    const double fG2 = (1.0 - opacity2) + opacity2 * sG / 255.0;
    const double fB2 = (1.0 - opacity2) + opacity2 * sB / 255.0;

    int changed = 0;
    for (int y = 0; y < h; ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            if (mask[row + x] == 0)
                continue;
            const QRgb px = line[x];
            double fR = 1.0, fG = 1.0, fB = 1.0;
            if (level2[row + x] != 0)
            {
                fR = fR2; fG = fG2; fB = fB2;
            }
            else if (level1[row + x] != 0)
            {
                fR = fR1; fG = fG1; fB = fB1;
            }
            else
            {
                continue;
            }
            const QRgb out = qRgba(static_cast<int>(qRound(qRed(px) * fR)),
                                   static_cast<int>(qRound(qGreen(px) * fG)),
                                   static_cast<int>(qRound(qBlue(px) * fB)),
                                   qAlpha(px));
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
