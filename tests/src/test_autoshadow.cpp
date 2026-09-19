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

// 基线：顶光、只有圆形渐变底场（无遮挡/无发射），四阶全白正片叠底=无变化
AutoShadowParams plainParams()
{
    AutoShadowParams p;
    p.lightX = 0.5;
    p.lightY = -50.0;         // 画面正上方远光 ≈ 平行
    p.gradientStrength = 100;
    p.occlusionStrength = 0;
    p.emissionStrength = 0;
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
    p.lightX = -50.0;                                    // 左侧远光
    p.lightY = 0.5;
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(12, 50) == qRgb(255, 255, 255));   // 白区近光：受光
    REQUIRE(img.pixel(35, 50) == qRgb(0, 0, 0));         // 白区远光：阶4黑
    REQUIRE(img.pixel(45, 50) == qRgb(0, 0, 0));         // 洞（黑区）：完全不动，保持原黑
}

TEST_CASE("AutoShadow-normal-emission")
{
    // 只有法线发射（BWF stage4）：宽条 x[10,49]，发射从边缘沿法线伸入，步长 2、长度 16；
    // 发射落点在距边缘 2/4/... 像素处，val=(1-d/17)²：t=4→F≈58、t=8→F≈28、t≥12→F<20
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 49, 89, qRgb(255, 255, 255));

    AutoShadowParams p = plainParams();
    p.gradientStrength = 0;
    p.emissionStrength = 100;
    p.emissionLength = 16;
    p.levels[1] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(30, 50) == qRgb(255, 255, 255));   // 离各边都超过 16：无发射
    REQUIRE(img.pixel(30, 73) == qRgb(255, 255, 255));   // 距底缘 16：F≈0.3
    REQUIRE(img.pixel(30, 77) == qRgb(255, 255, 255));   // 距底缘 12：F≈8.7
    REQUIRE(img.pixel(30, 81) == qRgb(128, 128, 128));   // 距底缘 8：F≈28
    REQUIRE(img.pixel(30, 85) == qRgb(128, 128, 128));   // 距底缘 4：F≈58
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
    p.lightX = -50.0;
    p.lightY = 0.5;

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
