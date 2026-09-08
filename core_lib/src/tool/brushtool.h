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

#ifndef BRUSHTOOL_H
#define BRUSHTOOL_H

#include "stroketool.h"
#include "brush/brushengine.h"
#include <QColor>
#include <QCursor>
#include <QTimer>

class Layer;

class BrushTool : public StrokeTool
{
    Q_OBJECT

public:
    explicit BrushTool(QObject* parent = 0);

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

    /** 应用一笔预设：宽度/羽化/压感同步进工具属性（滑杆联动），其余参数进引擎 */
    void applyBrushPreset(const BrushSettings& preset);
    /** 启动恢复用：只装载预设参数，不覆盖用户上次调过的宽度/羽化属性 */
    void initPresetExtras(const BrushSettings& preset);
    /** 工具选项面板编辑：整套参数生效并持久化（Krita 式选项面板的数据入口） */
    void applyBrushOptions(const BrushSettings& options);
    /** 启动时是否恢复了用户保存过的选项（预设面板据此跳过预设回灌） */
    bool hasUserOptions() const { return mUserOptionsRestored; }
    /** 当前生效的完整笔刷参数（工具属性 + 预设额外参数的合并视图） */
    BrushSettings currentBrushSettings();

protected:
    QPointF mLastBrushPoint;
    QPointF mMouseDownPoint;

    QColor mCurrentPressuredColor;
    qreal mOpacity = 1.0;

    StrokeToolProperties mSettings;

private:
    void syncEngineSettings();
    void persistUserOptions();
    BrushEngine::DabPainter dabPainter() const;

    // 预设里超出工具属性范围的参数（笔尖形状/扁率/角度/间距/曲线等）
    BrushSettings mPresetExtras;
    BrushEngine mEngine;
    QTimer mAirbrushTimer;
    bool mUserOptionsRestored = false;

    // 位图光标一次构造终身复用：每次新建 QCursor 都会重建 Windows
    // 原生 HCURSOR，间歇性创建失败会静默回退成系统箭头
    bool mCursorBuilt = false;
    QCursor mCursorSvg;
    QCursor mCursorCross;
};

#endif // BRUSHTOOL_H
