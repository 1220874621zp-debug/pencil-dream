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
    int chokeMatte = 0;                       // 阻塞遮罩（AE 简单阻塞语义）：正值收缩(阻塞)掩膜、负值扩展（px）
    int gradientStrength = 100;               // 圆形渐变强度 0..100：离光源越远越暗的底场
    int occlusionStrength = 0;                // 遮挡强度 0..100：沿射向光源采样掩膜，洞在光路上投出遮挡阴影（px 半径）
    int emissionStrength = 40;                // 边缘强度 0..100（BWF 法线发射）：边缘沿法线向白区内投衰减阴影带
    int emissionLength = 64;                  // 光线长度（px）：法线发射的深入距离（BWF 同名参数）
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

/** 自动上阴影：圆形渐变 + BWF 式变形 + 黑透白不透门控。

 * ── 掩膜生成段（黑透白不透）──
 *  1. 去色阈值二值化（线稿与深色区成洞）+ 简单阻塞修边（AE 语义）。
 *
 * ── 场生成段：三路合成，clamp 后 0..100 进映射 ──
 *  2. 圆形渐变（底场）：白区内 到光源距离−r0 按 vmax 归一化 0..1，离光越远越暗——
 *     本身就有阴影的感觉。
 *  3. 法线发射（BWF 黑山闪 stage2/4，变形之一）：掩膜边缘点取"指向白区内部"的法线
 *     （模糊掩膜的梯度），沿法线步进 2px、以 (1−d/长度)² 衰减投放软斑——阴影带从每条
 *     边缘顺着局部形状法线流入形体，贴合线稿/耳朵/褶皱，不依赖全局几何。
 *  4. 径向遮挡（变形之二）：对每像素沿射向光源采样掩膜（tent 权重，洞=0、内容外不计入），
 *     白色占比低=光被挡 → 遮挡阴影——渐变带在洞的背光侧断开、变形。
 *  5. 场 = clamp(渐变强度·渐变 + 遮挡强度·遮挡 + 边缘强度·发射) ×100。
 *
 * ── 映射段（CSP 色调设置）──
 *  6. 黑透白不透门控：只有白区（掩膜≥0.5）显示阴影，线稿与洞完全不动。
 *     场值进三阈值四色阶/平滑渐变/羽化/反转（每阶独立色+混合模式）。
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
