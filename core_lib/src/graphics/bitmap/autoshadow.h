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
    double lightX = 0.15;                     // 光源 X：图像宽度归一化坐标（0=左缘 1=右缘，可越界放远光）
    double lightY = 0.05;                     // 光源 Y：图像高度归一化坐标（0=上缘 1=下缘；Y 小=上方光源）
    int maskThreshold = 128;                  // 去色阈值（灰度 1..254）：≥此值=不透明白（受光面），低于=透明黑
    int chokeMatte = 0;                       // 阻塞遮罩（AE 简单阻塞语义）：正值收缩(阻塞)掩膜、负值扩展（px），小增量修边
    int shadowDistance = 16;                  // 内阴影距离：掩膜沿背光方向的取样平移量，即阴影带深入形体的宽度（px）
    int shadowSize = 8;                       // 内阴影大小：取样掩膜的高斯模糊半径，控制阴影边界的软硬（px，0=硬边）
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

/** 自动上阴影：内阴影模型（PS Inner Shadow 语义）+ CSP 色阶映射。

 * ── 掩膜生成段（黑透白不透）──
 *  1. 原图去色（直通亮度）→ 阈值二值化：灰度 ≥ maskThreshold 为不透明白（受光填色面），
 *     低于阈值为透明黑——线稿与深色区成为掩膜上的洞（凹槽），复杂度骤减；
 *     掩膜整体裁在原图 α 内（不出轮廓）。
 *  2. 简单阻塞（AE Simple Choker 语义）：chokeMatte 以小增量收缩/扩展掩膜边缘——
 *     正值阻塞（收缩白区，吃掉抗锯齿白边与细白丝）、负值扩展（并掉小黑洞），
 *     得到更整洁的掩膜后再进内阴影。
 *
 * ── 内阴影场段（椭圆→加耳朵→任意复杂剪影都成立）──
 *  2. shadow(p) = mask(p) − blur( mask(p + 背光方向·distance) )，钳 0..1：
 *     沿"光源→像素"方向（逐像素径向，光源远≈平行）把掩膜取样平移 distance 再高斯模糊
 *     （σ=shadowSize/2），与原掩膜相减——外轮廓的远光侧出现月牙形阴影带（凸台边），
 *     线稿凹槽出现贴线阴影带（槽壁迎光侧），耳朵/褶皱等任意复杂剪影自动成立。
 *
 * ── 映射段（CSP 色调设置，与前版一致）──
 *  3. 场值 F = shadow×100 → 三阈值切四色阶 / 平滑渐变映射 / 羽化 / 反转，
 *     每阶独立色+混合模式（正常/正片叠底/线性加深），只作用内容像素、α 不变。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回被修改的像素数（0 = 无变化）。
 */
namespace AutoShadow
{
    int apply(QImage& img, const AutoShadowParams& params);

    /** 遮罩视图（AE 简单阻塞的 Mask 视图同款）：黑白图——白=不透明、黑=透明（含画布空白），
        返回 Format_ARGB32_Premultiplied，尺寸与 img 相同，不改 img。 */
    QImage renderMattePreview(const QImage& img, const AutoShadowParams& params);
}

#endif // AUTOSHADOW_H
