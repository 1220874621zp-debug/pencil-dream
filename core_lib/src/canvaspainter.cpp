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

#include "canvaspainter.h"

#include <QtMath>
#include <QPainterPath>

#include "object.h"
#include "layerbitmap.h"
#include "layercolorize.h"
#include "colorizeimage.h"
#include "bitmapimage.h"
#include "tile.h"
#include "tiledbuffer.h"

#include "painterutils.h"

CanvasPainter::CanvasPainter(QPixmap& canvas) : mCanvas(canvas)
{
    reset();
}

CanvasPainter::~CanvasPainter()
{
}

void CanvasPainter::reset()
{
    mPostLayersPixmap = QPixmap(mCanvas.size());
    mPreLayersPixmap = QPixmap(mCanvas.size());
    mCurrentLayerPixmap = QPixmap(mCanvas.size());
    mOnionSkinPixmap = QPixmap(mCanvas.size());
    mPreLayersPixmap.fill(Qt::transparent);
    mCanvas.fill(Qt::transparent);
    mCurrentLayerPixmap.fill(Qt::transparent);
    mPostLayersPixmap.fill(Qt::transparent);
    mOnionSkinPixmap.fill(Qt::transparent);
    mCurrentLayerPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mPreLayersPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mPostLayersPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mOnionSkinPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
}

void CanvasPainter::setViewTransform(const QTransform view, const QTransform viewInverse)
{
    if (mViewTransform != view || mViewInverse != viewInverse) {
        mViewTransform = view;
        mViewInverse = viewInverse;
    }
}

void CanvasPainter::setTransformedSelection(QRect selection, QTransform transform, QPolygonF selectionPolygon)
{
    // Make sure that the selection is not empty
    if (selection.width() > 0 && selection.height() > 0)
    {
        mSelection = selection;
        mSelectionTransform = transform;
        mSelectionPolygon = selectionPolygon;
        mRenderTransform = true;
    }
    else
    {
        // Otherwise we shouldn't be in transformation mode
        ignoreTransformedSelection();
    }
}

void CanvasPainter::ignoreTransformedSelection()
{
    mRenderTransform = false;
    mSelectionTransform.reset();
    mSelection = QRect();
    mSelectionPolygon = QPolygonF();
}

void CanvasPainter::setDeformPreview(const QImage& preview, const QRectF& targetRect)
{
    mDeformPreview = preview;
    mDeformPreviewTargetRect = targetRect;
    mDeformPreviewActive = !preview.isNull();
}

void CanvasPainter::clearDeformPreview()
{
    mDeformPreviewActive = false;
    mDeformPreview = QImage();
}

void CanvasPainter::paintCached(const QRect& blitRect)
{
    if (!mPreLayersPixmapCacheValid)
    {
        if (mAnyClipMask)
        {
            ensureClipAccum();
            clearClipAccum(blitRect);
            mClipGroupValid = false;
        }
        QPainter preLayerPainter;
        initializePainter(preLayerPainter, mPreLayersPixmap, blitRect);
        renderPreLayers(preLayerPainter, blitRect);
        preLayerPainter.end();
        mPreLayersPixmapCacheValid = true;
        if (mAnyClipMask)
        {
            // snapshot for cached-pre passes: the current layer still needs
            // the alpha of everything below it
            mClipAccumAfterPre = mClipAccum;
            mClipAfterPreValid = true;
        }
    }
    else if (mAnyClipMask)
    {
        ensureClipAccum();
        if (mClipAfterPreValid)
        {
            // restore the after-pre state; QImage assignment is copy-on-write
            mClipAccum = mClipAccumAfterPre;
        }
        mClipGroupValid = false;
    }

    QPainter mainPainter;
    initializePainter(mainPainter, mCanvas, blitRect);
    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPreLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    paintCurrentFrame(mainPainter, blitRect, mCurrentLayerIndex, mCurrentLayerIndex);

    if (!mPostLayersPixmapCacheValid)
    {
        QPainter postLayerPainter;
        initializePainter(postLayerPainter, mPostLayersPixmap, blitRect);
        renderPostLayers(postLayerPainter, blitRect);
        postLayerPainter.end();
        mPostLayersPixmapCacheValid = true;
    }

    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPostLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);
}

void CanvasPainter::resetLayerCache()
{
    mPreLayersPixmapCacheValid = false;
    mPostLayersPixmapCacheValid = false;
}

void CanvasPainter::initializePainter(QPainter& painter, QPaintDevice& device, const QRect& blitRect)
{
    painter.begin(&device);

    // Only draw inside the clipped rectangle
    painter.setClipRect(blitRect);

    // Clear the area that's about to be painted again, to avoid painting on top of existing pixels
    // causing artifacts.
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(blitRect, Qt::transparent);

    // Surface has been cleared and is ready to be painted on
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setWorldMatrixEnabled(true);
    painter.setWorldTransform(mViewTransform);
}

void CanvasPainter::renderPreLayers(QPainter& painter, const QRect& blitRect)
{
    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, 0, mCurrentLayerIndex - 1);
    }

    paintOnionSkin(painter, blitRect);
    painter.setOpacity(1.0);
}

void CanvasPainter::renderPostLayers(QPainter& painter, const QRect& blitRect)
{
    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, mCurrentLayerIndex + 1, mObject->getLayerCount() - 1);
    }
}

void CanvasPainter::setPaintSettings(const Object* object, int currentLayer, int frame, TiledBuffer* tiledBuffer)
{
    Q_ASSERT(object);
    mObject = object;
    mAnyClipMask = false;
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        Layer* layer = object->getLayer(i);
        if (layer && layer->type() == Layer::BITMAP && layer->clipMask())
        {
            mAnyClipMask = true;
            break;
        }
    }

    CANVASPAINTER_LOG("Set CurrentLayerIndex = %d", currentLayer);
    mCurrentLayerIndex = currentLayer;
    mFrameNumber = frame;
    mTiledBuffer = tiledBuffer;
}

void CanvasPainter::paint(const QRect& blitRect)
{
    QPainter preLayerPainter;
    QPainter mainPainter;
    QPainter postLayerPainter;

    if (mAnyClipMask)
    {
        ensureClipAccum();
        clearClipAccum(blitRect);
        mClipGroupValid = false;
    }

    initializePainter(mainPainter, mCanvas, blitRect);

    initializePainter(preLayerPainter, mPreLayersPixmap, blitRect);
    renderPreLayers(preLayerPainter, blitRect);
    preLayerPainter.end();
    if (mAnyClipMask)
    {
        mClipAccumAfterPre = mClipAccum;
        mClipAfterPreValid = true;
    }

    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPreLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    paintCurrentFrame(mainPainter, blitRect, mCurrentLayerIndex, mCurrentLayerIndex);

    initializePainter(postLayerPainter, mPostLayersPixmap, blitRect);
    renderPostLayers(postLayerPainter, blitRect);
    postLayerPainter.end();

    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPostLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    mPreLayersPixmapCacheValid = true;
    mPostLayersPixmapCacheValid = true;
}

void CanvasPainter::paintOnionSkinOnLayer(QPainter& painter, const QRect& blitRect, Layer* layer)
{
    mOnionSkinSubPainter.paint(painter, layer, mOnionSkinPainterOptions, mFrameNumber, [&] (OnionSkinPaintState state, int onionFrameNumber) {
        if (state == OnionSkinPaintState::PREV) {
            switch (layer->type())
            {
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizePrevFrames); break; }
            default: break;
            }
        }
        if (state == OnionSkinPaintState::NEXT) {
            switch (layer->type())
            {
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizeNextFrames); break; }
            default: break;
            }
        }
    });
}

void CanvasPainter::paintOnionSkin(QPainter& painter, const QRect& blitRect)
{
    if (!mOptions.bOnionSkinMultiLayer || mOptions.eLayerVisibility == LayerVisibility::CURRENTONLY) {
        Layer* layer = mObject->getLayer(mCurrentLayerIndex);
        paintOnionSkinOnLayer(painter, blitRect, layer);
    } else {
        for (int i = 0; i < mObject->getLayerCount(); i++) {
            Layer* layer = mObject->getLayer(i);
            if (layer == nullptr) { continue; }

            paintOnionSkinOnLayer(painter, blitRect, layer);
        }
    }
}

void CanvasPainter::paintBitmapOnionSkinFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int nFrame, bool colorize)
{
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);

    BitmapImage* bitmapImage = bitmapLayer->getBitmapImageAtFrame(nFrame);

    if (bitmapImage == nullptr) { return; }
    bitmapImage->loadFile(); // Critical! force the BitmapImage to load the image

    QPainter onionSkinPainter;
    initializePainter(onionSkinPainter, mOnionSkinPixmap, blitRect);

    const OnionGhostTransform ghostT = onionGhostTransform(layer, nFrame);
    if (ghostT.isPlainTranslation())
    {
        onionSkinPainter.drawImage(bitmapImage->topLeft() + ghostT.offset, *bitmapImage->image());
    }
    else
    {
        // 旋转/缩放：painter 操作叠加在既有 viewTransform 之上（勿用 setTransform 整体替换），
        // 锚点 = 幽灵内容包围盒中心，与命中测试(alphaHit)用同一矩阵求逆
        const QPointF c = bitmapImage->bounds().center();
        onionSkinPainter.save();
        onionSkinPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        onionSkinPainter.setRenderHint(QPainter::Antialiasing, true);
        onionSkinPainter.translate(c + ghostT.offset);
        onionSkinPainter.rotate(ghostT.rotation);
        onionSkinPainter.scale(ghostT.scale, ghostT.scale);
        onionSkinPainter.translate(-c);
        onionSkinPainter.drawImage(bitmapImage->topLeft(), *bitmapImage->image());
        onionSkinPainter.restore();
    }
    paintOnionSkinFrame(painter, onionSkinPainter, nFrame, colorize, bitmapImage->getOpacity());
}

OnionGhostTransform CanvasPainter::onionGhostTransform(const Layer* layer, int nFrame) const
{
    if (mOnionGhostOffsets == nullptr || nFrame == mFrameNumber) { return OnionGhostTransform(); }
    auto it = mOnionGhostOffsets->constFind(layer->id());
    if (it == mOnionGhostOffsets->constEnd()) { return OnionGhostTransform(); }
    const OnionGhostOffset& ghost = it.value();
    if (nFrame < mFrameNumber && nFrame == ghost.prevFrameNumber)
    {
        return ghost.prev;
    }
    if (nFrame > mFrameNumber && nFrame == ghost.nextFrameNumber)
    {
        return ghost.next;
    }
    return OnionGhostTransform();
}

void CanvasPainter::paintOnionSkinFrame(QPainter& painter, QPainter& onionSkinPainter, int nFrame, bool colorize, qreal frameOpacity)
{
    // Don't transform the image here as we used the viewTransform in the image output
    painter.setWorldMatrixEnabled(false);
    // Remember to adjust overall opacity based on opacity value from image
    onionSkinPainter.setOpacity(frameOpacity - (1.0-painter.opacity()));
    if (colorize)
    {
        QColor colorBrush = Qt::transparent; //no color for the current frame

        if (nFrame < mFrameNumber)
        {
            colorBrush = Qt::red;
        }
        else if (nFrame > mFrameNumber)
        {
            colorBrush = Qt::blue;
        }
        onionSkinPainter.setWorldMatrixEnabled(false);

        onionSkinPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        onionSkinPainter.setBrush(colorBrush);
        onionSkinPainter.drawRect(painter.viewport());
    }
    painter.drawPixmap(mPointZero, mOnionSkinPixmap);
}

void CanvasPainter::paintCurrentBitmapFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer, QImage* clipMask)
{
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    // Block semantics: auto-length frames hold until the next keyframe, trimmed gaps render nothing
    BitmapImage* paintedImage = static_cast<BitmapImage*>(bitmapLayer->getKeyFrameWhichCovers(mFrameNumber));

    if (paintedImage == nullptr) { return; }
    paintedImage->loadFile(); // Critical! force the BitmapImage to load the image

    const bool isDrawing = mTiledBuffer && !mTiledBuffer->bounds().isEmpty();

    QPainter currentBitmapPainter;
    initializePainter(currentBitmapPainter, mCurrentLayerPixmap, blitRect);

    painter.setWorldMatrixEnabled(false);

    currentBitmapPainter.setOpacity(paintedImage->getOpacity() - (1.0-painter.opacity()));
    currentBitmapPainter.drawImage(paintedImage->topLeft(), *paintedImage->image());

    if (isCurrentLayer && isDrawing)
    {
        currentBitmapPainter.setCompositionMode(mOptions.cmBufferBlendMode);
        if (!mSelectionClipPath.isEmpty()) {
            // constrain live strokes to the active selection (lasso/rect)
            currentBitmapPainter.setClipPath(mSelectionClipPath);
        }
        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            currentBitmapPainter.drawPixmap(tile->posF(), tile->pixmap());
        }
    }

    // We do not wish to draw selection transformations on anything but the current layer
    Q_ASSERT(!isDrawing || mSelectionTransform.isIdentity());
    if (isCurrentLayer && mRenderTransform && !isDrawing) {
        paintTransformedSelection(currentBitmapPainter, paintedImage, mSelection);
    }

    // the layer pixmap can only host one painter at a time
    currentBitmapPainter.end();

    if (clipMask)
    {
        // keep the layer content only where the accumulated alpha below is
        // present (both share the canvas geometry and device pixel ratio)
        QPainter maskPainter(&mCurrentLayerPixmap);
        maskPainter.setWorldMatrixEnabled(false);
        maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        maskPainter.drawImage(mPointZero, *clipMask);
    }

    painter.drawPixmap(mPointZero, mCurrentLayerPixmap);
}

void CanvasPainter::ensureClipAccum()
{
    const qreal dpr = mCanvas.devicePixelRatioF();
    if (mClipAccum.size() != mCanvas.size()
        || !qFuzzyCompare(mClipAccum.devicePixelRatio(), dpr))
    {
        mClipAccum = QImage(mCanvas.size(), QImage::Format_ARGB32_Premultiplied);
        mClipAccum.setDevicePixelRatio(dpr);
        mClipAccum.fill(Qt::transparent);
        mClipGroupValid = false;
        mClipAfterPreValid = false;
    }
}

void CanvasPainter::clearClipAccum(const QRect& blitRect)
{
    QPainter accumPainter(&mClipAccum);
    accumPainter.setWorldMatrixEnabled(false);
    accumPainter.setCompositionMode(QPainter::CompositionMode_Clear);
    accumPainter.fillRect(blitRect, Qt::transparent);
}

void CanvasPainter::clipAccumulate(const QPixmap& layerContent, qreal opacity)
{
    QPainter accumPainter(&mClipAccum);
    accumPainter.setWorldMatrixEnabled(false);
    accumPainter.setOpacity(opacity);
    accumPainter.drawPixmap(mPointZero, layerContent);
}

void CanvasPainter::paintTransformedSelection(QPainter& painter, BitmapImage* bitmapImage, const QRect& selection) const
{
    // Make sure there is something selected
    if (selection.width() == 0 && selection.height() == 0)
        return;

    const bool polygonSelection = mSelectionPolygon.size() >= 3;

    QPixmap transformedPixmap;
    if (!mDeformPreviewActive)
    {
        transformedPixmap = QPixmap(mSelection.size());
        transformedPixmap.fill(Qt::transparent);

        QPainter imagePainter(&transformedPixmap);
        imagePainter.translate(-selection.topLeft());
        imagePainter.drawImage(bitmapImage->topLeft(), *bitmapImage->image());
        if (polygonSelection)
        {
            // mask the floating content to the lasso shape
            QPainterPath maskPath;
            maskPath.addPolygon(mSelectionPolygon.translated(-QPointF(selection.topLeft())));
            maskPath.setFillRule(Qt::OddEvenFill);
            imagePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            imagePainter.fillPath(maskPath, QColor(255, 255, 255, 255));
        }
        imagePainter.end();
    }

    painter.save();

    painter.setTransform(mViewTransform);

    // Clear the painted area to make it look like the content has been erased
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    if (polygonSelection)
    {
        QPainterPath clearPath;
        clearPath.addPolygon(mSelectionPolygon);
        clearPath.setFillRule(Qt::OddEvenFill);
        painter.fillPath(clearPath, QColor(255, 255, 255, 255));
    }
    else
    {
        painter.fillRect(selection, QColor(255, 255, 255, 255));
    }
    painter.restore();

    if (mDeformPreviewActive)
    {
        // free-deform tool: draw the warped preview instead of an
        // affine-transformed copy (possibly stretched back from a
        // down-scaled interactive warp)
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(mDeformPreviewTargetRect, mDeformPreview);
        painter.restore();
        return;
    }

    // Multiply the selection and view matrix to get proper rotation and scale values
    // Now the image origin will be topleft
    painter.setTransform(mSelectionTransform*mViewTransform);

    // Draw the selection image separately and on top
    painter.drawPixmap(selection, transformedPixmap);
    painter.restore();
}

/** Paints layers within the specified range for the current frame.
 *
 *  @param painter The painter to paint to
 *  @param startLayer The first layer to paint (inclusive)
 *  @param endLayer The last layer to paint (inclusive)
 */
void CanvasPainter::paintCurrentFrame(QPainter& painter, const QRect& blitRect, int startLayer, int endLayer)
{
    painter.setOpacity(1.0);
    qDebug() << "[填色] paintCurrentFrame" << startLayer << "-" << endLayer << "当前层" << mCurrentLayerIndex << "帧" << mFrameNumber;

    bool isCameraLayer = mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA;

    for (int i = startLayer; i <= endLayer; ++i)
    {
        Layer* layer = mObject->getLayer(i);
        qDebug() << "[填色] 层循环 i=" << i << "type=" << layer->type() << "visible=" << layer->visible() << layer->name();

        if (!layer->visible())
            continue;

        // the layer-panel opacity multiplies into the layer's overall presence
        if (mOptions.eLayerVisibility == LayerVisibility::RELATED && !isCameraLayer)
        {
            painter.setOpacity(layer->opacity() * calculateRelativeOpacityForLayer(mCurrentLayerIndex, i, mOptions.fLayerVisibilityThreshold));
        }
        else
        {
            painter.setOpacity(layer->opacity());
        }
        bool isCurrentLayer = mCurrentLayerIndex == i;

        CANVASPAINTER_LOG("  Render Layer[%d] %s", i, layer->name());
        switch (layer->type())
        {
        case Layer::BITMAP: {
            if (mAnyClipMask)
            {
                QImage* clip = nullptr;
                if (layer->clipMask())
                {
                    // a run of clipped layers shares one base snapshot
                    if (!mClipGroupValid)
                    {
                        mClipGroupMask = mClipAccum.copy();
                        mClipGroupMask.setDevicePixelRatio(mClipAccum.devicePixelRatio());
                        mClipGroupValid = true;
                    }
                    clip = &mClipGroupMask;
                }
                else
                {
                    mClipGroupValid = false;
                }
                paintCurrentBitmapFrame(painter, blitRect, layer, isCurrentLayer, clip);
                clipAccumulate(mCurrentLayerPixmap, painter.opacity());
            }
            else
            {
                paintCurrentBitmapFrame(painter, blitRect, layer, isCurrentLayer);
            }
            break;
        }
        case Layer::COLORIZE: {
            paintCurrentColorizeFrame(painter, blitRect, layer, i, isCurrentLayer);
            break;
        }
        default: break;
        }
    }
}

void CanvasPainter::paintCurrentColorizeFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int layerIndex, bool isCurrentLayer)
{
    qDebug() << "[填色] 渲染函数进入";
    auto colorizeLayer = static_cast<LayerColorize*>(layer);
    ColorizeImage* frame = colorizeLayer->getLastColorizeImageAtFrame(mFrameNumber);

    if (frame == nullptr) { return; }
    frame->loadFile();

    const bool isDrawing = mTiledBuffer && !mTiledBuffer->bounds().isEmpty();
    Q_UNUSED(isDrawing);
    qDebug() << "[填色] 渲染帧" << mFrameNumber << "着色" << (frame->coloringImage().isNull() ? QStringLiteral("空") : QString::number(frame->coloringImage().width()) + "x" + QString::number(frame->coloringImage().height())) << "显示填色" << colorizeLayer->showColoring() << "编辑模式" << colorizeLayer->editKeyStrokes();

    // 着色计算完全由用户手动触发（选项面板「刷新」→ ColorizeUpdateManager），
    // 渲染路径只负责显示现有缓存 + 笔画

    QPainter currentColorizePainter;
    initializePainter(currentColorizePainter, mCurrentLayerPixmap, blitRect);

    painter.setWorldMatrixEnabled(false);

    // 1) 着色结果垫底（Krita: Show output）
    if (colorizeLayer->showColoring() && !frame->coloringImage().isNull())
    {
        currentColorizePainter.setOpacity(frame->getOpacity() - (1.0 - painter.opacity()));
        currentColorizePainter.drawImage(frame->coloringBounds().topLeft(), frame->coloringImage());
    }

    // 2) 笔画全不透明显示（Krita: Edit key strokes 开时可见）
    if (colorizeLayer->editKeyStrokes() && frame->image() != nullptr && !frame->image()->isNull())
    {
        currentColorizePainter.setOpacity(qBound(0.0, painter.opacity(), 1.0));
        currentColorizePainter.drawImage(frame->topLeft(), *frame->image());
    }

    // 3) 当前层实时笔画缓冲（正在画）
    if (isCurrentLayer && isDrawing)
    {
        currentColorizePainter.setOpacity(frame->getOpacity() - (1.0 - painter.opacity()));
        currentColorizePainter.setCompositionMode(mOptions.cmBufferBlendMode);
        if (!mSelectionClipPath.isEmpty()) {
            currentColorizePainter.setClipPath(mSelectionClipPath);
        }
        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            currentColorizePainter.drawPixmap(tile->posF(), tile->pixmap());
        }
    }

    currentColorizePainter.end();

    painter.drawPixmap(mPointZero, mCurrentLayerPixmap);
}
