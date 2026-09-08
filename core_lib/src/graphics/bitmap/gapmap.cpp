/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

Adapted from Krita's KisGapMap (GPL-2.0-or-later, Copyright 2024 Maciej
Jesionowski <yavnrh@gmail.com>), distance calculation adapted from MyPaint
(Copyright 2018 by the MyPaint Development Team, GPL-2.0-or-later).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "gapmap.h"

GapMap::GapMap(int gapSize, const QBitArray& boundary, int width, int height)
    : mGapSize(gapSize)
    , mWidth(width)
    , mHeight(height)
    , mBoundary(boundary)
{
    mDistances.fill(DISTANCE_INFINITE, mWidth * mHeight);

    // Unlike Krita's lazy per-tile computation, the whole region is computed
    // eagerly in one go: pencil's fill region is already bounded.
    for (int y = 0; y < mHeight; ++y) {
        for (int x = 0; x < mWidth; ++x) {
            if (mBoundary[y * mWidth + x]) {
                gapDistanceSearch<TransformNone>(x, y);
                gapDistanceSearch<TransformRotateClockwiseMirrorHorizontally>(x, y);
                gapDistanceSearch<TransformRotateClockwise>(x, y);
                gapDistanceSearch<TransformMirrorHorizontally>(x, y);
            }
        }
    }
}

template<typename CoordinateTransform>
void GapMap::gapDistanceSearch(int x, int y)
{
    if (isBoundary(CoordinateTransform::op(x, y, 0, -1)) ||
        isBoundary(CoordinateTransform::op(x, y, 1, -1))) {
        return;
    }

    for (int yoffs = 2; yoffs < mGapSize + 2; ++yoffs) {
        const int yDistanceSq = (yoffs - 1) * (yoffs - 1);

        for (int xoffs = 0; xoffs <= yoffs; ++xoffs) {
            const int offsetDistance = yDistanceSq + xoffs * xoffs;

            if (offsetDistance >= 1 + mGapSize * mGapSize) {
                break;
            }

            if (isBoundary(CoordinateTransform::op(x, y, xoffs, -yoffs))) {
                // Walk back along the line toward the origin pixel and
                // write the distance to the pixels in between.
                const float dx = static_cast<float>(xoffs) / (yoffs - 1);
                float tx = 0;
                int cx = 0;

                for (int cy = 1; cy < yoffs; ++cy) {
                    updateDistance(CoordinateTransform::op(x, y, cx, -cy), offsetDistance);

                    tx += dx;
                    if (static_cast<int>(tx) > cx) {
                        cx++;
                        updateDistance(CoordinateTransform::op(x, y, cx, -cy), offsetDistance);
                    }

                    updateDistance(CoordinateTransform::op(x, y, cx + 1, -cy), offsetDistance);
                }
            }
        }
    }
}

void GapMap::updateDistance(const QPoint& p, quint16 newDistance)
{
    if (p.x() < 0 || p.x() >= mWidth || p.y() < 0 || p.y() >= mHeight) {
        return;
    }

    quint16& dist = mDistances[p.y() * mWidth + p.x()];
    if (dist > newDistance) {
        dist = newDistance;
    }
}
