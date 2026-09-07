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
#include "smudgetool.h"
#include <QPixmap>
#include <QSettings>

#include "pointerevent.h"
#include "editor.h"
#include "scribblearea.h"

#include "layermanager.h"
#include "viewmanager.h"
#include "undoredomanager.h"

#include "layerbitmap.h"

SmudgeTool::SmudgeTool(QObject* parent) : StrokeTool(parent)
{
    toolMode = 0;

    // 混合笔刷默认参数（对齐 Krita 混合预设 b)Basic 手感）：
    // 软笔尖（fade 0.9 → 硬度 0.1）、固定间距 10%、速率 50%、压感控速率不控大小
    mPresetExtras.diameter = 30.0;
    mPresetExtras.hardness = 0.1;
    mPresetExtras.opacity = 1.0;
    mPresetExtras.flow = 0.5;
    mPresetExtras.spacingMode = BrushSettings::SpacingMode::Fixed;
    mPresetExtras.spacing = 0.1;
    mPresetExtras.pressureSize = false;
    mPresetExtras.pressureOpacity = true;
    mEngine.setSettings(mPresetExtras);
    mEngine.setSmudgeMode(true);

    // 喷枪：静止悬停时按速率补 dab
    mAirbrushTimer.setInterval(16);
    connect(&mAirbrushTimer, &QTimer::timeout, this, [this]() {
        mEngine.airbrushTick(dabPainter());
    });
}

ToolType SmudgeTool::type() const
{
    return SMUDGE;
}

void SmudgeTool::loadSettings()
{
    StrokeTool::loadSettings();

    QHash<int, PropertyInfo> info;
    QSettings pencilSettings(PENCIL2D, PENCIL2D);
    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FEATHER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::PRESSURE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::STABILIZATION_VALUE] = { Layer::BITMAP };

    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 30.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 90.0 };
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::STABILIZATION_VALUE] = { StabilizationLevel::NONE, StabilizationLevel::STRONG, StabilizationLevel::NONE };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, pencilSettings.value("smudgeWidth", 30.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, pencilSettings.value("smudgeFeather", 90.0).toReal());

        pencilSettings.remove("smudgeWidth");
        pencilSettings.remove("smudgeFeather");
    }

    // Krita 式工具选项的持久化恢复
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    const QString saved = brushOptions.value("BrushOptions/SMUDGE").toString();
    if (!saved.isEmpty()) {
        BrushSettings restored;
        if (BrushSettings::fromXMLString(saved, restored)) {
            mPresetExtras = restored;
            mUserOptionsRestored = true;
        }
    }

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);

    syncEngineSettings();
}

bool SmudgeTool::emptyFrameActionEnabled()
{
    // Disabled till we get it working for vector layers...
    return false;
}

QCursor SmudgeTool::cursor()
{
    if (toolMode == 0) { //normal mode
        return QCursor(QPixmap(":icons/general/cursor-smudge.svg"), 4, 18);

    }
    else { // blured mode
        return QCursor(QPixmap(":icons/general/cursor-smudge-liquify.svg"), 4, 18);
    }
}

bool SmudgeTool::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt)
    {
        toolMode = 1; // alternative mode
        mScribbleArea->setCursor(cursor()); // update cursor
        return true;
    }
    return StrokeTool::keyPressEvent(event);
}

bool SmudgeTool::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt)
    {
        toolMode = 0; // default mode
        mScribbleArea->setCursor(cursor()); // update cursor
        return true;
    }
    return StrokeTool::keyReleaseEvent(event);
}

void SmudgeTool::pointerPressEvent(PointerEvent* event)
{
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();
    mLastBrushPoint = getCurrentPoint();

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (event->button() == Qt::LeftButton)
    {
        startStroke(event->inputType());
        if (layer->type() == Layer::BITMAP)
        {
            if (toolMode == 0) {
                syncEngineSettings();
                mHasPrevDab = false;
                // 每笔缓存图层采样图（bounds() 会 autoCrop，不能逐 dab 调）
                BitmapImage* bmi = mScribbleArea->currentBitmapImage(layer);
                if (bmi != nullptr && bmi->image() != nullptr) {
                    mSampleImage = *bmi->image();
                    mSampleOrigin = bmi->bounds().topLeft();
                } else {
                    mSampleImage = QImage();
                }
                // 混合笔刷的颜色无关（掩码 alpha 起作用），给黑色即可
                mEngine.beginStroke(getCurrentPoint(), mInterpolator.getPressure(),
                                    Qt::black, dabPainter());
                if (mEngine.settings().airbrushEnabled) {
                    mAirbrushTimer.start();
                }
            }
        }
    }

    StrokeTool::pointerPressEvent(event);
}

void SmudgeTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (layer->type() != Layer::BITMAP)
    {
        return;
    }

    if (event->inputType() == mCurrentInputType) {
        if (event->buttons() & Qt::LeftButton)   // the user is also pressing the mouse (dragging)
        {
            mCurrentPressure = mInterpolator.getPressure();
            drawStroke();
            if (mSettings.stabilizerLevel() != mInterpolator.getStabilizerLevel())
            {
                mInterpolator.setStabilizerLevel(mSettings.stabilizerLevel());
            }
        }
    }

    StrokeTool::pointerMoveEvent(event);
}

void SmudgeTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->inputType() != mCurrentInputType) return;

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (event->button() == Qt::LeftButton)
    {
        mEditor->backup(typeName());

        if (layer->type() == Layer::BITMAP)
        {
            if (toolMode == 0 && QLineF(getCurrentPoint(), mMouseDownPoint).length() < 1) {
                // 混合笔刷单击不产生效果（Krita 同语义）
            } else {
                drawStroke();
            }
            mAirbrushTimer.stop();
            mEngine.endStroke();
            mSampleImage = QImage();
            mScribbleArea->paintBitmapBuffer();
            mScribbleArea->clearDrawingBuffer();
            endStroke();
        }
    }

    StrokeTool::pointerReleaseEvent(event);
}

void SmudgeTool::paintAt(QPointF point)
{
    Q_UNUSED(point)
    // 混合笔刷的单击无效果（需要至少两枚 dab 才构成位移采样）
}

void SmudgeTool::drawStroke()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || !layer->isPaintable()) { return; }

    if (toolMode == 1)
    {
        legacyLiquifyStroke();
        return;
    }

    StrokeTool::drawStroke();
    if (layer->type() == Layer::BITMAP && !mSampleImage.isNull())
    {
        syncEngineSettings();
        mCurrentWidth = mEngine.dabDiameterAt(mCurrentPressure);
        mEngine.strokeTo(getCurrentPoint(), mCurrentPressure, dabPainter());
    }
}

void SmudgeTool::legacyLiquifyStroke()
{
    BitmapImage *sourceImage = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer())->getLastBitmapImageAtFrame(mEditor->currentFrame());
    if (sourceImage == nullptr) { return; }
    BitmapImage targetImage = sourceImage->copy();
    StrokeTool::drawStroke();
    QList<QPointF> p = mInterpolator.interpolateStroke();

    for (int i = 0; i < p.size(); i++)
    {
        p[i] = mEditor->view()->mapScreenToCanvas(p[i]);
    }

    qreal opacity = 1.0;
    mCurrentWidth = mSettings.width();
    qreal brushWidth = mCurrentWidth + 0.0 * mSettings.feather();
    qreal offset = qMax(0.0, mCurrentWidth - 0.5 * mSettings.feather()) / brushWidth;

    QPointF a = mLastBrushPoint;
    QPointF b = getCurrentPoint();

    qreal brushStep = 2;
    qreal distance = QLineF(b, a).length() / 2.0;
    int steps = qRound(distance / brushStep);

    QPointF sourcePoint = mLastBrushPoint;
    for (int i = 0; i < steps; i++)
    {
        targetImage.paste(&mScribbleArea->mTiledBuffer);
        QPointF targetPoint = mLastBrushPoint + (i + 1) * (brushStep) * (b - mLastBrushPoint) / distance;
        mScribbleArea->liquifyBrush(&targetImage,
                                    sourcePoint,
                                    targetPoint,
                                    brushWidth,
                                    offset,
                                    opacity);

        if (i == (steps - 1))
        {
            mLastBrushPoint = targetPoint;
        }
        sourcePoint = targetPoint;
    }
}

void SmudgeTool::applyBrushOptions(const BrushSettings& options)
{
    mPresetExtras = options;
    setWidth(options.diameter);
    setFeather((1.0 - qBound(0.01, options.hardness, 1.0)) * 100.0);
    syncEngineSettings();
    persistUserOptions();
    mUserOptionsRestored = true;
}

void SmudgeTool::persistUserOptions()
{
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    brushOptions.setValue("BrushOptions/SMUDGE", currentBrushSettings().toXMLString());
}

BrushSettings SmudgeTool::currentBrushSettings()
{
    syncEngineSettings();
    return mEngine.settings();
}

void SmudgeTool::syncEngineSettings()
{
    BrushSettings merged = mPresetExtras;
    merged.diameter = mSettings.width();
    merged.hardness = 1.0 - qBound(FEATHER_MIN, mSettings.feather(), FEATHER_MAX) / 100.0;
    // 压感控混合速率（不控大小）——Krita PressureSmudgeRate 默认开
    merged.pressureSize = mPresetExtras.pressureSize && mSettings.pressureEnabled();
    merged.pressureOpacity = mPresetExtras.pressureOpacity && mSettings.pressureEnabled();
    mEngine.setSettings(merged);
}

BrushEngine::DabPainter SmudgeTool::dabPainter()
{
    return [this](const BrushEngine::DabRequest& dab) {
        const QPointF center = QPointF(dab.topLeft)
                              + QPointF(dab.dab.width(), dab.dab.height()) * 0.5;
        if (!mHasPrevDab) {
            // Krita 混合笔刷：首 dab 只记录采样基准（引擎侧已跳过起笔落点）
            mPrevDabCenter = center;
            mHasPrevDab = true;
            return;
        }
        const QPointF delta = mPrevDabCenter - center;
        mPrevDabCenter = center;
        // 速率 = 流量 ×（压感曲线后的）不透明度
        const qreal rate = qBound(0.0, mEngine.settings().flow * dab.opacity, 1.0);
        mScribbleArea->drawSmudgeDab(dab.dab, dab.topLeft, delta, rate,
                                     mSampleImage, mSampleOrigin);
    };
}
