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
#ifndef SHADOW_ENGINE_H
#define SHADOW_ENGINE_H

#include <QImage>
#include <QRect>
#include <QRgb>
#include <QVector>

/*
 * 智能阴影引擎 — 手画分割线 + 方向判定的自动填阴影。
 *
 * 工作流：用户在智能阴影图层用标记色（红/黄/蓝，颜色只作标记）逐帧画
 * "明暗交界线"，线条把线稿封闭图形切成两半；选定方向（阴影在左/右/
 * 上/下哪一侧）后，引擎逐帧：
 *
 *   ① 双层连通域分割——只拿线稿当屏障得到"图形"，线稿+分割线一起当
 *      屏障得到"子区域"，子区域按锚点归属图形（复用 colorize 引擎的
 *      segmentRegions，屏障=任何非零高度）；
 *   ② 方向判定——同图形各子区域沿所选轴比较质心，阴影=极端组（与
 *      极端值相差 ≤ max(ambiguityMinPx, ratio×图形沿轴跨度) 的全部子
 *      区域，十字切割并列的整列一起中选）；极端组=全部子区域（分割线
 *      与所选方向垂直/并列）时方向无信息；
 *   ③ 空间邻近继承——方向无信息的子区域继承同帧质心最近"已判定"
 *      子区域的状态（阴影或非阴影都可能被继承）；仍无借鉴 → 按所选
 *      方向取极端（并列按次轴字典序）确定性选边——被切割图形必有
 *      填充，警告仍提示核对；
 *   ④ 涂色——阴影子区域整体填阴影色；分割线像素从贴着阴影侧的一圈
 *      向线内有限扩散涂色（消除明暗交界处的透明细缝，且不会越过线稿
 *      蔓延到图形外的线段上）。
 *
 * 只有封闭图形（区域不触计算域边缘）能被上阴影——分割线越过轮廓伸进
 * 背景抵达画布边缘、或线稿缺口导致内外连通时，开放区域即使被切开了
 * 也不参与判定与填充（否则方向判定会把半个背景矩形填满）。没画线的
 * 图形永不被自动上阴影。分割线颜色只作标记，引擎不区分；被标为
 * "透明"的标记色，其线条是禁用线——不作为屏障、不参与判定。
 *
 * 约定：传入的所有 QImage 尺寸一致，bounds 落在图像矩形内，
 * lineArt/strokes 均为 Format_ARGB32_Premultiplied（同 colorize 引擎）；
 * 返回的 Warning::area 与涂色均为图像本地坐标（不含 bounds.topLeft 偏移）。
 */
namespace ShadowFill
{

enum Direction
{
    DirLeft = 0,  // 阴影在分割线左侧（质心 x 最小的一侧）
    DirRight = 1, // 阴影在右侧
    DirUp = 2,    // 阴影在上侧（屏幕 y 向下：上 = 质心 y 最小）
    DirDown = 3   // 阴影在下侧
};

struct Params
{
    Direction direction = DirLeft;
    qreal gapRadius = 4.0;          // 分割线闭缝半径（px）——桥接线尾没搭到线稿的小缺口
    qreal ambiguityRatio = 0.10;    // 歧义阈值比例（× 图形沿所选轴的跨度）
    int ambiguityMinPx = 8;         // 歧义阈值的绝对下限（px）
    QRgb fillColor = qRgb(0, 0, 0); // 阴影填充色（按不透明处理）
};

struct Warning
{
    enum Kind
    {
        UnclosedDivider = 0,   // 图形上有分割线但未把图形完全切开（缺口/悬空线）
        UnresolvedAmbiguous    // 图形方向歧义且同帧无可借鉴的已判定区域
    };
    Kind kind;
    QRect area;                // 供定位的图形包围盒（图像本地坐标）
};

struct Result
{
    // ARGB32_Premultiplied，bounds 大小；isNull = 没有可涂的阴影区域
    // （帧上无有效分割线，或全部歧义未解决）。警告仍会给出。
    QImage fill;
    QVector<Warning> warnings;
};

/*
 * 计算一帧阴影。transparentMarkerColors = 被标"透明"的标记色集合：
 * 笔画里该颜色（反预乘、RGB 距离容差）的像素是禁用线。
 */
Result computeShadow(const QImage& lineArt,
                     const QImage& strokes,
                     const QRect& bounds,
                     const Params& params,
                     const QVector<QRgb>& transparentMarkerColors = QVector<QRgb>());

}

#endif // SHADOW_ENGINE_H
