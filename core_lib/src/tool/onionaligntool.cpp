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

QCursor OnionAlignTool::cursor()
{
    if (mDragSide != GhostSide::NONE || mHoverSide != GhostSide::NONE)
    {
        return QCursor(Qt::SizeAllCursor);
    }
    return QCursor(Qt::ArrowCursor);
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

bool OnionAlignTool::alphaHit(BitmapImage* image, const QPointF& canvasPos, const QPointF& offset) const
{
    if (image == nullptr || image->image() == nullptr) { return false; }
    const QImage& img = *image->image();
    // 细线在低缩放下几乎点不中，取 7x7 容差窗口内任意非透明像素
    const QPointF local = canvasPos - QPointF(image->topLeft()) - offset;
    const int cx = qFloor(local.x());
    const int cy = qFloor(local.y());
    for (int dy = -3; dy <= 3; ++dy)
    {
        for (int dx = -3; dx <= 3; ++dx)
        {
            const int x = cx + dx;
            const int y = cy + dy;
            if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) { continue; }
            if (qAlpha(img.pixel(x, y)) > 8) { return true; }
        }
    }
    return false;
}

bool OnionAlignTool::hitTestGhost(const QPointF& canvasPos, GhostSide& sideOut, int& frameOut) const
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return false; }

    const int frame = mEditor->currentFrame();
    const bool isAbsolute = onionIsAbsolute();

    int prevNum = layer->getPreviousFrameNumber(frame, isAbsolute);
    if (isAbsolute && prevNum >= 1)
    {
        // 绝对模式下当前帧所在关键帧不画幽灵，命中范围须与其一致
        KeyFrame* currentKey = layer->getLastKeyFrameAtPosition(frame);
        if (currentKey && prevNum == currentKey->pos())
        {
            prevNum = layer->getPreviousFrameNumber(prevNum, true);
        }
    }
    const int nextNum = layer->getNextFrameNumber(frame, isAbsolute);

    // 偏移后的实际显示位置参与命中
    const OnionGhostOffsetMap& ghosts = mScribbleArea->onionGhostOffsets();
    auto it = ghosts.constFind(layer->id());
    const OnionGhostOffset ghost = (it != ghosts.constEnd()) ? it.value() : OnionGhostOffset();
    const QPointF prevOff = (prevNum == ghost.prevFrameNumber) ? ghost.prevOffset : QPointF();
    const QPointF nextOff = (nextNum == ghost.nextFrameNumber) ? ghost.nextOffset : QPointF();

    // next 在 prev 之上绘制，先测 next
    if (nextNum >= 1 && alphaHit(layer->getBitmapImageAtFrame(nextNum), canvasPos, nextOff))
    {
        sideOut = GhostSide::NEXT;
        frameOut = nextNum;
        return true;
    }
    if (prevNum >= 1 && alphaHit(layer->getBitmapImageAtFrame(prevNum), canvasPos, prevOff))
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
}

void OnionAlignTool::pointerMoveEvent(PointerEvent* event)
{
    if (mDragSide != GhostSide::NONE)
    {
        applyOffsetDelta(event->canvasPos() - mLastCanvasPos);
        mLastCanvasPos = event->canvasPos();
        return;
    }

    // 未按下：更新 hover 命中态驱动光标
    GhostSide side = GhostSide::NONE;
    int ghostFrame = -1;
    hitTestGhost(event->canvasPos(), side, ghostFrame);
    if (side != mHoverSide)
    {
        mHoverSide = side;
        mScribbleArea->updateToolCursor();
    }
}

void OnionAlignTool::pointerReleaseEvent(PointerEvent* event)
{
    Q_UNUSED(event)
    if (mDragSide != GhostSide::NONE)
    {
        mDragSide = GhostSide::NONE;
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
    if (mHoverSide != GhostSide::NONE)
    {
        mHoverSide = GhostSide::NONE;
        mScribbleArea->updateToolCursor();
    }
    return true;
}

void OnionAlignTool::applyOffsetDelta(const QPointF& delta)
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return; }

    OnionGhostOffset& ghost = mScribbleArea->onionGhostOffsetRef(layer->id());
    if (mDragSide == GhostSide::PREV)
    {
        ghost.prevFrameNumber = mDragGhostFrame;
        ghost.prevOffset += delta;
    }
    else if (mDragSide == GhostSide::NEXT)
    {
        ghost.nextFrameNumber = mDragGhostFrame;
        ghost.nextOffset += delta;
    }
    mScribbleArea->invalidateOnionGhostVisual();
}

void OnionAlignTool::resetGhostOffset(GhostSide side)
{
    LayerBitmap* layer = currentBitmapLayer();
    if (layer == nullptr) { return; }

    OnionGhostOffset& ghost = mScribbleArea->onionGhostOffsetRef(layer->id());
    if (side == GhostSide::PREV)
    {
        ghost.prevOffset = QPointF();
    }
    else if (side == GhostSide::NEXT)
    {
        ghost.nextOffset = QPointF();
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
    const bool isAbsolute = onionIsAbsolute();

    int prevNum = layer->getPreviousFrameNumber(frame, isAbsolute);
    if (isAbsolute && prevNum >= 1)
    {
        KeyFrame* currentKey = layer->getLastKeyFrameAtPosition(frame);
        if (currentKey && prevNum == currentKey->pos())
        {
            prevNum = layer->getPreviousFrameNumber(prevNum, true);
        }
    }
    const int nextNum = layer->getNextFrameNumber(frame, isAbsolute);

    BitmapImage* prevImg = (prevNum >= 1) ? layer->getBitmapImageAtFrame(prevNum) : nullptr;
    BitmapImage* nextImg = (nextNum >= 1) ? layer->getBitmapImageAtFrame(nextNum) : nullptr;
    if (prevImg == nullptr && nextImg == nullptr) { return; }

    OnionGhostOffset& ghost = mScribbleArea->onionGhostOffsetRef(layer->id());
    if (prevImg != nullptr && nextImg != nullptr)
    {
        // 两帧内容中心对齐到中点（与旧版对位中割参考工具同算法）
        const QPointF mid = (QPointF(prevImg->bounds().center()) + QPointF(nextImg->bounds().center())) / 2.0;
        ghost.prevFrameNumber = prevNum;
        ghost.prevOffset = mid - QPointF(prevImg->bounds().center());
        ghost.nextFrameNumber = nextNum;
        ghost.nextOffset = mid - QPointF(nextImg->bounds().center());
    }
    else if (prevImg != nullptr)
    {
        ghost.prevFrameNumber = prevNum;
        ghost.prevOffset = QPointF();
    }
    else
    {
        ghost.nextFrameNumber = nextNum;
        ghost.nextOffset = QPointF();
    }
    mScribbleArea->invalidateOnionGhostVisual();
}
