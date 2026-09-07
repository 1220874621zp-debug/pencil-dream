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

#ifndef LASSOTOOL_H
#define LASSOTOOL_H

#include "transformtool.h"
#include "undoredomanager.h"

#include <QPolygonF>

class PointerEvent;

/** Freehand (lasso) selection tool: drag to draw a closed outline,
 *  release to commit the polygonal selection. */
class LassoTool : public TransformTool
{
    Q_OBJECT

public:
    explicit LassoTool(QObject* parent = nullptr);

    ToolType type() const override { return LASSO; }

    void loadSettings() override;
    QCursor cursor() override;

    bool isActive() const override;

    void paint(QPainter& painter, const QRect& blitRect) override;

private:
    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void beginLasso(const QPointF& pos);
    void extendLasso(const QPointF& pos);
    void endLasso(const QPointF& pos);

    QPolygonF mLassoPoints;
    bool mLassoActive = false;

    SAVESTATE_ID mUndoStateId = 0;
};

#endif // LASSOTOOL_H
