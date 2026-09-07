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

#include "mlswarp.h"

#include <QImage>
#include <QVector>
#include <QPointF>

namespace
{
    QImage makeSolidImage(int w, int h, const QColor& color)
    {
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(color);
        return img;
    }
}

TEST_CASE("MlsWarp identity")
{
    QImage src = makeSolidImage(64, 48, Qt::red);

    QVector<QPointF> ctrl;
    ctrl << QPointF(0, 0) << QPointF(64, 0) << QPointF(64, 48) << QPointF(0, 48);

    QPointF offset;
    QImage result = MlsWarp::warpImage(src, ctrl, ctrl, 1.0, true, &offset);

    REQUIRE(result.size() == src.size());
    // a solid color must survive an identity warp untouched
    REQUIRE(result.pixel(10, 10) == src.pixel(10, 10));
}

TEST_CASE("MlsWarp rigid translate of all control points")
{
    QImage src = makeSolidImage(40, 40, Qt::blue);

    QVector<QPointF> orig;
    orig << QPointF(0, 0) << QPointF(40, 0) << QPointF(40, 40) << QPointF(0, 40);

    const QPointF delta(10, 5);
    QVector<QPointF> moved;
    for (const QPointF& p : orig)
    {
        moved << p + delta;
    }

    QPointF offset;
    QImage result = MlsWarp::warpImage(src, orig, moved, 1.0, true, &offset);

    // uniform translation: result offset follows the delta and content moves
    REQUIRE(offset.x() == Approx(10.0).margin(1.5));
    REQUIRE(offset.y() == Approx(5.0).margin(1.5));
    REQUIRE(result.width() >= src.width());
    REQUIRE(result.height() >= src.height());

    // sample where the content should have moved to
    const int px = qRound(offset.x() + 20);
    const int py = qRound(offset.y() + 20);
    REQUIRE(qAlpha(result.pixel(px, py)) == 255);
    REQUIRE(qBlue(result.pixel(px, py)) > 200);

    // old top-left corner should now be empty
    REQUIRE(qAlpha(result.pixel(0, 0)) == 0);
}

TEST_CASE("MlsWarp single control point drag keeps surroundings")
{
    // gradient-ish image so we can reason about locality
    QImage src(80, 80, QImage::Format_ARGB32_Premultiplied);
    src.fill(QColor(0, 128, 0, 255));

    QVector<QPointF> orig;
    const int n = 4;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            orig << QPointF(c * 80.0 / (n - 1), r * 80.0 / (n - 1));

    QVector<QPointF> moved = orig;
    // drag the top-left control point far right/down
    moved[0] += QPointF(15, 10);

    QPointF offset;
    QImage result = MlsWarp::warpImage(src, orig, moved, 1.0, true, &offset);
    REQUIRE(!result.isNull());

    // rigid MLS: alpha coverage stays full somewhere in the middle
    const int cx = qRound(offset.x() + 40);
    const int cy = qRound(offset.y() + 40);
    REQUIRE(qAlpha(result.pixel(cx, cy)) == 255);
}

TEST_CASE("MlsWarp rejects mismatched control sets")
{
    QImage src = makeSolidImage(10, 10, Qt::green);

    QVector<QPointF> a { QPointF(0, 0), QPointF(10, 0) };
    QVector<QPointF> b { QPointF(0, 0) };

    QImage result = MlsWarp::warpImage(src, a, b, 1.0, true);
    // mismatched sizes return the source unchanged
    REQUIRE(result.size() == src.size());
    REQUIRE(result.pixel(5, 5) == src.pixel(5, 5));
}
