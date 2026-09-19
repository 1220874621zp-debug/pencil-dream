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
#ifndef AUTOSHADOW_H
#define AUTOSHADOW_H

#include <QRgb>

class QImage;

struct AutoShadowParams
{
    int lightAngle = 135;                     // 0..359 光源方向角（数学角、逆时针：0=右 90=上 135=左上 270=下）
    int lightDistance = 600;                  // 光源到内容中心的像素距离：近=圆形遮罩弯度高，远=接近平直
    int shadowRange = 40;                     // 圆形遮罩半径（以内容最近点为 0）：到光源超出该值开始出现阴影（px）
    int blurRadius = 12;                      // 高斯模糊半径：副本轮廓与遮罩边界的羽化（px）
    int displaceStrength = 8;                 // 置换强度：按原图亮度置换遮罩采样点，边界贴合线稿起伏（px）
    int secondLevelRange = 0;                 // 第二层遮罩半径（px），0 = 单层；>0 时须大于 shadowRange
    QRgb shadowColor = qRgb(150, 130, 200);   // 填充用的阴影色
    int shadowOpacity = 45;                   // 0..100 阴影层透明度
};

/** 自动上阴影：按 AE 合成语义实现（底层=原图，上层=阴影副本，五步效果链）。

 * 流程（AE 语义 → C++）：
 *  1. 复制输入源 + 填充阴影色 + 降低透明度：副本颜色恒为 S、透明度 O，
 *     且保留原图 α（轨道遮罩翻转后阴影只落在内容上，不出轮廓）。
 *  2. 高斯模糊：对副本 α 场做 3 次盒式模糊（近似高斯，O(N) 与半径无关），
 *     轮廓外扩羽化；随后圆形遮罩的羽化宽度也取该半径。
 *  3. 圆形遮罩（add 相交）：以光源点为圆心、r0+shadowRange 为半径（r0=内容上
 *     离光源最近点的距离），smoothstep 羽化；圆内 matte=1（露出底下的原图）、
 *     圆外 matte=0。光源近则边界弯成圆弧、远则接近平直。
 *  4. 置换贴图（贴图=原图亮度）：M'(p) = M(p + 强度×(亮度−0.5)×2)，双轴同强度、
 *     双线性采样；阴影边界随线稿明暗起伏，不再是完美几何圆。
 *  5. 轨道遮罩翻转：阴影 α = α原图×O×(1−M')——matte 空处（远光侧）显阴影色，
 *     matte 实处露出底下的原图；双层=两圈遮罩 a=1−(1−a1)(1−a2)；
 *     最后以普通 over（填充+透明度的合成语义）叠回原图：α 不降、透明像素不动。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回被修改的像素数（0 = 无变化）。
 */
namespace AutoShadow
{
    int apply(QImage& img, const AutoShadowParams& params);
}

#endif // AUTOSHADOW_H
