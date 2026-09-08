/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Pencil2D contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "onionaligntool.h"

#include <QtMath>
#include <QTransform>
#include <QPainter>
#include <QPixmap>

#include "pointerevent.h"
#include "editor.h"
#include "layermanager.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "layer.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "keyframe.h"

OnionAlignTool::OnionAlignTool(QObject* parent) : BaseTool(parent)
{
}

void OnionAlignTool::loadSettings()
{
    // 显示辅助工具，无可配置项
}

QCursor OnionAlignTool::rotateCursor()
{
    static const QCursor cursor = [] {
        QPixmap pixmap = QPixmap(24, 24);
        pixmap.fill(QColor(255, 255, 255, 0));
        QPainter painter(&pixmap);
        painter.drawImage(QPoint(6, 6), QImage("://icons/general/cursor-rotate.svg"));
        painter.end();
        return QCursor(pixmap);
    }();
    return cursor;
}

QCursor OnionAlignTool::cursor()
{
    if (mDragSide == GhostSide::NONE && mHoverSide == GhostSide::NONE)
    {
        return QCursor(Qt::ArrowCursor);
    }
    if (mDragSide != GhostSide::NONE)
    {
        switch (mDragMode)
        {
        case DragMode::Rotate: return rotateCursor();
        case DragMode::Scale:  return QCursor(Qt::SizeVerCursor);
        default:               return QCursor(Qt::SizeAllCursor);
        }
    }
    // 悬停：按修饰键预览将触发的操作（可拖交互必须配 hover 光标预览）
    if (mHoverModifiers & Qt::ControlModifier) { return rotateCursor(); }
    if (mHoverModifiers & Qt::ShiftModifier)   { return QCursor(Qt::SizeVerCursor); }
    return QCursor(Qt::SizeAllCursor);
}

LayerBitmap* OnionAlignTool::currentBitmapLayer() const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::BITMAP)
    {
        return nullptr;
    }
    return static_cast<LayerBitmap*>(layer);
}

bool OnionAlignTool::onionIsAbsolute() const
{
    return mEditor->preference()->getString(SETTING::ONION_TYPE) == "absolute";
}

bool OnionAlignTool::alphaHit(BitmapImage* image, const QPointF& canvasPos, const OnionGhostTransform& transform) const
{
    if (image == nullptr || image->image() == nullptr) { return false; }
    const QImage& img = *image->image();

    // 画布点 → 幽灵图像本地坐标：先抵消幽灵变换（与 CanvasPainter 绘制矩阵互逆）
    QPointF local = canvasPos;
    if (!transform.isPlainTranslation())
    {
        const QPointF c = image->bounds().center();
        QTransform m;
        m.translate(c.x() + transform.offset.x(), c.y() + transform.offset.y());
        m.rotate(transform.rotation);
        m.scale(transform.scale, transform.scale);
        m.translate(-c.x(), -c.y());
        local = m.inverted().map(canvasPos);
    }
    else
    {
        local -= transform.offset;
    }
    local -= QPointF(image->topLeft());

    // 细线在低缩放下几乎点不中，取 7x7 容差窗口内任意非透明像素
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

int OnionAlignTool::visiblePrevGhostFrame(LayerBitmap* layer, int frame) const
{
    const bool isAbsolute = onionIsAbsolute();
    const int steps = qMax(1, mEditor->preference()->getInt(SETTING::ONION_PREV_FRAMES_NUM));

    int f = layer->getPreviousFrameNumber(frame, isAbsolute);
    if (isAbsolute)
    {
        // 绝对模式不画当前覆盖关键帧的幽灵（不消耗步数，与 OnionskinSubPainter 一致）
        KeyFrame* currentKey = layer->getLastKeyFrameAtPosition(frame);
        if (currentKey != nullptr && f == currentKey->pos())
        {
            f = layer->getPreviousFrameNumber(f, true);
        }
    }
    for (int i = 0; i < steps && f >= 1; i++)
    {
        if (layer->getBitmapImageAtFrame(f) != nullptr) { return f; }
        f = layer->getPreviousFrameNumber(f, isAbsolute);
    }
    return -1;
}

int OnionAlignTool::visibleNextGhostFrame(LayerBitmap* layer, int frame) const
{
    const bool isAbsolute = onionIsAbsolute();
    const int steps = qMax(1, mEditor->preference()->getInt(SETTING::ONION_NEXT_FRAMES_NUM));

    int f = layer->getNextFrameNumber(frame, isAbsolute);
    for (int i = 0; i < steps && f >= 1; i++)
    {
        if (layer->getBitmapImageAtFrame(f) != nullptr) { return f; }
        f = layer->getNextFrameNumber(f, isAbsolute);
    }
    return -1;
}

bool OnionAlignTool::hitTestGhost(const QPointF& canvasPos, GhostSide& sideOut, int& frameOut) const
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return false; }

    const int prevNum = visiblePrevGhostFrame(layer, mEditor->currentFrame());
    const int nextNum = visibleNextGhostFrame(layer, mEditor->currentFrame());

    // 变换后的实际显示位置参与命中
    const OnionGhostOffsetMap& ghosts = mScribbleArea->onionGhostOffsets();
    auto it = ghosts.constFind(layer->id());
    const OnionGhostOffset ghost = (it != ghosts.constEnd()) ? it.value() : OnionGhostOffset();
    const OnionGhostTransform prevT = (prevNum == ghost.prevFrameNumber) ? ghost.prev : OnionGhostTransform();
    const OnionGhostTransform nextT = (nextNum == ghost.nextFrameNumber) ? ghost.next : OnionGhostTransform();

    // next 在 prev 之上绘制，先测 next
    if (nextNum >= 1 && alphaHit(layer->getBitmapImageAtFrame(nextNum), canvasPos, nextT))
    {
        sideOut = GhostSide::NEXT;
        frameOut = nextNum;
        return true;
    }
    if (prevNum >= 1 && alphaHit(layer->getBitmapImageAtFrame(prevNum), canvasPos, prevT))
    {
        sideOut = GhostSide::PREV;
        frameOut = prevNum;
        return true;
    }
    return false;
}

void OnionAlignTool::pointerPressEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }

    GhostSide side = GhostSide::NONE;
    int ghostFrame = -1;
    const bool hit = hitTestGhost(event->canvasPos(), side, ghostFrame);

    if (event->modifiers() & Qt::AltModifier)
    {
        // Alt+点击：命中侧归零，空白处清空整层
        if (hit)
        {
            resetGhostOffset(side);
        }
        else
        {
            resetAllGhostOffsets();
        }
        return;
    }

    if (!hit) { return; }

    mDragSide = side;
    mDragGhostFrame = ghostFrame;
    mLastCanvasPos = event->canvasPos();

    // Ctrl 优先于 Shift：Ctrl+Shift 同按时按旋转起步（旋转是唯一可吸附的模式）
    if (event->modifiers() & Qt::ControlModifier)
    {
        mDragMode = DragMode::Rotate;
    }
    else if (event->modifiers() & Qt::ShiftModifier)
    {
        mDragMode = DragMode::Scale;
    }
    else
    {
        mDragMode = DragMode::Move;
    }

    if (LayerBitmap* layer = currentBitmapLayer())
    {
        if (BitmapImage* img = layer->getBitmapImageAtFrame(mDragGhostFrame))
        {
            const OnionGhostTransform& tr = (mDragSide == GhostSide::PREV) ? ghostOf(layer).prev : ghostOf(layer).next;
            const QPointF anchor = ghostAnchor(img, tr);
            const QLineF line(anchor, event->canvasPos());
            mLastDragAngle = qRadiansToDegrees(qAtan2(line.dy(), line.dx()));
            mLastDragDist = qMax<qreal>(1.0, line.length());
        }
    }
    mScribbleArea->updateToolCursor();
}

void OnionAlignTool::pointerMoveEvent(PointerEvent* event)
{
    if (mDragSide != GhostSide::NONE)
    {
        switch (mDragMode)
        {
        case DragMode::Move:
            applyOffsetDelta(event->canvasPos() - mLastCanvasPos);
            break;
        case DragMode::Rotate:
        {
            LayerBitmap* layer = currentBitmapLayer();
            BitmapImage* img = (layer != nullptr) ? layer->getBitmapImageAtFrame(mDragGhostFrame) : nullptr;
            if (img != nullptr)
            {
                const OnionGhostTransform& tr = (mDragSide == GhostSide::PREV) ? ghostOf(layer).prev : ghostOf(layer).next;
                const QLineF line(ghostAnchor(img, tr), event->canvasPos());
                const qreal angle = qRadiansToDegrees(qAtan2(line.dy(), line.dx()));
                qreal delta = angle - mLastDragAngle;
                while (delta > 180.0) { delta -= 360.0; }
                while (delta <= -180.0) { delta += 360.0; }
                applyRotationDelta(delta, event->modifiers() & Qt::ShiftModifier);
                mLastDragAngle = angle;
            }
            break;
        }
        case DragMode::Scale:
        {
            LayerBitmap* layer = currentBitmapLayer();
            BitmapImage* img = (layer != nullptr) ? layer->getBitmapImageAtFrame(mDragGhostFrame) : nullptr;
            if (img != nullptr)
            {
                const OnionGhostTransform& tr = (mDragSide == GhostSide::PREV) ? ghostOf(layer).prev : ghostOf(layer).next;
                const qreal dist = qMax<qreal>(1.0, QLineF(ghostAnchor(img, tr), event->canvasPos()).length());
                applyScaleFactor(dist / mLastDragDist);
                mLastDragDist = dist;
            }
            break;
        }
        }
        mLastCanvasPos = event->canvasPos();
        return;
    }

    // 未按下：更新 hover 命中态与修饰键，驱动光标预览
    GhostSide side = GhostSide::NONE;
    int ghostFrame = -1;
    hitTestGhost(event->canvasPos(), side, ghostFrame);
    if (side != mHoverSide || event->modifiers() != mHoverModifiers)
    {
        mHoverSide = side;
        mHoverModifiers = event->modifiers();
        mScribbleArea->updateToolCursor();
    }
}

void OnionAlignTool::pointerReleaseEvent(PointerEvent* event)
{
    Q_UNUSED(event)
    if (mDragSide != GhostSide::NONE)
    {
        mDragSide = GhostSide::NONE;
        mDragMode = DragMode::Move;
        mScribbleArea->updateToolCursor();
    }
}

void OnionAlignTool::pointerDoubleClickEvent(PointerEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }
    autoAlignCenters();
}

bool OnionAlignTool::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    if (mHoverSide != GhostSide::NONE || mHoverModifiers != Qt::NoModifier)
    {
        mHoverSide = GhostSide::NONE;
        mHoverModifiers = Qt::NoModifier;
        mScribbleArea->updateToolCursor();
    }
    return true;
}

QPointF OnionAlignTool::ghostAnchor(BitmapImage* image, const OnionGhostTransform& transform) const
{
    return QPointF(image->bounds().center()) + transform.offset;
}

OnionGhostOffset& OnionAlignTool::ghostOf(LayerBitmap* layer)
{
    return mScribbleArea->onionGhostOffsetRef(layer->id());
}

void OnionAlignTool::applyOffsetDelta(const QPointF& delta)
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return; }

    OnionGhostOffset& ghost = ghostOf(layer);
    if (mDragSide == GhostSide::PREV)
    {
        ghost.prevFrameNumber = mDragGhostFrame;
        ghost.prev.offset += delta;
    }
    else if (mDragSide == GhostSide::NEXT)
    {
        ghost.nextFrameNumber = mDragGhostFrame;
        ghost.next.offset += delta;
    }
    mScribbleArea->invalidateOnionGhostVisual();
}

void OnionAlignTool::applyRotationDelta(qreal deltaDegrees, bool snapToIncrement)
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return; }

    OnionGhostOffset& ghost = ghostOf(layer);
    OnionGhostTransform& tr = (mDragSide == GhostSide::PREV) ? ghost.prev : ghost.next;
    ghost.prevFrameNumber = (mDragSide == GhostSide::PREV) ? mDragGhostFrame : ghost.prevFrameNumber;
    ghost.nextFrameNumber = (mDragSide == GhostSide::NEXT) ? mDragGhostFrame : ghost.nextFrameNumber;

    tr.rotation += deltaDegrees;
    if (snapToIncrement)
    {
        const qreal inc = qMax<qreal>(1.0, mEditor->preference()->getInt(SETTING::ROTATION_INCREMENT));
        tr.rotation = qRound(tr.rotation / inc) * inc;
    }
    mScribbleArea->invalidateOnionGhostVisual();
}

void OnionAlignTool::applyScaleFactor(qreal factor)
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr || factor <= 0.0) { return; }

    OnionGhostOffset& ghost = ghostOf(layer);
    OnionGhostTransform& tr = (mDragSide == GhostSide::PREV) ? ghost.prev : ghost.next;
    ghost.prevFrameNumber = (mDragSide == GhostSide::PREV) ? mDragGhostFrame : ghost.prevFrameNumber;
    ghost.nextFrameNumber = (mDragSide == GhostSide::NEXT) ? mDragGhostFrame : ghost.nextFrameNumber;

    tr.scale = qBound(0.05, tr.scale * factor, 20.0);
    mScribbleArea->invalidateOnionGhostVisual();
}

void OnionAlignTool::resetGhostOffset(GhostSide side)
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return; }

    OnionGhostOffset& ghost = ghostOf(layer);
    if (side == GhostSide::PREV)
    {
        ghost.prev = OnionGhostTransform();
    }
    else if (side == GhostSide::NEXT)
    {
        ghost.next = OnionGhostTransform();
    }
    mScribbleArea->invalidateOnionGhostVisual();
}

void OnionAlignTool::resetPrevGhostOffset()
{
    resetGhostOffset(GhostSide::PREV);
}

void OnionAlignTool::resetNextGhostOffset()
{
    resetGhostOffset(GhostSide::NEXT);
}

void OnionAlignTool::resetAllGhostOffsets()
{
    if (LayerBitmap* layer = currentBitmapLayer())
    {
        mScribbleArea->clearOnionGhostOffsets(layer->id());
        mScribbleArea->invalidateOnionGhostVisual();
    }
}

void OnionAlignTool::autoAlignCenters()
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return; }

    const int frame = mEditor->currentFrame();
    const int prevNum = visiblePrevGhostFrame(layer, frame);
    const int nextNum = visibleNextGhostFrame(layer, frame);

    BitmapImage* prevImg = (prevNum >= 1) ? layer->getBitmapImageAtFrame(prevNum) : nullptr;
    BitmapImage* nextImg = (nextNum >= 1) ? layer->getBitmapImageAtFrame(nextNum) : nullptr;
    if (prevImg == nullptr && nextImg == nullptr) { return; }

    OnionGhostOffset& ghost = ghostOf(layer);
    if (prevImg != nullptr && nextImg != nullptr)
    {
        // 两帧内容中心对齐到中点（与旧版对位中割参考工具同算法）；只动平移，保留旋转缩放
        const QPointF mid = (QPointF(prevImg->bounds().center()) + QPointF(nextImg->bounds().center())) / 2.0;
        ghost.prevFrameNumber = prevNum;
        ghost.prev.offset = mid - QPointF(prevImg->bounds().center());
        ghost.nextFrameNumber = nextNum;
        ghost.next.offset = mid - QPointF(nextImg->bounds().center());
    }
    else if (prevImg != nullptr)
    {
        ghost.prevFrameNumber = prevNum;
        ghost.prev.offset = QPointF();
    }
    else
    {
        ghost.nextFrameNumber = nextNum;
        ghost.next.offset = QPointF();
    }
    mScribbleArea->invalidateOnionGhostVisual();
}
