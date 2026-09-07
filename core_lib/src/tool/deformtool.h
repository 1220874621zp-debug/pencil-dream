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

#include <QImage>
#include <QPolygonF>
#include <QVector>
#include <QPointF>

class PointerEvent;

/** Free-deform tool (Krita-style warp): overlays a control-point grid on the
 *  current selection (or the whole drawing) and warps the region with a
 *  Moving-Least-Squares deformation while dragging points.
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

private:
    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void pointerDoubleClickEvent(PointerEvent*) override;

    bool keyPressEvent(QKeyEvent* event) override;

    void beginDeform(const QPointF& pos);
    void teardown();
    void updateWarpPreview();
    void commitDeform();
    void cancelDeform();

    int hitTestControlPoint(const QPointF& pos) const;

    // deform region in canvas coordinates
    QRect mRegion;
    QPolygonF mRegionPolygon;
    bool mRegionIsPolygon = false;

    QImage mSourceImage;
    QVector<QPointF> mOrigPoints;
    QVector<QPointF> mMovedPoints;
    int mGridSize = 4;

    int mDragIndex = -1;
    bool mDeformActive = false;
    bool mAnyPointMoved = false;

    // cached warp result for commit
    QImage mWarpedResult;
    QPointF mWarpedTopLeft;
};

#endif // DEFORMTOOL_H
