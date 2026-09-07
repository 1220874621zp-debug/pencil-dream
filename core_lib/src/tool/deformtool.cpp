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

#include "deformtool.h"

#include <QSettings>
#include <QPainter>
#include <QPainterPath>

#include "pointerevent.h"
#include "editor.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "viewmanager.h"
#include "bitmapimage.h"
#include "mlswarp.h"

namespace
{
    BitmapImage* bitmapImageFor(Editor* editor, Layer* layer)
    {
        Q_ASSERT(layer->type() == Layer::BITMAP);
        auto bitmapLayer = static_cast<LayerBitmap*>(layer);
        return static_cast<BitmapImage*>(bitmapLayer->getKeyFrameWhichCovers(editor->currentFrame()));
    }
}

DeformTool::DeformTool(QObject* parent) : TransformTool(parent)
{
}

void DeformTool::loadSettings()
{
    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    mPropertyUsed[TransformToolProperties::ANTI_ALIASING_ENABLED] = { Layer::BITMAP };

    QHash<int, PropertyInfo> info;
    info[TransformToolProperties::ANTI_ALIASING_ENABLED] = true;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);
}

QCursor DeformTool::cursor()
{
    if (mDeformActive && mDragIndex >= 0) { return QCursor(Qt::ClosedHandCursor); }
    return QCursor(Qt::ArrowCursor);
}

bool DeformTool::isActive() const
{
    return mDeformActive;
}

bool DeformTool::enteringThisTool()
{
    // a frame scrub or layer switch while a deform is in progress would
    // desync the preview from the layer: drop it losslessly
    mActiveConnections.append(connect(mEditor, &Editor::scrubbed, this, [this](int) {
        if (mDeformActive) { cancelDeform(); }
    }));
    mActiveConnections.append(connect(mEditor->layers(), &LayerManager::currentLayerChanged, this, [this](int) {
        if (mDeformActive) { cancelDeform(); }
    }));
    return true;
}

bool DeformTool::leavingThisTool()
{
    TransformTool::leavingThisTool();

    if (mDeformActive)
    {
        commitDeform();
    }
    return true;
}

void DeformTool::clearToolData()
{
    if (mDeformActive)
    {
        cancelDeform();
    }
}

void DeformTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect)

    if (!mDeformActive) { return; }

    painter.save();
    painter.setTransform(mEditor->view()->getView());
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int n = mGridSize;

    // grid lines through the moved control points
    QPen linePen(QColor(120, 170, 255, 180), 1.0);
    linePen.setCosmetic(true);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);

    for (int r = 0; r < n; ++r)
    {
        QPainterPath rowPath;
        rowPath.moveTo(mMovedPoints[r * n]);
        for (int c = 1; c < n; ++c)
        {
            rowPath.lineTo(mMovedPoints[r * n + c]);
        }
        painter.drawPath(rowPath);
    }
    for (int c = 0; c < n; ++c)
    {
        QPainterPath colPath;
        colPath.moveTo(mMovedPoints[c]);
        for (int r = 1; r < n; ++r)
        {
            colPath.lineTo(mMovedPoints[r * n + c]);
        }
        painter.drawPath(colPath);
    }

    // control points: small squares, moved ones highlighted
    const qreal viewScale = mEditor->view()->scaling();
    const qreal halfSize = qBound(2.5, 5.0 / viewScale, 10.0);
    for (int i = 0; i < mMovedPoints.size(); ++i)
    {
        const bool moved = mMovedPoints[i] != mOrigPoints[i];
        const bool dragging = i == mDragIndex;

        QRectF rect(mMovedPoints[i].x() - halfSize, mMovedPoints[i].y() - halfSize,
                    halfSize * 2, halfSize * 2);

        painter.setPen(dragging ? QPen(Qt::white, 1.5) : QPen(QColor(40, 40, 40), 1.0));
        painter.setBrush(moved ? QBrush(QColor(255, 170, 0)) : QBrush(QColor(120, 170, 255)));
        painter.drawRect(rect);
    }

    painter.restore();
}

void DeformTool::pointerPressEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) return;
    if (currentLayer->type() != Layer::BITMAP) { return; }
    if (event->button() != Qt::LeftButton) { return; }

    const QPointF pos = event->canvasPos();

    if (!mDeformActive)
    {
        beginDeform(pos);
        return;
    }

    const int hit = hitTestControlPoint(pos);
    if (hit >= 0)
    {
        mDragIndex = hit;
        mScribbleArea->updateToolCursor();
    }
    else
    {
        // click on empty space: bake the current warp and start over
        commitDeform();
        beginDeform(pos);
    }
}

void DeformTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) { return; }
    if (currentLayer->type() != Layer::BITMAP) { return; }

    if (mDragIndex >= 0 && mScribbleArea->isPointerInUse())
    {
        mMovedPoints[mDragIndex] = event->canvasPos();
        if (mMovedPoints[mDragIndex] != mOrigPoints[mDragIndex])
        {
            mAnyPointMoved = true;
        }
        updateWarpPreview();
    }
}

void DeformTool::pointerReleaseEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }

    if (mDragIndex >= 0)
    {
        mDragIndex = -1;
        mScribbleArea->updateToolCursor();
        mScribbleArea->updateFrame();
    }
}

void DeformTool::pointerDoubleClickEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }
    if (mDeformActive)
    {
        commitDeform();
    }
}

bool DeformTool::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (mDeformActive)
        {
            commitDeform();
            return true;
        }
        break;
    case Qt::Key_Escape:
        if (mDeformActive)
        {
            cancelDeform();
            return true;
        }
        break;
    case Qt::Key_Backspace:
        if (mDeformActive)
        {
            // reset all control points to their original positions
            mMovedPoints = mOrigPoints;
            mAnyPointMoved = false;
            updateWarpPreview();
            return true;
        }
        break;
    default:
        break;
    }

    return TransformTool::keyPressEvent(event);
}

void DeformTool::beginDeform(const QPointF& pos)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::BITMAP) { return; }

    mScribbleArea->handleDrawingOnEmptyFrame();

    BitmapImage* bitmapImage = bitmapImageFor(mEditor, layer);
    if (bitmapImage == nullptr) { return; }

    auto selectMan = mEditor->select();

    QPolygonF regionPolygon;
    bool isPolygon = false;
    if (selectMan->somethingSelected() && !selectMan->mySelectionRect().isEmpty())
    {
        if (selectMan->isPolygonSelection())
        {
            regionPolygon = selectMan->mySelectionPolygon();
            isPolygon = true;
        }
        else
        {
            regionPolygon = QPolygonF(QRectF(selectMan->mySelectionRect().toAlignedRect()));
        }
    }
    else
    {
        const QRect contentBounds = bitmapImage->bounds();
        if (contentBounds.isEmpty()) { return; }
        regionPolygon = QPolygonF(QRectF(contentBounds));

        // make the implicit region visible through the marquee
        selectMan->setSelection(QRectF(contentBounds), true);
    }

    Q_ASSERT(regionPolygon.size() >= 4);

    mRegion = regionPolygon.boundingRect().toAlignedRect();
    if (mRegion.width() < 4 || mRegion.height() < 4) { return; }

    mRegionPolygon = regionPolygon;
    mRegionIsPolygon = isPolygon;

    BitmapImage sourcePart = bitmapImage->copy(mRegionPolygon);
    if (sourcePart.width() <= 0 || sourcePart.height() <= 0) { return; }

    mSourceImage = *sourcePart.image();

    // build the control lattice over the region
    mOrigPoints.clear();
    const int n = mGridSize;
    for (int r = 0; r < n; ++r)
    {
        for (int c = 0; c < n; ++c)
        {
            const qreal x = mRegion.left() + qreal(c * mRegion.width()) / (n - 1);
            const qreal y = mRegion.top() + qreal(r * mRegion.height()) / (n - 1);
            mOrigPoints << QPointF(x, y);
        }
    }
    mMovedPoints = mOrigPoints;
    mAnyPointMoved = false;
    mDragIndex = -1;
    mDeformActive = true;
    emit isActiveChanged(DEFORM, true);

    Q_UNUSED(pos)

    // identity preview takes over rendering of the region
    mWarpedResult = mSourceImage;
    mWarpedTopLeft = QPointF(mRegion.topLeft());
    mScribbleArea->setDeformPreview(mSourceImage, QPointF(mRegion.topLeft()));
}

void DeformTool::teardown()
{
    mDeformActive = false;
    mDragIndex = -1;
    mAnyPointMoved = false;
    mSourceImage = QImage();
    mWarpedResult = QImage();
    mOrigPoints.clear();
    mMovedPoints.clear();
    emit isActiveChanged(DEFORM, false);

    mScribbleArea->clearDeformPreview();
}

void DeformTool::updateWarpPreview()
{
    if (!mDeformActive) { return; }

    // control points are local to the source image
    QVector<QPointF> origLocal;
    QVector<QPointF> movedLocal;
    origLocal.reserve(mOrigPoints.size());
    movedLocal.reserve(mMovedPoints.size());
    const QPointF topLeft(mRegion.topLeft());
    for (int i = 0; i < mOrigPoints.size(); ++i)
    {
        origLocal << mOrigPoints[i] - topLeft;
        movedLocal << mMovedPoints[i] - topLeft;
    }

    QPointF offset;
    const bool useAA = toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();
    mWarpedResult = MlsWarp::warpImage(mSourceImage, origLocal, movedLocal, 1.0, useAA, &offset);
    mWarpedTopLeft = topLeft + offset;

    mScribbleArea->setDeformPreview(mWarpedResult, mWarpedTopLeft);
}

void DeformTool::commitDeform()
{
    if (!mDeformActive) { return; }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::BITMAP)
    {
        teardown();
        return;
    }

    if (mAnyPointMoved && !mWarpedResult.isNull())
    {
        BitmapImage* bitmapImage = bitmapImageFor(mEditor, layer);
        if (bitmapImage != nullptr)
        {
            SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);

            bitmapImage->clear(mRegionPolygon);

            BitmapImage result(mWarpedTopLeft.toPoint(), mWarpedResult);
            bitmapImage->paste(&result, QPainter::CompositionMode_SourceOver);

            mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
            mEditor->undoRedo()->record(saveStateId, typeName());
        }
    }

    teardown();
}

void DeformTool::cancelDeform()
{
    if (!mDeformActive) { return; }

    // the layer was never modified, dropping the preview restores everything
    teardown();
    mScribbleArea->updateFrame();
}

int DeformTool::hitTestControlPoint(const QPointF& pos) const
{
    const qreal tolerance = 8.0 / qMax<qreal>(mEditor->view()->scaling(), 0.01);

    int best = -1;
    qreal bestDist = tolerance;
    for (int i = 0; i < mMovedPoints.size(); ++i)
    {
        const qreal dist = QLineF(mMovedPoints[i], pos).length();
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}
