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

    qreal crossProduct(const QPointF& a, const QPointF& b)
    {
        return a.x() * b.y() - a.y() * b.x();
    }

    // QPolygonF(QRectF) is a CLOSED polygon with 5 points, but the quad
    // consumers here (perspectiveWarpImage, drag math) want exactly 4
    // corners TL/TR/BR/BL
    QPolygonF quadFromRect(const QRectF& r)
    {
        QPolygonF quad;
        quad << r.topLeft() << QPointF(r.right(), r.top())
             << QPointF(r.right(), r.bottom()) << QPointF(r.left(), r.bottom());
        return quad;
    }

    // scale-handle local coordinates on the frame: (u,v) in [0,1]^2.
    // Indices: 0 TL, 1 TR, 2 BR, 3 BL, 4 top, 5 right, 6 bottom, 7 left.
    void freeHandleLocal(int handle, qreal& u, qreal& v)
    {
        static const qreal kLocal[8][2] = {
            { 0.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 1.0 }, { 0.0, 1.0 },
            { 0.5, 0.0 }, { 1.0, 0.5 }, { 0.5, 1.0 }, { 0.0, 0.5 }
        };
        u = kLocal[handle][0];
        v = kLocal[handle][1];
    }

    QPointF freeHandlePos(int handle, const QPolygonF& corners)
    {
        qreal u, v;
        freeHandleLocal(handle, u, v);
        return corners[0] + u * (corners[1] - corners[0]) + v * (corners[3] - corners[0]);
    }

    // rotation handle sits outside its corner along the diagonal from the
    // frame center, at a roughly screen-constant distance
    QPointF freeRotateHandlePos(int corner, const QPolygonF& corners, qreal viewScale)
    {
        const QPointF center = (corners[0] + corners[1] + corners[2] + corners[3]) / 4.0;
        QPointF dir = corners[corner] - center;
        const qreal len = std::hypot(dir.x(), dir.y());
        if (len < 1e-9) { return corners[corner]; }
        const qreal offset = 22.0 / qMax(viewScale, 0.01);
        return corners[corner] + dir / len * offset;
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
    info[TransformToolProperties::DEFORM_MODE_VALUE] = { 0, 4, 0 };
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
    if (mDeformMode == 1)
    {
        // liquify: the brush circle drawn in paint() is the cursor
        return QCursor(Qt::BlankCursor);
    }
    if ((mDeformMode == 0 && mFreeDragKind >= 0) ||
        (mDeformMode == 2 && mDragIndex >= 0) ||
        (mDeformMode == 3 && mCageDragVertex >= 0) ||
        (mDeformMode == 4 && mPerspDragCorner >= 0))
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

    if (mDeformActive && mDeformMode == 2)
    {
        rebuildLattice();
        mAnyPointMoved = false;
        updateWarpPreview(true);
    }
    mScribbleArea->updateFrame();
}

void DeformTool::setDeformMode(int mode)
{
    mode = qBound(0, mode, 4);
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

    if (mDeformActive && mDeformMode == 2 && mAnyPointMoved)
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

    if (mDeformActive && mDeformMode == 2 && mAnyPointMoved)
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

    if (mDeformMode == 1)
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
    else if (mDeformActive && mDeformMode == 0)
    {
        // free transform: original frame dashed, moved frame solid with
        // scale handles on corners/edges and rotation rings outside
        QPen origPen(QColor(255, 255, 255, 110), 1.0, Qt::DashLine);
        origPen.setCosmetic(true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(origPen);
        painter.drawPolygon(mFreeOrig);

        QPen movedPen(QColor(120, 170, 255, 220), 1.5);
        movedPen.setCosmetic(true);
        painter.setPen(movedPen);
        painter.drawPolygon(mFreeMoved);

        const bool anyMoved = mFreeMoved != mFreeOrig;
        for (int i = 0; i < 8; ++i)
        {
            const QPointF hp = freeHandlePos(i, mFreeMoved);
            const bool dragging = mFreeDragKind == 1 && mFreeDragHandle == i;
            QRectF rect(hp.x() - halfSize, hp.y() - halfSize, halfSize * 2, halfSize * 2);
            painter.setPen(dragging ? QPen(Qt::white, 1.5) : QPen(QColor(40, 40, 40), 1.0));
            painter.setBrush(anyMoved ? QBrush(QColor(255, 170, 0)) : QBrush(QColor(120, 170, 255)));
            painter.drawRect(rect);
        }

        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < 4; ++i)
        {
            const QPointF rp = freeRotateHandlePos(i, mFreeMoved, viewScale);
            const bool dragging = mFreeDragKind == 2 && mFreeDragHandle == i;
            QPen ringPen(dragging ? Qt::white : QColor(120, 170, 255, 220), 1.2);
            ringPen.setCosmetic(true);
            painter.setPen(ringPen);
            painter.drawEllipse(rp, halfSize, halfSize);
        }
    }
    else if (mDeformActive && mDeformMode == 2)
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
    else if (mDeformMode == 3)
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
    else if (mDeformActive && mDeformMode == 4)
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
    mLastHoverPos = pos;

    if (!mDeformActive)
    {
        beginSession();
        if (!mDeformActive) { return; }
    }

    switch (mDeformMode)
    {
    case 0: // free
    {
        int handle = -1;
        const int kind = hitTestFree(pos, handle);
        if (kind >= 0)
        {
            mFreeDragKind = kind;
            mFreeDragHandle = handle;
            mFreePressCorners = mFreeMoved;
            mFreePressPos = pos;
            mScribbleArea->updateToolCursor();
        }
        // clicks that miss everything keep the session running; use
        // Enter / double-click to apply (Krita semantics)
        break;
    }
    case 1: // liquify
        mLiquifyStrokeActive = true;
        mLiquifyLastPos = pos;
        break;
    case 2: // warp
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
    case 3: // cage
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
    case 4: // perspective
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
        if (mDeformMode == 1)
        {
            // hover only moves the brush ring: repaint just the union of its
            // old and new areas instead of the whole canvas
            const qreal radius = 3.0 * mLiquifySize + 2.0;
            const QRectF newRing(pos.x() - radius, pos.y() - radius, 2 * radius, 2 * radius);
            const QRectF oldRing(mLastHoverPos.x() - radius, mLastHoverPos.y() - radius, 2 * radius, 2 * radius);
            const QRect widgetRect = mEditor->view()->getView()
                .mapRect(newRing | oldRing).toAlignedRect().adjusted(-2, -2, 2, 2);
            mScribbleArea->update(widgetRect);
        }
        // other modes' hover visuals don't depend on the cursor
        mLastHoverPos = pos;
        return;
    }
    mLastHoverPos = pos; // the ring follows the pointer while dragging too

    switch (mDeformMode)
    {
    case 0:
        if (mFreeDragKind >= 0)
        {
            updateFreeDrag(pos, event->modifiers());
        }
        break;
    case 1:
        if (mLiquifyStrokeActive)
        {
            liquifyStrokeTo(pos);
        }
        break;
    case 2:
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
    case 3:
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
    case 4:
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
        if (mFreeDragKind >= 0)
        {
            mFreeDragKind = -1;
            mFreeDragHandle = -1;
            if (mAnyPointMoved)
            {
                // the drag preview may be down-sampled: restore full resolution
                updateFreePreview(false);
            }
            mScribbleArea->updateToolCursor();
        }
        break;
    case 1:
        if (mLiquifyStrokeActive)
        {
            mLiquifyStrokeActive = false;
            if (mAnyPointMoved)
            {
                updateLiquifyPreview(false);
            }
        }
        break;
    case 2:
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
    case 3:
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
    case 4:
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
    if (mDeformMode == 3 && mCageSet == false && mCageDrawPoints.size() >= 3)
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
            if (mDeformMode == 0)
            {
                mFreeMoved = mFreeOrig;
                mAnyPointMoved = false;
                updateFreePreview(true);
            }
            else if (mDeformMode == 2)
            {
                mMovedPoints = mOrigPoints;
                mAnyPointMoved = false;
                updateWarpPreview(true);
            }
            else if (mDeformMode == 3)
            {
                mCageMovedVertices = mCageOrigVertices;
                mAnyPointMoved = false;
                updateCagePreview();
            }
            else if (mDeformMode == 4)
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

    const bool hadSelection = selectMan->somethingSelected() && !selectMan->mySelectionRect().isEmpty();
    QPolygonF regionPolygon;
    bool isPolygon = false;
    if (hadSelection)
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

        // margin so pushed pixels have somewhere to go; the free mode hugs
        // the content bounds exactly, like Krita's transform frame
        const int margin = (mDeformMode == 1) ? qMax(64, mLiquifySize * 3)
                                              : (mDeformMode == 0 ? 0 : 64);
        contentBounds.adjust(-margin, -margin, margin, margin);
        regionPolygon = QPolygonF(QRectF(contentBounds));
    }

    Q_ASSERT(regionPolygon.size() >= 4);

    mRegion = regionPolygon.boundingRect().toAlignedRect();
    if (mRegion.width() < 4 || mRegion.height() < 4) { return; }

    mRegionPolygon = regionPolygon;
    mRegionIsPolygon = isPolygon;

    BitmapImage sourcePart = bitmapImage->copy(mRegionPolygon);
    if (sourcePart.width() <= 0 || sourcePart.height() <= 0) { return; }

    // the implicit selection drives the preview pipeline (the clear-region
    // + draw-warp pass only runs while something is selected), so it is
    // created here, once the session is certain to start; its marquee
    // visual is suppressed by ScribbleArea while the session runs
    mSessionOwnsSelection = !hadSelection;
    if (mSessionOwnsSelection)
    {
        selectMan->setSelection(QRectF(bitmapImage->bounds()), true);
    }

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
        // free transform frame over the region (TL TR BR BL)
        mFreeOrig = quadFromRect(QRectF(mRegion));
        mFreeMoved = mFreeOrig;
        mFreeDragKind = -1;
        mFreeDragHandle = -1;
    }
    else if (mDeformMode == 1)
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
    else if (mDeformMode == 2)
    {
        rebuildLattice();
    }
    else if (mDeformMode == 3)
    {
        mCageSet = false;
        mCageDrawPoints.clear();
        mCageOrigVertices.clear();
        mCageMovedVertices.clear();
        mCageDragVertex = -1;
    }
    else if (mDeformMode == 4)
    {
        mPerspOrig = quadFromRect(QRectF(mRegion));
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
    if (mSessionOwnsSelection)
    {
        mSessionOwnsSelection = false;
        // drop the tool-made selection: leaving it behind would pin every
        // later session to this stale rect instead of the content bounds
        mEditor->deselectAll();
    }
    mDeformActive = false;
    mDragIndex = -1;
    mCageDragVertex = -1;
    mPerspDragCorner = -1;
    mFreeDragKind = -1;
    mFreeDragHandle = -1;
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
    mFreeOrig = QPolygonF();
    mFreeMoved = QPolygonF();
    mFreePressCorners = QPolygonF();
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
    if (!mDeformActive || mDeformMode != 1 || mLiquifyMovedGrid.isEmpty()) { return; }

    // interactive frames render a down-scaled copy, release/commit full res
    const bool useScaled = interactive && mPreviewScale < 1.0 && !mPreviewSource.isNull();
    const QImage& src = useScaled ? mPreviewSource : mSourceImage;
    const qreal scale = useScaled ? mPreviewScale : 1.0;

    // cols/rows must be derived exactly like the lattice gridWarpImage builds
    // internally (ceil(size/cell)): any other count fails its point-count
    // check and the call silently returns the source unwarped
    const int cell = qMax(4, qRound(mLiquifyGridCell * scale));
    const int cols = qMax(1, (src.width() + cell - 1) / cell);
    const int rows = qMax(1, (src.height() + cell - 1) / cell);
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
    const QImage warped = MlsWarp::gridWarpImage(src, mapped, cell, useAA, &offset);

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
    if (!mDeformActive || mDeformMode != 2) { return; }

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
    if (!mDeformActive || mDeformMode != 3 || !mCageSet) { return; }

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
    if (!mDeformActive || mDeformMode != 4) { return; }

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

void DeformTool::updateFreeDrag(const QPointF& pos, Qt::KeyboardModifiers modifiers)
{
    if (!mDeformActive || mFreeDragKind < 0 || mFreeMoved.size() != 4
        || mFreePressCorners.size() != 4)
    {
        return;
    }

    // frame-at-press basis: origin TL, u-axis along the top edge, v-axis
    // along the left edge; everything is solved in its local (u,v) space
    const QPolygonF& base = mFreePressCorners;
    const QPointF origin = base[0];
    const QPointF uAxis = base[1] - base[0];
    const QPointF vAxis = base[3] - base[0];
    const qreal det = crossProduct(uAxis, vAxis);
    if (std::abs(det) < 1e-9) { return; }

    const auto toLocal = [&](const QPointF& p)
    {
        const QPointF d = p - origin;
        return QPointF(crossProduct(d, vAxis) / det, crossProduct(uAxis, d) / det);
    };

    QPolygonF result;
    if (mFreeDragKind == 0)
    {
        // translate
        const QPointF t = pos - mFreePressPos;
        for (const QPointF& c : base) { result << c + t; }
    }
    else if (mFreeDragKind == 2)
    {
        // rotate around the frame center (Ctrl snaps to 15 degree steps)
        const QPointF center = (base[0] + base[1] + base[2] + base[3]) / 4.0;
        const qreal a0 = std::atan2(mFreePressPos.y() - center.y(), mFreePressPos.x() - center.x());
        const qreal a1 = std::atan2(pos.y() - center.y(), pos.x() - center.x());
        qreal angle = a1 - a0;
        if (modifiers & Qt::ControlModifier)
        {
            const qreal step = M_PI / 12.0;
            angle = qRound(angle / step) * step;
        }
        const qreal cs = std::cos(angle), sn = std::sin(angle);
        for (const QPointF& c : base)
        {
            const QPointF d = c - center;
            result << center + QPointF(cs * d.x() - sn * d.y(), sn * d.x() + cs * d.y());
        }
    }
    else
    {
        // scale: the dragged handle must land under the cursor; the opposite
        // side of the frame stays anchored. Corner handles scale both axes,
        // edge handles only their own (Krita free-transform behavior).
        const QPointF target = toLocal(pos);
        qreal uh, vh;
        freeHandleLocal(mFreeDragHandle, uh, vh);
        const qreal au = 1.0 - uh;
        const qreal av = 1.0 - vh;

        qreal su = 1.0, sv = 1.0;
        if (std::abs(uh - au) > 1e-6) { su = (target.x() - au) / (uh - au); }
        if (std::abs(vh - av) > 1e-6) { sv = (target.y() - av) / (vh - av); }

        // clamp near zero but keep the sign so flips still work
        if (std::abs(su) < 0.02) { su = su < 0 ? -0.02 : 0.02; }
        if (std::abs(sv) < 0.02) { sv = sv < 0 ? -0.02 : 0.02; }

        if ((modifiers & Qt::ShiftModifier) && mFreeDragHandle < 4)
        {
            // Shift on a corner keeps the aspect ratio
            const qreal s = (std::abs(su) + std::abs(sv)) / 2.0;
            su = su < 0 ? -s : s;
            sv = sv < 0 ? -s : s;
        }

        static const qreal kCornerU[4] = { 0.0, 1.0, 1.0, 0.0 };
        static const qreal kCornerV[4] = { 0.0, 0.0, 1.0, 1.0 };
        for (int i = 0; i < 4; ++i)
        {
            const qreal u = au + su * (kCornerU[i] - au);
            const qreal v = av + sv * (kCornerV[i] - av);
            result << origin + u * uAxis + v * vAxis;
        }
    }

    mFreeMoved = result;
    mAnyPointMoved = mFreeMoved != mFreeOrig;

    updateFreePreview(true);
}

void DeformTool::updateFreePreview(bool interactive)
{
    if (!mDeformActive || mDeformMode != 0 || mFreeMoved.size() != 4) { return; }

    // the quad stays affine, so the perspective quad mapper is an exact
    // affine transform here; big regions warp a down-scaled copy while
    // dragging (release/commit recompute full res)
    const bool useScaled = interactive && mPreviewScale < 1.0 && !mPreviewSource.isNull();
    const QImage& src = useScaled ? mPreviewSource : mSourceImage;
    const qreal scale = useScaled ? mPreviewScale : 1.0;

    const QPointF topLeft(mRegion.topLeft());
    QPolygonF dstLocal;
    for (const QPointF& pt : mFreeMoved)
    {
        dstLocal << (pt - topLeft) * scale;
    }

    const bool useAA = toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();
    QPointF offset;
    const QImage warped = MlsWarp::perspectiveWarpImage(src, dstLocal, useAA, &offset);

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

int DeformTool::hitTestFree(const QPointF& pos, int& handleOut) const
{
    if (mFreeMoved.size() != 4) { return -1; }

    const qreal viewScale = mEditor->view()->scaling();
    const qreal tolerance = HandleTolerance / qMax<qreal>(viewScale, 0.01);

    // rotation rings first: they sit outside the corners, so they never
    // overlap the scale handles
    for (int i = 0; i < 4; ++i)
    {
        if (QLineF(freeRotateHandlePos(i, mFreeMoved, viewScale), pos).length() <= tolerance)
        {
            handleOut = i;
            return 2;
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        if (QLineF(freeHandlePos(i, mFreeMoved), pos).length() <= tolerance)
        {
            handleOut = i;
            return 1;
        }
    }
    if (mFreeMoved.containsPoint(pos, Qt::OddEvenFill))
    {
        handleOut = -1;
        return 0;
    }
    return -1;
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
                updateFreePreview(false);
            }
            else if (mDeformMode == 1)
            {
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
