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

// 不透明白色圆盘
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

// 关闭模糊与置换的极端参数：黑影 100% 透明度，阴影像素=纯黑、其余=原样
AutoShadowParams blackFullParams()
{
    AutoShadowParams p;
    p.shadowColor = qRgb(0, 0, 0);
    p.shadowOpacity = 100;
    p.blurRadius = 0;
    p.displaceStrength = 0;
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

TEST_CASE("AutoShadow-far-light-hard-circle")
{
    // 竖长白条 x[10,19] y[10,89]，远光正上方（90°，5000px）：羽化=0 的硬圆遮罩，
    // 半径 = r0+范围，边界是以光源为圆心的圆弧：光轴两列（x=14/15）y=31 起入影，
    // 外侧八列离光更远 y=30 即越阈 → 59 行整宽 + 第 30 行 8 像素
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 20;

    const int changed = AutoShadow::apply(img, p);
    REQUIRE(changed == 10 * 59 + 8);
    REQUIRE(img.pixel(15, 15) == qRgb(255, 255, 255));   // 近光端：matte=1 露出原图
    REQUIRE(img.pixel(15, 29) == qRgb(255, 255, 255));   // 阈值前一行
    REQUIRE(img.pixel(15, 30) == qRgb(255, 255, 255));   // 光轴列：v=20-ε 恰在圆内
    REQUIRE(img.pixel(10, 30) == qRgb(0, 0, 0));         // 外侧列：v=20+ε 已出圆（圆弧）
    REQUIRE(img.pixel(15, 31) == qRgb(0, 0, 0));         // 光轴列第一行阴影
    REQUIRE(img.pixel(15, 89) == qRgb(0, 0, 0));         // 远端
    REQUIRE(img.pixel(5, 60) == 0);                      // 掩膜外透明像素不动（副本α裁回内容）
}

TEST_CASE("AutoShadow-near-light-arc-boundary")
{
    // 方块 x[10,49] y[10,49]，近光右侧（0°，100px）：圆形遮罩弯成圆弧——
    // 左边缘中点离光近（v≈39）、四角离光远（v≈40.5）：角部入影而中点仍受光
    QImage img = makeImage(60, 60);
    fillWhiteRect(img, 10, 10, 49, 49);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 0;
    p.lightDistance = 100;
    p.shadowRange = 40;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(49, 30) == qRgb(255, 255, 255));   // 右边缘（r0 零点）
    REQUIRE(img.pixel(10, 30) == qRgb(255, 255, 255));   // 左边缘中点：v≈39 < 40
    REQUIRE(img.pixel(10, 11) == qRgb(0, 0, 0));         // 左上角：v≈40.4 ≥ 40
    REQUIRE(img.pixel(10, 49) == qRgb(0, 0, 0));         // 左下角：v≈40.6 ≥ 40
}

TEST_CASE("AutoShadow-point-light-on-disc")
{
    // 圆盘 (40,30) r=12，点光右侧 200px：右缘是 r0 零点（露出原图），左半盘出圆入影
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

TEST_CASE("AutoShadow-blur-feathers-boundary")
{
    // 宽白条 x[10,49] y[10,89]（宽度>3次盒式级联的有效核宽，中心模糊后仍实心 1），
    // 远光上方，模糊 10 = 遮罩羽化宽：边界从硬切变成 smoothstep 过渡带（v∈[15,25]）
    QImage img = makeImage(60, 100);
    fillWhiteRect(img, 10, 10, 49, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 20;
    p.blurRadius = 10;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(30, 25) == qRgb(255, 255, 255));   // v=15：羽化带下沿内，全亮
    const QRgb mid = img.pixel(30, 30);                  // v=20：羽化中点半影
    REQUIRE(qRed(mid) >= 126);
    REQUIRE(qRed(mid) <= 130);
    REQUIRE(img.pixel(30, 35) == qRgb(0, 0, 0));         // v=25：羽化带上沿外，全影
    REQUIRE(img.pixel(9, 50) == 0);                      // 轮廓外透明不动（模糊外扩不产生阴影）
}

TEST_CASE("AutoShadow-displace-warps-boundary")
{
    // 宽白条 x[10,39] y[10,89]，远光上方，范围 40（无置换时边界约在 v=40）。
    // 置换贴图=原图亮度：全白 → off=+8，遮罩采样点向右下偏移——
    // (22,45) 的采样点 (30,53) 已出圆（v≈43），matte≈0 → 提前入影
    QImage plain = makeImage(50, 100);
    fillWhiteRect(plain, 10, 10, 39, 89);
    QImage displaced = plain;

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 40;
    p.blurRadius = 8;

    AutoShadow::apply(plain, p);                          // 对照：无置换
    REQUIRE(plain.pixel(22, 45) == qRgb(255, 255, 255)); // v≈35：圆内

    p.displaceStrength = 8;
    REQUIRE(AutoShadow::apply(displaced, p) > 0);
    REQUIRE(qRed(displaced.pixel(22, 45)) < 32);         // 采样偏移后 matte≈0：提前入影
}

TEST_CASE("AutoShadow-two-levels")
{
    // 两圈遮罩同色叠加：单圈 a=0.4 → 153；双圈 1-(1-0.4)^2=0.64 → 255×0.36=92
    QImage img = makeImage(30, 100);
    fillWhiteRect(img, 10, 10, 19, 89);

    AutoShadowParams p = blackFullParams();
    p.lightAngle = 90;
    p.lightDistance = 5000;
    p.shadowRange = 15;
    p.secondLevelRange = 50;
    p.shadowOpacity = 40;

    REQUIRE(AutoShadow::apply(img, p) > 0);
    REQUIRE(img.pixel(15, 20) == qRgb(255, 255, 255));   // v≈10 < 15：两圈圆内全露原图
    REQUIRE(img.pixel(15, 30) == qRgb(153, 153, 153));   // 第一圈外、第二圈内：单层
    REQUIRE(img.pixel(15, 70) == qRgb(92, 92, 92));      // 两圈之外：叠加 0.64
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
    REQUIRE(qAlpha(px) == 255);                          // over 合成到不透明底：α 恒不透明
    REQUIRE(qRed(px) == 128);                            // 0×0.5+255×0.5=127.5→qRound=128
}
