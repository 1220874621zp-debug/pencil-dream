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
#include "catch.hpp"

#include "autoshadow.h"

#include <QImage>
#include <QRgb>
#include <cmath>

namespace
{

QImage makeImage(const int w, const int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    return img;
}

// 不透明指定直通色矩形（白色时预乘与直通一致）
void fillRect(QImage& img, const int x0, const int y0, const int x1, const int y1, const QRgb straight)
{
    for (int y = y0; y <= y1; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = x0; x <= x1; ++x)
            line[x] = straight;
    }
}

// 基线：分区四色渐变高度场（圆顶混合 0=纯方向渐变）、顶光、全强度，
// 四阶全白正片叠底=无变化。
AutoShadowParams plainParams()
{
    AutoShadowParams p;
    p.lights[0].x = 0.5;
    p.lights[0].y = -50.0;    // 画面正上方远光 ≈ 平行
    p.lights[0].height = 150;
    p.maskThreshold = 128;
    p.formHeight = 2;
    p.formRadius = 300;
    p.formSmooth = 6;
    p.normalStrength = 100;
    p.regionTolerance = 26;
    p.regionDomeWeight = 0;
    for (int i = 0; i < 4; ++i)
        p.levels[i] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
    return p;
}

// 竖白条 x[10,19] y[10,89]：单色区沿到顶光距离归一化，段1 陡坡迎光、段3 趋平
QImage farLightImage()
{
    QImage img = makeImage(30, 100);
    fillRect(img, 10, 10, 19, 89, qRgb(255, 255, 255));
    return img;
}

} // namespace

TEST_CASE("AutoShadow-format-guard")
{
    QImage img(10, 10, QImage::Format_ARGB32);
    img.fill(Qt::white);
    REQUIRE(AutoShadow::apply(img, AutoShadowParams{}) == 0);
}

TEST_CASE("AutoShadow-empty-image")
{
    QImage img = makeImage(10, 10);
    REQUIRE(AutoShadow::apply(img, AutoShadowParams{}) == 0);
}

TEST_CASE("AutoShadow-white-levels-noop")
{
    QImage img = farLightImage();
    REQUIRE(AutoShadow::apply(img, plainParams()) == 0); // 四阶全白正片叠底=无变化
}

TEST_CASE("AutoShadow-region-gradient")
{
    // 分区四色渐变（核心机制）：两色块无缝相邻——无描边线、掩膜全连通。
    // 按颜色切开（ΔE≈81 > 容差 20），每块沿光向各自重起四段折线渐变
    // （近光段陡坡迎光、远段趋平），交界线横跨每个色块。
    // 低仰角光（height=25≈14°）让三段斜率（×体积高度2：2.7/1.98/1.32）的 N·L
    // 拉开场值差；光源伪高度须远在渐变场表面之上（掠射光会让表面顶过光源、
    // 远侧 L 向量朝下而全黑）。圆顶混合 0（纯方向渐变）+ 细分阈值切带。
    const auto grayRamp = [](AutoShadowParams& p) {
        p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
        p.levels[1] = { qRgb(200, 200, 200), AutoShadowBlendMode::Multiply };
        p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
        p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };
    };
    const auto makeTwoBlocks = [] {
        QImage img = makeImage(80, 40);
        fillRect(img, 10, 10, 39, 29, qRgb(255, 150, 150)); // 左块：浅粉
        fillRect(img, 40, 10, 69, 29, qRgb(150, 255, 150)); // 右块：浅绿（无缝相邻）
        return img;
    };

    AutoShadowParams p = plainParams();
    p.formSmooth = 2;         // 折点轻圆化，采样点避开边界
    p.lights[0].x = -50.0;    // 左侧远光（=主光，渐变沿它定向）
    p.lights[0].y = 0.5;
    p.lights[0].height = 25;  // 低仰角≈14°：平坦区照度低、三段坡照度阶梯拉开
    p.thresholds[0] = 2;
    p.thresholds[1] = 4;
    p.thresholds[2] = 7;
    p.regionTolerance = 20;
    grayRamp(p);

    QImage img = makeTwoBlocks();
    REQUIRE(AutoShadow::apply(img, p) > 0);

    // 采样点离色块交界与渐变折点均 ≥5px（formSmooth 模糊会跨边界/折点混高，
    // 贴边采样的斜率被邻块渐变尾部污染）：段1 斜率 2.7 迎光=阶1 原色，段3
    // 斜率 1.32 趋平=阶4——每块近光亮、远光暗。
    REQUIRE(qGray(img.pixel(16, 20)) > qGray(img.pixel(34, 20)));   // 左块内渐变
    REQUIRE(qGray(img.pixel(46, 20)) > qGray(img.pixel(64, 20)));   // 右块内渐变
    REQUIRE(qGray(img.pixel(46, 20)) > qGray(img.pixel(34, 20)));   // 分区重起：右块近界亮于左块远端
    REQUIRE(img.pixel(5, 20) == 0);                                 // 掩膜外透明像素不动
    REQUIRE(qAlpha(img.pixel(34, 20)) == 255);                      // 乘性混合不动 α
}

TEST_CASE("AutoShadow-region-tolerance-merges")
{
    // 分区容差：容差 0=精确色匹配，近色（ΔE≈2）被切开成两块各自渐变；
    // 容差拉到 30 后并回一块。以两块中缝（左块远端 vs 右块近端）亮度判别：
    // 分开=右块近端重起迎光（亮），合并=左块远端连续变暗（暗）。
    AutoShadowParams p = plainParams();
    p.formSmooth = 2;
    p.lights[0].x = -50.0;
    p.lights[0].y = 0.5;
    p.lights[0].height = 25;
    p.thresholds[0] = 2;
    p.thresholds[1] = 4;
    p.thresholds[2] = 7;
    p.levels[1] = { qRgb(200, 200, 200), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    const auto makeNearBlocks = [] {
        QImage img = makeImage(80, 40);
        fillRect(img, 10, 10, 39, 29, qRgb(255, 150, 150)); // 浅粉
        fillRect(img, 40, 10, 69, 29, qRgb(250, 158, 156)); // 近似粉（ΔE 小）
        return img;
    };

    p.regionTolerance = 0;   // 精确匹配：切开
    QImage split = makeNearBlocks();
    REQUIRE(AutoShadow::apply(split, p) > 0);
    REQUIRE(qGray(split.pixel(46, 20)) > qGray(split.pixel(34, 20))); // 右块重起迎光

    p.regionTolerance = 30;  // 近色并入：一块连续渐变
    QImage merged = makeNearBlocks();
    REQUIRE(AutoShadow::apply(merged, p) > 0);
    REQUIRE(qGray(merged.pixel(46, 20)) < qGray(merged.pixel(20, 20))); // 整条单调：中缝已深处
}

TEST_CASE("AutoShadow-matte-gates-display")
{
    // 黑透白不透显示阴影：黑区（洞）完全不动，白区照常上阴影（分区渐变场）
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 39, 89, qRgb(255, 255, 255));
    fillRect(img, 40, 10, 49, 89, qRgb(0, 0, 0));        // 深色区成洞

    AutoShadowParams p = plainParams();
    p.formSmooth = 2;
    p.lights[0].x = -50.0;                               // 左侧远光
    p.lights[0].y = 0.5;
    p.lights[0].height = 25;
    p.thresholds[0] = 2;
    p.thresholds[1] = 4;
    p.thresholds[2] = 7;
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(12, 50) == qRgb(255, 255, 255));   // 近光段1：迎光=阶1
    REQUIRE(img.pixel(33, 50) == qRgb(0, 0, 0));         // 远光段3：趋平=阶4
    REQUIRE(img.pixel(45, 50) == qRgb(0, 0, 0));         // 洞（黑区）：完全不动，保持原黑
}

TEST_CASE("AutoShadow-multi-light-opposite")
{
    // 多光源互补照明（圆顶分量）：宽条 x[10,49]，左右各一盏对称远光（45° 仰角）。
    // 方向渐变恒沿主光（光源 1）定向，纯斜面（圆顶 0）背光侧辅光照不到；
    // 圆顶混合 100 时坡面法线放射、辅光可照——单左光时右坡 F=100 全暗，
    // 加右光后照度=Σ max(0,N·L) 只增不减，右坡被照亮、丘顶照度饱和仍全亮。
    // 灰阶单色带保证场值→灰度单调。
    const auto grayRamp = [](AutoShadowParams& p) {
        p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
        p.levels[1] = { qRgb(200, 200, 200), AutoShadowBlendMode::Multiply };
        p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
        p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };
    };
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 49, 89, qRgb(255, 255, 255));

    AutoShadowParams single = plainParams();
    single.regionDomeWeight = 100;   // 纯球冠：坡面放射法线对辅光最敏感
    single.lights[0].height = 100;
    single.lights[0].x = -50.0;
    single.lights[0].y = 0.5;
    grayRamp(single);
    REQUIRE(AutoShadow::apply(img, single) > 0);
    REQUIRE(img.pixel(45, 50) == qRgb(64, 64, 64));    // 基线：右坡深处 F=100=阶4
    REQUIRE(img.pixel(24, 50) == qRgb(255, 255, 255)); // 迎光坡：F≈1=阶1

    QImage img2 = makeImage(60, 100);
    fillRect(img2, 10, 10, 49, 89, qRgb(255, 255, 255));
    AutoShadowParams dual = single;
    AutoShadowLight second;
    second.x = 50.0;
    second.y = 0.5;
    second.height = 100;
    dual.lights.append(second);
    REQUIRE(AutoShadow::apply(img2, dual) > 0);
    REQUIRE(img2.pixel(29, 50) == qRgb(255, 255, 255)); // 丘顶照度饱和 F=0：仍全亮（单光时 F≈27=阶2）
    REQUIRE(qGray(img2.pixel(45, 50)) > 64);            // 右坡被右光照亮 F≈19：严格亮于单光
    REQUIRE(qGray(img2.pixel(14, 50)) > 64);            // 左坡同理被左光照住
    REQUIRE(qGray(img2.pixel(45, 50)) >= qGray(img.pixel(45, 50))); // 单调保证：加光不减照度
}

TEST_CASE("AutoShadow-light-intensity-off")
{
    // 强度 0=该光源关闭：照度恒 0 → 场值恒 100 → 整条落最深阶（灰阶带=64）
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 49, 89, qRgb(255, 255, 255));

    AutoShadowParams p = plainParams();
    p.lights[0].height = 100;
    p.lights[0].x = -50.0;
    p.lights[0].y = 0.5;
    p.lights[0].intensity = 0;
    p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
    p.levels[1] = { qRgb(200, 200, 200), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 50) == qRgb(64, 64, 64)); // 近光侧也无光：全条最深阶
    REQUIRE(img.pixel(45, 50) == qRgb(64, 64, 64));
    REQUIRE(img.pixel(5, 50) == 0);                 // 掩膜外不动
}

TEST_CASE("AutoShadow-hatch-pattern")
{
    // 排线输出（漫画网点）：竖白条顶光分区渐变场（段3 F≈9=阶4，段1 F≈0.7=阶1），
    // 135° 斜线、间距 4（线宽≈1.33）——t=0.7071·(y−x) 对 4 取模 <1.33 为线上
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.formSmooth = 0;   // 10px 窄条禁模糊：σ2 磨圆窄脊使法线外翻、稀释竖直照度
    p.lights[0].height = 25;   // 低仰角拉开场值
    p.thresholds[0] = 2;
    p.thresholds[1] = 4;
    p.thresholds[2] = 7;
    p.hatch = true;
    p.hatchAngle = 135;
    p.hatchSpacing = 4;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    // y=80（F≈9=阶4）：x=12 → t=0.7071·68=48.08 对 4 取模 0.08 在线上=黑；
    // x=15 → 45.96 取模 1.96 线隙=透出原白
    REQUIRE(img.pixel(12, 80) == qRgb(0, 0, 0));
    REQUIRE(img.pixel(15, 80) == qRgb(255, 255, 255));
    // 受光区 y=20（F≈0.7<2=阶1 白正片叠底）：排线不可见，保持原色
    REQUIRE(img.pixel(13, 20) == qRgb(255, 255, 255));
}

TEST_CASE("AutoShadow-invert-level-order")
{
    // 只把阶1 设为黑、反转 → 黑色带被镜像到最深阴影区；受光区吃原阶4白=不变
    // （低仰角+细分阈值：受光段2 F≈3=反转后阶2白、远端段3 F≈9=反转后阶4黑）
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.formSmooth = 0;   // 10px 窄条禁模糊：σ2 磨圆窄脊使法线外翻、稀释竖直照度
    p.lights[0].height = 25;
    p.thresholds[0] = 2;
    p.thresholds[1] = 4;
    p.thresholds[2] = 7;
    p.levels[0] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.invertLevels = true;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 50) == qRgb(255, 255, 255));   // 受光区吃白阶→不变
    REQUIRE(img.pixel(15, 80) == qRgb(0, 0, 0));         // 远端吃镜像后的阶4=原阶1黑
}


TEST_CASE("AutoShadow-despeckle-mask")
{
    // 椒盐清理：肤色骑阈值会撒椒盐、法线发射从噪点喷刺——≤3px 孤立连通域翻转；
    // 长细线（大连通域）保留
    QImage img = makeImage(30, 30);
    fillRect(img, 5, 5, 24, 24, qRgb(255, 255, 255));
    img.setPixel(10, 10, qRgb(0, 0, 0));   // 白区孤立黑点
    img.setPixel(2, 2, qRgb(255, 255, 255)); // 黑底孤立白点
    for (int y = 6; y <= 20; ++y)          // 1px 竖细线（15px 长连通域）
        img.setPixel(18, y, qRgb(0, 0, 0));

    AutoShadowParams p = plainParams();
    QImage view = AutoShadow::renderMattePreview(img, p);
    REQUIRE(view.pixel(10, 10) == qRgb(255, 255, 255)); // 黑点清成白
    REQUIRE(view.pixel(2, 2) == qRgb(0, 0, 0));         // 白点清成黑
    REQUIRE(view.pixel(18, 10) == qRgb(0, 0, 0));       // 细线保留
    REQUIRE(view.pixel(15, 15) == qRgb(255, 255, 255));
}

TEST_CASE("AutoShadow-blend-modes-and-opacity")
{
    // 灰128条 + 阈值压到 [1,2,3] → 整条落阶4，逐模式验证直通域公式与不透明度回混
    const auto makeGrayBar = [] {
        QImage img = makeImage(30, 100);
        fillRect(img, 10, 10, 19, 89, qRgb(128, 128, 128));
        return img;
    };
    const auto run = [](QImage img, AutoShadowParams p) {
        p.thresholds[0] = 1;
        p.thresholds[1] = 2;
        p.thresholds[2] = 3;
        REQUIRE(AutoShadow::apply(img, p) > 0);
        return img.pixel(15, 50);
    };
    AutoShadowParams p = plainParams();

    p.levels[3] = { qRgb(255, 255, 255), AutoShadowBlendMode::Screen, 100 };
    REQUIRE(run(makeGrayBar(), p) == qRgb(255, 255, 255));      // 滤色+白：x+1−x=1

    p.levels[3] = { qRgb(0, 64, 0), AutoShadowBlendMode::LinearDodge, 100 };
    REQUIRE(run(makeGrayBar(), p) == qRgb(128, 192, 128));      // 线性减淡：c+s

    p.levels[3] = { qRgb(128, 128, 128), AutoShadowBlendMode::ColorBurn, 100 };
    REQUIRE(run(makeGrayBar(), p) == qRgb(2, 2, 2));            // 颜色加深 1−(127/128)

    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Darken, 100 };
    REQUIRE(run(makeGrayBar(), p) == qRgb(64, 64, 64));         // 变暗：min

    p.levels[3] = { qRgb(255, 255, 255), AutoShadowBlendMode::SoftLight, 100 };
    REQUIRE(run(makeGrayBar(), p) == qRgb(192, 192, 192));      // 柔光+白：x(2−x)

    QImage whiteBar = farLightImage();
    AutoShadowParams q = plainParams();
    q.formSmooth = 2;
    q.lights[0].height = 25;   // 低仰角：远端段3 场值≈8 落阶4
    q.thresholds[0] = 2;
    q.thresholds[1] = 4;
    q.thresholds[2] = 7;
    q.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply, 40 };
    REQUIRE(AutoShadow::apply(whiteBar, q) > 0);
    REQUIRE(whiteBar.pixel(15, 80) == qRgb(153, 153, 153));     // 不透明度40%：1+0.4(0−1)=0.6
}

TEST_CASE("AutoShadow-matte-preview-view")
{
    // 遮罩视图：白=不透明、黑=透明（画布空白也是黑）
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();

    QImage view = AutoShadow::renderMattePreview(img, p);
    REQUIRE(view.format() == QImage::Format_ARGB32_Premultiplied);
    REQUIRE(view.size() == img.size());
    REQUIRE(view.pixel(15, 50) == qRgb(255, 255, 255));
    REQUIRE(view.pixel(15, 89) == qRgb(255, 255, 255));
    REQUIRE(view.pixel(5, 50) == qRgb(0, 0, 0));
}
