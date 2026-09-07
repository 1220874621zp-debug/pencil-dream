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

#include "lassotool.h"

#include <QSettings>
#include <QPainter>

#include "pointerevent.h"
#include "editor.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "toolmanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "viewmanager.h"

LassoTool::LassoTool(QObject* parent) : TransformTool(parent)
{
}

void LassoTool::loadSettings()
{
    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    mPropertyUsed[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = { Layer::BITMAP };

    QHash<int, PropertyInfo> info;
    info[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = false;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);
}

QCursor LassoTool::cursor()
{
    if (mScribbleArea->isPointerInUse()) { return QCursor(Qt::BlankCursor); }
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

bool LassoTool::isActive() const
{
    return mLassoActive;
}

void LassoTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect)

    if (!mLassoActive || mLassoPoints.size() < 2) { return; }

    painter.save();
    painter.setTransform(mEditor->view()->getView());

    QPen pen(Qt::white, 1.0, Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawPolyline(mLassoPoints);

    painter.restore();
}

void LassoTool::pointerPressEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) return;
    if (!currentLayer->isPaintable()) { return; }
    if (event->button() != Qt::LeftButton) { return; }

    mUndoStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);

    beginLasso(event->canvasPos());
}

void LassoTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) { return; }
    if (!currentLayer->isPaintable()) { return; }

    if (mLassoActive && mScribbleArea->isPointerInUse())
    {
        extendLasso(event->canvasPos());
    }
}

void LassoTool::pointerReleaseEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) return;
    if (event->button() != Qt::LeftButton) return;

    if (!mLassoActive) { return; }

    endLasso(event->canvasPos());

    mEditor->undoRedo()->record(mUndoStateId, typeName());
}

void LassoTool::beginLasso(const QPointF& pos)
{
    mLassoPoints.clear();
    mLassoPoints << pos;
    mLassoActive = true;
    emit isActiveChanged(LASSO, true);
}

void LassoTool::extendLasso(const QPointF& pos)
{
    const QPointF& last = mLassoPoints.last();
    if (QLineF(last, pos).length() >= 2.0)
    {
        mLassoPoints << pos;
        mScribbleArea->updateFrame();
    }
}

void LassoTool::endLasso(const QPointF& pos)
{
    // close the shape with the final point
    if (mLassoPoints.size() > 0 && QLineF(mLassoPoints.last(), pos).length() >= 2.0)
    {
        mLassoPoints << pos;
    }

    mLassoActive = false;
    emit isActiveChanged(LASSO, false);

    Layer* currentLayer = mEditor->layers()->currentLayer();
    const bool roundPixels = currentLayer != nullptr && currentLayer->type() == Layer::BITMAP;

    const QRectF bounds = mLassoPoints.boundingRect();
    if (mLassoPoints.size() < 3 || bounds.width() < 2.0 || bounds.height() < 2.0)
    {
        mEditor->deselectAll();
    }
    else
    {
        mEditor->select()->setSelection(mLassoPoints, roundPixels);
    }

    mLassoPoints.clear();
    mScribbleArea->updateFrame();
}
