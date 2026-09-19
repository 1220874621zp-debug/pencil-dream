/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2026 Pencil Dream contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "pantotool.h"

#include <QKeyEvent>
#include <QLineF>
#include <QPixmap>
#include <QSettings>

#include "bitmapimage.h"
#include "clipboardmanager.h"
#include "editor.h"
#include "keyframe.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "pointerevent.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "undoredomanager.h"
#include "viewmanager.h"

PantoTool::PantoTool(QObject* parent) : StrokeTool(parent)
{
    mPresetExtras.name = QStringLiteral("圆头笔");
    mEngine.setSettings(mPresetExtras);

    // 喷枪：静止悬停时按速率补 dab（与 BrushTool 同款）
    mAirbrushTimer.setInterval(16);
    connect(&mAirbrushTimer, &QTimer::timeout, this, [this]() {
        mEngine.airbrushTick(dabPainter());
    });
}

ToolType PantoTool::type() const
{
    return PANTO;
}

void PantoTool::loadSettings()
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
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::STABILIZATION_VALUE] = { StabilizationLevel::NONE, StabilizationLevel::STRONG, StabilizationLevel::STRONG };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);

    // 笔刷参数档（轻量 XML：图像笔尖/纹理不入档，同 BrushTool 约定）
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    const QString saved = brushOptions.value("BrushOptions/PANTO").toString();
    if (!saved.isEmpty()) {
        BrushSettings restored;
        if (BrushSettings::fromXMLString(saved, restored)) {
            restored.eraser = false;
            mPresetExtras = restored;
        }
    }

    // Panto 自有选项：取样源 / 偏移模式 / 偏移量
    mSourceMode = static_cast<SourceMode>(qBound(0, pencilSettings.value("PantoTool/SourceMode", 0).toInt(), 3));
    mOffsetMode = static_cast<OffsetMode>(qBound(0, pencilSettings.value("PantoTool/OffsetMode", 0).toInt(), 2));
    mOffset = QPointF(pencilSettings.value("PantoTool/OffsetX", 0.0).toDouble(),
                      pencilSettings.value("PantoTool/OffsetY", 0.0).toDouble());

    syncEngineSettings();
}

QCursor PantoTool::cursor()
{
    if (!mCursorBuilt)
    {
        mCursorBuilt = true;
        mCursorSvg = QCursor(QPixmap(":icons/general/cursor-brush.svg"), 4, 14);
        mCursorCross = QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
    }
    if (mAltHeld) {
        // Alt 按下 = 即将定义偏移/源点，十字更贴切
        return mCursorCross;
    }
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return mCursorSvg;
    }
    return mCursorCross;
}

bool PantoTool::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt)
    {
        // Panto 的 Alt = 定义偏移（拖动）/ 跟随源点（点击）。抢先消费，
        // 避免 StrokeTool 切出临时吸色（SmudgeTool 同款先例）
        mAltHeld = true;
        mScribbleArea->setCursor(cursor());
        return true;
    }
    return StrokeTool::keyPressEvent(event);
}

bool PantoTool::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt)
    {
        mAltHeld = false;
        mScribbleArea->setCursor(cursor());
        return true;
    }
    return StrokeTool::keyReleaseEvent(event);
}

void PantoTool::pointerPressEvent(PointerEvent* event)
{
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    // Alt+按下：定义偏移拖拽的起点（松手结算：拖了=偏移向量，没拖=跟随源点）
    if (event->modifiers() & Qt::AltModifier)
    {
        if (event->button() == Qt::LeftButton)
        {
            mDefiningOffset = true;
            mOffsetDragStart = getCurrentPoint();
        }
        StrokeTool::pointerPressEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton || !layer->isBitmapKind())
    {
        StrokeTool::pointerPressEvent(event);
        return;
    }

    mMouseDownPoint = getCurrentPoint();

    // 解析取样源必须先于 startStroke：源=当前帧时建键动作会替换覆盖帧，
    // 此刻读到的才是"下笔前"的画面
    if (!prepareCloneSource())
    {
        // 无可用源（如首帧没有上一帧、剪贴板为空）：不起笔，不建键不进撤销栈
        StrokeTool::pointerPressEvent(event);
        return;
    }

    startStroke(event->inputType());

    syncEngineSettings();
    if (mEngine.settings().mirrorX || mEngine.settings().mirrorY) {
        mEngine.setMirrorCenter(mEditor->view()->mapScreenToCanvas(
            QPointF(mScribbleArea->width(), mScribbleArea->height()) * 0.5));
    }
    // Clone 模式颜色由源图逐像素提供，笔色只是占位
    mEngine.beginStroke(getCurrentPoint(), mInterpolator.getPressure(),
                        Qt::black, dabPainter());
    if (mEngine.settings().airbrushEnabled) {
        mAirbrushTimer.start();
    }

    StrokeTool::pointerPressEvent(event);
}

void PantoTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (mDefiningOffset && event->buttons() & Qt::LeftButton)
    {
        // Alt+拖动实时结算偏移（面板滑杆跟手，采样未进行无副作用）
        mOffset = getCurrentPoint() - mOffsetDragStart;
        emit pantoOptionsChanged();
        StrokeTool::pointerMoveEvent(event);
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

void PantoTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (mDefiningOffset)
    {
        mDefiningOffset = false;
        if (QLineF(QPointF(), getCurrentPoint() - mOffsetDragStart).length() < 2.0)
        {
            // Alt+点击：设定跟随模式的源点锚
            mFollowAnchorValid = true;
            mFollowAnchor = mOffsetDragStart;
        }
        else
        {
            // Alt+拖动：偏移向量生效并切到固定偏移模式
            mOffsetMode = OffsetMode::FixedOffset;
            persistPantoOptions();
        }
        emit pantoOptionsChanged();
        StrokeTool::pointerReleaseEvent(event);
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

    endStroke();                       // 落层（瓦片缓冲 → 当前帧）
    mEngine.endStroke();
    mEngine.clearCloneSource();
    mAirbrushTimer.stop();

    StrokeTool::pointerReleaseEvent(event);
}

void PantoTool::paintAt(QPointF point)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->isBitmapKind())
    {
        syncEngineSettings();
        mCurrentWidth = mEngine.dabDiameterAt(mCurrentPressure);
        mEngine.dabAt(point, mCurrentPressure, dabPainter());
    }
}

void PantoTool::drawStroke()
{
    StrokeTool::drawStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->isBitmapKind())
    {
        syncEngineSettings();
        mCurrentWidth = mEngine.dabDiameterAt(mCurrentPressure);
        mEngine.strokeTo(getCurrentPoint(), mCurrentPressure, dabPainter());
    }
}

void PantoTool::setSourceMode(SourceMode mode)
{
    if (mSourceMode == mode) { return; }
    mSourceMode = mode;
    persistPantoOptions();
    emit pantoOptionsChanged();
}

void PantoTool::setOffsetMode(OffsetMode mode)
{
    if (mOffsetMode == mode) { return; }
    mOffsetMode = mode;
    if (mode == OffsetMode::Follow && !mFollowAnchorValid) {
        // 切到跟随时还没有锚点：以画布原点兜底，等 Alt+点击重新定
        mFollowAnchor = QPointF(0, 0);
        mFollowAnchorValid = true;
    }
    persistPantoOptions();
    emit pantoOptionsChanged();
}

void PantoTool::setCloneOffset(const QPointF& offset)
{
    mOffset = offset;
    persistPantoOptions();
    emit pantoOptionsChanged();
}

void PantoTool::resetOffset()
{
    mOffset = QPointF(0, 0);
    mOffsetMode = OffsetMode::InPlace;
    mFollowAnchorValid = false;
    persistPantoOptions();
    emit pantoOptionsChanged();
}

void PantoTool::applyBrushPreset(const BrushSettings& preset)
{
    mPresetExtras = preset;
    mPresetExtras.eraser = false;

    setWidth(preset.diameter);
    setFeather((1.0 - preset.hardness) * 100.0);
    setPressureEnabled(preset.pressureSize || preset.pressureOpacity);

    syncEngineSettings();
    persistUserOptions();
}

void PantoTool::initPresetExtras(const BrushSettings& preset)
{
    mPresetExtras = preset;
    mPresetExtras.eraser = false;
    syncEngineSettings();
}

BrushSettings PantoTool::currentBrushSettings()
{
    syncEngineSettings();
    BrushSettings settings = mEngine.settings();
    // 对外（存预设/选项面板）不含仿制语义：Clone 由本工具在起笔时强制注入
    settings.colorSource = BrushSettings::ColorSource::Plain;
    return settings;
}

BitmapImage* PantoTool::ghostSourceImage() const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || !layer->isBitmapKind()) { return nullptr; }
    auto* bitmapLayer = static_cast<LayerBitmap*>(layer);

    // 与灯桌（洋葱皮）同源：循环层先回绕到显示帧域，再按 ONION_TYPE 决定
    // "上一帧"的含义——absolute=上一个关键帧（跳过当前覆盖键，与
    // OnionskinSubPainter 绝对模式一致）；relative=逐帧 ±1 的覆盖画面
    const int frame = bitmapLayer->displayFrameFor(mEditor->currentFrame());
    const bool isAbsolute = mEditor->preference()->getString(SETTING::ONION_TYPE) == "absolute";
    if (isAbsolute)
    {
        if (mSourceMode == SourceMode::NextFrame)
        {
            const int f = bitmapLayer->getNextFrameNumber(frame, true);
            return (f >= 1) ? bitmapLayer->getBitmapImageAtFrame(f) : nullptr;
        }
        int f = bitmapLayer->getPreviousFrameNumber(frame, true);
        KeyFrame* currentKey = bitmapLayer->getLastKeyFrameAtPosition(frame);
        if (currentKey != nullptr && f == currentKey->pos())
        {
            f = bitmapLayer->getPreviousFrameNumber(f, true);
        }
        return (f >= 1) ? bitmapLayer->getBitmapImageAtFrame(f) : nullptr;
    }

    const int f = frame + (mSourceMode == SourceMode::NextFrame ? 1 : -1);
    if (f < 1) { return nullptr; }
    return bitmapLayer->getLastBitmapImageAtFrame(f);
}

bool PantoTool::prepareCloneSource()
{
    mEngine.clearCloneSource();

    QImage sample;
    QPoint origin(0, 0);

    switch (mSourceMode)
    {
    case SourceMode::PreviousFrame:
    case SourceMode::NextFrame: {
        BitmapImage* bmi = ghostSourceImage();
        if (bmi == nullptr || bmi->image() == nullptr || bmi->image()->isNull()) {
            return false;
        }
        // 深拷贝：源帧与编辑中的目标帧彻底隔离（自克隆读的也是下笔前画面）
        sample = *bmi->image();
        // bounds() 带 autoCrop 副作用，一笔只取一次（SmudgeTool 同款约定）
        origin = bmi->bounds().topLeft();
        break;
    }
    case SourceMode::CurrentFrame: {
        Layer* layer = mEditor->layers()->currentLayer();
        BitmapImage* bmi = (layer != nullptr && layer->isBitmapKind())
                               ? mScribbleArea->currentBitmapImage(layer) : nullptr;
        if (bmi == nullptr || bmi->image() == nullptr || bmi->image()->isNull()) {
            return false;
        }
        sample = *bmi->image();
        origin = bmi->bounds().topLeft();
        break;
    }
    case SourceMode::Clipboard: {
        // image() 非 const 接口，拷贝一份再取（QImage 隐式共享，代价可忽略）
        BitmapImage clip = mEditor->clipboards()->getBitmapClipboard();
        if (clip.image() == nullptr || clip.image()->isNull()) {
            return false;
        }
        sample = *clip.image();
        origin = clip.bounds().topLeft();
        break;
    }
    }

    QPointF offset = QPointF(0, 0);
    if (mOffsetMode == OffsetMode::FixedOffset) {
        offset = mOffset;
    } else if (mOffsetMode == OffsetMode::Follow && mFollowAnchorValid) {
        // PS 仿制图章语义：每笔以首 dab 落点对齐源点锚
        offset = getCurrentPoint() - mFollowAnchor;
    }

    mEngine.setCloneSource(sample, origin, offset);
    return true;
}

void PantoTool::syncEngineSettings()
{
    BrushSettings merged = mPresetExtras;
    merged.diameter = mSettings.width();
    merged.hardness = 1.0 - qBound(FEATHER_MIN, mSettings.feather(), FEATHER_MAX) / 100.0;
    merged.pressureSize = mPresetExtras.pressureSize && mSettings.pressureEnabled();
    merged.pressureOpacity = mPresetExtras.pressureOpacity && mSettings.pressureEnabled();
    // Panto 固有语义：颜色来自仿制源；橡皮化与双笔尖（纯 alpha 蒙版复合，
    // 会与逐像素源色打架）不参与
    merged.colorSource = BrushSettings::ColorSource::Clone;
    merged.eraser = false;
    merged.mask.enabled = false;
    merged.mask.sub.reset();
    mEngine.setSettings(merged);
}

void PantoTool::persistUserOptions()
{
    QSettings brushOptions(PENCIL2D, PENCIL2D);
    // 内嵌图像不入注册表（同 BrushTool 约定），重启回到关闭态
    BrushSettings lite = currentBrushSettings();
    lite.tipImage = QImage();
    lite.tipMask = QImage();
    if (lite.tipShape == BrushSettings::TipShape::Image) {
        lite.tipShape = BrushSettings::TipShape::Circle;
    }
    lite.texture = BrushTextureSettings();
    lite.colorSource = BrushSettings::ColorSource::Plain;
    lite.mask.enabled = false;
    lite.mask.sub.reset();
    brushOptions.setValue("BrushOptions/PANTO", lite.toXMLString());
}

void PantoTool::persistPantoOptions()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue("PantoTool/SourceMode", static_cast<int>(mSourceMode));
    settings.setValue("PantoTool/OffsetMode", static_cast<int>(mOffsetMode));
    settings.setValue("PantoTool/OffsetX", mOffset.x());
    settings.setValue("PantoTool/OffsetY", mOffset.y());
}

BrushEngine::DabPainter PantoTool::dabPainter() const
{
    return [this](const BrushEngine::DabRequest& dab) {
        DabPasteParams params;
        params.opacity = dab.opacity;
        params.flow = dab.flow;
        params.buildup = dab.buildup;
        params.blendMode = dab.blendMode;
        params.perPixelColor = dab.perPixelColor;
        mScribbleArea->drawDab(dab.dab, dab.topLeft, params);
    };
}
