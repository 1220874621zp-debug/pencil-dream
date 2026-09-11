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

#ifndef MOVETOOL_H
#define MOVETOOL_H

#include "transformtool.h"
#include "movemode.h"
#include "preferencemanager.h"
#include "undoredomanager.h"
#include "bitmapimage.h"

class Layer;
class LayerBitmap;


class MoveTool : public TransformTool
{
    Q_OBJECT
public:
    explicit MoveTool(QObject* parent);
    QCursor cursor() override;

    QCursor cursor(MoveMode mode) const;
    ToolType type() const override;

    ToolProperties& toolProperties() override { return mSettings.toolProperties(); }
    void loadSettings() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;

    bool leavingThisTool() override;
    bool isActive() const override;

    /** 穿透模式：选中幽灵的包围盒 + 角柄（画布坐标系叠加层） */
    void paint(QPainter& painter, const QRect& blitRect) override;

    void applyTransformationAndDeselect();

private:
    void applyTransformation();
    void updateSettings(const SETTING setting);

    void beginInteraction(const QPointF& pos, Qt::KeyboardModifiers keyMod, Layer* layer);
    void transformSelection(const QPointF& pos, Qt::KeyboardModifiers keyMod);

    Layer* currentPaintableLayer();

    // --- 穿透模式：拖拽任意关键帧幽灵像（真改位图像素，走撤销栈） ---
    enum class XrayDragMode { None, Move, Scale };
    bool xrayApplicable() const;
    bool xrayBeginInteraction(const QPointF& pos);
    void xrayUpdateDrag(const QPointF& pos);
    void xrayCommitDrag();
    void xrayCancelDrag();
    int xrayHitGhost(const QPointF& pos) const;
    int xrayHitScaleHandle(const QPointF& pos) const;
    bool xrayAlphaHit(BitmapImage* image, const QPointF& canvasPos) const;
    QTransform xrayCurrentTransform() const;

    QPointF mCurrentPoint;
    qreal mRotatedAngle = 0.0;
    int mRotationIncrement = 0;
    MoveMode mPerspMode;
    QPointF mOffset;

    SAVESTATE_ID mUndoSaveStateId = 0;

    bool mXrayDragging = false;
    XrayDragMode mXrayDragMode = XrayDragMode::None;
    int mXrayTargetFrame = -1;
    QPointF mXrayPressPos;
    QPointF mXrayDragTranslation;
    qreal mXrayScaleX = 1.0;
    qreal mXrayScaleY = 1.0;
    QPointF mXrayScaleAnchor;
    BitmapImage mXrayUndoSnapshot;
    int mXrayHoverFrame = -1;
    int mXrayHoverHandle = -1;
};

#endif
