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

#ifndef ONIONALIGNTOOL_H
#define ONIONALIGNTOOL_H

#include "basetool.h"
#include "toolproperties.h"

/** 洋葱皮对位工具：直接拖动画布上的洋葱皮前后帧幽灵像，实现对位中割参考。
 *  - 左键拖动最近的红色(前)/蓝色(后)幽灵
 *  - Alt+点击：命中幽灵则归零该侧，空白处则清空两侧
 *  - 双击：前后帧内容中心一键对齐到中点
 *  偏移是显示辅助态（存于 ScribbleArea，不落盘不入撤销栈），
 *  切回笔画工具后参考仍然可见。 */
class LayerBitmap;
class BitmapImage;

class OnionAlignTool : public BaseTool
{
    Q_OBJECT
public:
    explicit OnionAlignTool(QObject* parent = nullptr);

    ToolType type() const override { return ONION_ALIGN; }
    ToolProperties& toolProperties() override { return mSettings; }
    void loadSettings() override;
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent* event) override;
    void pointerMoveEvent(PointerEvent* event) override;
    void pointerReleaseEvent(PointerEvent* event) override;
    void pointerDoubleClickEvent(PointerEvent* event) override;

    bool leaveEvent(QEvent* event) override;

    /** 拖拽中视为活动工具：paintEvent 走实时重绘路径而非贴帧级缓存 */
    bool isActive() const override { return mDragSide != GhostSide::NONE; }

public slots:
    /** 工具选项面板入口 */
    void resetPrevGhostOffset();   ///< 归零前帧(红)幽灵偏移
    void resetNextGhostOffset();   ///< 归零后帧(蓝)幽灵偏移
    void resetAllGhostOffsets();   ///< 全部复位
    void autoAlignCenters();       ///< 前后帧内容中心对齐到中点

private:
    enum class GhostSide { NONE, PREV, NEXT };

    LayerBitmap* currentBitmapLayer() const;
    bool onionIsAbsolute() const;

    /** 命中最近的幽灵（细线容差采样）；next 在上层先测 */
    bool hitTestGhost(const QPointF& canvasPos, GhostSide& sideOut, int& frameOut) const;
    bool alphaHit(BitmapImage* image, const QPointF& canvasPos, const QPointF& offset) const;

    void resetGhostOffset(GhostSide side);
    void applyOffsetDelta(const QPointF& delta);

    ToolProperties mSettings;

    GhostSide mDragSide = GhostSide::NONE;
    GhostSide mHoverSide = GhostSide::NONE;
    int mDragGhostFrame = -1;
    QPointF mLastCanvasPos;
};

#endif // ONIONALIGNTOOL_H
