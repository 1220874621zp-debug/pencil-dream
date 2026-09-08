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
#include "bitmapbucket.h"

#include <QtMath>
#include <QDebug>
#include <QPainter>
#include <QElapsedTimer>

#include "editor.h"
#include "layermanager.h"
#include "selectionmanager.h"

#include "layerbitmap.h"

BitmapBucket::BitmapBucket()
{
}

BitmapBucket::BitmapBucket(Editor* editor,
                           QColor color,
                           QRect maxFillRegion,
                           QPointF fillPoint,
                           BucketToolProperties properties):
    mEditor(editor),
    mMaxFillRegion(maxFillRegion),
    mProperties(properties)

{
    Layer* initialLayer = editor->layers()->currentLayer();
    int initialLayerIndex = mEditor->currentLayerIndex();
    int frameIndex = mEditor->currentFrame();

    mBucketColor = qPremultiply(color.rgba());

    mTargetFillToLayer = initialLayer;
    mTargetFillToLayerIndex = initialLayerIndex;

    mTolerance = mProperties.colorToleranceEnabled() ? mProperties.tolerance() : 0;
    const QPoint& point = QPoint(qFloor(fillPoint.x()), qFloor(fillPoint.y()));

    Q_ASSERT(mTargetFillToLayer);

    BitmapImage singleLayerImage = *static_cast<BitmapImage*>(initialLayer->getLastKeyFrameAtPosition(frameIndex));
    if (properties.fillReferenceMode() == 1) // All layers
    {
        mReferenceImage = flattenBitmapLayersToImage();
    } else {
        mReferenceImage = singleLayerImage;
    }
    mStartReferenceColor = mReferenceImage.constScanLine(point.x(), point.y());
    mUseDragToFill = canUseDragToFill(point, color, singleLayerImage);

    mPixelCache = new QHash<QRgb, bool>();
}

bool BitmapBucket::canUseDragToFill(const QPoint& fillPoint, const QColor& bucketColor, const BitmapImage& referenceImage)
{
    QRgb pressReferenceColorSingleLayer = referenceImage.constScanLine(fillPoint.x(), fillPoint.y());
    QRgb startRef = qUnpremultiply(pressReferenceColorSingleLayer);

    if (mProperties.fillMode() == 0 && ((QColor(qRed(startRef), qGreen(startRef), qBlue(startRef)) == bucketColor.rgb() && qAlpha(startRef) == 255) || bucketColor.alpha() == 0)) {
        // In overlay mode: When the reference pixel matches the bucket color and the reference is fully opaque
        // Otherwise when the bucket alpha is zero.
        return false;
    } else if (mProperties.fillMode() == 2 && qAlpha(startRef) == 255) {
        // In behind mode: When the reference pixel is already fully opaque, the output will be invisible.
        return false;
    }

    return true;
}

bool BitmapBucket::allowFill(const QPoint& checkPoint, const QRgb& checkColor) const
{
    // A normal click to fill should happen unconditionally, because the alternative is utterly confusing.
    if (!mFilledOnce) {
        return true;
    }

    return allowContinuousFill(checkPoint, checkColor);
}

bool BitmapBucket::allowContinuousFill(const QPoint& checkPoint, const QRgb& checkColor) const
{
    if (!mUseDragToFill) {
        return false;
    }

    const QRgb& colorOfReferenceImage = mReferenceImage.constScanLine(checkPoint.x(), checkPoint.y());

    if (checkColor == mBucketColor && (mProperties.fillMode() == 1 || qAlpha(checkColor) == 255))
    {
        // Avoid filling if target pixel color matches fill color
        // to avoid creating numerous seemingly useless undo operations
        return false;
    }

    return BitmapImage::compareColor(colorOfReferenceImage, mStartReferenceColor, mTolerance, mPixelCache) &&
           (checkColor == 0 || BitmapImage::compareColor(checkColor, mStartReferenceColor, mTolerance, mPixelCache));
}

void BitmapBucket::paint(const QPointF& updatedPoint, std::function<void(BucketState, int, int)> state)
{
    const int currentFrameIndex = mEditor->currentFrame();

    BitmapImage* targetImage = static_cast<LayerBitmap*>(mTargetFillToLayer)->getLastBitmapImageAtFrame(currentFrameIndex);
    if (targetImage == nullptr) { return; } // Can happen if the first frame is deleted while drawing

    QPoint point = QPoint(qFloor(updatedPoint.x()), qFloor(updatedPoint.y()));

    // The flood treats pixels beyond the content bounds as transparent, so a
    // click on empty canvas (e.g. inside a lasso) fills AT the click position.
    // The camera-derived region can be degenerate, so instead of rejecting
    // the click, grow the scanned area to always cover the click and the
    // active selection (which bounds the visible result anyway).
    const QPainterPath selectionClip = mEditor->select()->selectionClipPath();
    int expandValue = mProperties.fillExpandEnabled() ? mProperties.fillExpandAmount() : 0;
    QRect fillRegion = mMaxFillRegion;
    QRect hardCap; // empty = the flood may sweep the whole camera region
    if (!selectionClip.isEmpty())
    {
        // Krita parity (kis_tool_fill): an active selection bounds the
        // scanned area to its rect - everything outside is masked away by
        // the caller anyway, and on an empty canvas this keeps the flood
        // from sweeping all ~2M pixels of the camera view
        const int margin = expandValue + 4;
        hardCap = selectionClip.boundingRect().toAlignedRect()
                      .adjusted(-margin, -margin, margin, margin);
        fillRegion = fillRegion.intersected(hardCap);
    }
    if (!fillRegion.contains(point))
    {
        fillRegion = fillRegion.united(QRect(point, QSize(1, 1)));
    }

    QElapsedTimer fillTimer;
    fillTimer.start();

    if (!targetImage->isLoaded())
    {
        // A keyframe nobody has drawn into yet keeps a null image, and the
        // fill used to bail out silently on the isLoaded check. Materialize
        // the frame over the fill region first - a transparent paste grows
        // the bounds and allocates the image without changing any pixel.
        BitmapImage materialize(fillRegion, Qt::transparent);
        targetImage->paste(&materialize);
    }

    const QRgb& targetPixelColor = targetImage->constScanLine(point.x(), point.y());

    qDebug() << "[bucket] paint pt=" << point << " refBounds=" << mReferenceImage.bounds()
             << " selEmpty=" << selectionClip.isEmpty()
             << " selBounds=" << selectionClip.boundingRect()
             << " fillRegion=" << fillRegion
             << " fillMode=" << mProperties.fillMode() << " tol=" << mTolerance
             << " targetPx=" << targetPixelColor;

    if (!allowFill(point, targetPixelColor)) {
        qDebug() << "[bucket] rejected by allowFill";
        return;
    }

    if (!selectionClip.isEmpty() && !selectionClip.contains(updatedPoint))
    {
        // a click outside the selection cannot produce a visible fill
        qDebug() << "[bucket] click outside selection, dropped";
        return;
    }

    QRgb fillColor = mBucketColor;
    if (mProperties.fillMode() == 1)
    {
        // Pass a fully opaque version of the new color to floodFill
        // This is required so we can fully mask out the existing data before
        // writing the new color.
        QColor tempColor;
        tempColor.setRgba(fillColor);
        tempColor.setAlphaF(1);
        fillColor = tempColor.rgba();
    }

    BitmapImage* replaceImage = nullptr;

    bool didFloodFill = BitmapImage::floodFill(&replaceImage,
                           &mReferenceImage,
                           fillRegion,
                           point,
                           fillColor,
                           mTolerance,
                           expandValue,
                           hardCap.isEmpty() ? nullptr : &hardCap);

    if (!didFloodFill) {
        qDebug() << "[bucket] floodFill returned false";
        delete replaceImage;
        return;
    }
    Q_ASSERT(replaceImage != nullptr);

    // constrain the fill to the active selection, the same way brush
    // strokes are (Krita's fill tools are selection-aware too)
    if (!selectionClip.isEmpty())
    {
        if (!selectionClip.intersects(QRectF(replaceImage->bounds())))
        {
            qDebug() << "[bucket] fill" << replaceImage->bounds() << "does not intersect selection, dropped";
            delete replaceImage;
            return;
        }

        // erase the fill outside the selection through an odd-even inverse
        // fill; (DestinationIn + drawPath is a no-op on the raster engine,
        // verified by isolation test)
        QImage* fillData = replaceImage->image();
        QPainter masker(fillData);
        masker.translate(-replaceImage->topLeft());
        QPainterPath erasePath;
        erasePath.setFillRule(Qt::OddEvenFill);
        erasePath.addRect(QRectF(replaceImage->bounds()).adjusted(-2.0, -2.0, 2.0, 2.0));
        erasePath.addPath(selectionClip);
        masker.setCompositionMode(QPainter::CompositionMode_Clear);
        masker.fillPath(erasePath, Qt::white);
        masker.end();
    }

    qDebug() << "[bucket] filling bounds=" << replaceImage->bounds()
             << " masked=" << !selectionClip.isEmpty() << " mode=" << mProperties.fillMode()
             << " took" << fillTimer.elapsed() << "ms";

    state(BucketState::WillFillTarget, mTargetFillToLayerIndex, currentFrameIndex);

    if (mProperties.fillMode() == 0)
    {
        targetImage->paste(replaceImage);
    }
    else if (mProperties.fillMode() == 2)
    {
        targetImage->paste(replaceImage, QPainter::CompositionMode_DestinationOver);
    }
    else
    {
        // fill mode replace
        targetImage->paste(replaceImage, QPainter::CompositionMode_DestinationOut);
        // Reduce the opacity of the fill to match the new color
        BitmapImage properColor(replaceImage->bounds(), QColor::fromRgba(mBucketColor));
        properColor.paste(replaceImage, QPainter::CompositionMode_DestinationIn);
        // Write reduced-opacity fill image on top of target image
        targetImage->paste(&properColor);
    }

    targetImage->modification();
    delete replaceImage;

    state(BucketState::DidFillTarget, mTargetFillToLayerIndex, currentFrameIndex);
    mFilledOnce = true;
}

BitmapImage BitmapBucket::flattenBitmapLayersToImage()
{
    BitmapImage flattenImage = BitmapImage();
    int currentFrame = mEditor->currentFrame();
    auto layerMan = mEditor->layers();
    for (int i = 0; i < layerMan->count(); i++)
    {
        Layer* layer = layerMan->getLayer(i);
        Q_ASSERT(layer);
        if (layer->type() == Layer::BITMAP && layer->visible())
        {
            BitmapImage* image = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(currentFrame);
            if (image) {
                flattenImage.paste(image);
            }
        }
    }
    return flattenImage;
}
