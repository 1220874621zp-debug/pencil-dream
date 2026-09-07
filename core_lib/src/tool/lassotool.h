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

/** Freehand (lasso) selection tool with Krita-style selection actions:
 *  replace / add / subtract / intersect / symmetric difference, picked in
 *  the tool options or mid-stroke with modifier keys
 *  (Ctrl=replace, Shift=add, Alt=subtract, Shift+Alt=intersect,
 *  Ctrl+Alt=symmetric difference), plus grow/shrink of the result. */
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

    int selectionAction() const { return mSelectionAction; }
    void setSelectionAction(int action);

    int growValue() const { return mGrowValue; }
    void setGrowValue(int grow);

signals:
    void selectionActionChanged(int action);
    void growValueChanged(int grow);

private:
    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void beginLasso(const QPointF& pos);
    void extendLasso(const QPointF& pos);
    void endLasso(const QPointF& pos);

    /** Krita KisSelectionModifierMapper defaults (no-remap config) */
    int actionFromModifiers(Qt::KeyboardModifiers modifiers) const;

    QPolygonF mLassoPoints;
    bool mLassoActive = false;

    // 0 replace / 1 add / 2 subtract / 3 intersect / 4 symmetric difference
    int mSelectionAction = 0;
    int mActiveAction = 0;
    int mGrowValue = 0;

    SAVESTATE_ID mUndoStateId = 0;
};

#endif // LASSOTOOL_H
