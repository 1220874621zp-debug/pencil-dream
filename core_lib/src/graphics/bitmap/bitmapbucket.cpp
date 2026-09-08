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

#include "scanlinefill.h"
#include "fillfilters.h"

BitmapBucket::BitmapBucket()
{
}

BitmapBucket::BitmapBucket(Editor* editor,
                           QColor color,
                           QRect maxFillRegion,
                           QPointF fillPoint,
                           BucketToolProperties properties,
                           int regionModeOverride):
    mEditor(editor),
    mMaxFillRegion(maxFillRegion),
    mProperties(properties),
    mRegionModeOverride(regionModeOverride)
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

    const bool dragEnabled = properties.dragFillMode() != static_cast<int>(DragFillMode::Disabled);
    mUseDragToFill = dragEnabled && canUseDragToFill(point, color, singleLayerImage);

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

    if (checkColor == mBucketColor && (mProperties.fillMode() == 1 || qAlpha(checkColor) == 255))
    {
        // Avoid filling if target pixel color matches fill color
        // to avoid creating numerous seemingly useless undo operations
        return false;
    }

    if (mProperties.dragFillMode() == static_cast<int>(DragFillMode::AnyRegion)) {
        // Fill regions of any color: no reference color gating.
        return true;
    }

    const QRgb& colorOfReferenceImage = mReferenceImage.constScanLine(checkPoint.x(), checkPoint.y());

    return BitmapImage::compareColor(colorOfReferenceImage, mStartReferenceColor, mTolerance, mPixelCache) &&
           (checkColor == 0 || BitmapImage::compareColor(checkColor, mStartReferenceColor, mTolerance, mPixelCache));
}

QImage BitmapBucket::referenceRegionImage(const QRect& workRect)
{
    QImage region(workRect.size(), QImage::Format_ARGB32_Premultiplied);
    region.fill(Qt::transparent);

    BitmapImage& ref = mReferenceImage;
    QRect srcRect = ref.bounds().intersected(workRect);
    if (!srcRect.isEmpty()) {
        QPainter p(&region);
        p.drawImage(srcRect.topLeft() - workRect.topLeft(),
                    *ref.image(), srcRect.translated(-ref.bounds().topLeft()));
        p.end();
    }
    return region;
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
    const int growValue = mProperties.fillExpandEnabled() ? mProperties.fillExpandAmount() : 0;
    const int featherValue = qMax(0, mProperties.featherPx());
    const int closeGapValue = qMax(0, mProperties.closeGapPx());
    // The post filters can move the mask outward, so reserve a margin for
    // them around the scanned fill region (Krita grows the selection rect the
    // same way before filtering).
    const int filterMargin = qMax(qMax(growValue, 0), featherValue);

    // The fill region must also cover the reference content, so regions
    // connected through transparent pixels beyond the camera view still fill
    // (parity with the previous implementation).
    QRect fillRegion = mMaxFillRegion.united(mReferenceImage.bounds().adjusted(-1, -1, 1, 1));
    QRect hardCap; // empty = the flood may sweep the whole camera region
    if (!selectionClip.isEmpty())
    {
        // Krita parity (kis_tool_fill): an active selection bounds the
        // scanned area to its rect - everything outside is masked away by
        // the caller anyway, and on an empty canvas this keeps the flood
        // from sweeping all ~2M pixels of the camera view
        const int margin = qMax(growValue, 0) + 4;
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

    if (!allowFill(point, targetPixelColor)) {
        return;
    }

    if (!selectionClip.isEmpty() && !selectionClip.contains(updatedPoint))
    {
        // a click outside the selection cannot produce a visible fill
        return;
    }

    // === Mask pipeline (Krita's fill architecture): generate a grayscale
    // selection mask over the work region, post-process it, then paint the
    // fill color through it. ===

    const QRect workRect = fillRegion.adjusted(-filterMargin, -filterMargin,
                                               filterMargin, filterMargin);
    const int w = workRect.width();
    const int h = workRect.height();
    Q_ASSERT(w > 0 && h > 0);

    const QImage refRegion = referenceRegionImage(workRect);

    // 兜底：部分构造路径（如测试）插入的属性集不含新键，此时 PropertyInfo
    // 为 INVALID，整值读出 -1，按默认的连续区域处理。
    int regionMode = (mRegionModeOverride >= 0)
            ? mRegionModeOverride : mProperties.regionFillMode();
    if (regionMode < 0) {
        regionMode = static_cast<int>(FillRegionMode::Contiguous);
    }

    QVector<quint8> mask(w * h, 0);
    const QPoint seedLocal = point - workRect.topLeft();
    // compareColor wants the squared tolerance
    const int squaredTolerance = static_cast<int>(qPow(mTolerance, 2));

    switch (regionMode)
    {
    case static_cast<int>(FillRegionMode::Contiguous):
    case static_cast<int>(FillRegionMode::UntilColor):
    {
        ScanlineFill fill(refRegion, seedLocal, QRect(0, 0, w, h));
        fill.setThreshold(squaredTolerance);
        if (regionMode == static_cast<int>(FillRegionMode::UntilColor)) {
            fill.setUntilColor(true, qPremultiply(static_cast<QRgb>(mProperties.boundaryColor())));
        }
        fill.setCloseGap(closeGapValue);
        fill.fillSelection(mask);
        break;
    }
    case static_cast<int>(FillRegionMode::Similar):
    {
        // Fill every region of a similar color, connected or not: a plain
        // sweep of the color test over the whole region (Krita's
        // createSimilarColorsSelection).
        const QRgb seedColor = mStartReferenceColor;
        for (int y = 0; y < h; ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(refRegion.constScanLine(y));
            for (int x = 0; x < w; ++x) {
                if (BitmapImage::compareColor(row[x], seedColor, squaredTolerance, mPixelCache)) {
                    mask[y * w + x] = 255;
                }
            }
        }
        break;
    }
    case static_cast<int>(FillRegionMode::Selection):
    {
        // Fill the active selection (or, without one, the whole region).
        // The selection clip below trims the mask either way.
        mask.fill(255);
        break;
    }
    default:
        break;
    }

    // === Post filters, in Krita's order: grow/shrink, then feather or
    // antialias (feathering already smooths, so antialias is skipped). ===

    if (growValue > 0) {
        if (mProperties.growStopDarkestEnabled()) {
            FillFilters::growUntilDarkestPixel(mask, refRegion, w, h, growValue);
        } else {
            FillFilters::growSelection(mask, w, h, growValue);
        }
    } else if (growValue < 0) {
        FillFilters::shrinkSelection(mask, w, h, -growValue);
    }

    if (featherValue > 0) {
        FillFilters::featherSelection(mask, w, h, featherValue);
    } else if (mProperties.antiAliasingEnabled()) {
        FillFilters::antialiasSelection(mask, w, h);
    }

    // Constrain the fill to the active selection, the same way brush
    // strokes are (Krita's fill tools are selection-aware too): multiply
    // the mask by the rasterized selection alpha.
    if (!selectionClip.isEmpty())
    {
        QImage selectionRaster(w, h, QImage::Format_ARGB32_Premultiplied);
        selectionRaster.fill(Qt::transparent);
        QPainter rasterPainter(&selectionRaster);
        rasterPainter.translate(-workRect.topLeft());
        rasterPainter.setRenderHint(QPainter::Antialiasing, true);
        rasterPainter.fillPath(selectionClip, Qt::white);
        rasterPainter.end();

        for (int y = 0; y < h; ++y) {
            const QRgb* selRow = reinterpret_cast<const QRgb*>(selectionRaster.constScanLine(y));
            quint8* maskRow = mask.data() + y * w;
            for (int x = 0; x < w; ++x) {
                maskRow[x] = static_cast<quint8>((maskRow[x] * qAlpha(selRow[x])) / 255);
            }
        }
    }

    // Tight bounds of the filled area, so the pasted image (and the undo
    // footprint) stay minimal.
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y) {
        const quint8* row = mask.constData() + y * w;
        for (int x = 0; x < w; ++x) {
            if (row[x]) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < 0) {
        // nothing to fill (e.g. clicked on a boundary pixel with no tolerance)
        return;
    }
    const QRect maskBounds(minX, minY, maxX - minX + 1, maxY - minY + 1);

    qDebug() << "[bucket] mask bounds=" << maskBounds.translated(workRect.topLeft())
             << " regionMode=" << regionMode << " tol=" << mTolerance
             << " grow=" << growValue << " feather=" << featherValue
             << " closeGap=" << closeGapValue << " took" << fillTimer.elapsed() << "ms";

    // === Paint the fill color through the mask ===

    QRgb fillColor = mBucketColor;
    if (mProperties.fillMode() == 1)
    {
        // Pass a fully opaque version of the new color to the mask
        // This is required so we can fully mask out the existing data before
        // writing the new color.
        QColor tempColor;
        tempColor.setRgba(fillColor);
        tempColor.setAlphaF(1);
        fillColor = tempColor.rgba();
    }

    BitmapImage* replaceImage = new BitmapImage(maskBounds.translated(workRect.topLeft()), Qt::transparent);
    {
        QImage* out = replaceImage->image();
        const int colorAlpha = qAlpha(fillColor);
        const int outR = qRed(fillColor), outG = qGreen(fillColor), outB = qBlue(fillColor);
        for (int y = maskBounds.top(); y <= maskBounds.bottom(); ++y) {
            const quint8* row = mask.constData() + y * w;
            QRgb* outRow = reinterpret_cast<QRgb*>(out->scanLine(y - maskBounds.top()));
            for (int x = maskBounds.left(); x <= maskBounds.right(); ++x) {
                const quint8 m = row[x];
                if (m == 0) continue;
                if (m == 255) {
                    outRow[x - maskBounds.left()] = fillColor;
                } else {
                    // scale the premultiplied fill color by the mask value
                    const int a = (colorAlpha * m + 127) / 255;
                    outRow[x - maskBounds.left()] = qRgba((outR * m + 127) / 255,
                                                          (outG * m + 127) / 255,
                                                          (outB * m + 127) / 255,
                                                          a);
                }
            }
        }
    }

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
