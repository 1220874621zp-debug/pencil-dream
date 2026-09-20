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
#include <QVector>

class QImage;

/** 色阶混合模式（PS/AE 语义，直通域公式后回混不透明度、重预乘） */
enum class AutoShadowBlendMode
{
    Normal,       // 正常：直通色按该像素 α 替换
    Multiply,     // 正片叠底：c·s，压暗保细节（阴影首选）
    LinearBurn,   // 线性加深：c+s−1，比正片叠底更沉
    Darken,       // 变暗：min(c,s)
    ColorBurn,    // 颜色加深：1−min(1,(1−c)/s)，对比更强的加深
    Lighten,      // 变亮：max(c,s)
    Screen,       // 滤色：c+s−c·s，提亮（高光/受光面可用）
    Overlay,      // 叠加：暗部正片叠底、亮部滤色
    SoftLight,    // 柔光：温和的叠加
    HardLight,    // 强光：以混合色决定正片叠底/滤色
    LinearDodge,  // 线性减淡（添加）：c+s
};

/** 一个色阶：独立颜色 + 独立混合模式 + 独立不透明度 */
struct AutoShadowLevel
{
    QRgb color = qRgb(255, 255, 255);
    AutoShadowBlendMode mode = AutoShadowBlendMode::Multiply;
    int opacity = 100; // 0..100：混合结果按此比例回混原色（100=全强度）
};

/** 一个光源（CSP 光源设置语义）：位置 + 仰角高度 + 强度 */
struct AutoShadowLight
{
    double x = 0.15;       // 光源 X：图像宽度归一化坐标（0=左缘 1=右缘，可越界放远光）
    double y = 0.05;       // 光源 Y：图像高度归一化坐标（0=上缘 1=下缘；Y 小=上方光源）
    int height = 150;      // 光源高度 0..300（%）：以 max(对角线, 光到内容距离) 为基——100≈45°仰角，越大越顶光
    int intensity = 100;   // 强度 0..100：多光源叠加照明（照度=Σ 强度·max(0,N·L)），0=该光源关闭
};

struct AutoShadowParams
{
    QVector<AutoShadowLight> lights = { AutoShadowLight{} }; // 光源列表（≥1，CSP 添加光源同款）
    int maskThreshold = 128;                  // 去色阈值（灰度 1..254）：≥此值=不透明白（受光面），低于=透明黑
    int chokeMatte = 0;                       // 阻塞遮罩（AE 简单阻塞语义）：正值收缩(阻塞)掩膜、负值扩展（px）
    int gradientStrength = 30;                // 圆形渐变强度 0..100：离光源越远越暗的底场（与体积场叠加）
    int normalStrength = 100;                 // 体积法线强度 0..100：SDF 伪法线 N·L 形体明暗交界线（主阴影场）
    int formHeight = 6;                       // 体积高度 1..40：伪高度场的 z 放大——越大形体越鼓、明暗交界越贴近边缘
    int formRadius = 30;                      // 部件半径 1..200（px）：半椭球丘的鼓起半径——每个色块鼓成球冠，
                                               // 法线在部件内部连续放射（球面 lambert），明暗交界线横切形体中部；
                                               // 越大交界线越往部件中心移（脸颊/躯干出横切宽面），越小越贴线稿
    int formSmooth = 6;                       // 形体圆滑度（px）：伪高度场的高斯模糊 σ，越大丘顶越圆、交界线越弧
    int occlusionStrength = 0;                // 遮挡强度 0..100：沿射向光源采样掩膜，洞在光路上投出遮挡阴影（px 半径）
    int thresholds[3] = { 20, 45, 80 };       // 色阶阈值：归一化场值 0..100，递增，切出 4 个色阶
    int edgeFeather = 0;                      // 色调分离模式下阈值过渡带宽（场值单位），0=硬边
    bool smooth = false;                      // false=色调分离阴影（分阶），true=平滑阴影（连续梯度映射）
    bool invertLevels = false;                // 反转应用色阶的顺序（色带 1↔4 镜像）
    bool hatch = false;                       // 排线输出（漫画网点）：色阶改为固定角度斜线图案，线隙透出原图
    int hatchAngle = 135;                     // 排线角度（度，0..180）
    int hatchSpacing = 6;                     // 排线间距（px，1..24）
    // 默认色带：标准阴影（CSP/AI 卡渲同款观感）——受光不动原图、暗部同色系深蓝灰逐级加深；
    // 风格化彩色带（暖橙→洋红→蓝紫）见对话框「风格化彩色」预设
    AutoShadowLevel levels[4] = {
        { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply, 100 },
        { qRgb(128, 136, 172), AutoShadowBlendMode::Multiply, 60 },
        { qRgb(100, 108, 146), AutoShadowBlendMode::Multiply, 80 },
        { qRgb(74, 82, 120), AutoShadowBlendMode::Multiply, 100 },
    };
};

/** 自动上阴影：SDF 伪法线卡渲（Relight 无 AI 近似）+ 黑透白不透门控。

 * ── 掩膜生成段（黑透白不透）──
 *  1. 去色阈值二值化（线稿与深色区成洞/山谷）+ 简单阻塞修边（AE 语义）+ 去椒盐。
 *
 * ── 场生成段：多光源叠加照明，clamp 后 0..100 进映射 ──
 *  2. 圆形渐变（底场）：白区内 到各光源距离−r0 按 vmax 归一化，取各光源最近者
 *     （照明=各光源最强处），离所有光越远越暗。
 *  3. SDF 伪法线 N·L（形体明暗交界线，主阴影场）：掩膜距离变换经**半椭球剖面**
 *     （dNorm=min(1,d/部件半径)，z=√(2·dNorm−dNorm²)）当伪高度场——每个连通区域
 *     鼓成球冠而非平顶台地、线稿/洞是丘间山谷；按 σ 圆滑后取梯度得伪法线 N
 *     （球面 lambert：部件内部法线连续放射），照度 = Σ 强度i·max(0, N·L_i)——
 *     多光源互补照明，所有光都照不到的坡面才全暗；阴影深度 = 1−照度。
 *     明暗交界线横切形体中部（脸颊弧线、脖子横切宽面），贴线阴影自动成立
 *     （山谷两侧法线相背）。这是 AE 卡渲 Relight 类插件（AI 法线打光）的解析式
 *     近似：AI 用网络估计表面朝向，我们用"角色=圆润体积"的几何先验构造朝向。
 *  4. 径向遮挡：对每像素沿射向各光源采样掩膜（tent 权重，洞=0、内容外不计入），
 *     任一光源光路通畅即无遮挡（取各光源受阻最轻者）——洞/前层在所有光源的
 *     背光侧才投出遮挡阴影。
 *  5. 场 = clamp(渐变强度·渐变 + 体积强度·(1−照度) + 遮挡) ×100。
 *
 * ── 映射段（CSP 色调设置）──
 *  6. 黑透白不透门控：只有白区（掩膜≥0.5）显示阴影，线稿与洞完全不动。
 *     场值进三阈值四色阶/平滑渐变/羽化/反转（每阶独立色+混合模式+不透明度）。
 *  7. 排线输出（可选，漫画网点）：色阶覆盖度再乘固定角度斜线图案（线内上色、
 *     线隙透出原图），画面呈黑白漫画排线质感。
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
