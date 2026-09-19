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

// 极端参数基线：四阶全为白色正片叠底（= 全图不变）；顶光、硬边
AutoShadowParams plainParams()
{
    AutoShadowParams p;
    p.lightX = 0.5;
    p.lightY = -50.0;         // 画面正上方远光 ≈ 平行
    p.shadowDistance = 20;
    p.shadowSize = 0;
    for (int i = 0; i < 4; ++i)
        p.levels[i] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
    return p;
}

// 竖白条 x[10,19] y[10,89]：顶光下底部 20px 出内阴影月牙（y∈[70,89] 场值=100）
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

TEST_CASE("AutoShadow-inner-shadow-bottom-band")
{
    // 顶光、距离 20、硬边：底部月牙 y∈[70,89] 场值=100 → 色阶4
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 50) == qRgb(255, 255, 255));   // 取样 y+20=70 在掩膜内：受光
    REQUIRE(img.pixel(15, 69) == qRgb(255, 255, 255));   // 月牙前一行
    REQUIRE(img.pixel(15, 70) == qRgb(0, 0, 0));         // 取样出底边：第一行阴影
    REQUIRE(img.pixel(15, 89) == qRgb(0, 0, 0));         // 底缘最深
    REQUIRE(img.pixel(5, 60) == 0);                      // 掩膜外透明像素不动
    REQUIRE(qAlpha(img.pixel(15, 80)) == 255);           // 乘性混合不动 α
}

TEST_CASE("AutoShadow-groove-casts-shadow")
{
    // 上下两条白块夹 1px 透明缝（模拟线稿槽）：顶光下缝上方正好距离 d 处出现贴线阴影带，
    // 外轮廓底部照常出月牙——复杂度增加无需任何额外处理
    QImage img = makeImage(30, 100);
    fillRect(img, 10, 10, 19, 44, qRgb(255, 255, 255));
    // y=45 为缝
    fillRect(img, 10, 46, 19, 89, qRgb(255, 255, 255));

    AutoShadowParams p = plainParams();
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 15) == qRgb(255, 255, 255));   // 顶缘正对光：受光
    REQUIRE(img.pixel(15, 24) == qRgb(255, 255, 255));   // 缝阴影带前一行
    REQUIRE(img.pixel(15, 25) == qRgb(0, 0, 0));         // 取样 y+20=45 落进缝：贴线阴影
    REQUIRE(img.pixel(15, 26) == qRgb(255, 255, 255));   // 取样回到下块：受光
    REQUIRE(img.pixel(15, 45) == 0);                     // 缝本身透明，不上色
    REQUIRE(img.pixel(15, 80) == qRgb(0, 0, 0));         // 底部月牙照常
}

TEST_CASE("AutoShadow-mask-threshold-black-transparent")
{
    // 黑透白不透：白区 x[10,39] + 黑区 x[40,49]（黑区成洞）。左侧远光：
    // 白区内取样向右（背光）落进黑洞的像素出阴影；黑区本身无阴影
    QImage img = makeImage(60, 20);
    fillRect(img, 10, 5, 39, 14, qRgb(255, 255, 255));
    fillRect(img, 40, 5, 49, 14, qRgb(0, 0, 0));

    AutoShadowParams p = plainParams();
    p.lightX = -50.0;                                    // 画面左侧远光，背光方向≈向右
    p.lightY = 0.5;
    p.shadowDistance = 10;
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(25, 10) == qRgb(255, 255, 255));   // 取样 x+10=35 仍在白区：受光
    REQUIRE(img.pixel(30, 10) == qRgb(0, 0, 0));         // 取样 x+10=40 落进黑洞：阴影
    REQUIRE(img.pixel(35, 10) == qRgb(0, 0, 0));         // 深入阴影带
    REQUIRE(img.pixel(45, 10) == qRgb(0, 0, 0));         // 黑区（洞）：无阴影，保持原黑
}

TEST_CASE("AutoShadow-light-direction")
{
    // 方块 x[10,49] y[10,49]，右侧光：阴影落在左缘（x∈[10,29]），右缘受光
    QImage img = makeImage(60, 60);
    fillRect(img, 10, 10, 49, 49, qRgb(255, 255, 255));

    AutoShadowParams p = plainParams();
    p.lightX = 2.0;
    p.lightY = 0.5;                                      // 光在 (120,30)，背光方向≈向左
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 30) == qRgb(0, 0, 0));         // 左缘月牙
    REQUIRE(img.pixel(29, 30) == qRgb(0, 0, 0));         // 月牙最后一行（距离 20）
    REQUIRE(img.pixel(30, 30) == qRgb(255, 255, 255));   // 取样回到方块内：受光
    REQUIRE(img.pixel(45, 30) == qRgb(255, 255, 255));   // 靠近光源一侧受光
}

TEST_CASE("AutoShadow-blur-size-soft-edge")
{
    // 大小=10（σ≈5）：月牙边界从硬切变成软坡，色阶阈值在坡上切层（中间阶给灰阶才可见）。
    // 宽条 x[10,49]（宽度>盒式级联有效核宽，内部模糊后仍≈1），断言列 x=30 在光轴上
    QImage img = makeImage(60, 100);
    fillRect(img, 10, 10, 49, 89, qRgb(255, 255, 255));
    AutoShadowParams p = plainParams();
    p.shadowSize = 10;
    p.levels[1] = { qRgb(128, 128, 128), AutoShadowBlendMode::Multiply };
    p.levels[2] = { qRgb(64, 64, 64), AutoShadowBlendMode::Multiply };
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(30, 50) == qRgb(255, 255, 255));   // 深受光
    REQUIRE(img.pixel(30, 85) == qRgb(0, 0, 0));         // 深阴影
    const int mid = qRed(img.pixel(30, 70));             // 边界坡上：必然落进中间灰阶
    REQUIRE(mid > 0);
    REQUIRE(mid < 255);
}

TEST_CASE("AutoShadow-invert-level-order")
{
    // 只把阶1 设为黑、反转 → 黑色带被镜像到最深的月牙区；受光区吃原阶4白=不变
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.levels[0] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };
    p.invertLevels = true;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 50) == qRgb(255, 255, 255));   // 受光区吃原阶4白→不变
    REQUIRE(img.pixel(15, 80) == qRgb(0, 0, 0));         // 月牙吃镜像后的阶4=原阶1黑
}

TEST_CASE("AutoShadow-choke-matte-shrinks")
{
    // 正值阻塞（收缩白区 3px）：掩膜底边 89→86，月牙起点从 70 提前到 67，
    // 底部 3 行被吃成洞（F=0 → 色阶1=白，不变）
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.chokeMatte = 3;
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 66) == qRgb(255, 255, 255));   // 取样 86 仍在收缩后掩膜内：受光
    REQUIRE(img.pixel(15, 67) == qRgb(0, 0, 0));         // 取样 87 出掩膜：阴影提前开始
    REQUIRE(img.pixel(15, 85) == qRgb(0, 0, 0));         // 阴影带内
    REQUIRE(img.pixel(15, 87) == qRgb(255, 255, 255));   // 底部 3 行成洞：无阴影
}

TEST_CASE("AutoShadow-choke-matte-expands")
{
    // 负值扩展（白区外长 3px）：掩膜底边 89→92，月牙起点从 70 推迟到 73；扩展不出原图内容
    QImage img = farLightImage();
    AutoShadowParams p = plainParams();
    p.chokeMatte = -3;
    p.levels[3] = { qRgb(0, 0, 0), AutoShadowBlendMode::Multiply };

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 72) == qRgb(255, 255, 255));   // 取样 92 仍在扩展后掩膜内
    REQUIRE(img.pixel(15, 73) == qRgb(0, 0, 0));         // 取样 93 出掩膜：阴影推迟开始
    REQUIRE(img.pixel(15, 89) == qRgb(0, 0, 0));         // 底行阴影
    REQUIRE(img.pixel(5, 50) == 0);                      // 画布透明区不被扩展上色
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
