/*

Pencil2D - Traditional Animation Software
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

#include "shadowengine.h"

#include <QColor>
#include <QImage>
#include <QLine>
#include <QPainter>
#include <QRect>

namespace
{

QImage makeCanvas(int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    return img;
}

// 线稿惯例：不透明白线只看 alpha
void drawSquareOutline(QImage& img, const QRect& r)
{
    QPainter p(&img);
    p.setPen(QPen(Qt::white, 4));
    p.drawRect(r);
    p.end();
}

void drawStrokeLine(QImage& img, const QLine& line, const QColor& color, int width = 3)
{
    QPainter p(&img);
    p.setPen(QPen(color, width));
    p.drawLine(line);
    p.end();
}

bool isColor(const QImage& img, int x, int y, QRgb expected)
{
    return img.pixel(x, y) == qPremultiply(expected);
}

bool isTransparent(const QImage& img, int x, int y)
{
    return qAlpha(img.pixel(x, y)) == 0;
}

struct FramePair
{
    QImage lineArt;
    QImage strokes;
};

// 正方形线稿 (10,10,180,180) + 一条分割线的标准画布
FramePair makeSquareWithLine(const QLine& divider, const QColor& color = Qt::red)
{
    FramePair fp;
    fp.lineArt = makeCanvas(200, 200);
    drawSquareOutline(fp.lineArt, QRect(10, 10, 180, 180));
    fp.strokes = makeCanvas(200, 200);
    drawStrokeLine(fp.strokes, divider, color);
    return fp;
}
} // namespace

TEST_CASE("ShadowFill_VerticalDividerPickLeft")
{
    FramePair fp = makeSquareWithLine(QLine(100, 8, 100, 192));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    params.fillColor = qRgb(0, 0, 0);
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    REQUIRE(result.warnings.isEmpty());
    CHECK(isColor(result.fill, 50, 100, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 150, 100));
    // 分割线像素贴阴影侧被接缝涂色，不残留透明细缝
    CHECK(isColor(result.fill, 100, 100, qRgb(0, 0, 0)));
    // 阴影严格止于分割线
    CHECK(isTransparent(result.fill, 102, 100));
}

TEST_CASE("ShadowFill_HorizontalDividerPickDown")
{
    FramePair fp = makeSquareWithLine(QLine(8, 100, 192, 100));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirDown;
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 100, 150, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 100, 50));
    CHECK(isColor(result.fill, 100, 100, qRgb(0, 0, 0)));
}

TEST_CASE("ShadowFill_PickUpMeansSmallerY")
{
    FramePair fp = makeSquareWithLine(QLine(8, 100, 192, 100));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirUp;
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 100, 50, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 100, 150));
}

TEST_CASE("ShadowFill_AmbiguousInheritsSpatialNeighbor")
{
    // 上方正方形横线分割（选"左"=歧义），下方正方形竖线分割（可信左）；
    // 上方正方形上下两半都继承下方左侧（阴影）区域的状态
    QImage lineArt = makeCanvas(200, 400);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    drawSquareOutline(lineArt, QRect(10, 210, 180, 180));
    QImage strokes = makeCanvas(200, 400);
    drawStrokeLine(strokes, QLine(8, 100, 192, 100), Qt::red);   // 上：横线 → 歧义
    drawStrokeLine(strokes, QLine(140, 208, 140, 392), Qt::red); // 下：竖线偏右 → 左侧为阴影

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 400), params);

    REQUIRE_FALSE(result.fill.isNull());
    // 下方正方形：可信判定，左阴影右不阴影
    CHECK(isColor(result.fill, 60, 300, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 170, 300));
    // 上方正方形：空间邻近继承 → 两半均为阴影
    CHECK(isColor(result.fill, 100, 50, qRgb(0, 0, 0)));
    CHECK(isColor(result.fill, 100, 150, qRgb(0, 0, 0)));
}

TEST_CASE("ShadowFill_UnclosedDividerWarning")
{
    // 悬空分割线（没搭到底边）→ 图形未切开 → 警告且不上阴影
    FramePair fp = makeSquareWithLine(QLine(100, 8, 100, 80));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    CHECK(result.fill.isNull());
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0].kind == ShadowFill::Warning::UnclosedDivider);
    CHECK(result.warnings[0].area.contains(100, 100));
}

TEST_CASE("ShadowFill_TransparentMarkerDisablesLine")
{
    // 红竖线（有效）+ 蓝横线（标透明=禁用）：图形只按红竖线切分，
    // 阴影跨过蓝线位置连续填充
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(60, 8, 60, 192), Qt::red);
    drawStrokeLine(strokes, QLine(8, 100, 192, 100), Qt::blue);

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    QVector<QRgb> disabled;
    disabled.append(qRgb(0, 0, 255));
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params, disabled);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 30, 50, qRgb(0, 0, 0)));
    CHECK(isColor(result.fill, 30, 100, qRgb(0, 0, 0))); // 蓝线位置照常填充
    CHECK(isColor(result.fill, 30, 150, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 120, 100));
}

TEST_CASE("ShadowFill_EnabledCrossMakesTieAmbiguous")
{
    // 同一画布，蓝线不标透明：十字切割出四个象限，选"左"时左右两列
    // 质心成对并列 → 极端者与次极端差为 0 → 整组歧义，且同帧无可借鉴
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(60, 8, 60, 192), Qt::red);
    drawStrokeLine(strokes, QLine(8, 100, 192, 100), Qt::blue);

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params);

    CHECK(result.fill.isNull());
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0].kind == ShadowFill::Warning::UnresolvedAmbiguous);
}

TEST_CASE("ShadowFill_ThreeStripsPickLeftmost")
{
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(60, 8, 60, 192), Qt::red);
    drawStrokeLine(strokes, QLine(140, 8, 140, 192), Qt::red);

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 30, 100, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 100, 100));
    CHECK(isTransparent(result.fill, 170, 100));
}

TEST_CASE("ShadowFill_NoDividerNoFill")
{
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);

    ShadowFill::Params params;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params);

    CHECK(result.fill.isNull());
    CHECK(result.warnings.isEmpty());
}

TEST_CASE("ShadowFill_CustomFillColor")
{
    FramePair fp = makeSquareWithLine(QLine(100, 8, 100, 192));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    params.fillColor = qRgb(255, 0, 255);
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 50, 100, qRgb(255, 0, 255)));
}

TEST_CASE("ShadowFill_EmptyStrokeLayerNoFill")
{
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(100, 8, 100, 192), Qt::red);

    // bounds 外的笔画被忽略：给一个不含分割线的 bounds
    ShadowFill::Params params;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(150, 150, 40, 40), params);

    CHECK(result.fill.isNull());
    CHECK(result.warnings.isEmpty());
}
