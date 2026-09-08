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
#include "canvaspainter.h"
#include "toolproperties.h"

/** 洋葱皮对位工具：直接拖动画布上的洋葱皮前后帧幽灵像，实现对位中割参考。
 *  - 左键拖动最近的红色(前)/蓝色(后)幽灵：平移
 *  - Ctrl+拖动：绕幽灵内容中心旋转（Shift 追加步进吸附，同 MoveTool 设置）
 *  - Shift+拖动：以幽灵内容中心为锚等比缩放（5%~2000% 钳制）
 *  - Alt+点击：命中幽灵则归零该侧全部变换，空白处则清空两侧
 *  - 双击：前后帧内容中心一键对齐到中点（纯平移语义）
 *  变换是显示辅助态（存于 ScribbleArea，不落盘不入撤销栈），
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
    void resetPrevGhostOffset();   ///< 归零前帧(红)幽灵全部变换（位移/旋转/缩放）
    void resetNextGhostOffset();   ///< 归零后帧(蓝)幽灵全部变换
    void resetAllGhostOffsets();   ///< 全部复位
    void autoAlignCenters();       ///< 前后帧内容中心对齐到中点

private:
    enum class GhostSide { NONE, PREV, NEXT };
    enum class DragMode { Move, Rotate, Scale };

    LayerBitmap* currentBitmapLayer() const;
    bool onionIsAbsolute() const;

    /** 命中最近的幽灵（细线容差采样）；next 在上层先测 */
    bool hitTestGhost(const QPointF& canvasPos, GhostSide& sideOut, int& frameOut) const;
    bool alphaHit(BitmapImage* image, const QPointF& canvasPos, const OnionGhostTransform& transform) const;

    /** 与 OnionskinSubPainter 相同的步进，找实际可见（有图像）的前/后幽灵关键帧；不可见返回 -1。
     *  相对模式逐帧回走会路过无关键帧的位置（getBitmapImageAtFrame 为 null，画布上不显示幽灵），
     *  绝对模式跳过当前覆盖关键帧——命中/对齐都必须只认真正画出来的那一帧。 */
    int visiblePrevGhostFrame(LayerBitmap* layer, int frame) const;
    int visibleNextGhostFrame(LayerBitmap* layer, int frame) const;

    /** 幽灵内容中心 + 当前偏移 = 旋转/缩放锚点（画布坐标） */
    QPointF ghostAnchor(BitmapImage* image, const OnionGhostTransform& transform) const;

    /** 当前层的幽灵状态（不存在则创建默认项） */
    OnionGhostOffset& ghostOf(LayerBitmap* layer);

    /** 旋转操作光标（Qt 无内置旋转箭头，复用 MoveTool 的自绘资源） */
    static QCursor rotateCursor();

    void resetGhostOffset(GhostSide side);
    void applyOffsetDelta(const QPointF& delta);
    void applyRotationDelta(qreal deltaDegrees, bool snapToIncrement);
    void applyScaleFactor(qreal factor);

    ToolProperties mSettings;

    GhostSide mDragSide = GhostSide::NONE;
    GhostSide mHoverSide = GhostSide::NONE;
    DragMode mDragMode = DragMode::Move;
    int mDragGhostFrame = -1;
    QPointF mLastCanvasPos;
    qreal mLastDragAngle = 0.0;   ///< 旋转拖拽：上一事件的锚点方位角（度）
    qreal mLastDragDist = 0.0;    ///< 缩放拖拽：上一事件到锚点距离
    Qt::KeyboardModifiers mHoverModifiers = Qt::NoModifier; ///< 悬停时修饰键，驱动光标预览
};

#endif // ONIONALIGNTOOL_H
