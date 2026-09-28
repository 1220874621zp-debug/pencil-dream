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
    double x = 0.32;       // 光源 X：图像宽度归一化坐标（0=左缘 1=右缘，可越界放远光）
    double y = 0.38;       // 光源 Y：图像高度归一化坐标（0=上缘 1=下缘；Y 小=上方光源）
    int height = 218;      // 光源高度 0..300（%）：以 max(对角线, 光到内容距离) 为基——100≈45°仰角，越大越顶光
    int intensity = 100;   // 强度 0..100：多光源叠加照明（照度=Σ 强度·max(0,N·L)），0=该光源关闭
};

struct AutoShadowParams
{
    QVector<AutoShadowLight> lights = { AutoShadowLight{} }; // 光源列表（≥1，CSP 添加光源同款）：
                                                             // 光源 1=主光（分区渐变沿它定向），其余=辅光只补 N·L 照明
    int maskThreshold = 238;                  // 去色阈值（灰度 1..254）：≥此值=不透明白（受光面），低于=透明黑
    int normalStrength = 100;                 // 阴影总强度 0..100：场值=强度·(1−照度)，0=无变化
    int formHeight = 1;                       // 体积高度 1..40：高度场斜率放大（倍）——调大交界过渡变陡
    int formRadius = 2000;                    // 部件最大半径 8..2000（px）：区内球冠半径封顶（巨域防横切）
    int formSmooth = 40;                      // 形体圆滑度（px）：高度场高斯模糊 σ，圆化渐变折点与球冠棱
    int regionTolerance = 26;                 // 分区颜色容差（Lab ΔE 0..100）：白区内色差≤容差并入同一色块，
                                             // 纯色稿默认即可，带压缩噪点/轻渐变的图适当调大
    int regionDomeWeight = 35;                // 圆顶混合 0..100：分区高度场里方向渐变与球冠的配比——
                                             // 0=纯方向渐变（斜面感、单向明暗），100=纯球冠（枕头感、轮廓圆角）
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

/** 自动上阴影：分区四色渐变法线卡渲（纯色块/扁平图）+ 黑透白不透门控。

 * ── 分区段 ──
 *  1. 掩膜：去色阈值二值化（线稿与深色区=洞）+ 去椒盐。
 *  2. 颜色分区：白区内按 Lab ΔE 4-连通泛洪（种子锚定防渐变漂移，线稿/洞是墙），
 *     每区统计 dmax（区内切半径，球冠用）与到主光距离范围 dlMin/dlMax。
 *
 * ── 高度场段（分区四色渐变）──
 *  3. 每区沿「到主光距离」在自身范围内归一化 t，刷凹形四段折线渐变 ramp4(t)·E
 *     （标定 0→0.45→0.78→1，段斜率 1.35/0.99/0.66——近光侧陡迎光、远侧趋平；
 *     E=区域沿光向尺度，斜率与色块大小无关=尺度不变），与区内自适应球冠
 *     （z=√(2Dd−d²)，D=min(dmax,formRadius)）按圆顶混合配比：渐变给沿光向
 *     单向明暗（每块各自重起渐变，交界线横跨每个色块），球冠补轮廓圆角与贴线暗带。
 *  4. formSmooth 高斯圆滑 → 高度场法线 N=normalize(−∂z/∂x,−∂z/∂y,1)（z=formHeight·h）
 *     → 多光照度 = Σ 强度i·max(0, N·L_i)（主光定向渐变，辅光补照明）
 *     → 阴影深度 = 1−照度。
 *
 * ── 映射段（CSP 色调设置）──
 *  5. 场值 = 总强度·阴影深度 ×100，黑透白不透门控：只有白区显示阴影，
 *     线稿与洞完全不动。场值进三阈值四色阶/平滑渐变/羽化/反转
 *     （每阶独立色+混合模式+不透明度）。
 *  6. 排线输出（可选，漫画网点）：色阶覆盖度再乘固定角度斜线图案（线内上色、
 *     线隙透出原图），画面呈黑白漫画排线质感。
 *
 * img 原地修改，须为 Format_ARGB32_Premultiplied。
 * 返回被修改的像素数（0 = 无变化）。
 */
namespace AutoShadow
{
    int apply(QImage& img, const AutoShadowParams& params);

    /** 遮罩视图：黑白图——白=不透明、黑=透明（含画布空白），
        返回 Format_ARGB32_Premultiplied，尺寸与 img 相同，不改 img。 */
    QImage renderMattePreview(const QImage& img, const AutoShadowParams& params);
}

#endif // AUTOSHADOW_H
