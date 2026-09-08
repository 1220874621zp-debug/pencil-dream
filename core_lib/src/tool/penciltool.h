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

#ifndef PENCILTOOL_H
#define PENCILTOOL_H

#include "stroketool.h"
#include <QColor>
#include <QCursor>

class Layer;

class PencilTool : public StrokeTool
{
    Q_OBJECT
public:
    explicit PencilTool(QObject* parent);

    ToolType type() const override { return PENCIL; }

    ToolProperties& toolProperties() override { return mSettings.toolProperties(); }
    const StrokeToolProperties& strokeToolProperties() const override { return mSettings; }

    void loadSettings() override;
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void drawStroke();
    void paintAt(QPointF point);

private:
    QPointF mLastBrushPoint{ 0, 0 };
    QPointF mMouseDownPoint;

    StrokeToolProperties mSettings;

    // 位图光标一次构造终身复用：每次新建 QCursor 都会重建 Windows
    // 原生 HCURSOR，间歇性创建失败会静默回退成系统箭头
    bool mCursorBuilt = false;
    QCursor mCursorSvg;
    QCursor mCursorCross;
};

#endif // PENCILTOOL_H
