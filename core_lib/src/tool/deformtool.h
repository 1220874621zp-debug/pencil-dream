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

#ifndef DEFORMTOOL_H
#define DEFORMTOOL_H

#include "transformtool.h"
#include "undoredomanager.h"
#include "mlswarp.h"

#include <QImage>
#include <QPolygonF>
#include <QVector>
#include <QPointF>

class PointerEvent;

/** Multi-mode deform tool, a port of Krita's transform-tool deformation modes:
 *  - Liquify: brush dabs push/scale/rotate/offset/restore pixels
 *    (KisLiquifyTransformWorker semantics)
 *  - Warp: MLS control-point grid, rigid/similitude/affine + alpha
 *    (KisWarpTransformWorker)
 *  - Cage: freehand cage outline + draggable vertices, Green coordinates
 *    (KisCageTransformWorker / KisGreenCoordinatesMath)
 *  - Perspective: 4-corner quad mapping
 *
 *  The layer data is only touched on commit; the live preview goes through
 *  ScribbleArea::setDeformPreview, so cancelling (Esc) is loss-free. */
class DeformTool : public TransformTool
{
    Q_OBJECT

public:
    explicit DeformTool(QObject* parent = nullptr);

    ToolType type() const override { return DEFORM; }

    void loadSettings() override;
    QCursor cursor() override;

    bool isActive() const override;

    void paint(QPainter& painter, const QRect& blitRect) override;

    bool leavingThisTool() override;
    bool enteringThisTool() override;

    void clearToolData() override;

    int gridSize() const { return mGridSize; }
    void setGridSize(int size);

    int deformMode() const { return mDeformMode; }
    void setDeformMode(int mode);

    int liquifyOp() const { return mLiquifyOp; }
    void setLiquifyOp(int op);

    int liquifySize() const { return mLiquifySize; }
    void setLiquifySize(int size);

    qreal liquifyAmount() const { return mLiquifyAmount; }
    void setLiquifyAmount(qreal amount);

    bool liquifyReverse() const { return mLiquifyReverse; }
    void setLiquifyReverse(bool reverse);

    qreal warpAlpha() const { return mWarpAlpha; }
    void setWarpAlpha(qreal alpha);

    int warpType() const { return mWarpType; }
    void setWarpType(int type);

signals:
    void gridSizeChanged(int size);
    void deformModeChanged(int mode);
    void liquifyOpChanged(int op);
    void liquifySizeChanged(int size);
    void liquifyAmountChanged(qreal amount);
    void liquifyReverseChanged(bool reverse);
    void warpAlphaChanged(qreal alpha);
    void warpTypeChanged(int type);

private:
    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void pointerDoubleClickEvent(PointerEvent*) override;

    bool keyPressEvent(QKeyEvent* event) override;

    // session shared by all modes
    void beginSession();
    void teardown();
    void commitDeform();
    void cancelDeform();

    // per-mode interactions
    void liquifyStrokeTo(const QPointF& pos);
    void updateWarpPreview(bool interactive);
    void rebuildLattice();
    void finalizeCage(const QPointF& pos);
    void updateCagePreview();
    void updatePerspectivePreview();
    int hitTestControlPoint(const QPointF& pos) const;
    int hitTestCageVertex(const QPointF& pos) const;
    int hitTestPerspectiveCorner(const QPointF& pos) const;

    // deform region in canvas coordinates
    QRect mRegion;
    QPolygonF mRegionPolygon;
    bool mRegionIsPolygon = false;

    QImage mSourceImage;   // captured once per session (never modified)
    QImage mWorkImage;     // liquify accumulates dabs here

    // interactive-warp downscale (warp mode, big regions)
    QImage mPreviewSource;
    qreal mPreviewScale = 1.0;

    // warp mode state
    QVector<QPointF> mOrigPoints;
    QVector<QPointF> mMovedPoints;
    int mGridSize = 4;

    // liquify mode state
    bool mLiquifyStrokeActive = false;
    QPointF mLiquifyLastPos;
    QPointF mCursorPos;

    // cage mode state
    bool mCageSet = false;
    QPolygonF mCageDrawPoints;          // while drawing the outline
    QVector<QPointF> mCageOrigVertices; // canvas coords
    QVector<QPointF> mCageMovedVertices;
    int mCageDragVertex = -1;

    // perspective mode state
    QPolygonF mPerspOrig; // 4 corners, canvas coords
    QPolygonF mPerspMoved;
    int mPerspDragCorner = -1;

    int mDragIndex = -1; // warp grid drag
    bool mDeformActive = false;
    bool mAnyPointMoved = false;

    // options (persisted via tool properties)
    int mDeformMode = 0;      // 0 liquify / 1 warp / 2 cage / 3 perspective
    int mLiquifyOp = 0;       // 0 move / 1 scale / 2 rotate / 3 offset / 4 undo
    int mLiquifySize = 60;
    qreal mLiquifyAmount = 0.1;
    bool mLiquifyReverse = false;
    qreal mWarpAlpha = 1.0;
    int mWarpType = 2;        // MlsWarp::WarpType

    // cached warp result for commit (full resolution)
    QImage mWarpedResult;
    QPointF mWarpedTopLeft;
};

#endif // DEFORMTOOL_H
