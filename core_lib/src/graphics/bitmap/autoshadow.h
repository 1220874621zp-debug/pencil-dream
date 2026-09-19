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
    int lightDistance = 600;                  // 光源到内容中心的像素距离：近=圆形径向渐变，远=接近平行条带
    int shadowRange = 40;                     // 第一档阈值：到光源的距离（以最近点为 0）≥ 此值的像素进入阴影（px）
    int secondLevelRange = 0;                 // 第二档阈值（px），0 = 单层；>0 时须大于 shadowRange
    QRgb shadowColor = qRgb(150, 130, 200);   // 阴影色（正片叠底语义）
    int shadowOpacity = 45;                   // 0..100 第一档浓度；第二档浓度为其 1.5 倍（封顶 100）
    int choke = 2;                            // 阻塞：阴影掩膜向四周膨胀的像素数（咬进线条，裁回原内容边界）
};

/** 自动上阴影：平涂画面一键叠赛璐璐阴影（CSP「Shading Assist」的程序化近似）。

 * 原理 = 径向渐变 + 色调分离 + 简单阻塞（对不透明掩膜 M）：
 *  1. 径向渐变充当光源：值场 v(p) = 像素到光源点 L 的欧氏距离（L 近=以 L 为圆心的
 *     圆形径向渐变，L 远=接近平行的线性条带）。以 M 上离 L 最近的点归零起步，
 *     近端全亮、远端渐暗。
 *  2. 色调分离：v ≥ shadowRange 为第一档阴影，v ≥ secondLevelRange 为第二档
 *     （更暗），连续渐变被切成硬边断层色阶（赛璐璐观感）。
 *  3. 简单阻塞：色阶掩膜膨胀 choke px 后裁回 M，阴影边界咬进线条、不出内容边界。
 *  4. 上色：预乘域逐通道正片叠底 c' = c·((1-k)+k·s/255)，第一档 k=浓度、
 *     第二档 k=1.5×浓度；α 不变，透明像素不动。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回被修改的像素数（0 = 无变化）。
 */
namespace AutoShadow
{
    int apply(QImage& img, const AutoShadowParams& params);
}

#endif // AUTOSHADOW_H
