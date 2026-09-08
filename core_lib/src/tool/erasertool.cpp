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
#include "erasertool.h"

#include <QSettings>
#include <QPixmap>
#include <QPainter>

#include "editor.h"
#include "blitrect.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "undoredomanager.h"
#include "pointerevent.h"


EraserTool::EraserTool(QObject* parent) : StrokeTool(parent)
{
    // 橡皮走笔刷引擎生成 dab（硬度/压感曲线/自动间距/子像素与画笔同源），
    // 擦除本身由绘制缓冲的 alpha + ScribbleArea 终局 DestinationOut 完成
    mPresetExtras.eraser = true;
    mEngine.setSettings(mPresetExtras);

    // 喷枪：静止悬停时按速率补 dab
    mAirbrushTimer.setInterval(16);
    connect(&mAirbrushTimer, &QTimer::timeout, this, [this]() {
        mEngine.airbrushTick(dabPainter());
    });
}

ToolType EraserTool::type() const
{
    return ERASER;
}

void EraserTool::loadSettings()
{
    StrokeTool::loadSettings();

    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    QHash<int, PropertyInfo> info;

    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FEATHER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FEATHER_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::PRESSURE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::STABILIZATION_VALUE] = { Layer::BITMAP };

    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 24.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 48.0 };
    info[StrokeToolProperties::FEATHER_ENABLED] = true;
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::STABILIZATION_VALUE] = { StabilizationLevel::NONE, StabilizationLevel::STRONG, StabilizationLevel::NONE };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, pencilSettings.value("eraserWidth", 24.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, pencilSettings.value("eraserFeather", 48.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::STABILIZATION_VALUE, pencilSettings.value("stabilizerLevel", StabilizationLevel::NONE).toInt());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_ENABLED, pencilSettings.value("eraserUseFeather", true).toBool());
        toolProperties().setBaseValue(StrokeToolProperties::PRESSURE_ENABLED, pencilSettings.value("eraserPressure", true).toBool());

        pencilSettings.remove("eraserWidth");
        pencilSettings.remove("eraserFeather");
        pencilSettings.remove("stabilizerLevel");
        pencilSettings.remove("eraserUseFeather");
        pencilSettings.remove("eraserPressure");
        pencilSettings.remove("eraserAA");
    }

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);

    // Krita 式工具选项的持久化恢复（XML 整套存取）
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    const QString saved = brushOptions.value("BrushOptions/ERASER").toString();
    if (!saved.isEmpty()) {
        BrushSettings restored;
        if (BrushSettings::fromXMLString(saved, restored)) {
            restored.eraser = true;
            mPresetExtras = restored;
            mUserOptionsRestored = true;
        }
    }

    syncEngineSettings();
}

QCursor EraserTool::cursor()
{
    if (!mCursorBuilt)
    {
        mCursorBuilt = true;
        mCursorCross = QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
    }
    return mCursorCross;
}

void EraserTool::pointerPressEvent(PointerEvent *event)
{
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();
    mLastBrushPoint = getCurrentPoint();

    startStroke(event->inputType());

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        syncEngineSettings();
        // 镜像绘画对称中心 = 视口中心（画布坐标）
        if (mEngine.settings().mirrorX || mEngine.settings().mirrorY) {
            mEngine.setMirrorCenter(mEditor->view()->mapScreenToCanvas(
                QPointF(mScribbleArea->width(), mScribbleArea->height()) * 0.5));
        }
        // 擦除的 dab 颜色无关紧要（DestinationOut 只用 alpha），给黑色即可
        mEngine.beginStroke(getCurrentPoint(), mInterpolator.getPressure(),
                            Qt::black, dabPainter());
        if (mEngine.settings().airbrushEnabled) {
            mAirbrushTimer.start();
        }
    }

    StrokeTool::pointerPressEvent(event);
}

void EraserTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->buttons() & Qt::LeftButton && event->inputType() == mCurrentInputType)
    {
        mCurrentPressure = mInterpolator.getPressure();
        drawStroke();
        if (mSettings.stabilizerLevel() != mInterpolator.getStabilizerLevel())
        {
            mInterpolator.setStabilizerLevel(mSettings.stabilizerLevel());
        }
    }

    StrokeTool::pointerMoveEvent(event);
}

void EraserTool::pointerReleaseEvent(PointerEvent *event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->inputType() != mCurrentInputType) return;

    mEditor->backup(typeName());

    qreal distance = QLineF(getCurrentPoint(), mMouseDownPoint).length();
    if (distance < 1)
    {
        paintAt(mMouseDownPoint);
    }
    else
    {
        drawStroke();
    }

    endStroke();
    mEngine.endStroke();
    mAirbrushTimer.stop();

    StrokeTool::pointerReleaseEvent(event);
}

// draw a single paint dab at the given location
void EraserTool::paintAt(QPointF point)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        syncEngineSettings();
        mCurrentWidth = mEngine.dabDiameterAt(mCurrentPressure);
        mEngine.dabAt(point, mCurrentPressure, dabPainter());
    }
}

void EraserTool::drawStroke()
{
    StrokeTool::drawStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::BITMAP)
    {
        syncEngineSettings();
        mCurrentWidth = mEngine.dabDiameterAt(mCurrentPressure);
        mEngine.strokeTo(getCurrentPoint(), mCurrentPressure, dabPainter());
    }
}

void EraserTool::applyBrushPreset(const BrushSettings& preset)
{
    mPresetExtras = preset;
    mPresetExtras.eraser = true;

    setWidth(preset.diameter);
    // 硬度与羽化互为倒数映射：硬度 1 → 羽化 1，硬度 0.05 → 羽化 95
    setFeather((1.0 - preset.hardness) * 100.0);
    setPressureEnabled(preset.pressureSize || preset.pressureOpacity);

    syncEngineSettings();
    persistUserOptions();
}

void EraserTool::initPresetExtras(const BrushSettings& preset)
{
    mPresetExtras = preset;
    mPresetExtras.eraser = true;
    syncEngineSettings();
}

void EraserTool::applyBrushOptions(const BrushSettings& options)
{
    mPresetExtras = options;
    mPresetExtras.eraser = true;
    setWidth(options.diameter);
    setFeather((1.0 - qBound(0.01, options.hardness, 1.0)) * 100.0);
    syncEngineSettings();
    persistUserOptions();
    mUserOptionsRestored = true;
}

void EraserTool::persistUserOptions()
{
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    brushOptions.setValue("BrushOptions/ERASER", currentBrushSettings().toXMLString());
}

BrushSettings EraserTool::currentBrushSettings()
{
    syncEngineSettings();
    return mEngine.settings();
}

void EraserTool::syncEngineSettings()
{
    BrushSettings merged = mPresetExtras;
    merged.eraser = true;
    merged.diameter = mSettings.width();
    merged.hardness = mSettings.featherEnabled()
                      ? 1.0 - qBound(FEATHER_MIN, mSettings.feather(), FEATHER_MAX) / 100.0
                      : 1.0;
    // 工具选项里的"压感"勾选框是总开关；预设只决定用压感控什么
    merged.pressureSize = mPresetExtras.pressureSize && mSettings.pressureEnabled();
    merged.pressureOpacity = mPresetExtras.pressureOpacity && mSettings.pressureEnabled();
    mEngine.setSettings(merged);
}

BrushEngine::DabPainter EraserTool::dabPainter() const
{
    return [this](const BrushEngine::DabRequest& dab) {
        // 橡皮与画笔共用合成路径：缓冲 alpha 累积 = 擦除量
        // （涂抹=封顶 opacity，叠加=越擦越净），实时预览与终局
        // paste 的 DestinationOut 负责反转语义
        DabPasteParams params;
        params.opacity = dab.opacity;
        params.flow = dab.flow;
        params.buildup = dab.buildup;
        mScribbleArea->drawDab(dab.dab, dab.topLeft, params);
    };
}
