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
    p.lightDistance = 5000;         // 平行光
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

TEST_CASE("AutoShadow-parallel-top-light")
{
    // 竖长白条 x[10,19] y[10,89]，顶光（90°）：朝光面不暗、深度≥阈值起进入阴影
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;              // 正上方
    p.shadowRange = 20;

    const int changed = AutoShadow::apply(img, p);
    // 阴影带 y[30,89]（深度=y-10），宽 10px
    REQUIRE(changed == 10 * 60);
    REQUIRE(img.pixel(15, 15) == qRgb(255, 255, 255));   // 受光面
    REQUIRE(img.pixel(15, 29) == qRgb(255, 255, 255));   // 深度 19：阈值前一行
    REQUIRE(img.pixel(15, 30) == qRgb(0, 0, 0));         // 深度 20：第一行阴影
    REQUIRE(img.pixel(15, 89) == qRgb(0, 0, 0));         // 底部
    REQUIRE(img.pixel(5, 60) == 0);                      // 掩膜外透明像素不动
}

TEST_CASE("AutoShadow-point-light-from-right")
{
    // 圆盘 (40,30) r=12，点光在右侧：左缘深陷阴影、右缘受光
    QImage img = makeImage(60, 60);
    fillWhiteDisc(img, 40, 30, 12);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 0;               // 右侧
    p.lightDistance = 200;          // 点光
    p.shadowRange = 10;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(52, 30) == qRgb(255, 255, 255));   // 右缘：第一步入掩膜外
    REQUIRE(img.pixel(30, 30) == qRgb(0, 0, 0));         // 左侧：向光穿过整盘
}

TEST_CASE("AutoShadow-two-levels")
{
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.shadowRange = 15;
    p.secondLevelRange = 50;
    p.shadowOpacity = 40;           // 第一档 f=0.6→153；第二档 k=0.6→102

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 20) == qRgb(255, 255, 255));   // 深度 10 < 15
    REQUIRE(img.pixel(15, 30) == qRgb(153, 153, 153));   // 第一档（深度 20）
    REQUIRE(img.pixel(15, 70) == qRgb(102, 102, 102));   // 第二档（深度 60 ≥ 50）
}

TEST_CASE("AutoShadow-choke-expands-to-line")
{
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.shadowRange = 25;
    p.choke = 3;                    // 阴影边界向受光侧膨胀 3px（无阻塞时 y≥35）

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 34) == qRgb(0, 0, 0));         // 深度 24：靠阻塞进入阴影
    REQUIRE(img.pixel(15, 31) == qRgb(255, 255, 255));   // 深度 21：阻塞之外
    REQUIRE(img.pixel(9, 40) == 0);                      // 膨胀不越过掩膜（透明区不动）
}

TEST_CASE("AutoShadow-closure-seals-pen-gaps")
{
    // 上下两条白块夹 1px 透明缝：闭运算封缝后，光不再漏过缝隙（不封缝时深度只有 1）
    QImage img = makeImage(30, 60);
    fillWhiteRect(img, 10, 10, 19, 14);
    // y=15 为缝
    fillWhiteRect(img, 10, 16, 19, 50);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.shadowRange = 6;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 17) == qRgb(0, 0, 0));         // 穿缝深度 7 ≥ 6
    REQUIRE(img.pixel(15, 12) == qRgb(255, 255, 255));   // 上块顶部仍受光（深度 2）
    REQUIRE(img.pixel(15, 15) == 0);                     // 缝本身透明，不上色
}

TEST_CASE("AutoShadow-alpha-preserved")
{
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.shadowRange = 20;
    p.shadowOpacity = 50;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    const QRgb px = img.pixel(15, 60);
    REQUIRE(qAlpha(px) == 255);                          // 浓度只改色、不碰 α
    REQUIRE(qRed(px) == 128);                            // 255*(1-0.5)=127.5→qRound=128
}
