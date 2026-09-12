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
 *  4. 填充（交互式试错，算法不替用户选方向）：
 *     每个待填像素沿用户点击循环指定的方向（左/右/上/下）步进，
 *     取第一碰到的参考色；射线沿缝平行逃逸出画面（扫空）时
 *     回退到最近邻颜色，保证任何方向都能填满。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回填充的像素数（0 = 未检测到镂空，图像不变）。
 */
namespace HoleFiller
{
    enum FillMode
    {
        TakeLeft = 0,  ///< 左取色：每个待填像素向左步进取第一参考色
        TakeRight = 1, ///< 右取色
        TakeUp = 2,    ///< 上取色
        TakeDown = 3,  ///< 下取色
        NearestFill = 4, ///< 最近邻（每像素取最近参考色，中线分界）——内部回退基准，UI 不暴露
    };

    int fillHoles(QImage& img, int mode = TakeLeft);
}

#endif // HOLEFILLER_H
