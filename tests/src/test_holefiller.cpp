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

TEST_CASE("holefiller interior miss behind a narrow channel is filled together")
{
    // 用户场景：色块中间的镂空通过笔触缺口（细通道）与外界连通——
    // 老"连到边界即背景"的判定会把镂空留下；开运算抹掉通道后
    // 镂空与边界断开，通道+镂空应整片填掉
    QImage img = makeImage(64, 64);
    fillRect(img, 4, 12, 59, 59, RED);           // 红色块，上缘留 12px 透明边距（≥11px 保持背景）
    fillRect(img, 20, 32, 39, 51, 0);            // 色块中央 20x20 镂空
    fillRect(img, 29, 0, 32, 31, 0);             // 4px 细通道：从画面上边穿过边距直插镂空

    const int filled = HoleFiller::fillHoles(img);
    REQUIRE(filled > 0);

    // 镂空整体填成红色
    for (int y = 36; y <= 47; ++y)
        for (int x = 24; x <= 35; ++x)
            REQUIRE(img.pixel(x, y) == RED);

    // 色块内的通道段（越过贴边保守区/背景膨胀环）也填掉
    REQUIRE(img.pixel(30, 22) == RED);
    REQUIRE(img.pixel(30, 28) == RED);

    // 边距仍保持透明（宽边距不是镂空），通道贴边段开放
    REQUIRE(qAlpha(img.pixel(10, 5)) == 0);
    REQUIRE(qAlpha(img.pixel(30, 8)) == 0);
}

TEST_CASE("holefiller each side of a crack takes its own nearest color")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 20, 0, 30, 63, RED);           // 左红条
    fillRect(img, 36, 0, 46, 63, BLUE);          // 右蓝条，5px 缝贯通上下

    const int filled = HoleFiller::fillHoles(img, HoleFiller::NearestFill);
    REQUIRE(filled > 0);

    // 缝全部填实
    for (int y = 12; y <= 51; ++y)
        for (int x = 31; x <= 35; ++x)
            REQUIRE(qAlpha(img.pixel(x, y)) == 255);

    // 左半缝取红、右半缝取蓝（各像素取最近参考色）
    REQUIRE(qRed(img.pixel(31, 32)) == 200);
    REQUIRE(qRed(img.pixel(35, 32)) == 30);
    REQUIRE(qBlue(img.pixel(35, 32)) == 220);
}

TEST_CASE("holefiller leaves anti-aliased edges untouched")
{
    QImage img = makeImage(40, 40);
    fillRect(img, 4, 4, 35, 35, RED);
    fillRect(img, 14, 14, 25, 25, 0);            // 封闭镂空

    // 贴着镂空的一圈半透明（预乘红，alpha=51 整除可无损往返：40*255/51=200）
    const QRgb fringe = qRgba(200 * 51 / 255, 30 * 51 / 255, 30 * 51 / 255, 51);
    fillRect(img, 13, 13, 26, 13, fringe);
    fillRect(img, 13, 26, 26, 26, fringe);
    fillRect(img, 13, 14, 13, 25, fringe);
    fillRect(img, 26, 14, 26, 25, fringe);

    const int filled = HoleFiller::fillHoles(img);
    REQUIRE(filled > 0);

    // 镂空（alpha=0）整体填成不透明红
    for (int y = 14; y <= 25; ++y)
        for (int x = 14; x <= 25; ++x)
            REQUIRE(img.pixel(x, y) == RED);

    // 半透明边缘一律不碰：原样保留（不外溢、不变胖）
    REQUIRE(img.pixel(13, 20) == fringe);
    REQUIRE(img.pixel(26, 20) == fringe);
    REQUIRE(img.pixel(20, 13) == fringe);
    REQUIRE(img.pixel(20, 26) == fringe);
    REQUIRE(img.pixel(13, 13) == fringe);

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

TEST_CASE("holefiller directional fill takes whole-side color on vertical crack")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 20, 0, 30, 63, RED);
    fillRect(img, 36, 0, 46, 63, BLUE);          // 5px 竖缝贯通上下

    QImage imgL = img;
    REQUIRE(HoleFiller::fillHoles(imgL, HoleFiller::TakeLeft) > 0);
    // 左取色：整条缝取红
    for (int y = 12; y <= 51; ++y)
        for (int x = 31; x <= 35; ++x)
            REQUIRE(imgL.pixel(x, y) == RED);

    QImage imgR = img;
    REQUIRE(HoleFiller::fillHoles(imgR, HoleFiller::TakeRight) > 0);
    // 右取色：整条缝取蓝
    for (int y = 12; y <= 51; ++y)
        for (int x = 31; x <= 35; ++x)
            REQUIRE(imgR.pixel(x, y) == BLUE);
}

TEST_CASE("holefiller directional fill uses up and down sides for horizontal cracks")
{
    QImage img = makeImage(64, 64);
    const QRgb GREEN = qRgb(20, 180, 90);
    const QRgb ORANGE = qRgb(240, 140, 30);
    fillRect(img, 0, 8, 63, 20, GREEN);
    fillRect(img, 0, 28, 63, 40, ORANGE);        // 7px 横缝贯通左右

    QImage imgU = img;
    REQUIRE(HoleFiller::fillHoles(imgU, HoleFiller::TakeUp) > 0);
    // 上取色：整条缝取绿（缝口 ~8px 开放带不断言）
    for (int y = 22; y <= 26; ++y)
        for (int x = 12; x <= 51; ++x)
            REQUIRE(imgU.pixel(x, y) == GREEN);

    QImage imgD = img;
    REQUIRE(HoleFiller::fillHoles(imgD, HoleFiller::TakeDown) > 0);
    // 下取色：整条缝取橙
    for (int y = 22; y <= 26; ++y)
        for (int x = 12; x <= 51; ++x)
            REQUIRE(imgD.pixel(x, y) == ORANGE);
}

TEST_CASE("holefiller directional fill falls back to nearest when the ray escapes")
{
    QImage img = makeImage(64, 64);
    fillRect(img, 20, 0, 30, 63, RED);
    fillRect(img, 36, 0, 46, 63, BLUE);          // 竖缝：上取色的射线沿缝逃逸出画面

    QImage imgU = img;
    REQUIRE(HoleFiller::fillHoles(imgU, HoleFiller::TakeUp) > 0);
    // 全部回退最近邻：缝填实、中线分界（左红右蓝）
    for (int y = 12; y <= 51; ++y)
        for (int x = 31; x <= 35; ++x)
            REQUIRE(qAlpha(imgU.pixel(x, y)) == 255);
    REQUIRE(imgU.pixel(31, 32) == RED);
    REQUIRE(imgU.pixel(35, 32) == BLUE);

    // 封闭镂空四方向都能取到色（射线必命中边界）
    QImage hole = makeImage(32, 32);
    fillRect(hole, 4, 4, 27, 27, RED);
    fillRect(hole, 15, 15, 15, 15, 0);
    for (int mode = HoleFiller::TakeLeft; mode <= HoleFiller::TakeDown; ++mode)
    {
        QImage probe = hole;
        REQUIRE(HoleFiller::fillHoles(probe, mode) > 0);
        REQUIRE(probe.pixel(15, 15) == RED);
    }
}

TEST_CASE("holefiller degenerate inputs return zero")
{
    QImage opaque = makeImage(32, 32);
    fillRect(opaque, 0, 0, 31, 31, RED);
    REQUIRE(HoleFiller::fillHoles(opaque) == 0);

    QImage transparent = makeImage(32, 32);
    REQUIRE(HoleFiller::fillHoles(transparent) == 0);
}
