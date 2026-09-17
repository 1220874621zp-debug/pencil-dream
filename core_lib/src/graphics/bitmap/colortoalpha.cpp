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
#include "colortoalpha.h"
#include "colordistance.h"

#include <QImage>

#include <algorithm>
#include <cmath>

namespace
{

// 预乘分量 → 直通（与 holefiller 的 lift 同式）
int liftChannel(const int premul, const int alpha)
{
    return std::min(255, (premul * 255 + alpha / 2) / alpha);
}

int clamp255(const double v)
{
    return static_cast<int>(std::lround(std::min(255.0, std::max(0.0, v))));
}

} // namespace

namespace ColorToAlpha
{

int apply(QImage& img, const ColorToAlphaParams& params)
{
    if (img.isNull() || img.format() != QImage::Format_ARGB32_Premultiplied)
        return 0;

    const int threshold = std::min(255, std::max(1, params.threshold));
    const double thresholdF = threshold;

    const int tr = qRed(params.targetColor);
    const int tg = qGreen(params.targetColor);
    const int tb = qBlue(params.targetColor);
    const ColorDistance::LabF targetLab = ColorDistance::rgbToLab(tr, tg, tb);

    int changed = 0;

    for (int y = 0; y < img.height(); ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue; // 全透明像素：Krita 语义下不动

            // 预乘 → 直通
            int r = liftChannel(qRed(px), a);
            int g = liftChannel(qGreen(px), a);
            int b = liftChannel(qBlue(px), a);

            double dE = 255.0;
            if (r == tr && g == tg && b == tb)
            {
                dE = 0.0; // 快路径：像素就是目标色
            }
            else
            {
                const ColorDistance::LabF lab = ColorDistance::rgbToLab(r, g, b);
                dE = std::min(255.0, ColorDistance::deltaE(lab, targetLab));
            }

            // 线性坡道：≥阈值全保留，否则按 ΔE/阈值渐变
            const double newOpacity = dE >= thresholdF ? 1.0 : dE / thresholdF;

            // 透明度只降不升
            int newA = a;
            if (newOpacity < a / 255.0)
                newA = static_cast<int>(std::lround(newOpacity * 255.0));

            // 反混合：结果叠回目标色可还原原像素。
            // newOpacity==1 时数学上恒等（跳过省舍入），==0 时颜色不可见（保留原直通色）
            if (newOpacity > 0.0 && newOpacity < 1.0)
            {
                r = clamp255((r - tr) / newOpacity + tr);
                g = clamp255((g - tg) / newOpacity + tg);
                b = clamp255((b - tb) / newOpacity + tb);
            }

            const QRgb out = qPremultiply(qRgba(r, g, b, newA));
            if (out != px)
            {
                line[x] = out;
                ++changed;
            }
        }
    }
    return changed;
}

} // namespace ColorToAlpha
