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

#include <cmath>

namespace
{
    BitmapImage* bitmapImageFor(Editor* editor, Layer* layer)
    {
        Q_ASSERT(layer->type() == Layer::BITMAP);
        auto bitmapLayer = static_cast<LayerBitmap*>(layer);
        return static_cast<BitmapImage*>(bitmapLayer->getKeyFrameWhichCovers(editor->currentFrame()));
    }

    QPointF rightUnitNormal(const QPointF& v)
    {
        const qreal len = std::hypot(v.x(), v.y());
        if (len < 1e-9) { return QPointF(0, 0); }
        return QPointF(v.y() / len, -v.x() / len);
    }

    const qreal HandleTolerance = 10.0;
}

DeformTool::DeformTool(QObject* parent) : TransformTool(parent)
{
}

void DeformTool::loadSettings()
{
    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    mPropertyUsed[TransformToolProperties::ANTI_ALIASING_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::GRID_SIZE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::DEFORM_MODE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::LIQUIFY_OP_VALUE] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::LIQUIFY_SIZE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::LIQUIFY_AMOUNT_VALUE] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::LIQUIFY_REVERSE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::WARP_ALPHA_VALUE] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::WARP_TYPE_VALUE] = { Layer::BITMAP };

    QHash<int, PropertyInfo> info;
    info[TransformToolProperties::ANTI_ALIASING_ENABLED] = true;
    info[TransformToolProperties::GRID_SIZE_VALUE] = { 2, 10, 4 };
    info[TransformToolProperties::DEFORM_MODE_VALUE] = { 0, 3, 0 };
    info[TransformToolProperties::LIQUIFY_OP_VALUE] = { 0, 4, 0 };
    info[TransformToolProperties::LIQUIFY_SIZE_VALUE] = { 5, 1000, 60 };
    info[TransformToolProperties::LIQUIFY_AMOUNT_VALUE] = { 0.01, 1.0, 0.1 };
    info[TransformToolProperties::LIQUIFY_REVERSE_ENABLED] = false;
    info[TransformToolProperties::WARP_ALPHA_VALUE] = { 0.1, 5.0, 1.0 };
    info[TransformToolProperties::WARP_TYPE_VALUE] = { 0, 2, 2 };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    mGridSize = toolProperties().getInfo(TransformToolProperties::GRID_SIZE_VALUE).intValue();
    mDeformMode = toolProperties().getInfo(TransformToolProperties::DEFORM_MODE_VALUE).intValue();
    mLiquifyOp = toolProperties().getInfo(TransformToolProperties::LIQUIFY_OP_VALUE).intValue();
    mLiquifySize = toolProperties().getInfo(TransformToolProperties::LIQUIFY_SIZE_VALUE).intValue();
    mLiquifyAmount = toolProperties().getInfo(TransformToolProperties::LIQUIFY_AMOUNT_VALUE).realValue();
    mLiquifyReverse = toolProperties().getInfo(TransformToolProperties::LIQUIFY_REVERSE_ENABLED).boolValue();
    mWarpAlpha = toolProperties().getInfo(TransformToolProperties::WARP_ALPHA_VALUE).realValue();
    mWarpType = toolProperties().getInfo(TransformToolProperties::WARP_TYPE_VALUE).intValue();
}

QCursor DeformTool::cursor()
{
    if (mDeformMode == 0)
    {
        // liquify: the brush circle drawn in paint() is the cursor
        return QCursor(Qt::BlankCursor);
    }
    if ((mDeformMode == 1 && mDragIndex >= 0) ||
        (mDeformMode == 2 && mCageDragVertex >= 0) ||
        (mDeformMode == 3 && mPerspDragCorner >= 0))
    {
        return QCursor(Qt::ClosedHandCursor);
    }
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
        // re-arm the session on the new frame's content, like Krita does
        if (!mDeformActive) { beginSession(); }
    }));
    mActiveConnections.append(connect(mEditor->layers(), &LayerManager::currentLayerChanged, this, [this](int) {
        if (mDeformActive) { cancelDeform(); }
        if (!mDeformActive) { beginSession(); }
    }));

    // show the controls right away: entering the deform tool is enough
    // (Krita's transform tool behaves the same way)
    if (!mDeformActive) { beginSession(); }
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

void DeformTool::setGridSize(int size)
{
    size = qBound(2, size, 10);
    if (size == mGridSize) { return; }

    mGridSize = size;
    toolProperties().setBaseValue(TransformToolProperties::GRID_SIZE_VALUE, size);
    emit gridSizeChanged(size);

    if (mDeformActive && mDeformMode == 1)
    {
        rebuildLattice();
        mAnyPointMoved = false;
        updateWarpPreview(true);
    }
    mScribbleArea->updateFrame();
}

void DeformTool::setDeformMode(int mode)
{
    mode = qBound(0, mode, 3);
    if (mode == mDeformMode) { return; }

    // switching modes drops the running session losslessly
    if (mDeformActive) { cancelDeform(); }

    mDeformMode = mode;
    toolProperties().setBaseValue(TransformToolProperties::DEFORM_MODE_VALUE, mode);
    emit deformModeChanged(mode);

    mScribbleArea->updateToolCursor();
    mScribbleArea->updateFrame();
}

void DeformTool::setLiquifyOp(int op)
{
    op = qBound(0, op, 4);
    if (op == mLiquifyOp) { return; }

    mLiquifyOp = op;
    toolProperties().setBaseValue(TransformToolProperties::LIQUIFY_OP_VALUE, op);
    emit liquifyOpChanged(op);
}

void DeformTool::setLiquifySize(int size)
{
    size = qBound(5, size, 1000);
    if (size == mLiquifySize) { return; }

    mLiquifySize = size;
    toolProperties().setBaseValue(TransformToolProperties::LIQUIFY_SIZE_VALUE, size);
    emit liquifySizeChanged(size);
    mScribbleArea->updateFrame();
}

void DeformTool::setLiquifyAmount(qreal amount)
{
    amount = qBound(0.01, amount, 1.0);
    if (qFuzzyCompare(amount, mLiquifyAmount) && amount != 0.0) { return; }

    mLiquifyAmount = amount;
    toolProperties().setBaseValue(TransformToolProperties::LIQUIFY_AMOUNT_VALUE, amount);
    emit liquifyAmountChanged(amount);
}

void DeformTool::setLiquifyReverse(bool reverse)
{
    if (reverse == mLiquifyReverse) { return; }

    mLiquifyReverse = reverse;
    toolProperties().setBaseValue(TransformToolProperties::LIQUIFY_REVERSE_ENABLED, reverse);
    emit liquifyReverseChanged(reverse);
}

void DeformTool::setWarpAlpha(qreal alpha)
{
    alpha = qBound(0.1, alpha, 5.0);
    if (qFuzzyCompare(alpha, mWarpAlpha)) { return; }

    mWarpAlpha = alpha;
    toolProperties().setBaseValue(TransformToolProperties::WARP_ALPHA_VALUE, alpha);
    emit warpAlphaChanged(alpha);

    if (mDeformActive && mDeformMode == 1 && mAnyPointMoved)
    {
        updateWarpPreview(true);
    }
}

void DeformTool::setWarpType(int type)
{
    type = qBound(0, type, 2);
    if (type == mWarpType) { return; }

    mWarpType = type;
    toolProperties().setBaseValue(TransformToolProperties::WARP_TYPE_VALUE, type);
    emit warpTypeChanged(type);

    if (mDeformActive && mDeformMode == 1 && mAnyPointMoved)
    {
        updateWarpPreview(true);
    }
}

void DeformTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect)

    painter.save();
    painter.setTransform(mEditor->view()->getView());
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal viewScale = mEditor->view()->scaling();
    const qreal halfSize = qBound(2.5, HandleTolerance / 2 / viewScale, 8.0);

    if (mDeformMode == 0)
    {
        // liquify brush cursor: solid ring at sigma, faint ring at 3*sigma
        const qreal sigma = mLiquifySize;
        painter.setBrush(Qt::NoBrush);

        QPen outer(QColor(255, 255, 255, 90), 1.0);
        outer.setCosmetic(true);
        painter.setPen(outer);
        painter.drawEllipse(mCursorPos, 3.0 * sigma, 3.0 * sigma);

        QPen ring(QColor(255, 255, 255, 200), 1.0);
        ring.setCosmetic(true);
        painter.setPen(ring);
        painter.drawEllipse(mCursorPos, sigma, sigma);
    }
    else if (mDeformActive && mDeformMode == 1)
    {
        // warp grid through the moved control points
        QPen linePen(QColor(120, 170, 255, 180), 1.0);
        linePen.setCosmetic(true);
        painter.setPen(linePen);
        painter.setBrush(Qt::NoBrush);

        const int n = mGridSize;
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
    }
    else if (mDeformMode == 2)
    {
        if (!mCageSet && mCageDrawPoints.size() >= 2)
        {
            // drawing the cage outline
            QPen underlay(Qt::black, 2.0, Qt::SolidLine);
            underlay.setCosmetic(true);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(underlay);
            painter.drawPolyline(mCageDrawPoints);

            QPen dashes(Qt::white, 1.0, Qt::DashLine);
            dashes.setCosmetic(true);
            painter.setPen(dashes);
            painter.drawPolyline(mCageDrawPoints);
        }
        else if (mCageSet)
        {
            // original cage dashed, moved cage solid + vertices
            QPen origPen(QColor(255, 255, 255, 110), 1.0, Qt::DashLine);
            origPen.setCosmetic(true);
            painter.setPen(origPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPolygon(QPolygonF(mCageOrigVertices));

            QPen movedPen(QColor(120, 170, 255, 220), 1.5);
            movedPen.setCosmetic(true);
            painter.setPen(movedPen);
            painter.drawPolygon(QPolygonF(mCageMovedVertices));

            for (int i = 0; i < mCageMovedVertices.size(); ++i)
            {
                const bool moved = mCageMovedVertices[i] != mCageOrigVertices[i];
                QRectF rect(mCageMovedVertices[i].x() - halfSize, mCageMovedVertices[i].y() - halfSize,
                            halfSize * 2, halfSize * 2);
                painter.setPen(i == mCageDragVertex ? QPen(Qt::white, 1.5) : QPen(QColor(40, 40, 40), 1.0));
                painter.setBrush(moved ? QBrush(QColor(255, 170, 0)) : QBrush(QColor(120, 170, 255)));
                painter.drawRect(rect);
            }
        }
    }
    else if (mDeformActive && mDeformMode == 3)
    {
        // perspective: original quad dashed, moved quad + corner handles
        QPen origPen(QColor(255, 255, 255, 110), 1.0, Qt::DashLine);
        origPen.setCosmetic(true);
        painter.setPen(origPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(mPerspOrig);

        QPen movedPen(QColor(120, 170, 255, 220), 1.5);
        movedPen.setCosmetic(true);
        painter.setPen(movedPen);
        painter.drawPolygon(mPerspMoved);

        for (int i = 0; i < mPerspMoved.size(); ++i)
        {
            const bool moved = mPerspMoved[i] != mPerspOrig[i];
            QRectF rect(mPerspMoved[i].x() - halfSize, mPerspMoved[i].y() - halfSize,
                        halfSize * 2, halfSize * 2);
            painter.setPen(i == mPerspDragCorner ? QPen(Qt::white, 1.5) : QPen(QColor(40, 40, 40), 1.0));
            painter.setBrush(moved ? QBrush(QColor(255, 170, 0)) : QBrush(QColor(120, 170, 255)));
            painter.drawRect(rect);
        }
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
    mCursorPos = pos;

    if (!mDeformActive)
    {
        beginSession();
        if (!mDeformActive) { return; }
    }

    switch (mDeformMode)
    {
    case 0: // liquify
        mLiquifyStrokeActive = true;
        mLiquifyLastPos = pos;
        break;
    case 1: // warp
    {
        const int hit = hitTestControlPoint(pos);
        if (hit >= 0)
        {
            mDragIndex = hit;
            mScribbleArea->updateToolCursor();
        }
        // clicks that miss a control point keep the session running; use
        // Enter / double-click to apply (Krita semantics)
        break;
    }
    case 2: // cage
        if (!mCageSet)
        {
            mCageDrawPoints.clear();
            mCageDrawPoints << pos;
        }
        else
        {
            const int hit = hitTestCageVertex(pos);
            if (hit >= 0)
            {
                mCageDragVertex = hit;
                mScribbleArea->updateToolCursor();
            }
        }
        break;
    case 3: // perspective
    {
        const int hit = hitTestPerspectiveCorner(pos);
        if (hit >= 0)
        {
            mPerspDragCorner = hit;
            mScribbleArea->updateToolCursor();
        }
        break;
    }
    default:
        break;
    }
}

void DeformTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) { return; }
    if (currentLayer->type() != Layer::BITMAP) { return; }

    const QPointF pos = event->canvasPos();
    mCursorPos = pos;

    if (!mScribbleArea->isPointerInUse())
    {
        if (mDeformMode == 0 || mDeformMode == 2)
        {
            mScribbleArea->updateFrame(); // keep brush cursor / cage visuals fresh
        }
        return;
    }

    switch (mDeformMode)
    {
    case 0:
        if (mLiquifyStrokeActive)
        {
            liquifyStrokeTo(pos);
        }
        break;
    case 1:
        if (mDragIndex >= 0)
        {
            mMovedPoints[mDragIndex] = pos;
            if (mMovedPoints[mDragIndex] != mOrigPoints[mDragIndex])
            {
                mAnyPointMoved = true;
            }
            updateWarpPreview(true);
        }
        break;
    case 2:
        if (!mCageSet)
        {
            if (mCageDrawPoints.size() > 0 && QLineF(mCageDrawPoints.last(), pos).length() >= 3.0)
            {
                mCageDrawPoints << pos;
                mScribbleArea->updateFrame();
            }
        }
        else if (mCageDragVertex >= 0)
        {
            mCageMovedVertices[mCageDragVertex] = pos;
            mAnyPointMoved = true;
            updateCagePreview();
        }
        break;
    case 3:
        if (mPerspDragCorner >= 0)
        {
            mPerspMoved[mPerspDragCorner] = pos;
            mAnyPointMoved = true;
            updatePerspectivePreview();
        }
        break;
    default:
        break;
    }
}

void DeformTool::pointerReleaseEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }

    switch (mDeformMode)
    {
    case 0:
        if (mLiquifyStrokeActive)
        {
            mLiquifyStrokeActive = false;
            if (mAnyPointMoved)
            {
                updateLiquifyPreview(false);
            }
        }
        break;
    case 1:
        if (mDragIndex >= 0)
        {
            mDragIndex = -1;
            if (mAnyPointMoved)
            {
                // the drag preview may be down-sampled: restore full resolution
                updateWarpPreview(false);
            }
            mScribbleArea->updateToolCursor();
        }
        break;
    case 2:
        if (!mCageSet)
        {
            finalizeCage(event->canvasPos());
        }
        else if (mCageDragVertex >= 0)
        {
            mCageDragVertex = -1;
            mScribbleArea->updateToolCursor();
        }
        break;
    case 3:
        if (mPerspDragCorner >= 0)
        {
            mPerspDragCorner = -1;
            mScribbleArea->updateToolCursor();
        }
        break;
    default:
        break;
    }

    mScribbleArea->updateFrame();
}

void DeformTool::pointerDoubleClickEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }
    if (mDeformMode == 2 && mCageSet == false && mCageDrawPoints.size() >= 3)
    {
        finalizeCage(event->canvasPos());
        return;
    }
    if (mDeformActive && mAnyPointMoved)
    {
        commitDeform();
        beginSession();
    }
}

bool DeformTool::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (mDeformActive && mAnyPointMoved)
        {
            commitDeform();
            beginSession();
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
            if (mDeformMode == 1)
            {
                mMovedPoints = mOrigPoints;
                mAnyPointMoved = false;
                updateWarpPreview(true);
            }
            else if (mDeformMode == 2)
            {
                mCageMovedVertices = mCageOrigVertices;
                mAnyPointMoved = false;
                updateCagePreview();
            }
            else if (mDeformMode == 3)
            {
                mPerspMoved = mPerspOrig;
                mAnyPointMoved = false;
                updatePerspectivePreview();
            }
            return true;
        }
        break;
    default:
        break;
    }

    return TransformTool::keyPressEvent(event);
}

void DeformTool::beginSession()
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
        QRect contentBounds = bitmapImage->bounds();
        if (contentBounds.isEmpty()) { return; }

        // margin so pushed pixels have somewhere to go
        const int margin = qMax(64, mDeformMode == 0 ? mLiquifySize * 3 : 32);
        contentBounds.adjust(-margin, -margin, margin, margin);
        regionPolygon = QPolygonF(QRectF(contentBounds));

        // make the implicit region visible through the marquee
        selectMan->setSelection(QRectF(bitmapImage->bounds()), true);
    }

    Q_ASSERT(regionPolygon.size() >= 4);

    mRegion = regionPolygon.boundingRect().toAlignedRect();
    if (mRegion.width() < 4 || mRegion.height() < 4) { return; }

    mRegionPolygon = regionPolygon;
    mRegionIsPolygon = isPolygon;

    BitmapImage sourcePart = bitmapImage->copy(mRegionPolygon);
    if (sourcePart.width() <= 0 || sourcePart.height() <= 0) { return; }

    mSourceImage = *sourcePart.image();

    // large regions get a down-scaled copy for interactive warp dragging
    const qreal area = qreal(mSourceImage.width()) * mSourceImage.height();
    const qreal kInteractiveBudget = 220000.0;
    mPreviewScale = (area > kInteractiveBudget) ? qSqrt(kInteractiveBudget / area) : 1.0;
    if (mPreviewScale < 1.0)
    {
        mPreviewSource = mSourceImage.scaled(
            qMax(1, qRound(mSourceImage.width() * mPreviewScale)),
            qMax(1, qRound(mSourceImage.height() * mPreviewScale)),
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
        mPreviewSource = QImage();
    }

    // per-mode state
    if (mDeformMode == 0)
    {
        // control-point grid over the region (local coords, row-major)
        mLiquifyGridCols = qMax(1, (mSourceImage.width() + mLiquifyGridCell - 1) / mLiquifyGridCell);
        mLiquifyGridRows = qMax(1, (mSourceImage.height() + mLiquifyGridCell - 1) / mLiquifyGridCell);
        mLiquifyOrigGrid.clear();
        for (int r = 0; r <= mLiquifyGridRows; r++)
        {
            for (int c = 0; c <= mLiquifyGridCols; c++)
            {
                mLiquifyOrigGrid << QPointF(qreal(c * mSourceImage.width()) / mLiquifyGridCols,
                                            qreal(r * mSourceImage.height()) / mLiquifyGridRows);
            }
        }
        mLiquifyMovedGrid = mLiquifyOrigGrid;
    }
    if (mDeformMode == 1)
    {
        rebuildLattice();
    }
    else if (mDeformMode == 2)
    {
        mCageSet = false;
        mCageDrawPoints.clear();
        mCageOrigVertices.clear();
        mCageMovedVertices.clear();
        mCageDragVertex = -1;
    }
    else if (mDeformMode == 3)
    {
        mPerspOrig = QPolygonF(QRectF(mRegion));
        mPerspMoved = mPerspOrig;
        mPerspDragCorner = -1;
    }

    mLiquifyStrokeActive = false;
    mAnyPointMoved = false;
    mDragIndex = -1;
    mDeformActive = true;
    emit isActiveChanged(DEFORM, true);

    mWarpedResult = mSourceImage;
    mWarpedTopLeft = QPointF(mRegion.topLeft());
    mScribbleArea->setDeformPreview(mSourceImage, QRectF(QPointF(mRegion.topLeft()), QSizeF(mSourceImage.size())));
}

void DeformTool::rebuildLattice()
{
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
}

void DeformTool::teardown()
{
    mDeformActive = false;
    mDragIndex = -1;
    mCageDragVertex = -1;
    mPerspDragCorner = -1;
    mLiquifyStrokeActive = false;
    mAnyPointMoved = false;
    mSourceImage = QImage();
    mPreviewSource = QImage();
    mLiquifyOrigGrid.clear();
    mLiquifyMovedGrid.clear();
    mLiquifyGridCols = 0;
    mLiquifyGridRows = 0;
    mPreviewScale = 1.0;
    mWarpedResult = QImage();
    mOrigPoints.clear();
    mMovedPoints.clear();
    mCageSet = false;
    mCageDrawPoints.clear();
    mCageOrigVertices.clear();
    mCageMovedVertices.clear();
    emit isActiveChanged(DEFORM, false);

    mScribbleArea->clearDeformPreview();
}

void DeformTool::liquifyStrokeTo(const QPointF& pos)
{
    if (!mDeformActive || mLiquifyMovedGrid.isEmpty()) { return; }

    // Krita spacing: dabs every 0.2 * size along the drag path.
    // Each dab moves control POINTS within 3*sigma (gaussian falloff),
    // exactly like KisLiquifyTransformWorker - pixels are only touched by
    // the per-frame render, which keeps strokes cheap.
    const qreal sigma = mLiquifySize;
    const qreal stepLength = qMax(2.0, 0.2 * sigma);

    const QPointF from = mLiquifyLastPos;
    const QPointF delta = pos - from;
    const qreal length = QLineF(from, pos).length();
    if (length < 0.5 && mLiquifyOp != 4) { return; }

    const QPointF topLeft(mRegion.topLeft());
    const qreal reverse = mLiquifyReverse ? -1.0 : 1.0;

    const int steps = qMax(1, qMin(qCeil(length / stepLength), 64));
    const qreal maxDist = 3.0 * sigma;
    const qreal inv2SigmaSq = 1.0 / (2.0 * sigma * sigma);
    const qreal angle = 2.0 * M_PI * mLiquifyAmount;

    for (int i = 1; i <= steps; i++)
    {
        const qreal t = qreal(i) / steps;
        const QPointF atCanvas = from + t * delta;
        const QPointF base = atCanvas - topLeft;
        const QPointF dirVec = (length > 1e-6) ? (delta / length) : QPointF(1, 0);

        QPointF dabVector;
        qreal amount = mLiquifyAmount;
        switch (mLiquifyOp)
        {
        case 0: // move: displacement = drawing direction * size * amount
            dabVector = dirVec * (sigma * mLiquifyAmount * reverse);
            break;
        case 3: // offset: push perpendicular to the drawing direction
            dabVector = rightUnitNormal(dirVec) * (sigma * mLiquifyAmount * reverse);
            break;
        case 1: // scale
        case 2: // rotate
            amount = qAbs(mLiquifyAmount); // negative scale/rotate folds the image
            break;
        case 4: // undo
        default:
            amount = mLiquifyAmount;
            break;
        }

        // box of grid points that can be affected by this dab
        const int c0 = qMax(0, int((base.x() - maxDist) / mLiquifyGridCell) - 1);
        const int c1 = qMin(mLiquifyGridCols, int((base.x() + maxDist) / mLiquifyGridCell) + 1);
        const int r0 = qMax(0, int((base.y() - maxDist) / mLiquifyGridCell) - 1);
        const int r1 = qMin(mLiquifyGridRows, int((base.y() + maxDist) / mLiquifyGridCell) + 1);

        for (int r = r0; r <= r1; r++)
        {
            for (int c = c0; c <= c1; c++)
            {
                const int idx = r * (mLiquifyGridCols + 1) + c;
                const QPointF diff = mLiquifyMovedGrid[idx] - base;
                const qreal distSq = diff.x() * diff.x() + diff.y() * diff.y();
                if (distSq > maxDist * maxDist) { continue; }

                const qreal lambda = std::exp(-distSq * inv2SigmaSq);
                switch (mLiquifyOp)
                {
                case 0: // move
                case 3: // offset
                    mLiquifyMovedGrid[idx] += lambda * dabVector;
                    break;
                case 1: // scale
                    mLiquifyMovedGrid[idx] = base + (1.0 + amount * lambda) * diff;
                    break;
                case 2: // rotate
                {
                    const qreal a = angle * lambda;
                    const qreal cs = std::cos(a), sn = std::sin(a);
                    mLiquifyMovedGrid[idx] = base + QPointF(cs * diff.x() - sn * diff.y(),
                                                            sn * diff.x() + cs * diff.y());
                    break;
                }
                case 4: // undo: pull the point back toward its origin
                {
                    const QPointF origDiff = mLiquifyOrigGrid[idx] - mLiquifyMovedGrid[idx];
                    mLiquifyMovedGrid[idx] += qBound<qreal>(0.0, amount * lambda, 1.0) * origDiff;
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    mAnyPointMoved = true;
    mLiquifyLastPos = pos;

    updateLiquifyPreview(true);
}

void DeformTool::updateLiquifyPreview(bool interactive)
{
    if (!mDeformActive || mDeformMode != 0 || mLiquifyMovedGrid.isEmpty()) { return; }

    // interactive frames render a down-scaled copy, release/commit full res
    const bool useScaled = interactive && mPreviewScale < 1.0 && !mPreviewSource.isNull();
    const QImage& src = useScaled ? mPreviewSource : mSourceImage;
    const qreal scale = useScaled ? mPreviewScale : 1.0;

    const int cols = qMax(1, qRound(mLiquifyGridCols * scale));
    const int rows = qMax(1, qRound(mLiquifyGridRows * scale));
    const int stride = mLiquifyGridCols + 1;

    QVector<QPointF> mapped;
    mapped.reserve((cols + 1) * (rows + 1));
    for (int r = 0; r <= rows; r++)
    {
        const int srcR = qMin(mLiquifyGridRows, qRound(qreal(r * mLiquifyGridRows) / rows));
        for (int c = 0; c <= cols; c++)
        {
            const int srcC = qMin(mLiquifyGridCols, qRound(qreal(c * mLiquifyGridCols) / cols));
            mapped << mLiquifyMovedGrid[srcR * stride + srcC] * scale;
        }
    }

    const bool useAA = toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();
    QPointF offset;
    const QImage warped = MlsWarp::gridWarpImage(src, mapped,
                                                 qMax(4, qRound(mLiquifyGridCell * scale)),
                                                 useAA, &offset);

    const QPointF topLeft(mRegion.topLeft());
    if (useScaled)
    {
        mScribbleArea->setDeformPreview(warped,
            QRectF(topLeft + offset / scale,
                   QSizeF(warped.width() / scale, warped.height() / scale)));
    }
    else
    {
        mWarpedResult = warped;
        mWarpedTopLeft = topLeft + offset;
        mScribbleArea->setDeformPreview(mWarpedResult, QRectF(mWarpedTopLeft, QSizeF(mWarpedResult.size())));
    }
}

void DeformTool::updateWarpPreview(bool interactive)
{
    if (!mDeformActive || mDeformMode != 1) { return; }

    // while dragging, large regions are warped at reduced resolution and
    // stretched back for display; pointer release / commit recompute full res
    const bool useScaled = interactive && mPreviewScale < 1.0 && !mPreviewSource.isNull();
    const QImage& src = useScaled ? mPreviewSource : mSourceImage;
    const qreal scale = useScaled ? mPreviewScale : 1.0;

    QVector<QPointF> origLocal;
    QVector<QPointF> movedLocal;
    origLocal.reserve(mOrigPoints.size());
    movedLocal.reserve(mMovedPoints.size());
    const QPointF topLeft(mRegion.topLeft());
    for (int i = 0; i < mOrigPoints.size(); ++i)
    {
        origLocal << (mOrigPoints[i] - topLeft) * scale;
        movedLocal << (mMovedPoints[i] - topLeft) * scale;
    }

    QPointF offset;
    const bool useAA = toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();
    const QImage warped = MlsWarp::warpImage(src, origLocal, movedLocal, mWarpAlpha, useAA, &offset,
                                             static_cast<MlsWarp::WarpType>(mWarpType));

    if (useScaled)
    {
        mScribbleArea->setDeformPreview(warped,
            QRectF(topLeft + offset / scale,
                   QSizeF(warped.width() / scale, warped.height() / scale)));
    }
    else
    {
        mWarpedResult = warped;
        mWarpedTopLeft = topLeft + offset;
        mScribbleArea->setDeformPreview(mWarpedResult, QRectF(mWarpedTopLeft, QSizeF(mWarpedResult.size())));
    }
}

void DeformTool::finalizeCage(const QPointF& pos)
{
    if (mCageDrawPoints.size() > 0 && QLineF(mCageDrawPoints.last(), pos).length() >= 3.0)
    {
        mCageDrawPoints << pos;
    }

    if (mCageDrawPoints.size() < 3) { return; }

    // simplify very dense outlines (green coordinates are O(vertices) per point)
    QPolygonF pts = mCageDrawPoints;
    const int kMaxVertices = 64;
    while (pts.size() > kMaxVertices)
    {
        QPolygonF reduced;
        for (int i = 0; i < pts.size(); i += 2)
        {
            reduced << pts[i];
        }
        pts = reduced;
    }

    mCageOrigVertices = QVector<QPointF>(pts.begin(), pts.end());
    mCageMovedVertices = mCageOrigVertices;
    mCageSet = true;
    mCageDrawPoints.clear();
    mAnyPointMoved = false;

    mScribbleArea->updateFrame();
}

void DeformTool::updateCagePreview()
{
    if (!mDeformActive || mDeformMode != 2 || !mCageSet) { return; }

    const QPointF topLeft(mRegion.topLeft());
    QVector<QPointF> origLocal;
    QVector<QPointF> movedLocal;
    origLocal.reserve(mCageOrigVertices.size());
    movedLocal.reserve(mCageMovedVertices.size());
    for (int i = 0; i < mCageOrigVertices.size(); ++i)
    {
        origLocal << mCageOrigVertices[i] - topLeft;
        movedLocal << mCageMovedVertices[i] - topLeft;
    }

    const bool useAA = toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();
    QPointF offset;
    mWarpedResult = MlsWarp::cageWarpImage(mSourceImage, origLocal, movedLocal, useAA, &offset);
    mWarpedTopLeft = topLeft + offset;

    mScribbleArea->setDeformPreview(mWarpedResult, QRectF(mWarpedTopLeft, QSizeF(mWarpedResult.size())));
}

void DeformTool::updatePerspectivePreview()
{
    if (!mDeformActive || mDeformMode != 3) { return; }

    const QPointF topLeft(mRegion.topLeft());
    QPolygonF dstLocal;
    for (const QPointF& pt : mPerspMoved)
    {
        dstLocal << pt - topLeft;
    }

    const bool useAA = toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();
    QPointF offset;
    mWarpedResult = MlsWarp::perspectiveWarpImage(mSourceImage, dstLocal, useAA, &offset);
    mWarpedTopLeft = topLeft + offset;

    mScribbleArea->setDeformPreview(mWarpedResult, QRectF(mWarpedTopLeft, QSizeF(mWarpedResult.size())));
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

    if (mAnyPointMoved)
    {
        BitmapImage* bitmapImage = bitmapImageFor(mEditor, layer);
        if (bitmapImage != nullptr)
        {
            if (mDeformMode == 0)
            {
                // make sure the committed result is the full-resolution warp
                updateLiquifyPreview(false);
            }

            QImage result = mWarpedResult;
            QPointF resultTopLeft = mWarpedTopLeft;

            if (!result.isNull())
            {
                SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);

                bitmapImage->clear(mRegionPolygon);

                BitmapImage pastedImage(resultTopLeft.toPoint(), result);
                bitmapImage->paste(&pastedImage, QPainter::CompositionMode_SourceOver);

                mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
                mEditor->undoRedo()->record(saveStateId, typeName());
            }
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
    const qreal tolerance = HandleTolerance / qMax<qreal>(mEditor->view()->scaling(), 0.01);

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

int DeformTool::hitTestCageVertex(const QPointF& pos) const
{
    const qreal tolerance = HandleTolerance / qMax<qreal>(mEditor->view()->scaling(), 0.01);

    int best = -1;
    qreal bestDist = tolerance;
    for (int i = 0; i < mCageMovedVertices.size(); ++i)
    {
        const qreal dist = QLineF(mCageMovedVertices[i], pos).length();
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

int DeformTool::hitTestPerspectiveCorner(const QPointF& pos) const
{
    const qreal tolerance = HandleTolerance / qMax<qreal>(mEditor->view()->scaling(), 0.01);

    int best = -1;
    qreal bestDist = tolerance;
    for (int i = 0; i < mPerspMoved.size(); ++i)
    {
        const qreal dist = QLineF(mPerspMoved[i], pos).length();
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}
