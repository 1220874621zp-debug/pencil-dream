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

void fillWhiteDisc(QImage& img, const double cx, const double cy, const double r)
{
    for (int y = static_cast<int>(cy - r) - 1; y <= static_cast<int>(cy + r) + 1; ++y)
    {
        if (y < 0 || y >= img.height())
            continue;
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = static_cast<int>(cx - r) - 1; x <= static_cast<int>(cx + r) + 1; ++x)
        {
            if (x < 0 || x >= img.width())
                continue;
            const double dx = x - cx;
            const double dy = y - cy;
            if (dx * dx + dy * dy <= r * r)
                line[x] = qRgb(255, 255, 255);
        }
    }
}

// 纯黑阴影 + 浓度 100%：阴影像素被压成 0，非阴影保持 255，判定无歧义
AutoShadowParams blackFullParams()
{
    AutoShadowParams p;
    p.shadowColor = qRgb(0, 0, 0);
    p.shadowOpacity = 100;
    p.choke = 0;
    return p;
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

TEST_CASE("AutoShadow-far-light-parallel-bands")
{
    // 竖长白条 x[10,19] y[10,89]，远光正上方（90°，5000px）：近似平行条带，
    // 渐变以内容最高点归零，阴影从近光端往远处切
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 20;

    const int changed = AutoShadow::apply(img, p);
    // 阴影边界是以光源为圆心的圆弧：光轴上（x=14/15，横向偏移 0.5px）y=31 起入影，
    // 外侧八列横向偏移更大、离光更远，y=30 即越阈 → 59 行整宽 + 第 30 行 8 像素
    REQUIRE(changed == 10 * 59 + 8);
    REQUIRE(img.pixel(15, 15) == qRgb(255, 255, 255));   // 近光端
    REQUIRE(img.pixel(15, 29) == qRgb(255, 255, 255));   // 阈值前一行
    REQUIRE(img.pixel(15, 30) == qRgb(255, 255, 255));   // 光轴列：v=20-ε 恰不达标
    REQUIRE(img.pixel(10, 30) == qRgb(0, 0, 0));         // 外侧列：v=20+ε 已入影（圆弧）
    REQUIRE(img.pixel(15, 31) == qRgb(0, 0, 0));         // 光轴列第一行阴影
    REQUIRE(img.pixel(15, 89) == qRgb(0, 0, 0));         // 远端
    REQUIRE(img.pixel(5, 60) == 0);                      // 掩膜外透明像素不动
}

TEST_CASE("AutoShadow-near-light-radial-bands")
{
    // 方块 x[10,49] y[10,49]，近光右侧（0°，100px）：圆形径向渐变——
    // 左边缘中点离光近（v≈39）、四角离光远（v≈40.5），断层带应弯成圆弧：
    // 角部入阴影而左边缘中点仍受光（平行光下两者距离相同，不可能分出差异）
    QImage img = makeImage(60, 60);
    fillWhiteRect(img, 10, 10, 49, 49);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 0;
    p.lightDistance = 100;
    p.shadowRange = 40;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(49, 30) == qRgb(255, 255, 255));   // 右边缘（渐变零点）
    REQUIRE(img.pixel(10, 30) == qRgb(255, 255, 255));   // 左边缘中点：v≈39 < 40
    REQUIRE(img.pixel(10, 11) == qRgb(0, 0, 0));         // 左上角：v≈40.4 ≥ 40
    REQUIRE(img.pixel(10, 49) == qRgb(0, 0, 0));         // 左下角：v≈40.6 ≥ 40
}

TEST_CASE("AutoShadow-point-light-on-disc")
{
    // 圆盘 (40,30) r=12，点光在右侧 200px：右缘是渐变零点，左半盘入阴影
    QImage img = makeImage(60, 60);
    fillWhiteDisc(img, 40, 30, 12);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 0;
    p.lightDistance = 200;
    p.shadowRange = 10;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(52, 30) == qRgb(255, 255, 255));   // 右缘：v=0
    REQUIRE(img.pixel(30, 30) == qRgb(0, 0, 0));         // 左侧：v=22 ≥ 10
}

TEST_CASE("AutoShadow-two-levels")
{
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 15;
    p.secondLevelRange = 50;
    p.shadowOpacity = 40;           // 第一档 f=0.6→153；第二档 k=0.6→102

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 20) == qRgb(255, 255, 255));   // v≈10 < 15
    REQUIRE(img.pixel(15, 30) == qRgb(153, 153, 153));   // 第一档（v≈20）
    REQUIRE(img.pixel(15, 70) == qRgb(102, 102, 102));   // 第二档（v≈60 ≥ 50）
}

TEST_CASE("AutoShadow-choke-expands-to-line")
{
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 25;
    p.choke = 3;                    // 阴影边界向受光侧膨胀 3px（无阻塞时 y≥36）

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 34) == qRgb(0, 0, 0));         // 靠阻塞进入阴影
    REQUIRE(img.pixel(15, 31) == qRgb(255, 255, 255));   // 阻塞之外
    REQUIRE(img.pixel(9, 40) == 0);                      // 膨胀不越过掩膜（透明区不动）
}

TEST_CASE("AutoShadow-alpha-preserved")
{
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 20;
    p.shadowOpacity = 50;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    const QRgb px = img.pixel(15, 60);
    REQUIRE(qAlpha(px) == 255);                          // 浓度只改色、不碰 α
    REQUIRE(qRed(px) == 128);                            // 255*(1-0.5)=127.5→qRound=128
}
