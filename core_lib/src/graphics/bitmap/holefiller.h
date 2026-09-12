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
#ifndef HOLEFILLER_H
#define HOLEFILLER_H

class QImage;

/** 镂空检测：方向引导的孔洞颜色填充。
 *
 * 一键补齐线稿帧里的透明缺口，流程：
 *  1. 镂空定位：8 连通标记透明像素（镂空像素 RGB 不可信，只看 alpha），
 *     能连到图像边界的视为背景，连不到的即封闭镂空；
 *  2. 细缝检测：对不透明区做 11x11 形态学闭运算，被闭运算盖住的
 *     透明像素是窄缝（宽度 < ~11px，含延伸出画面边缘的裂缝）；
 *  3. 毛边吸收：填充范围向非完全不透明像素膨胀 2px，把贴着镂空的
 *     抗锯齿半透明边一起吃掉，避免填完留一圈透底毛边；
 *  4. 填充（三种模式）：
 *     - NearestFill 自动：多源 BFS——每个待填像素各自沿"最近的方向"
 *       取最近不透明像素的颜色；细缝左右两侧各取各的，中线自然分界；
 *     - SideA / SideB 方向取色：对每个镂空连通域求主轴（PCA），
 *       垂直主轴的两个方向即"两侧"（规范成 左/上 与 右/下），
 *       每个像素沿指定方向步进取第一碰到的参考色——整条细缝
 *       整体取某一侧的颜色；射线扫空（沿缝平行逃逸）自动回退
 *       最近邻颜色，无方向可言的孤立小域同样回退。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回填充的像素数（0 = 未检测到镂空，图像不变）。
 */
namespace HoleFiller
{
    enum FillMode
    {
        NearestFill = 0, ///< 自动：每像素取最近不透明像素颜色（中线分界）
        SideA = 1,       ///< 方向一：沿主轴垂直方向一侧（竖缝=左，横缝=上）整体取色
        SideB = 2,       ///< 方向二：另一侧（竖缝=右，横缝=下）整体取色
    };

    int fillHoles(QImage& img, int mode = NearestFill);
}

#endif // HOLEFILLER_H
