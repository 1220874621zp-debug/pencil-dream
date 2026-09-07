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

#ifndef ERASERTOOL_H
#define ERASERTOOL_H

#include "stroketool.h"

#include "brush/brushengine.h"

class EraserTool : public StrokeTool
{
    Q_OBJECT

public:
    explicit EraserTool(QObject* parent = nullptr);
    ToolType type() const override;

    ToolProperties& toolProperties() override { return mSettings.toolProperties(); }
    const StrokeToolProperties& strokeToolProperties() const override { return mSettings; }

    void loadSettings() override;
    QCursor cursor() override;

    void pointerMoveEvent(PointerEvent*) override;
    void pointerPressEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void drawStroke();
    void paintAt(QPointF point);

    /** 预设应用：与 BrushTool 同款接口，橡皮预设也走笔刷引擎 */
    void applyBrushPreset(const BrushSettings& preset);
    void initPresetExtras(const BrushSettings& preset);
    BrushSettings currentBrushSettings();

protected:
    QPointF mLastBrushPoint;
    QPointF mMouseDownPoint;

    StrokeToolProperties mSettings;

private:
    void syncEngineSettings();
    BrushEngine::DabPainter dabPainter() const;

    BrushEngine mEngine;
    BrushSettings mPresetExtras;
};

#endif // ERASERTOOL_H
