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
 *  1. 镂空定位（限步测地重建）：镂空像素 RGB 不可信，只看 alpha。
 *     先腐蚀透明掩码得到"宽区核心"——宽度 ≤ ~10px 的缝、细通道、小斑点
 *     被整条抹掉，其背后的镂空因此与外界断开；再从核心出发只在透明像素
 *     内做 ≤5 步十字测地膨胀补回真背景（核心区连同自身边缘环完整恢复，
 *     但走不出任何 ≥1px 的不透明墙，也不会顺着细通道爬进深处）。其余
 *     透明像素（缝本身、封闭镂空、窄通道背后的镂空、小斑点）全部进入
 *     填充范围，实现"图像区域内部没有任何镂空"。贴边 5px 内的透明按
 *     开放处理（不封画布边上的缝）；
 *  2. 只填镂空本身：填充范围 = 检测出的透明（alpha≈0）像素，绝不向
 *     抗锯齿半透明边缘膨胀——0 < alpha < 255 的边缘像素一律不碰，
 *     色块轮廓/笔画边缘保持原样不外溢；
 *  3. 填充（交互式试错，算法不替用户选方向）：
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
