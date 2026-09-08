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
#ifndef BITMAPBUCKET_H
#define BITMAPBUCKET_H

#include "bitmapimage.h"
#include "basetool.h"

#include <functional>

class Layer;
class Editor;

enum class BucketState
{
    WillFillTarget, // Before applying to target image
    DidFillTarget, // After calling floodfill and applied to target
};

/** Which pixels the bucket fills (Krita's fill tool "fill mode"). */
enum class FillRegionMode
{
    Contiguous = 0, ///< flood fill a contiguous region (default)
    Similar = 1,    ///< fill every region of a similar color, connected or not
    UntilColor = 2, ///< contiguous fill that only stops at the boundary color
    Selection = 3,  ///< fill the active selection (or the whole fill region)
};

/** Behavior while dragging the bucket (Krita's "continuous fill mode"). */
enum class DragFillMode
{
    SimilarRegions = 0, ///< only fill regions similar in color to the initial one
    AnyRegion = 1,      ///< fill regions of any color
    Disabled = 2,       ///< dragging does not fill
};

class BitmapBucket
{
public:
    explicit BitmapBucket();
    explicit BitmapBucket(Editor* editor, QColor color, QRect maxFillRegion, QPointF fillPoint,
                          BucketToolProperties properties, int regionModeOverride = -1);

    /** Will paint at the given point, given that it makes sense.. canUse is always called prior to painting
     *
     * @param updatedPoint - the point where to point
     * @param progress - a function that returns the progress of the paint operation,
     * the layer and frame that was affected at the given point.
     */
    void paint(const QPointF& updatedPoint, std::function<void(BucketState, int, int)> progress);

private:


    /** Based on the various factors dependant on which tool properties are set,
     *  the result will:
     *
     *  BucketProgress: BeforeFill
     *  to allow filling
     *
     * @param checkPoint
     * @return True if you are allowed to fill, otherwise false
     */
    bool allowFill(const QPoint& checkPoint, const QRgb& checkColor) const;
    bool allowContinuousFill(const QPoint& checkPoint, const QRgb& checkColor) const;

    /** Determines whether fill to drag feature can be used */
    bool canUseDragToFill(const QPoint& fillPoint, const QColor& bucketColor, const BitmapImage& referenceImage);

    BitmapImage flattenBitmapLayersToImage();

    /** Renders the reference image over the work region into a transparent
     *  ARGB32 premultiplied buffer (pixels outside the reference bounds read
     *  as transparent). */
    QImage referenceRegionImage(const QRect& workRect);

    Editor* mEditor = nullptr;
    Layer* mTargetFillToLayer = nullptr;

    QHash<QRgb, bool> *mPixelCache;

    BitmapImage mReferenceImage;
    QRgb mBucketColor = 0;
    QRgb mStartReferenceColor = 0;

    QRect mMaxFillRegion;

    int mTolerance = 0;

    int mTargetFillToLayerIndex = -1;
    bool mFilledOnce = false;
    bool mUseDragToFill = false;

    int mRegionModeOverride = -1;

    BucketToolProperties mProperties;
};

#endif // BITMAPBUCKET_H
