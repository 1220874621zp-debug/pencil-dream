/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

Scanline flood fill with gap closing, adapted from Krita's KisScanlineFill
(GPL-2.0-or-later, Copyright 2014 Dmitry Kazakov <dimula73@gmail.com>; gap
closing passes Copyright 2024 Maciej Jesionowski <yavnrh@gmail.com>).

The fill runs over a reference QImage (ARGB32_Premultiplied, region-local
coordinates starting at (0,0)) and writes a grayscale selection mask over
the same region. Pixels are selected when their color is within the
tolerance of the reference (flood) color, or - in boundary fill mode -
when they differ from the boundary color.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef SCANLINEFILL_H
#define SCANLINEFILL_H

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QStack>
#include <QVector>
#include <QHash>

#include <functional>
#include <memory>
#include <queue>
#include <vector>

#include "fillintervalmap.h"
#include "gapmap.h"

class ScanlineFill
{
public:
    /**
     * @param reference the fill region of the reference image; pixels outside
     *                  the original image bounds must already read as
     *                  transparent. Coordinates are region-local.
     * @param seed      fill start point, region-local, inside boundingRect
     * @param boundingRect the fill bounds, region-local, starting at (0, 0)
     */
    ScanlineFill(const QImage& reference, const QPoint& seed, const QRect& boundingRect);

    /** Squared color tolerance, see BitmapImage::compareColor. */
    void setThreshold(int squaredTolerance);

    /** Try to close gaps in lines up to this size in pixels (0 = off). */
    void setCloseGap(int closeGap);

    /** Boundary fill mode: fill everything until (excluding) pixels that
     *  match boundaryColor within the tolerance. */
    void setUntilColor(bool enabled, QRgb boundaryColor);

    /** Runs the fill and writes selection opacity (0 or 255) into the mask,
     *  which must cover the whole boundingRect. */
    void fillSelection(QVector<quint8>& mask);

    QRect fillExtent() const { return mFillExtent; }

private:
    /** A work item for the gap closing fill: a seed point or the next
     *  queued pixel to continue the fill. */
    struct CloseGapFillPoint
    {
        int x = 0;
        int y = 0;

        quint32 distance = 0;   ///< previous pixel's distance from the nearest lineart gap
        bool allowExpand = false; ///< whether the fill at this pixel can expand to greater distances

        // Used with the priority queue: the "greater" operator places the
        // smallest element on top.
        friend bool operator>(const CloseGapFillPoint& a, const CloseGapFillPoint& b)
        {
            return std::make_tuple(!a.allowExpand, a.distance, a.y, a.x) >
                   std::make_tuple(!b.allowExpand, b.distance, b.y, b.x);
        }
    };

    inline QRgb referencePixel(int x, int y) const
    {
        return reinterpret_cast<const QRgb*>(mReference.constScanLine(y))[x];
    }

    /** True when the pixel passes the color test and may be filled. */
    inline bool pixelSelected(int x, int y);

    /** If the pixel is near a gap (finite distance), push it onto the gap
     *  closing queue instead of letting the scanline fill it.
     *  @returns true if a gap closing pixel was pushed. */
    bool tryPushingCloseGapSeed(int x, int y, bool allowExpand);

    void extendedPass(FillInterval* currentInterval, int srcRow, bool extendRight);
    void processLine(FillInterval interval, int rowIncrement);
    FillInterval closeGapPass();
    void runImpl();

    inline void swapDirection()
    {
        mRowIncrement *= -1;
        mForwardStack = QStack<FillInterval>(mBackwardMap.fetchAllIntervals(mRowIncrement));
        mBackwardMap.clear();
    }

    inline void fillMaskPixel(int x, int y)
    {
        (*mMask)[y * mBounds.width() + x] = MAX_SELECTED;
        mFillExtent = mFillExtent.united(QRect(x, y, 1, 1));
    }

    static constexpr quint8 MAX_SELECTED = 255;
    static constexpr quint8 MIN_SELECTED = 0;

    const QImage& mReference;
    QPoint mSeed;
    QRect mBounds;

    int mThreshold = 0;
    int mRowIncrement = 1;

    FillIntervalMap mBackwardMap;
    QStack<FillInterval> mForwardStack;

    int mCloseGap = 0;
    std::unique_ptr<GapMap> mGapMap;
    QBitArray mBoundaryBitmap;

    bool mUntilColor = false;
    QRgb mBoundaryColor = 0;

    QRect mFillExtent;

    using GreaterPriorityQueue = std::priority_queue<CloseGapFillPoint,
                                                     std::vector<CloseGapFillPoint>,
                                                     std::greater<CloseGapFillPoint>>;
    GreaterPriorityQueue mCloseGapQueue;

    QVector<quint8>* mMask = nullptr;

    /** Cache of compareColor results keyed by pixel color. */
    QHash<QRgb, bool> mDiffCache;
    QRgb mReferenceColor = 0;
};

#endif // SCANLINEFILL_H
