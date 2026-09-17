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
#ifndef COLORTOALPHA_H
#define COLORTOALPHA_H

#include <QRgb>

class QImage;

struct ColorToAlphaParams
{
    QRgb targetColor = qRgb(255, 255, 255); // 目标色（直通、不透明），默认白
    int threshold = 100;                    // 1..255，Lab ΔE 尺度：差值≥阈值全保留
};

/** 颜色转透明度：Krita「Color to Alpha」滤镜移植。

 * 语义（忠实移植 plugins/filters/colors/kis_color_to_alpha.cpp）：
 *  1. 每像素与目标色求 Lab ΔE（CIE76，感知色差，0..255）；
 *  2. 新透明度 = ΔE ≥ 阈值 ? 不透明 : ΔE/阈值（线性坡道，抗锯齿边保留为半透明）；
 *  3. 透明度只降不升，全透明像素不动；
 *  4. 颜色反混合 c' = (c-t)/α'+t，使结果叠回目标色可还原原图（白底提线后
 *     仍可放回任意底色）。
 * 与 lcms 原版差异：Lab 转换用标准 sRGB→XYZ(D65) 而非 ICC PCS(D50)，
 * ΔE 数值偏差 <1~2；α'=0 时跳过颜色重算（原版除零后 clamp，结果同样不可见）。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回被修改的像素数（0 = 无变化）。
 */
namespace ColorToAlpha
{
    int apply(QImage& img, const ColorToAlphaParams& params);
}

#endif // COLORTOALPHA_H
