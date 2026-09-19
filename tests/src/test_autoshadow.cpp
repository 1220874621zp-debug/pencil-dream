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

// 不透明白色矩形（预乘与直通一致），px == qRgb(255,255,255)
void fillWhiteRect(QImage& img, const int x0, const int y0, const int x1, const int y1)
{
    for (int y = y0; y <= y1; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = x0; x <= x1; ++x)
            line[x] = qRgb(255, 255, 255);
    }
}

// 极端参数基线：无置换、四阶全为白色正片叠底（= 全图不变）
AutoShadowParams plainParams()
{
    AutoShadowParams p;
    p.displaceStrength = 0;
    for (int i = 0; i < 4; ++i)
        p.levels[i] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
    return p;
}

// 远光（画面正上方，Y=-5000%≈平行光）竖条测试通用几何：条 x[10,19] y[10,89]，
// 光源在 (15,-5000)：列 15 场值 v=y-10 精确，内容最大场值≈79.0025，F=100·(y-10)/79.0025
AutoShadowParams farLightParams()
{
    AutoShadowParams p = plainParams();
    p.lightX = 0.5;
    p.lightY = -50.0;
    return p;
}

QImage farLightImage()
{
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);
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
    REQUIRE(AutoShadow::apply(img, farLightParams()) == 0); // 四阶全白正片叠底=无变化
}

TEST_CASE("AutoShadow-four-bands-posterized")
{
    // 阈值[20,45,80]切四阶（硬边）：F≈1.2658·(y-10) → 边界 y≈25.8/45.6/73.2
    // 阶1=白正片叠底(不变255) 阶2=黑正片叠底(0) 阶3=灰128正片叠底(128) 阶4=白线性加深(255)
    QImage img = farLightImage();
    AutoShadowParams p = farLightParams();
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(255, 255, 255), AutoShadowBlendMode::LinearBurn };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 20) == qRgb(255, 255, 255));   // F≈12.7 阶1
    REQUIRE(img.pixel(15, 30) == qRgb(0, 0, 0));         // F≈25.3 阶2
    REQUIRE(img.pixel(15, 60) == qRgb(128, 128, 128));   // F≈63.3 阶3
    REQUIRE(img.pixel(15, 80) == qRgb(255, 255, 255));   // F≈88.6 阶4（线性加深+白=不变）
    REQUIRE(img.pixel(5, 60) == 0);                      // 透明像素不动
    const QRgb px = img.pixel(15, 30);
    REQUIRE(qAlpha(px) == 255);                          // 乘性混合不动 α
}

TEST_CASE("AutoShadow-invert-level-order")
{
    // 反转=色带 1↔4 镜像：阶1↔阶4、阶2↔阶3 互换
    QImage img = farLightImage();
    AutoShadowParams p = farLightParams();
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(255, 255, 255), AutoShadowBlendMode::LinearBurn };
    p.invertLevels = true;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 20) == qRgb(255, 255, 255));   // 原》阶4→白线性加深
    REQUIRE(img.pixel(15, 30) == qRgb(128, 128, 128));   // 原》阶3→灰128
    REQUIRE(img.pixel(15, 60) == qRgb(0, 0, 0));         // 原》阶2→黑
    REQUIRE(img.pixel(15, 80) == qRgb(255, 255, 255));   // 原》阶1→白正片叠底
}

TEST_CASE("AutoShadow-linear-burn-mode")
{
    // 阈值压到 [1,2,3]：除顶部一行外全落阶4；红色线性加深：白底 → 纯红
    QImage img = farLightImage();
    AutoShadowParams p = farLightParams();
    p.thresholds[0] = 1;
    p.thresholds[1] = 2;
    p.thresholds[2] = 3;
    p.levels[3] = { qRgb(255, 0, 0), AutoShadowBlendMode::LinearBurn };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 10) == qRgb(255, 255, 255));   // F=0 阶1
    REQUIRE(img.pixel(15, 50) == qRgb(255, 0, 0));       // 阶4：R 255+255-255，G/B 255+0-255=0
}

TEST_CASE("AutoShadow-normal-mode")
{
    QImage img = farLightImage();
    AutoShadowParams p = farLightParams();
    p.thresholds[0] = 1;
    p.thresholds[1] = 2;
    p.thresholds[2] = 3;
    p.levels[3] = { qRgb(0, 255, 0), AutoShadowBlendMode::Normal };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 50) == qRgb(0, 255, 0));       // 正常=直通色替换
    REQUIRE(qAlpha(img.pixel(15, 50)) == 255);           // α 仍不动
}

TEST_CASE("AutoShadow-smooth-gradient")
{
    // 平滑=连续梯度映射：s1=F/t1 线性、段内线性插值；阶4=灰64正片叠底(→64)
    QImage img = farLightImage();
    AutoShadowParams p = farLightParams();
    p.smooth = true;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 10) == qRgb(255, 255, 255));   // F=0 → 纯阶1
    const int r20 = qRed(img.pixel(15, 20));             // F≈12.66：w1=1-F/20 → ≈94
    REQUIRE(r20 >= 92);
    REQUIRE(r20 <= 96);
    const int r35 = qRed(img.pixel(15, 35));             // F≈31.64：段[t1,t2]插值黑→128 ≈60
    REQUIRE(r35 >= 58);
    REQUIRE(r35 <= 62);
    REQUIRE(img.pixel(15, 89) == qRgb(64, 64, 64));      // 条底行 F≈100 → 纯阶4
}

TEST_CASE("AutoShadow-edge-feather")
{
    // 羽化 20（场值）：t1 两侧 [t1-10, t1+10]=[10,30] smoothstep 过渡
    QImage img = farLightImage();
    AutoShadowParams p = farLightParams();
    p.edgeFeather = 20;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 15) == qRgb(255, 255, 255));   // F≈6.3：过渡带外
    const int r20 = qRed(img.pixel(15, 20));             // F≈12.7：带内浅影 ≈243
    REQUIRE(r20 >= 240);
    REQUIRE(r20 <= 246);
    const int r = qRed(img.pixel(15, 26));               // F≈20.25 ≈ 阈值中心 → ≈123
    REQUIRE(r >= 121);
    REQUIRE(r <= 125);
    REQUIRE(img.pixel(15, 35) == qRgb(0, 0, 0));         // F≈31.6：过渡带外全黑
}

TEST_CASE("AutoShadow-displace-warps-field")
{
    // 宽白条 x[10,39]，置换=按原图亮度偏移场采样：全白 off=+8 向右下采样——
    // (22,45) 原场值 F≈44.3（阶2 黑）；采样点 (30,53) 场值 F≈54.4（阶3 灰128）
    QImage plain = makeImage(50, 100);
    fillWhiteRect(plain, 10, 10, 39, 89);
    QImage displaced = plain;

    AutoShadowParams p = plainParams();
    p.lightX = 0.5;
    p.lightY = -50.0;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };

    AutoShadow::apply(plain, p);
    REQUIRE(plain.pixel(22, 45) == qRgb(0, 0, 0));       // 对照：无置换落阶2

    p.displaceStrength = 8;
    REQUIRE(AutoShadow::apply(displaced, p) > 0);
    REQUIRE(displaced.pixel(22, 45) == qRgb(128, 128, 128)); // 采样偏移后落阶3
}

TEST_CASE("AutoShadow-near-light-arc-boundary")
{
    // 方块 x[10,49] y[10,49]，近光右侧 100px：光在 (129.5,29.5)=(215.8%, 49.2%)，场弯成圆弧——
    // 左边缘中点 F≈96.1、四角 F≈99.6：阈值取 [96,97,98] 后分属不同色阶（平行光下不可能）
    QImage img = makeImage(60, 60);
    fillWhiteRect(img, 10, 10, 49, 49);

    AutoShadowParams p = plainParams();
    p.lightX = 129.5 / 60.0;
    p.lightY = 29.5 / 60.0;
    p.thresholds[0] = 96;
    p.thresholds[1] = 97;
    p.thresholds[2] = 98;
    p.levels[1] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(49, 30) == qRgb(255, 255, 255));   // 右边缘：F≈0 阶1
    REQUIRE(img.pixel(10, 30) == qRgb(0, 0, 0));         // 左边缘中点：F≈96.1 落 [96,97) 阶2
    REQUIRE(img.pixel(10, 11) == qRgb(64, 64, 64));      // 左上角：F≈99.6 ≥98 阶4
    REQUIRE(img.pixel(10, 49) == qRgb(64, 64, 64));      // 左下角同
}
