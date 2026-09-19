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
    int lightDistance = 600;                  // 光源到内容中心的像素距离，越远越接近平行光（≥4000 按平行光处理）
    int shadowRange = 40;                     // 第一档阈值：朝光源方向的材料深度 ≥ 此值的像素进入阴影（px）
    int secondLevelRange = 0;                 // 第二档阈值（px），0 = 单层；>0 时须大于 shadowRange
    QRgb shadowColor = qRgb(150, 130, 200);   // 阴影色（正片叠底语义）
    int shadowOpacity = 45;                   // 0..100 第一档浓度；第二档浓度为其 1.5 倍（封顶 100）
    int choke = 2;                            // 阻塞：阴影掩膜向四周膨胀的像素数（咬进线条，裁回原内容边界）
};

/** 自动上阴影：平涂画面一键叠赛璐璐阴影（CSP「Shading Assist」的程序化近似）。

 * 原理（对不透明掩膜 M）：
 *  1. 方向深度场：对 M 内每像素沿光线方向（点光源=指向光源；平行光=恒定方向）
 *     步进采样，累计在 M 内走过的欧氏长度 d(p)——朝光面边缘 d≈0（受光），
 *     背光凹陷处 d 大（阴影）；线宽/形体厚度天然计入 d，即「径向渐变」光场。
 *  2. 预阻塞：对 M 做半径 2 的闭运算封住 ≤2px 的漏光细缝，光不再穿过
 *     笔缝漏到背光侧（闭运算只用于行进判定，不上色）。
 *  3. 色调分离：d ≥ shadowRange 为第一档，d ≥ secondLevelRange 为第二档
 *     （更深的凹陷压得更暗），得到硬边色阶阴影。
 *  4. 阻塞：色阶掩膜膨胀 choke px 后裁回 M，阴影边界咬进线条、不留亮缝。
 *  5. 上色：预乘域逐通道正片叠底 c' = c·((1-k)+k·s/255)，第一档 k=浓度、
 *     第二档 k=1.5×浓度；α 不变，透明像素不动。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回被修改的像素数（0 = 无变化）。
 */
namespace AutoShadow
{
    constexpr int PARALLEL_DIST = 4000; // 光源距离 ≥ 此值按平行光处理（预览缩放须保持该语义）

    int apply(QImage& img, const AutoShadowParams& params);
}

#endif // AUTOSHADOW_H
