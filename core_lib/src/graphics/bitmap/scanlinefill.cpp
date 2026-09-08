/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

Adapted from Krita's KisScanlineFill (GPL-2.0-or-later, Copyright 2014
Dmitry Kazakov <dimula73@gmail.com>; gap closing passes Copyright 2024
Maciej Jesionowski <yavnrh@gmail.com>).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "scanlinefill.h"

#include <tuple>

#include <QtMath>
#include "bitmapimage.h"

namespace
{

/** This ratio determines whether the fill can spread to pixels with
 *  a different gap distance compared to the previous pixel.
 *
 *  <1.0 is a pixel closer to a gap, or more in a corner of lineart.
 *  =1.0 is the same distance.
 *  >1.0 is a pixel that is farther from a gap, or away from tight corners.
 *
 *  At high gap sizes, the distance map can vary slightly and the fill could
 *  leave out empty pixels. To prevent that, the fill is allowed to spill to
 *  more distant pixels as well, up to this tolerance. If the value is too
 *  high, the fill will spill too much. */
constexpr float SpreadTolerance = 1.3f;

} // anonymous namespace

ScanlineFill::ScanlineFill(const QImage& reference, const QPoint& seed, const QRect& boundingRect)
    : mReference(reference)
    , mSeed(seed)
    , mBounds(boundingRect)
{
    // The gap closing fill assumes coordinates start at (0, 0).
    Q_ASSERT(boundingRect.left() == 0 && boundingRect.top() == 0);
    Q_ASSERT(boundingRect.contains(seed));
}

void ScanlineFill::setThreshold(int squaredTolerance)
{
    mThreshold = squaredTolerance;
}

void ScanlineFill::setCloseGap(int closeGap)
{
    mCloseGap = closeGap;
}

void ScanlineFill::setUntilColor(bool enabled, QRgb boundaryColor)
{
    mUntilColor = enabled;
    mBoundaryColor = boundaryColor;
}

inline bool ScanlineFill::pixelSelected(int x, int y)
{
    const QRgb pixel = referencePixel(x, y);

    if (mUntilColor) {
        // Select all until color: selected when the pixel is NOT similar to
        // the boundary color (Krita's SelectAllUntilColorHardSelectionPolicy).
        return !BitmapImage::compareColor(pixel, mBoundaryColor, mThreshold, nullptr);
    }

    return BitmapImage::compareColor(pixel, mReferenceColor, mThreshold, &mDiffCache);
}

bool ScanlineFill::tryPushingCloseGapSeed(int x, int y, bool allowExpand)
{
    bool pushed = false;

    if (mGapMap && mGapMap->gapSize() > 0) {
        const quint32 distance = mGapMap->distance(x, y);

        if (distance != GapMap::DISTANCE_INFINITE) {
            pushed = true;

            CloseGapFillPoint seed;
            seed.x = x;
            seed.y = y;
            seed.distance = distance;
            seed.allowExpand = allowExpand;

            mCloseGapQueue.push(seed);
        }
    }

    return pushed;
}

void ScanlineFill::extendedPass(FillInterval* currentInterval, int srcRow, bool extendRight)
{
    int x;
    int endX;
    int columnIncrement;
    int* intervalBorder;
    int* backwardIntervalBorder;
    FillInterval backwardInterval(currentInterval->start, currentInterval->end, srcRow);

    if (extendRight) {
        x = currentInterval->end;
        endX = mBounds.right();
        if (x >= endX) return;
        columnIncrement = 1;
        intervalBorder = &currentInterval->end;

        backwardInterval.start = currentInterval->end + 1;
        backwardIntervalBorder = &backwardInterval.end;
    } else {
        x = currentInterval->start;
        endX = mBounds.left();
        if (x <= endX) return;
        columnIncrement = -1;
        intervalBorder = &currentInterval->start;

        backwardInterval.end = currentInterval->start - 1;
        backwardIntervalBorder = &backwardInterval.start;
    }

    do {
        x += columnIncrement;

        const bool selected = pixelSelected(x, srcRow);
        const bool stopOnGap = tryPushingCloseGapSeed(x, srcRow, false);

        if (!stopOnGap && selected) {
            *intervalBorder = x;
            *backwardIntervalBorder = x;
            fillMaskPixel(x, srcRow);
        } else {
            break;
        }
    } while (x != endX);

    if (backwardInterval.isValid()) {
        mBackwardMap.insertInterval(backwardInterval);
    }
}

void ScanlineFill::processLine(FillInterval interval, const int rowIncrement)
{
    mBackwardMap.cropInterval(&interval);

    if (!interval.isValid()) return;

    const int firstX = interval.start;
    const int lastX = interval.end;
    int x = firstX;
    const int row = interval.row;
    const int nextRow = row + rowIncrement;

    FillInterval currentForwardInterval;

    while (x <= lastX) {
        const bool selected = pixelSelected(x, row);
        const bool stopOnGap = tryPushingCloseGapSeed(x, row, false);

        if (!stopOnGap && selected) {
            if (!currentForwardInterval.isValid()) {
                currentForwardInterval.start = x;
                currentForwardInterval.end = x;
                currentForwardInterval.row = nextRow;
            } else {
                currentForwardInterval.end = x;
            }

            fillMaskPixel(x, row);

            if (x == firstX) {
                extendedPass(&currentForwardInterval, row, false);
            }

            if (x == lastX) {
                extendedPass(&currentForwardInterval, row, true);
            }
        } else {
            if (currentForwardInterval.isValid()) {
                mForwardStack.push(currentForwardInterval);
                currentForwardInterval.invalidate();
            }
        }

        x++;
    }

    if (currentForwardInterval.isValid()) {
        mForwardStack.push(currentForwardInterval);
    }
}

FillInterval ScanlineFill::closeGapPass()
{
    FillInterval interval;

    while (!mCloseGapQueue.empty()) {
        const CloseGapFillPoint p = mCloseGapQueue.top();
        mCloseGapQueue.pop();

        if ((p.x < 0) || (p.x >= mBounds.width()) || (p.y < 0) || (p.y >= mBounds.height())) {
            continue;
        }

        // The initial pixel (before the fill) is non-opaque, so we try to fill it.
        if (pixelSelected(p.x, p.y)) {
            // However, check if it has already been filled during the ongoing
            // operation: if the mask is set, the pixel is already in the selection.
            if ((*mMask)[p.y * mBounds.width() + p.x] != MIN_SELECTED) {
                continue;
            }

            const quint32 previousDistance = p.distance;
            const quint32 distance = mGapMap->distance(p.x, p.y);
            const bool allowExpand = p.allowExpand && (distance >= previousDistance);
            const float relativeDiff = static_cast<float>(distance) / previousDistance;

            if ((relativeDiff < SpreadTolerance) || allowExpand) {
                if (distance == GapMap::DISTANCE_INFINITE) {
                    // Only return one interval at a time. Otherwise, just skip this pixel.
                    if (!interval.isValid()) {
                        interval.start = p.x;
                        interval.end = p.x;
                        interval.row = p.y;
                    }
                    // We still continue the loop, to process all the pixels we can fill.
                } else {
                    fillMaskPixel(p.x, p.y);

                    // Forward the context information to the next pixels.
                    // Spread the fill in four directions: up, down, left, right.

                    CloseGapFillPoint next;
                    next.distance = distance;
                    next.allowExpand = allowExpand;

                    next.x = p.x - 1;
                    next.y = p.y;
                    mCloseGapQueue.push(next);

                    next.x = p.x + 1;
                    next.y = p.y;
                    mCloseGapQueue.push(next);

                    next.x = p.x;
                    next.y = p.y - 1;
                    mCloseGapQueue.push(next);

                    next.x = p.x;
                    next.y = p.y + 1;
                    mCloseGapQueue.push(next);
                }
            }
        }
    }

    // If valid, the scanline fill will take over next.
    return interval;
}

void ScanlineFill::runImpl()
{
    Q_ASSERT(mForwardStack.isEmpty());

    if (mUntilColor) {
        // Boundary fill: the reference color used for the color test is the
        // boundary color, nothing more to sample.
    } else {
        // Sampled before anything else: the boundary bitmap precomputed for
        // the gap map runs the same color test.
        mReferenceColor = referencePixel(mSeed.x(), mSeed.y());
    }

    if (mCloseGap > 0) {
        // Precompute the boundary (lineart) bitmap: a pixel is a boundary
        // when it fails the fill's own color test. This mirrors Krita, which
        // derives the gap map's opacity from the fill selection policies.
        mBoundaryBitmap = QBitArray(mBounds.width() * mBounds.height(), false);
        for (int y = 0; y < mBounds.height(); ++y) {
            for (int x = 0; x < mBounds.width(); ++x) {
                mBoundaryBitmap[y * mBounds.width() + x] = !pixelSelected(x, y);
            }
        }

        mGapMap.reset(new GapMap(mCloseGap, mBoundaryBitmap,
                                 mBounds.width(), mBounds.height()));
    }

    mFillExtent = QRect();

    FillInterval startInterval(mSeed.x(), mSeed.x(), mSeed.y());

    // Decide if we should start with a scanline fill or a gap closing fill.
    if (!tryPushingCloseGapSeed(startInterval.start, startInterval.row, true)) {
        mForwardStack.push(startInterval);
    }

    // The outer loop is to continue scanline filling after a gap closing fill.
    do {
        /**
         * In the end of the first pass we should add an interval
         * containing the starting pixel, but directed into the opposite
         * direction. We cannot do it in the very beginning because the
         * intervals are offset by 1 pixel during every swap operation.
         */
        bool firstPass = true;

        // The scanline fill can only fill the pixels that are "in the open",
        // that is have DISTANCE_INFINITE distance. Gap pixels will be pushed
        // to the gap closing fill's queue.

        while (!mForwardStack.isEmpty()) {
            while (!mForwardStack.isEmpty()) {
                FillInterval interval = mForwardStack.pop();

                if (interval.row > mBounds.bottom() ||
                    interval.row < mBounds.top()) {

                    continue;
                }

                processLine(interval, mRowIncrement);
            }
            swapDirection();

            if (firstPass) {
                startInterval.row--;
                mForwardStack.push(startInterval);
                firstPass = false;
            }
        }

        // Perform a gap closing pass. This pass can in turn feed the scanline
        // fill's forward stack. It can only fill pixels with distance <
        // DISTANCE_INFINITE. This way, both passes complement each other to
        // complete the fill.

        if (!mCloseGapQueue.empty()) {
            startInterval = closeGapPass();
            if (startInterval.isValid()) {
                mForwardStack.push(startInterval);
            }
        }
    } while (!mForwardStack.isEmpty());
}

void ScanlineFill::fillSelection(QVector<quint8>& mask)
{
    mMask = &mask;
    runImpl();
    mMask = nullptr;
}
