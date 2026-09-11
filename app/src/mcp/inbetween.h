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

*/

#ifndef INBETWEEN_H
#define INBETWEEN_H

#include <QColor>
#include <QImage>

/** 自动画中割：两张原画之间按距离场插值生成中间帧（确定性算法，纯 QImage 进出）。
 *
 * 原理：对两张二值化线稿各做 3-4 chamfer 距离变换（到最近线条的欧氏近似距离），
 * 在 t 处线性插值两张距离场，等值带 {d_mid <= epsilon} 即中间帧线条位置。
 * 平移/小幅形变稳定；大幅旋转会收缩模糊（确定性方法上限）。
 */
namespace Inbetween
{

struct Options
{
    qreal epsilon = 1.2;             // 等值带半宽（越大线条越粗）
    int blurPasses = 1;              // 距离场平滑次数（抑制碎点）
    int denoiseArea = 6;             // 小于该像素数的孤立连通域被清除（0=关闭）
    QColor strokeColor = QColor(Qt::black);
};

/** 生成 t∈(0,1) 处的中间帧；a/b 须同尺寸，返回 ARGB32_Premultiplied */
QImage interpolate(const QImage& a, const QImage& b, qreal t, const Options& options = {});

} // namespace Inbetween

#endif // INBETWEEN_H
