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

// 基线：顶光、只有圆形渐变底场（无遮挡/无法线），四阶全白正片叠底=无变化
AutoShadowParams plainParams()
{
    AutoShadowParams p;
    p.lights[0].x = 0.5;
    p.lights[0].y = -50.0;    // 画面正上方远光 ≈ 平行
    p.gradientStrength = 100;
    p.normalStrength = 0;     // 纯渐变基线：关掉 SDF 伪法线场
    p.occlusionStrength = 0;
    for (int i = 0; i < 4; ++i)
        p.levels[i] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
    return p;
}

// 竖白条 x[10,19] y[10,89]：顶光下渐变场 F≈100·(y-10)/79.0025（v4 同几何）
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

TEST_CASE("AutoShadow-gradient-bands")
{
    // 只有圆形渐变：白条（掩膜=整条）顶光，F≈1.266·(y-10)，阈值[20,45,80]切带 y=26/46/74
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 20) == qRgb(255, 255, 255));   // F≈12.7 阶1：受光
    REQUIRE(img.pixel(15, 30) == qRgb(0, 0, 0));         // F≈25.3 阶2
    REQUIRE(img.pixel(15, 60) == qRgb(128, 128, 128));   // F≈63.3 阶3
    REQUIRE(img.pixel(15, 80) == qRgb(64, 64, 64));      // F≈88.6 阶4
    REQUIRE(img.pixel(5, 60) == 0);                      // 掩膜外透明像素不动
    REQUIRE(qAlpha(img.pixel(15, 80)) == 255);           // 乘性混合不动 α
}

TEST_CASE("AutoShadow-matte-gates-display")
{
    // 黑透白不透显示阴影：黑区（洞）完全不动，白区照常上阴影
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 39, 89, qRgb(255, 255, 255));
    fillRect(img, 40, 10, 49, 89, qRgb(0, 0, 0));        // 深色区成洞

    AutoShadowParams p = plainParams();
    p.lights[0].x = -50.0;                               // 左侧远光
    p.lights[0].y = 0.5;
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(12, 50) == qRgb(255, 255, 255));   // 白区近光：受光
    REQUIRE(img.pixel(35, 50) == qRgb(0, 0, 0));         // 白区远光：阶4黑
    REQUIRE(img.pixel(45, 50) == qRgb(0, 0, 0));         // 洞（黑区）：完全不动，保持原黑
}

TEST_CASE("AutoShadow-normal-terminator")
{
    // 只有 SDF 伪法线 N·L（主阴影场）：宽条 x[10,49]，左侧远光（约 45° 仰角）——
    // 条成一座丘（丘脊 x≈29.5），左坡迎光亮、右坡背光暗，明暗交界线横切形体。
    // 场值实测 y=50：x=14→15、28→0、29→14（阶1 白）、30→63（阶3）、31→100（阶4）
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 49, 89, qRgb(255, 255, 255));

    AutoShadowParams p = plainParams();
    p.gradientStrength = 0;
    p.normalStrength = 100;
    p.lights[0].height = 100;
    p.lights[0].x = -50.0;   // 左侧远光
    p.lights[0].y = 0.5;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(14, 50) == qRgb(255, 255, 255));   // 左坡：迎光=阶1
    REQUIRE(img.pixel(28, 50) == qRgb(255, 255, 255));   // 丘脊左侧：仍受光
    REQUIRE(img.pixel(29, 50) == qRgb(255, 255, 255));   // 丘脊像素：F≈14=阶1
    REQUIRE(img.pixel(30, 50) == qRgb(128, 128, 128));   // 脊右一像素：F≈63=阶3（交界带）
    REQUIRE(img.pixel(31, 50) == qRgb(64, 64, 64));      // 再右一像素：F=100=阶4（背光）
    REQUIRE(img.pixel(45, 50) == qRgb(64, 64, 64));      // 右坡深处：阶4
    REQUIRE(img.pixel(5, 50) == 0);                      // 掩膜外透明像素不动
    REQUIRE(qAlpha(img.pixel(45, 50)) == 255);           // 乘性混合不动 α
}

TEST_CASE("AutoShadow-normal-flat-plateau")
{
    // 大方形 x[5,54]，左中光——左缘坡迎光亮、上缘坡背光暗、丘顶居中：
    // 场值实测 (8,30)→14（左缘，阶1）、(30,30)→58（丘顶，阶3）、(30,8)→87（上缘，阶4）
    QImage img = makeImage(60, 60);
    fillRect(img, 5, 5, 54, 54, qRgb(255, 255, 255));

    AutoShadowParams p = plainParams();
    p.gradientStrength = 0;
    p.normalStrength = 100;
    p.lights[0].height = 100;
    p.lights[0].x = -50.0;
    p.lights[0].y = 0.5;     // 光在左中：中部像素 ly≈0
    p.thresholds[0] = 20;
    p.thresholds[1] = 50;
    p.thresholds[2] = 70;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(8, 30) == qRgb(255, 255, 255));    // 左缘坡：迎光=阶1（F≈14）
    REQUIRE(img.pixel(30, 30) == qRgb(128, 128, 128));   // 丘顶：F≈58=阶3
    REQUIRE(img.pixel(30, 8) == qRgb(64, 64, 64));       // 上缘坡：背光=阶4（F≈87）
}

TEST_CASE("AutoShadow-normal-groove")
{
    // 贴线阴影：两白条夹一条透明山谷（x=40 线稿槽），左侧远光——
    // 左条整条成丘，右坡背光；山谷左壁暗带渐弱入谷；山谷右壁迎光亮缘；山谷本身不动。
    // 场值实测 y=20：x=13→4、27→67、36→86、38→62、42→10、65→100
    QImage img = makeImage(80, 40);
    fillRect(img, 10, 10, 39, 29, qRgb(255, 255, 255));
    fillRect(img, 41, 10, 69, 29, qRgb(255, 255, 255));  // x=40 留空=山谷

    AutoShadowParams p = plainParams();
    p.gradientStrength = 0;
    p.normalStrength = 100;
    p.lights[0].height = 100;
    p.lights[0].x = -50.0;
    p.lights[0].y = 0.5;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(13, 20) == qRgb(255, 255, 255));   // 左条外缘坡：迎光=阶1
    REQUIRE(img.pixel(27, 20) == qRgb(128, 128, 128));   // 左条右坡：F≈67=阶3
    REQUIRE(img.pixel(36, 20) == qRgb(64, 64, 64));      // 山谷左壁：F≈86=阶4（贴线暗带）
    REQUIRE(img.pixel(38, 20) == qRgb(128, 128, 128));   // 近谷底：F≈62=阶3（渐弱入谷）
    REQUIRE(img.pixel(40, 20) == 0);                     // 山谷线稿：门控不动
    REQUIRE(img.pixel(42, 20) == qRgb(255, 255, 255));   // 山谷右壁：F≈10=阶1（迎光亮缘）
    REQUIRE(img.pixel(65, 20) == qRgb(64, 64, 64));      // 右条外缘坡：F=100=阶4
}

TEST_CASE("AutoShadow-multi-light-opposite")
{
    // 多光源互补照明：宽条 x[10,49]，左右各一盏对称远光（45° 仰角）——
    // 单左光时右坡 F=100 全暗；加右光后照度=Σ max(0,N·L) 只增不减，
    // 右坡被右光照亮（灰阶变浅）、丘顶照度饱和仍全亮。灰阶单色带保证场值→灰度单调。
    const auto grayRamp = [](AutoShadowParams& p) {
        p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
        p.levels[1] = { qRgb(200, 200, 200), AutoShadowBlendMode::Multiply };
        p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
        p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };
    };
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 49, 89, qRgb(255, 255, 255));

    AutoShadowParams single = plainParams();
    single.gradientStrength = 0;
    single.normalStrength = 100;
    single.lights[0].height = 100;
    single.lights[0].x = -50.0;
    single.lights[0].y = 0.5;
    grayRamp(single);
    REQUIRE(AutoShadow::apply(img, single) > 0);
    REQUIRE(img.pixel(45, 50) == qRgb(64, 64, 64));    // 基线：右坡深处=阶4
    REQUIRE(img.pixel(29, 50) == qRgb(255, 255, 255)); // 丘顶=阶1

    QImage img2 = makeImage(60, 100);
    fillRect(img2, 10, 10, 49, 89, qRgb(255, 255, 255));
    AutoShadowParams dual = single;
    AutoShadowLight second;
    second.x = 50.0;
    second.y = 0.5;
    second.height = 100;
    dual.lights.append(second);
    REQUIRE(AutoShadow::apply(img2, dual) > 0);
    REQUIRE(img2.pixel(29, 50) == qRgb(255, 255, 255)); // 丘顶照度饱和：仍全亮
    REQUIRE(qGray(img2.pixel(45, 50)) > 64);            // 右坡被右光照亮：严格亮于单光
    REQUIRE(qGray(img2.pixel(14, 50)) > 64);            // 左坡同理被左光照住
    REQUIRE(qGray(img2.pixel(45, 50)) >= qGray(img.pixel(45, 50))); // 单调保证：加光不减照度
}

TEST_CASE("AutoShadow-light-intensity-off")
{
    // 强度 0=该光源关闭：照度恒 0 → 场值恒 100 → 整条落最深阶（灰阶带=64）
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 49, 89, qRgb(255, 255, 255));

    AutoShadowParams p = plainParams();
    p.gradientStrength = 0;
    p.normalStrength = 100;
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
    // 排线输出（漫画网点）：纯渐变场 F≈1.266·(y-10)，135° 斜线、间距 4（线宽≈1.33）——
    // 阶3/4 区（F>45）线上=黑、线隙=透出原白；t=0.7071·(y−x) 对 4 取模 <1.33 为线上
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.hatch = true;
    p.hatchAngle = 135;
    p.hatchSpacing = 4;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    // y=60（F≈63=阶3）：x=13 → t=1.234 在线上=黑；x=15 → t=3.820 线隙=原白
    REQUIRE(img.pixel(13, 60) == qRgb(0, 0, 0));
    REQUIRE(img.pixel(15, 60) == qRgb(255, 255, 255));
    // y=80（F≈89=阶4）：x=12 → t=0.083 在线上=黑；x=10 → t=1.497 线隙=原白
    REQUIRE(img.pixel(12, 80) == qRgb(0, 0, 0));
    REQUIRE(img.pixel(10, 80) == qRgb(255, 255, 255));
    // 受光区 y=20（F≈13=阶1 白正片叠底）：排线不可见，保持原色
    REQUIRE(img.pixel(13, 20) == qRgb(255, 255, 255));
}

TEST_CASE("AutoShadow-radial-occlusion")
{
    // 只有径向遮挡：白区 x[10,69]，洞 x[25,29]，左侧光——
    // 洞背光侧（x=30/33）光路被洞挡 → 遮挡阴影；洞迎光侧（x=15）与远处（x=45）不受影响
    QImage img = makeImage(80, 40);
    fillRect(img, 10, 10, 69, 29, qRgb(255, 255, 255));
    fillRect(img, 25, 10, 29, 29, qRgb(0, 0, 0));        // 洞（深色区）

    AutoShadowParams p = plainParams();
    p.gradientStrength = 0;
    p.occlusionStrength = 10;                            // 采样半径 10px
    p.levels[1] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.lights[0].x = -50.0;
    p.lights[0].y = 0.5;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 20) == qRgb(255, 255, 255));   // 迎光侧：射向光源全是白区
    REQUIRE(img.pixel(45, 20) == qRgb(255, 255, 255));   // 远处：采样不经过洞
    REQUIRE(img.pixel(33, 20) == qRgb(128, 128, 128));   // 洞背光侧：光路被挡 F≈33
    REQUIRE(img.pixel(30, 20) == qRgb(128, 128, 128));   // 更贴近洞 F≈49
    REQUIRE(img.pixel(27, 20) == qRgb(0, 0, 0));         // 洞内：门控，完全不动
}

TEST_CASE("AutoShadow-invert-level-order")
{
    // 只把阶1 设为黑、反转 → 黑色带被镜像到最深阴影区；受光区吃原阶4白=不变
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.levels[0] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.invertLevels = true;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 50) == qRgb(255, 255, 255));   // 受光区吃原阶4白→不变
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
    q.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply, 40 };
    REQUIRE(AutoShadow::apply(whiteBar, q) > 0);
    REQUIRE(whiteBar.pixel(15, 80) == qRgb(153, 153, 153));     // 不透明度40%：1+0.4(0−1)=0.6
}

TEST_CASE("AutoShadow-matte-preview-view")
{
    // 遮罩视图（AE 简单阻塞的 Mask 视图）：白=不透明、黑=透明（画布空白也是黑）
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();

    QImage view = AutoShadow::renderMattePreview(img, p);
    REQUIRE(view.format() == QImage::Format_ARGB32_Premultiplied);
    REQUIRE(view.size() == img.size());
    REQUIRE(view.pixel(15, 50) == qRgb(255, 255, 255));
    REQUIRE(view.pixel(15, 89) == qRgb(255, 255, 255));
    REQUIRE(view.pixel(5, 50) == qRgb(0, 0, 0));

    p.chokeMatte = 3;
    QImage choked = AutoShadow::renderMattePreview(img, p);
    REQUIRE(choked.pixel(15, 50) == qRgb(255, 255, 255));
    REQUIRE(choked.pixel(15, 66) == qRgb(255, 255, 255));
    REQUIRE(choked.pixel(15, 88) == qRgb(0, 0, 0));      // 底部 3 行被阻塞成黑
}
