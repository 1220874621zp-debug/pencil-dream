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

/** 色阶混合模式（对原图做乘性/替换式混合，CSP 自动阴影同款语义） */
enum class AutoShadowBlendMode
{
    Normal,      // 正常：直通色按该像素 α 替换
    Multiply,    // 正片叠底：c·s/255，压暗保细节
    LinearBurn,  // 线性加深：c+s−255，比正片叠底更沉
};

/** 一个色阶：独立颜色 + 独立混合模式 */
struct AutoShadowLevel
{
    QRgb color = qRgb(255, 255, 255);
    AutoShadowBlendMode mode = AutoShadowBlendMode::Multiply;
};

struct AutoShadowParams
{
    int lightAngle = 135;                     // 0..359 光源方向角（数学角、逆时针：0=右 90=上 135=左上 270=下）
    int lightDistance = 600;                  // 光源到内容中心的像素距离：近=场弯成圆弧，远=接近平直
    int displaceStrength = 8;                 // 置换强度：按原图亮度置换场采样点，色阶边界贴合线稿起伏（px）
    int thresholds[3] = { 20, 45, 80 };       // 色阶阈值：归一化场值 0..100，递增，切出 4 个色阶
    int edgeFeather = 0;                      // 色调分离模式下阈值过渡带宽（场值单位），0=硬边
    bool smooth = false;                      // false=色调分离阴影（分阶），true=平滑阴影（连续梯度映射）
    bool invertLevels = false;                // 反转应用色阶的顺序（色带 1↔4 镜像）
    // 默认色带：受光暖黄→橙→洋红→背光蓝紫（CSP 截图同款暖到冷序列）
    AutoShadowLevel levels[4] = {
        { qRgb(255, 244, 186), AutoShadowBlendMode::Multiply },
        { qRgb(255, 191, 128), AutoShadowBlendMode::Multiply },
        { qRgb(255, 92, 158), AutoShadowBlendMode::LinearBurn },
        { qRgb(96, 76, 176), AutoShadowBlendMode::Multiply },
    };
};

/** 自动上阴影：CSP「自动阴影」参数模型的两段式实现。

 * ── 场生成段（径向渐变充当光源）──
 *  1. 光场 g(p) = p 到光源点的距离 − r0（r0=内容上离光源最近点，归零起步），
 *     按内容最大场值归一化到 0..100（阈值与画幅/角色大小无关）。
 *  2. 置换贴图（贴图=原图亮度）：F(p) = g(p + 强度×(亮度−0.5)×2)，双轴等强度、
 *     双线性采样；色阶边界随线稿明暗起伏。场在全图有定义，置换无越界漏光问题。
 *
 * ── 映射段（CSP 色调设置）──
 *  3. 阈值 [t1,t2,t3] 把场切成 4 个色阶；每阶独立颜色+混合模式（正常/正片叠底/
 *     线性加深），反转=色带镜像（1↔4）。
 *  4. 权重统一框架：色调分离=阶跃权重（edgeFeather>0 时阈值两侧 smoothstep 过渡）；
 *     平滑=相邻色阶间分段线性插值（连续梯度映射）。out = Σ wᵢ·blend(原图, 色阶ᵢ)，
 *     混合按各阶模式独立计算后按权重混合，模式不同也能连续过渡。
 *  5. 只作用于不透明内容像素（α≥16），α 不变，透明像素不动。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回被修改的像素数（0 = 无变化）。
 */
namespace AutoShadow
{
    int apply(QImage& img, const AutoShadowParams& params);
}

#endif // AUTOSHADOW_H
