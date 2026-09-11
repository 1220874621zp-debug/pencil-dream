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

#include "inbetween.h"

namespace
{

QImage makeVerticalLine(int width, int height, int x)
{
    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    for (int y = 0; y < height; ++y)
        img.setPixel(x, y, qPremultiply(qRgba(0, 0, 0, 255)));
    return img;
}

bool columnHasInk(const QImage& img, int x)
{
    for (int y = 0; y < img.height(); ++y)
    {
        if (qAlpha(img.pixel(x, y)) > 64)
            return true;
    }
    return false;
}

} // namespace

TEST_CASE("Inbetween interpolate produces a stroke between the two sources")
{
    const QImage a = makeVerticalLine(200, 100, 30);
    const QImage b = makeVerticalLine(200, 100, 170);

    const QImage mid = Inbetween::interpolate(a, b, 0.5);
    REQUIRE(mid.size() == a.size());

    // 中点：线条应大致落在 (30+170)/2 = 100 附近
    REQUIRE(columnHasInk(mid, 100));
    // 两端原线位置不应再有完整线条
    REQUIRE_FALSE(columnHasInk(mid, 30));
    REQUIRE_FALSE(columnHasInk(mid, 170));
}

TEST_CASE("Inbetween interpolate honors the t parameter direction")
{
    const QImage a = makeVerticalLine(200, 100, 30);
    const QImage b = makeVerticalLine(200, 100, 170);

    const QImage nearA = Inbetween::interpolate(a, b, 0.1);
    const QImage nearB = Inbetween::interpolate(a, b, 0.9);

    // 位移与 t 成正比：t=0.1 → x=30+14=44；t=0.9 → x=30+126=156
    REQUIRE(columnHasInk(nearA, 44));
    REQUIRE_FALSE(columnHasInk(nearA, 156));
    REQUIRE(columnHasInk(nearB, 156));
    REQUIRE_FALSE(columnHasInk(nearB, 44));
}

TEST_CASE("Inbetween interpolate rejects mismatched sizes")
{
    const QImage a = makeVerticalLine(100, 100, 50);
    const QImage b = makeVerticalLine(120, 100, 60);
    REQUIRE(Inbetween::interpolate(a, b, 0.5).isNull());
}

TEST_CASE("Inbetween interpolate of identical sources stays in place")
{
    const QImage a = makeVerticalLine(200, 100, 100);
    const QImage same = Inbetween::interpolate(a, a, 0.5);
    REQUIRE(columnHasInk(same, 100));
    REQUIRE_FALSE(columnHasInk(same, 60));
    REQUIRE_FALSE(columnHasInk(same, 140));
}
