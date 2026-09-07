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

#include "brushtool.h"

#include <cmath>
#include <QSettings>
#include <QPixmap>
#include <QPainter>
#include <QColor>

#include "editor.h"
#include "colormanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "undoredomanager.h"
#include "scribblearea.h"
#include "pointerevent.h"


BrushTool::BrushTool(QObject* parent) : StrokeTool(parent)
{
    mPresetExtras.name = QStringLiteral("圆头笔");
    mEngine.setSettings(mPresetExtras);

    // 喷枪：静止悬停时按速率补 dab
    mAirbrushTimer.setInterval(16);
    connect(&mAirbrushTimer, &QTimer::timeout, this, [this]() {
        mEngine.airbrushTick(dabPainter());
    });
}

ToolType BrushTool::type() const
{
    return BRUSH;
}

void BrushTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FEATHER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::PRESSURE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::STABILIZATION_VALUE] = { Layer::BITMAP };

    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    QHash<int, PropertyInfo> info;
    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 24.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 10.0 };
    info[StrokeToolProperties::FEATHER_ENABLED] = true;
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::STABILIZATION_VALUE] = { StabilizationLevel::NONE, StabilizationLevel::STRONG, StabilizationLevel::STRONG } ;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, pencilSettings.value("brushWidth", 24.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, pencilSettings.value("brushFeather", 48.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::PRESSURE_ENABLED, pencilSettings.value("brushPressure", true).toBool());
        toolProperties().setBaseValue(StrokeToolProperties::STABILIZATION_VALUE, pencilSettings.value("brushLineStabilization", StabilizationLevel::STRONG).toInt());

        pencilSettings.remove("brushWidth");
        pencilSettings.remove("brushFeather");
        pencilSettings.remove("brushPressure");
        pencilSettings.remove("brushLineStabilization");
    }

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);

    // Krita 式工具选项的持久化恢复（XML 整套存取）
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    const QString saved = brushOptions.value("BrushOptions/BRUSH").toString();
    if (!saved.isEmpty()) {
        BrushSettings restored;
        if (BrushSettings::fromXMLString(saved, restored)) {
            restored.eraser = false;
            mPresetExtras = restored;
            mUserOptionsRestored = true;
        }
    }

    syncEngineSettings();
}

QCursor BrushTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":icons/general/cursor-brush.svg"), 4, 14);
    }
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void BrushTool::pointerPressEvent(PointerEvent *event)
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
        mEngine.beginStroke(getCurrentPoint(), mInterpolator.getPressure(),
                            mEditor->color()->frontColor(), dabPainter());
        if (mEngine.settings().airbrushEnabled) {
            mAirbrushTimer.start();
        }
    }

    StrokeTool::pointerPressEvent(event);
}

void BrushTool::pointerMoveEvent(PointerEvent* event)
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

void BrushTool::pointerReleaseEvent(PointerEvent *event)
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
void BrushTool::paintAt(QPointF point)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        syncEngineSettings();
        mCurrentWidth = mEngine.dabDiameterAt(mCurrentPressure);
        mEngine.dabAt(point, mCurrentPressure, dabPainter());
    }
}

void BrushTool::drawStroke()
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

void BrushTool::applyBrushPreset(const BrushSettings& preset)
{
    mPresetExtras = preset;
    mPresetExtras.eraser = false;

    setWidth(preset.diameter);
    // 硬度与羽化互为倒数映射：硬度 1 → 羽化 1，硬度 0.05 → 羽化 95
    setFeather((1.0 - preset.hardness) * 100.0);
    setPressureEnabled(preset.pressureSize || preset.pressureOpacity);

    syncEngineSettings();
    persistUserOptions();
}

void BrushTool::initPresetExtras(const BrushSettings& preset)
{
    mPresetExtras = preset;
    mPresetExtras.eraser = false;
    syncEngineSettings();
}

void BrushTool::applyBrushOptions(const BrushSettings& options)
{
    mPresetExtras = options;
    mPresetExtras.eraser = false;
    setWidth(options.diameter);
    setFeather((1.0 - qBound(0.01, options.hardness, 1.0)) * 100.0);
    syncEngineSettings();
    persistUserOptions();
    mUserOptionsRestored = true;
}

void BrushTool::persistUserOptions()
{
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    brushOptions.setValue("BrushOptions/BRUSH", currentBrushSettings().toXMLString());
}

BrushSettings BrushTool::currentBrushSettings()
{
    syncEngineSettings();
    return mEngine.settings();
}

void BrushTool::syncEngineSettings()
{
    BrushSettings merged = mPresetExtras;
    merged.diameter = mSettings.width();
    merged.hardness = 1.0 - qBound(FEATHER_MIN, mSettings.feather(), FEATHER_MAX) / 100.0;
    // 工具选项里的"压感"勾选框是总开关；预设只决定用压感控什么
    merged.pressureSize = mPresetExtras.pressureSize && mSettings.pressureEnabled();
    merged.pressureOpacity = mPresetExtras.pressureOpacity && mSettings.pressureEnabled();
    mEngine.setSettings(merged);
}

BrushEngine::DabPainter BrushTool::dabPainter() const
{
    return [this](const BrushEngine::DabRequest& dab) {
        DabPasteParams params;
        params.opacity = dab.opacity;
        params.flow = dab.flow;
        params.buildup = dab.buildup;
        params.blendMode = dab.blendMode;
        mScribbleArea->drawDab(dab.dab, dab.topLeft, params);
    };
}
