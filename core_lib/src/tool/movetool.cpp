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

#include "movetool.h"

#include <QMessageBox>
#include <QSettings>
#include <QtMath>

#include "pointerevent.h"
#include "editor.h"
#include "toolmanager.h"
#include "strokeinterpolator.h"
#include "selectionmanager.h"
#include "overlaymanager.h"
#include "undoredomanager.h"
#include "undoredocommand.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "layercamera.h"
#include "layerbitmap.h"
#include "viewmanager.h"
#include "mathutils.h"

MoveTool::MoveTool(QObject* parent) : TransformTool(parent)
{
}

ToolType MoveTool::type() const
{
    return MOVE;
}

void MoveTool::loadSettings()
{
    mRotationIncrement = mEditor->preference()->getInt(SETTING::ROTATION_INCREMENT);
    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    mPropertyUsed[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::ANTI_ALIASING_ENABLED] = { Layer::BITMAP };
    QHash<int, PropertyInfo> info;

    info[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = false;
    info[TransformToolProperties::ANTI_ALIASING_ENABLED] = true;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(TransformToolProperties::SHOWSELECTIONINFO_ENABLED, pencilSettings.value("ShowSelectionInfo", false).toBool());
        toolProperties().setBaseValue(TransformToolProperties::ANTI_ALIASING_ENABLED, pencilSettings.value("moveAA", true).toBool());
    }

    connect(mEditor->preference(), &PreferenceManager::optionChanged, this, &MoveTool::updateSettings);
}

QCursor MoveTool::cursor()
{
    // 穿透模式：拖拽/悬停幽灵或角柄时给专属光标，未命中则落回常规（选区/透视）
    if (xrayApplicable())
    {
        if (mXrayDragging)
        {
            if (mXrayDragMode == XrayDragMode::Scale)
            {
                return QCursor((mXrayHoverHandle == 0 || mXrayHoverHandle == 2)
                               ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
            }
            return QCursor(Qt::SizeAllCursor);
        }
        if (mXrayHoverHandle == 0 || mXrayHoverHandle == 2) { return QCursor(Qt::SizeFDiagCursor); }
        if (mXrayHoverHandle == 1 || mXrayHoverHandle == 3) { return QCursor(Qt::SizeBDiagCursor); }
        if (mXrayHoverFrame >= 1) { return QCursor(Qt::SizeAllCursor); }
    }

    MoveMode mode = MoveMode::NONE;
    SelectionManager* selectMan = mEditor->select();
    if (selectMan->somethingSelected())
    {
        mode = mEditor->select()->getMoveMode();
    }
    else if (mEditor->overlays()->anyOverlayEnabled())
    {
        mode = mPerspMode;
    }

    return cursor(mode);
}

void MoveTool::updateSettings(const SETTING setting)
{
    switch (setting)
    {
    case SETTING::ROTATION_INCREMENT:
        mRotationIncrement = mEditor->preference()->getInt(SETTING::ROTATION_INCREMENT);
        break;
    case SETTING::OVERLAY_PERSPECTIVE1:
        mEditor->overlays()->settingsUpdated(setting, mEditor->preference()->isOn(setting));
        break;
    case SETTING::OVERLAY_PERSPECTIVE2:
        mEditor->overlays()->settingsUpdated(setting, mEditor->preference()->isOn(setting));
        break;
    case SETTING::OVERLAY_PERSPECTIVE3:
        mEditor->overlays()->settingsUpdated(setting, mEditor->preference()->isOn(setting));
        break;
    default:
        break;
    }
}

void MoveTool::pointerPressEvent(PointerEvent* event)
{
    Layer* currentLayer = currentPaintableLayer();
    if (currentLayer == nullptr) return;

    // 穿透模式：幽灵命中优先（角柄缩放 / alpha 命中平移）；
    // 未命中则清穿透选中后落回常规选区/透视行为
    if (xrayApplicable() && event->button() == Qt::LeftButton)
    {
        if (xrayBeginInteraction(event->canvasPos()))
        {
            mEditor->updateFrame();
            return;
        }
        if (mScribbleArea->xrayStateRef().selectedFrame != -1)
        {
            mScribbleArea->xrayStateRef().selectedFrame = -1;
            mScribbleArea->invalidateXrayVisual();
        }
    }

    if (mEditor->select()->somethingSelected())
    {
        beginInteraction(event->canvasPos(), event->modifiers(), currentLayer);
    }
    else if (mEditor->overlays()->anyOverlayEnabled())
    {
        LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
        Q_ASSERT(layerCam);

        mPerspMode = mEditor->overlays()->getMoveModeForPoint(event->canvasPos(), layerCam->getViewAtFrame(mEditor->currentFrame()));
        mEditor->overlays()->setMoveMode(mPerspMode);
        QPoint mapped = layerCam->getViewAtFrame(mEditor->currentFrame()).map(event->canvasPos()).toPoint();
        mEditor->overlays()->updatePerspective(mapped);
    }

    mEditor->updateFrame();
}

void MoveTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* currentLayer = currentPaintableLayer();
    if (currentLayer == nullptr) return;

    if (mXrayDragging)
    {
        xrayUpdateDrag(event->canvasPos());
        mEditor->updateFrame();
        return;
    }

    if (mScribbleArea->isPointerInUse())   // the user is also pressing the mouse (dragging)
    {
        transformSelection(event->canvasPos(), event->modifiers());

        if (mEditor->overlays()->anyOverlayEnabled())
        {
            LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
            Q_ASSERT(layerCam);
            mEditor->overlays()->updatePerspective(layerCam->getViewAtFrame(mEditor->currentFrame()).map(event->canvasPos()));
        }
        if (mEditor->select()->somethingSelected())
        {
            transformSelection(event->canvasPos(), event->modifiers());
        }
    }
    else
    {
        // the user is moving the mouse without pressing it
        // update cursor to reflect selection corner interaction
        if (xrayApplicable())
        {
            mXrayHoverFrame = xrayHitGhost(event->canvasPos());
            mXrayHoverHandle = xrayHitScaleHandle(event->canvasPos());
        }
        else if (mXrayHoverFrame != -1 || mXrayHoverHandle != -1)
        {
            mXrayHoverFrame = -1;
            mXrayHoverHandle = -1;
        }
        mEditor->select()->setMoveModeForAnchorInRange(event->canvasPos());
        if (mEditor->overlays()->anyOverlayEnabled())
        {
            LayerCamera *layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
            Q_ASSERT(layerCam);
            mPerspMode = mEditor->overlays()->getMoveModeForPoint(event->canvasPos(), layerCam->getViewAtFrame(mEditor->currentFrame()));
        }
        mScribbleArea->updateToolCursor();
    }
    mEditor->updateFrame();
}

void MoveTool::pointerReleaseEvent(PointerEvent*)
{
    if (mXrayDragging)
    {
        xrayCommitDrag();
        mXrayHoverFrame = -1;
        mXrayHoverHandle = -1;
        mScribbleArea->updateToolCursor();
        return;
    }

    mEditor->undoRedo()->record(mUndoSaveStateId, typeName());

    if (mEditor->overlays()->anyOverlayEnabled())
    {
        mEditor->overlays()->setMoveMode(MoveMode::NONE);
        mPerspMode = MoveMode::NONE;
    }

    auto selectMan = mEditor->select();
    if (!selectMan->somethingSelected())
        return;

    mScribbleArea->updateToolCursor();
    emit mEditor->frameModified(mEditor->currentFrame());
}

void MoveTool::transformSelection(const QPointF& pos, Qt::KeyboardModifiers keyMod)
{
    auto selectMan = mEditor->select();
    if (selectMan->somethingSelected())
    {
        int rotationIncrement = 0;
        if (selectMan->getMoveMode() == MoveMode::ROTATION && keyMod & Qt::ShiftModifier)
        {
            rotationIncrement = mRotationIncrement;
        }

        selectMan->maintainAspectRatio(keyMod == Qt::ShiftModifier);
        selectMan->alignPositionToAxis(keyMod == Qt::ShiftModifier);

        qreal newAngle = 0;
        if (selectMan->getMoveMode() == MoveMode::ROTATION) {
            QPointF anchorPoint = selectMan->currentTransformAnchor();
            newAngle = selectMan->angleFromPoint(pos, anchorPoint) - mRotatedAngle;
        }

        selectMan->adjustSelection(pos, mOffset, newAngle, rotationIncrement);
    }
    else // there is nothing selected
    {
        selectMan->setMoveMode(MoveMode::NONE);
    }
}

void MoveTool::beginInteraction(const QPointF& pos, Qt::KeyboardModifiers keyMod, Layer* layer)
{
    auto selectMan = mEditor->select();
    QRectF selectionRect = selectMan->mySelectionRect();
    if (!selectionRect.isNull())
    {
        mUndoSaveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
        mEditor->backup(typeName());
    }

    if (keyMod != Qt::ShiftModifier)
    {
        if (selectMan->isOutsideSelectionArea(pos))
        {
            applyTransformation();
            mEditor->deselectAll();
        }
    }

    if (selectMan->getMoveMode() == MoveMode::MIDDLE)
    {
        if (keyMod == Qt::ControlModifier) // --- rotation
        {
            selectMan->setMoveMode(MoveMode::ROTATION);
        }
    }

    selectMan->setTransformAnchor(selectMan->getSelectionAnchorPoint());
    selectMan->setDragOrigin(pos);
    mOffset = selectMan->myTranslation();

    if(selectMan->getMoveMode() == MoveMode::ROTATION) {
        mRotatedAngle = selectMan->angleFromPoint(pos, selectMan->currentTransformAnchor()) - selectMan->myRotation();
    }
}

void MoveTool::applyTransformation()
{
    SelectionManager* selectMan = mEditor->select();
    mScribbleArea->applyTransformedSelection();

    // When the selection has been applied, a new rect is applied based on the bounding box.
    // This ensures that if the selection has been rotated, it will still fit the bounds of the image.
    selectMan->setSelection(selectMan->mapToSelection(QPolygonF(selectMan->mySelectionRect())).boundingRect());
    mRotatedAngle = 0;
}

void MoveTool::applyTransformationAndDeselect()
{
    // We apply transform changes upon leaving a layer and deselect all
    mScribbleArea->applyTransformedSelection();

    mEditor->select()->resetSelectionProperties();
    mRotatedAngle = 0;
}

bool MoveTool::leavingThisTool()
{
    TransformTool::leavingThisTool();

    // 穿透拖拽中途切工具：放弃预览不入撤销（未提交即未改像素）
    if (mXrayDragging) { xrayCancelDrag(); }

    if (currentPaintableLayer())
    {
        applyTransformation();
    }

    return true;
}

bool MoveTool::isActive() const {
    return mXrayDragging ||
           (mScribbleArea->isPointerInUse() &&
           (mEditor->select()->somethingSelected() || mEditor->overlays()->getMoveMode() != MoveMode::NONE));
}

Layer* MoveTool::currentPaintableLayer()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
        return nullptr;
    if (!layer->isPaintable())
        return nullptr;
    return layer;
}

QCursor MoveTool::cursor(MoveMode mode) const
{
    QPixmap cursorPixmap = QPixmap(24, 24);

    cursorPixmap.fill(QColor(255, 255, 255, 0));
    QPainter cursorPainter(&cursorPixmap);
    cursorPainter.setRenderHint(QPainter::Antialiasing);

    switch(mode)
    {
    case MoveMode::PERSP_LEFT:
    case MoveMode::PERSP_RIGHT:
    case MoveMode::PERSP_MIDDLE:
    case MoveMode::PERSP_SINGLE:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-move.svg"));
        break;
    }
    case MoveMode::TOPLEFT:
    case MoveMode::BOTTOMRIGHT:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-diagonal-left.svg"));
        break;
    }
    case MoveMode::TOPRIGHT:
    case MoveMode::BOTTOMLEFT:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-diagonal-right.svg"));
        break;
    }
    case MoveMode::ROTATIONLEFT:
    case MoveMode::ROTATIONRIGHT:
    case MoveMode::ROTATION:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-rotate.svg"));
        break;
    }
    case MoveMode::MIDDLE:
    case MoveMode::CENTER:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-move.svg"));
        break;
    }
    default:
        return Qt::ArrowCursor;
    }
    cursorPainter.end();

    return QCursor(cursorPixmap);
}

// --- 穿透模式 -----------------------------------------------------------------

bool MoveTool::xrayApplicable() const
{
    if (!mScribbleArea->xrayMode()) { return false; }
    Layer* layer = mEditor->layers()->currentLayer();
    return layer != nullptr && layer->type() == Layer::BITMAP && layer->isPaintable();
}

bool MoveTool::xrayAlphaHit(BitmapImage* image, const QPointF& canvasPos) const
{
    if (image == nullptr || image->image() == nullptr) { return false; }
    const QImage& img = *image->image();

    QPointF local = canvasPos - QPointF(image->topLeft());

    // 细线在低缩放下几乎点不中，取 7x7 容差窗口内任意非透明像素（洋葱对位同款）
    const int cx = qFloor(local.x());
    const int cy = qFloor(local.y());
    for (int dy = -3; dy <= 3; dy++)
    {
        for (int dx = -3; dx <= 3; dx++)
        {
            const int x = cx + dx;
            const int y = cy + dy;
            if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) { continue; }
            if (qAlpha(img.pixel(x, y)) > 8) { return true; }
        }
    }
    return false;
}

int MoveTool::xrayHitGhost(const QPointF& pos) const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::BITMAP) { return -1; }
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);

    // 当前显示帧覆盖的关键帧已作为正式内容绘制，不参与幽灵命中
    const KeyFrame* coveringKey = bitmapLayer->getKeyFrameWhichCovers(
        bitmapLayer->displayFrameFor(mEditor->currentFrame()));
    const int skipPos = (coveringKey != nullptr) ? coveringKey->pos() : -1;

    // 后帧号后画在上层，从顶往下测（与绘制顺序一致）
    for (int k = bitmapLayer->getMaxKeyFramePosition(); k >= bitmapLayer->firstKeyFramePosition(); k--)
    {
        if (!bitmapLayer->keyExists(k) || k == skipPos) { continue; }
        BitmapImage* img = bitmapLayer->getBitmapImageAtFrame(k);
        if (img == nullptr) { continue; }
        img->loadFile();
        if (xrayAlphaHit(img, pos)) { return k; }
    }
    return -1;
}

int MoveTool::xrayHitScaleHandle(const QPointF& pos) const
{
    const XrayVisualState& xs = mScribbleArea->xrayStateRef();
    if (xs.selectedFrame < 1) { return -1; }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::BITMAP) { return -1; }
    BitmapImage* img = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(xs.selectedFrame);
    if (img == nullptr) { return -1; }

    const QTransform t = (mXrayDragging && mXrayTargetFrame == xs.selectedFrame)
                             ? xrayCurrentTransform() : QTransform();
    const QPolygonF box = t.map(QPolygonF(QRectF(img->bounds())));

    // 与选区锚点同级容差（10 设备像素折算画布）
    const qreal tol = qMax<qreal>(4.0, 10.0 / qMax<qreal>(0.01, mEditor->view()->scaling()));
    for (int i = 0; i < 4; ++i)
    {
        if (QLineF(box.at(i), pos).length() <= tol) { return i; }
    }
    return -1;
}

bool MoveTool::xrayBeginInteraction(const QPointF& pos)
{
    XrayVisualState& xs = mScribbleArea->xrayStateRef();
    Layer* layer = mEditor->layers()->currentLayer();
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);

    // 1) 已选中帧的角柄 → 缩放（对角为不动锚点）
    const int corner = xrayHitScaleHandle(pos);
    if (corner >= 0)
    {
        BitmapImage* img = bitmapLayer->getBitmapImageAtFrame(xs.selectedFrame);
        if (img != nullptr)
        {
            const QRectF b = QRectF(img->bounds());
            const QPointF corners[4] = { b.topLeft(), b.topRight(), b.bottomRight(), b.bottomLeft() };

            mXrayDragging = true;
            mXrayDragMode = XrayDragMode::Scale;
            mXrayTargetFrame = xs.selectedFrame;
            mXrayUndoSnapshot = *img;
            mXrayScaleX = mXrayScaleY = 1.0;
            mXrayScaleAnchor = corners[(corner + 2) % 4];
            mXrayDragTranslation = QPointF();
            mXrayPressPos = pos;
            return true;
        }
        xs.selectedFrame = -1;
        return false;
    }

    // 2) alpha 命中幽灵 → 平移
    const int hit = xrayHitGhost(pos);
    if (hit < 1) { return false; }
    BitmapImage* img = bitmapLayer->getBitmapImageAtFrame(hit);
    if (img == nullptr) { return false; }

    mXrayDragging = true;
    mXrayDragMode = XrayDragMode::Move;
    mXrayTargetFrame = hit;
    mXrayUndoSnapshot = *img;
    mXrayDragTranslation = QPointF();
    mXrayScaleX = mXrayScaleY = 1.0;
    mXrayScaleAnchor = QPointF();
    mXrayPressPos = pos;
    xs.selectedFrame = hit;
    mScribbleArea->invalidateXrayVisual(); // 提亮新选中帧
    return true;
}

QTransform MoveTool::xrayCurrentTransform() const
{
    QTransform t;
    if (mXrayDragMode == XrayDragMode::Scale)
    {
        t.translate(mXrayScaleAnchor.x(), mXrayScaleAnchor.y());
        t.scale(mXrayScaleX, mXrayScaleY);
        t.translate(-mXrayScaleAnchor.x(), -mXrayScaleAnchor.y());
    }
    else
    {
        t.translate(mXrayDragTranslation.x(), mXrayDragTranslation.y());
    }
    return t;
}

void MoveTool::xrayUpdateDrag(const QPointF& pos)
{
    if (!mXrayDragging) { return; }
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    XrayVisualState& xs = mScribbleArea->xrayStateRef();
    XrayDragPreview& p = xs.drag;
    p.active = true;
    p.layerId = layer->id();
    p.framePos = mXrayTargetFrame;

    if (mXrayDragMode == XrayDragMode::Move)
    {
        mXrayDragTranslation = pos - mXrayPressPos;
        p.translation = mXrayDragTranslation;
        p.scaleX = p.scaleY = 1.0;
    }
    else
    {
        // 投影法：分量 = |当前-锚| / |起点-锚|；起点该轴分量≈0 则保持 1（防 0 除）
        const qreal dx0 = mXrayPressPos.x() - mXrayScaleAnchor.x();
        const qreal dy0 = mXrayPressPos.y() - mXrayScaleAnchor.y();
        const qreal dx1 = pos.x() - mXrayScaleAnchor.x();
        const qreal dy1 = pos.y() - mXrayScaleAnchor.y();
        if (!qFuzzyIsNull(dx0)) { mXrayScaleX = qBound(0.05, dx1 / dx0, 20.0); }
        if (!qFuzzyIsNull(dy0)) { mXrayScaleY = qBound(0.05, dy1 / dy0, 20.0); }
        p.translation = QPointF();
        p.scaleX = mXrayScaleX;
        p.scaleY = mXrayScaleY;
        p.scaleAnchor = mXrayScaleAnchor;
    }
    mScribbleArea->invalidateXrayVisual();
}

void MoveTool::xrayCommitDrag()
{
    mXrayDragging = false;
    mXrayDragMode = XrayDragMode::None;
    XrayVisualState& xs = mScribbleArea->xrayStateRef();

    Layer* layer = mEditor->layers()->currentLayer();
    BitmapImage* img = (layer != nullptr && layer->type() == Layer::BITMAP)
                           ? static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(mXrayTargetFrame)
                           : nullptr;

    if (img == nullptr)
    {
        xs.drag = XrayDragPreview();
        mScribbleArea->invalidateXrayVisual();
        return;
    }

    const QTransform t = xrayCurrentTransform();
    if (!t.isIdentity())
    {
        const bool useAA = toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();
        BitmapImage transformedImage = img->transformed(img->bounds(), t, useAA);
        img->clear();
        img->paste(&transformedImage, QPainter::CompositionMode_SourceOver);

        // 撤销：显式双快照（操作任意关键帧，不经"当前帧"快照链）
        BitmapImage redoSnapshot = *img;
        mEditor->undoRedo()->pushUndoCommand(
            new BitmapReplaceCommand(&mXrayUndoSnapshot, &redoSnapshot, layer->id(),
                                     tr("穿透模式：变换帧图像"), mEditor));
        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mXrayTargetFrame);
    }

    xs.drag = XrayDragPreview();
    mScribbleArea->invalidateXrayVisual();
}

void MoveTool::xrayCancelDrag()
{
    mXrayDragging = false;
    mXrayDragMode = XrayDragMode::None;
    mScribbleArea->xrayStateRef().drag = XrayDragPreview();
    mScribbleArea->invalidateXrayVisual();
}

void MoveTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect)

    if (!xrayApplicable()) { return; }
    const XrayVisualState& xs = mScribbleArea->xrayStateRef();
    if (xs.selectedFrame < 1) { return; }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::BITMAP) { return; }
    BitmapImage* img = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(xs.selectedFrame);
    if (img == nullptr || img->image() == nullptr || img->image()->isNull()) { return; }

    painter.save();
    painter.setTransform(mEditor->view()->getView());
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 预览包围盒（拖拽中含预览变换；角序 0..3 = TL/TR/BR/BL）
    const QTransform t = (mXrayDragging && mXrayTargetFrame == xs.selectedFrame)
                             ? xrayCurrentTransform() : QTransform();
    const QPolygonF box = t.map(QPolygonF(QRectF(img->bounds())));

    QPen boxPen(QColor(0xE8, 0x38, 0x5A, 230), 1.2);
    boxPen.setCosmetic(true);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(boxPen);
    painter.drawPolygon(box);

    const qreal viewScale = qMax<qreal>(0.01, mEditor->view()->scaling());
    const qreal half = qBound(2.5, 5.0 / viewScale, 8.0);
    for (int i = 0; i < 4; ++i)
    {
        const QPointF hp = box.at(i);
        const bool hot = mXrayHoverHandle == i
                         || (mXrayDragging && mXrayDragMode == XrayDragMode::Scale);
        painter.setPen(hot ? QPen(Qt::white, 1.5) : QPen(QColor(40, 40, 40), 1.0));
        painter.setBrush(QBrush(QColor(0xE8, 0x38, 0x5A)));
        painter.drawRect(QRectF(hp.x() - half, hp.y() - half, half * 2, half * 2));
    }
    painter.restore();
}
