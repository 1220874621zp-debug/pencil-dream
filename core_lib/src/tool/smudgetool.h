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
#ifndef SMUDGETOOL_H
#define SMUDGETOOL_H

#include "stroketool.h"

#include "brush/brushengine.h"
#include <QTimer>

class SmudgeTool : public StrokeTool
{
    Q_OBJECT
public:
    explicit SmudgeTool(QObject* parent = 0);

    ToolType type() const override;

    ToolProperties& toolProperties() override { return mSettings.toolProperties(); }
    const StrokeToolProperties& strokeToolProperties() const override { return mSettings; }

    uint toolMode;  // 0=混合(Krita smudge) 1=液化(旧像素滑移)
    void loadSettings() override;
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent *) override;
    void pointerReleaseEvent(PointerEvent *) override;
    void pointerMoveEvent(PointerEvent *) override;

    bool keyPressEvent(QKeyEvent *) override;
    bool keyReleaseEvent(QKeyEvent *) override;

    void drawStroke();
    void paintAt(QPointF point);

    /** 工具选项面板编辑：整套参数生效并持久化 */
    void applyBrushOptions(const BrushSettings& options);
    bool hasUserOptions() const { return mUserOptionsRestored; }
    BrushSettings currentBrushSettings();

protected:
    bool emptyFrameActionEnabled() override;

private:
    void syncEngineSettings();
    void persistUserOptions();
    BrushEngine::DabPainter dabPainter(); // 非常量：回调里更新采样基准
    void legacyLiquifyStroke();

    QPointF mLastBrushPoint;
    QPointF mMouseDownPoint;

    StrokeToolProperties mSettings;

    // 混合笔刷引擎（Krita smudge）：掩码/间距/压感与画笔同源
    BrushEngine mEngine;
    BrushSettings mPresetExtras;
    QTimer mAirbrushTimer;
    bool mUserOptionsRestored = false;

    // 采样基准：上一枚 dab 的中心（Krita：每 dab 从上一 dab 位置采样）
    QPointF mPrevDabCenter;
    bool mHasPrevDab = false;
    // 每笔缓存的图层采样图（避免逐 dab 触发 autoCrop）
    QImage mSampleImage;
    QPoint mSampleOrigin;
};

#endif // SMUDGETOOL_H
