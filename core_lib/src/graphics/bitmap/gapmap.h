/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

Gap distance map for the bucket fill, adapted from Krita's KisGapMap
(GPL-2.0-or-later, Copyright 2024 Maciej Jesionowski <yavnrh@gmail.com>),
whose distance calculation method is in turn adapted from MyPaint source
code, originally implemented by Jesper Lloyd (Copyright 2018 by the MyPaint
Development Team, GPL-2.0-or-later).

The map holds, for every pixel, the squared distance of the nearest pair of
lineart (boundary) pixels whose connecting segment passes over it: a pixel
with a finite distance sits inside a potential lineart gap, and the value
tells how big that gap is. DISTANCE_INFINITE pixels are "in the open".

Unlike Krita's lazily-computed tiled version, this implementation computes
the whole region up front: pencil's fill region is already capped by the
camera view and the active selection, so the area is bounded.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef GAPMAP_H
#define GAPMAP_H

#include <QBitArray>
#include <QPoint>
#include <QVector>

class GapMap
{
public:
    /** A magic number to express a pixel that is very far from any gaps. */
    static constexpr quint16 DISTANCE_INFINITE = UINT16_MAX;

    /**
     * @param gapSize maximum size of lineart gap to look for, in pixels
     * @param boundary bitmap over the fill region (region-local coordinates,
     *                 one bit per pixel, w*h bits); a set bit is a boundary
     *                 (lineart) pixel
     */
    GapMap(int gapSize, const QBitArray& boundary, int width, int height);

    /** Query the gap distance at a pixel. (x, y) are region-local. */
    inline quint16 distance(int x, int y) const
    {
        return mDistances[y * mWidth + x];
    }

    inline int gapSize() const { return mGapSize; }

private:
    // The four transformation functions cover the four octants (a 1/8th
    // circular sector) of a half-circle covering a part of the map.
    struct TransformNone
    {
        static inline QPoint op(int x, int y, int xOffset, int yOffset)
        {
            return QPoint(x + xOffset, y + yOffset);
        }
    };

    struct TransformRotateClockwiseMirrorHorizontally
    {
        static inline QPoint op(int x, int y, int xOffset, int yOffset)
        {
            return QPoint(x - yOffset, y - xOffset);
        }
    };

    struct TransformRotateClockwise
    {
        static inline QPoint op(int x, int y, int xOffset, int yOffset)
        {
            return QPoint(x - yOffset, y + xOffset);
        }
    };

    struct TransformMirrorHorizontally
    {
        static inline QPoint op(int x, int y, int xOffset, int yOffset)
        {
            return QPoint(x + xOffset, y - yOffset);
        }
    };

    inline bool isBoundary(const QPoint& p) const
    {
        if (p.x() < 0 || p.x() >= mWidth || p.y() < 0 || p.y() >= mHeight) {
            return false;
        }
        return mBoundary[p.y() * mWidth + p.x()];
    }

    /**
     * Update the distance map in an octant (a 45-degree sector of a
     * half-circle) originating from the (x, y) point. Calling it for a point
     * modifies the RIGHT half-circle of the distance map, excluding the
     * point; conversely, to fully determine the distance at any point, this
     * function must be called for all points in its LEFT half-circle.
     */
    template<typename CoordinateTransform>
    void gapDistanceSearch(int x, int y);

    void updateDistance(const QPoint& p, quint16 newDistance);

    const int mGapSize;
    const int mWidth;
    const int mHeight;
    const QBitArray& mBoundary;   ///< one bit per pixel, true = boundary pixel
    QVector<quint16> mDistances;
};

#endif // GAPMAP_H
