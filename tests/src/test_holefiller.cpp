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

*/
#include "catch.hpp"

#include "holefiller.h"

#include <QColor>
#include <QImage>
#include <QRgb>

namespace
{

QImage makeImage(const int w, const int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    return img;
}

// 直接写预乘像素（不透明色预乘与直通一致），保证断言精确
void fillRect(QImage& img, const int x0, const int y0, const int x1, const int y1, const QRgb premul)
{
    for (int y = y0; y <= y1; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = x0; x <= x1; ++x)
            line[x] = premul;
    }
}

const QRgb RED = qRgb(200, 30, 30);
const QRgb BLUE = qRgb(30, 60, 220);

} // namespace

TEST_CASE("holefiller enclosed hole is filled and background stays untouched")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 2, 2, 61, 61, RED);            // 不透明红色块，四周留 2px 透明背景
    fillRect(img, 20, 20, 43, 43, 0);            // 24px 宽封闭镂空（比闭运算核大）

    const int filled = HoleFiller::fillHoles(img);
    REQUIRE(filled > 0);

    // 镂空整体变不透明红
    for (int y = 22; y <= 41; ++y)
    {
        for (int x = 22; x <= 41; ++x)
        {
            const QRgb px = img.pixel(x, y);
            REQUIRE(qAlpha(px) == 255);
            REQUIRE(qRed(px) == 200);
            REQUIRE(qGreen(px) == 30);
            REQUIRE(qBlue(px) == 30);
        }
    }

    // 连到边界的背景透明区保持透明
    REQUIRE(qAlpha(img.pixel(1, 1)) == 0);
    REQUIRE(qAlpha(img.pixel(62, 62)) == 0);
    REQUIRE(qAlpha(img.pixel(32, 1)) == 0);

    // 远离镂空的笔画不变
    REQUIRE(img.pixel(5, 5) == RED);
}

TEST_CASE("holefiller crack reaching the border is caught by closing")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 28, 0, 35, 63, RED);           // 贯通上下的红条
    fillRect(img, 31, 0, 33, 63, 0);             // 3px 细缝，两端出画面 → 连通域法抓不到

    const int filled = HoleFiller::fillHoles(img);
    REQUIRE(filled > 0);

    // 中段缝被填成红色（贴边 ~5px 因腐蚀边界处理保守，不断言）
    for (int y = 20; y <= 44; ++y)
    {
        for (int x = 31; x <= 33; ++x)
        {
            const QRgb px = img.pixel(x, y);
            REQUIRE(qAlpha(px) == 255);
            REQUIRE(qRed(px) == 200);
        }
    }

    // 两侧笔画不变
    REQUIRE(img.pixel(29, 32) == RED);
    REQUIRE(img.pixel(35, 32) == RED);
}

TEST_CASE("holefiller each side of a crack takes its own nearest color")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 20, 0, 30, 63, RED);           // 左红条
    fillRect(img, 36, 0, 46, 63, BLUE);          // 右蓝条，5px 缝贯通上下

    const int filled = HoleFiller::fillHoles(img);
    REQUIRE(filled > 0);

    // 缝全部填实
    for (int y = 10; y <= 54; ++y)
        for (int x = 31; x <= 35; ++x)
            REQUIRE(qAlpha(img.pixel(x, y)) == 255);

    // 左半缝取红、右半缝取蓝（各像素取最近参考色）
    REQUIRE(qRed(img.pixel(31, 32)) == 200);
    REQUIRE(qRed(img.pixel(35, 32)) == 30);
    REQUIRE(qBlue(img.pixel(35, 32)) == 220);
}

TEST_CASE("holefiller anti-aliased fringe around hole is absorbed")
{
    QImage img = makeImage(40, 40);
    fillRect(img, 4, 4, 35, 35, RED);
    fillRect(img, 14, 14, 25, 25, 0);            // 封闭镂空

    // 贴着镂空的一圈半透明（预乘红，alpha=128）
    const QRgb fringe = qRgba(200 * 128 / 255, 30 * 128 / 255, 30 * 128 / 255, 128);
    fillRect(img, 13, 13, 26, 13, fringe);
    fillRect(img, 13, 26, 26, 26, fringe);
    fillRect(img, 13, 14, 13, 25, fringe);
    fillRect(img, 26, 14, 26, 25, fringe);

    const int filled = HoleFiller::fillHoles(img);
    REQUIRE(filled > 0);

    // 镂空 + 毛边圈全部变不透明红（无透底毛边）
    for (int y = 14; y <= 25; ++y)
        for (int x = 14; x <= 25; ++x)
            REQUIRE(qAlpha(img.pixel(x, y)) == 255);
    REQUIRE(qAlpha(img.pixel(13, 20)) == 255);
    REQUIRE(qAlpha(img.pixel(26, 20)) == 255);
    REQUIRE(qAlpha(img.pixel(20, 13)) == 255);
    REQUIRE(qAlpha(img.pixel(20, 26)) == 255);

    // 深处笔画不变
    REQUIRE(img.pixel(6, 6) == RED);
}

TEST_CASE("holefiller open gap wider than the closing kernel stays open")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 20, 0, 29, 63, RED);
    fillRect(img, 50, 0, 59, 63, BLUE);          // 20px 开放缝（> 11px 核），贯通上下

    const int filled = HoleFiller::fillHoles(img);
    REQUIRE(filled == 0);

    REQUIRE(qAlpha(img.pixel(40, 32)) == 0);
    REQUIRE(qAlpha(img.pixel(31, 32)) == 0);
    REQUIRE(qAlpha(img.pixel(49, 32)) == 0);
}

TEST_CASE("holefiller directional modes take whole-side color on vertical crack")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 20, 0, 30, 63, RED);
    fillRect(img, 36, 0, 46, 63, BLUE);          // 5px 竖缝贯通上下

    QImage imgA = img;
    REQUIRE(HoleFiller::fillHoles(imgA, HoleFiller::SideA) > 0);
    // 方向一 = 竖缝左侧：整条缝取红
    for (int y = 10; y <= 54; ++y)
        for (int x = 31; x <= 35; ++x)
            REQUIRE(imgA.pixel(x, y) == RED);

    QImage imgB = img;
    REQUIRE(HoleFiller::fillHoles(imgB, HoleFiller::SideB) > 0);
    // 方向二 = 竖缝右侧：整条缝取蓝
    for (int y = 10; y <= 54; ++y)
        for (int x = 31; x <= 35; ++x)
            REQUIRE(imgB.pixel(x, y) == BLUE);

    // 自动模式保持中线分界（左红右蓝）不受方向模式影响
    QImage imgAuto = img;
    REQUIRE(HoleFiller::fillHoles(imgAuto, HoleFiller::NearestFill) > 0);
    REQUIRE(imgAuto.pixel(31, 32) == RED);
    REQUIRE(imgAuto.pixel(35, 32) == BLUE);
}

TEST_CASE("holefiller directional modes use up and down sides for horizontal cracks")
{
    QImage img = makeImage(64, 64);
    const QRgb GREEN = qRgb(20, 180, 90);
    const QRgb ORANGE = qRgb(240, 140, 30);
    fillRect(img, 0, 8, 63, 20, GREEN);
    fillRect(img, 0, 28, 63, 40, ORANGE);        // 7px 横缝贯通左右

    QImage imgA = img;
    REQUIRE(HoleFiller::fillHoles(imgA, HoleFiller::SideA) > 0);
    // 方向一 = 横缝上侧：整条缝取绿
    for (int y = 22; y <= 26; ++y)
        for (int x = 8; x <= 55; ++x)
            REQUIRE(imgA.pixel(x, y) == GREEN);

    QImage imgB = img;
    REQUIRE(HoleFiller::fillHoles(imgB, HoleFiller::SideB) > 0);
    // 方向二 = 横缝下侧：整条缝取橙
    for (int y = 22; y <= 26; ++y)
        for (int x = 8; x <= 55; ++x)
            REQUIRE(imgB.pixel(x, y) == ORANGE);
}

TEST_CASE("holefiller directional fill falls back on directionless components")
{
    QImage img = makeImage(32, 32);
    fillRect(img, 4, 4, 27, 27, RED);
    fillRect(img, 15, 15, 15, 15, 0);            // 单像素封闭镂空：无方向可言

    const int filled = HoleFiller::fillHoles(img, HoleFiller::SideA);
    REQUIRE(filled > 0);
    // 回退最近邻颜色（红），方向模式不崩不漏
    REQUIRE(img.pixel(15, 15) == RED);
}

TEST_CASE("holefiller degenerate inputs return zero")
{
    QImage opaque = makeImage(32, 32);
    fillRect(opaque, 0, 0, 31, 31, RED);
    REQUIRE(HoleFiller::fillHoles(opaque) == 0);

    QImage transparent = makeImage(32, 32);
    REQUIRE(HoleFiller::fillHoles(transparent) == 0);
}
