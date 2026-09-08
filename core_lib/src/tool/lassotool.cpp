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
#include <QPainterPath>
#include <QPainterPathStroker>

#include "pointerevent.h"
#include "editor.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "toolmanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "viewmanager.h"

namespace
{
    QPainterPath polygonToPath(const QPolygonF& polygon)
    {
        QPainterPath path;
        if (polygon.size() >= 3)
        {
            path.moveTo(polygon.first());
            for (int i = 1; i < polygon.size(); i++)
            {
                path.lineTo(polygon.at(i));
            }
            path.closeSubpath();
            path.setFillRule(Qt::OddEvenFill);
        }
        return path;
    }

    QPolygonF pathToPolygon(const QPainterPath& path)
    {
        // flatten to one polygon: concatenate the closed subpaths, an
        // even-odd fill of the concatenated polygon fills the same area
        QPolygonF combined;
        const QList<QPolygonF> subpaths = path.toSubpathPolygons();
        for (const QPolygonF& sub : subpaths)
        {
            if (sub.size() < 3) { continue; }
            if (!combined.isEmpty())
            {
                combined << sub.first(); // implicit close of the previous lobe
            }
            combined << sub;
        }
        return combined;
    }

    QPainterPath boundaryBand(const QPainterPath& path, qreal width)
    {
        QPainterPathStroker stroker;
        stroker.setWidth(width);
        stroker.setJoinStyle(Qt::RoundJoin);
        stroker.setCapStyle(Qt::RoundCap);
        return stroker.createStroke(path);
    }
}

LassoTool::LassoTool(QObject* parent) : TransformTool(parent)
{
}

void LassoTool::loadSettings()
{
    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    mPropertyUsed[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::SELECTION_ACTION_VALUE] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::SELECTION_GROW_VALUE] = { Layer::BITMAP };

    QHash<int, PropertyInfo> info;
    info[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = false;
    info[TransformToolProperties::SELECTION_ACTION_VALUE] = { 0, 4, 0 };
    info[TransformToolProperties::SELECTION_GROW_VALUE] = { -50, 50, 0 };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    mSelectionAction = toolProperties().getInfo(TransformToolProperties::SELECTION_ACTION_VALUE).intValue();
    mGrowValue = toolProperties().getInfo(TransformToolProperties::SELECTION_GROW_VALUE).intValue();
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

void LassoTool::setSelectionAction(int action)
{
    action = qBound(0, action, 4);
    if (action == mSelectionAction) { return; }

    mSelectionAction = action;
    toolProperties().setBaseValue(TransformToolProperties::SELECTION_ACTION_VALUE, action);
    emit selectionActionChanged(action);
}

void LassoTool::setGrowValue(int grow)
{
    grow = qBound(-50, grow, 50);
    if (grow == mGrowValue) { return; }

    mGrowValue = grow;
    toolProperties().setBaseValue(TransformToolProperties::SELECTION_GROW_VALUE, grow);
    emit growValueChanged(grow);
}

int LassoTool::actionFromModifiers(Qt::KeyboardModifiers modifiers) const
{
    // Krita defaults (no remap): Ctrl=replace, Shift=add, Alt=subtract,
    // Shift+Alt=intersect, Ctrl+Alt=symmetric difference
    if (modifiers == (Qt::ShiftModifier | Qt::AltModifier)) { return 3; }
    if (modifiers == (Qt::ControlModifier | Qt::AltModifier)) { return 4; }
    if (modifiers == Qt::ShiftModifier) { return 1; }
    if (modifiers == Qt::AltModifier) { return 2; }
    if (modifiers == Qt::ControlModifier) { return 0; }
    return -1; // keep the tool-option action
}

void LassoTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect)

    if (!mLassoActive || mLassoPoints.size() < 2) { return; }

    painter.save();
    painter.setTransform(mEditor->view()->getView());

    // two-pass stroke: visible on white paper and dark workspace alike
    QPen underlay(Qt::black, 2.0, Qt::SolidLine);
    underlay.setCosmetic(true);
    painter.setPen(underlay);
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawPolyline(mLassoPoints);

    QPen dashes(Qt::white, 1.0, Qt::DashLine);
    dashes.setCosmetic(true);
    painter.setPen(dashes);
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

    mActiveAction = mSelectionAction;

    beginLasso(event->canvasPos());
}

void LassoTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) { return; }
    if (!currentLayer->isPaintable()) { return; }

    if (mLassoActive && mScribbleArea->isPointerInUse())
    {
        // Krita allows changing the action mid-stroke with modifier keys
        const int fromMods = actionFromModifiers(event->modifiers());
        if (fromMods >= 0) { mActiveAction = fromMods; }

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
        mLassoPoints.clear();
        mScribbleArea->updateFrame();
        return;
    }

    auto selectMan = mEditor->select();

    // freehand points arrive every >=2px, so long strokes hold thousands of
    // vertices and the QPainterPath boolean ops below slow down badly.
    // Halve until manageable (same approach as DeformTool::finalizeCage).
    QPolygonF simplified = mLassoPoints;
    constexpr int kMaxLassoVertices = 256;
    while (simplified.size() > kMaxLassoVertices)
    {
        QPolygonF reduced;
        reduced.reserve((simplified.size() + 1) / 2);
        for (int i = 0; i < simplified.size(); i += 2)
        {
            reduced << simplified[i];
        }
        simplified = reduced;
    }
    mLassoPoints.clear();

    qDebug() << "[lasso] end pts=" << simplified.size()
             << " bounds=" << simplified.boundingRect()
             << " action=" << mActiveAction;

    QPainterPath newPath = polygonToPath(simplified);

    // combine with the existing selection according to the action
    QPainterPath result;
    const bool hasOld = selectMan->somethingSelected() && !selectMan->mySelectionPolygon().isEmpty();
    switch (mActiveAction)
    {
    case 1: // add
        result = hasOld ? polygonToPath(selectMan->mySelectionPolygon()).united(newPath) : newPath;
        break;
    case 2: // subtract
        result = hasOld ? polygonToPath(selectMan->mySelectionPolygon()).subtracted(newPath) : QPainterPath();
        break;
    case 3: // intersect
        result = hasOld ? polygonToPath(selectMan->mySelectionPolygon()).intersected(newPath) : QPainterPath();
        break;
    case 4: // symmetric difference: (a - b) union (b - a)
    {
        const QPainterPath oldPath = hasOld ? polygonToPath(selectMan->mySelectionPolygon()) : QPainterPath();
        if (hasOld)
        {
            result = oldPath.subtracted(newPath).united(newPath.subtracted(oldPath));
        }
        else
        {
            result = newPath;
        }
        break;
    }
    case 0: // replace
    default:
        result = newPath;
        break;
    }

    // grow / shrink the combined shape
    if (mGrowValue > 0)
    {
        result = result.united(boundaryBand(result, 2.0 * mGrowValue));
    }
    else if (mGrowValue < 0)
    {
        result = result.subtracted(boundaryBand(result, 2.0 * (-mGrowValue)));
    }

    const QPolygonF combined = pathToPolygon(result);
    const QRectF resultBounds = combined.boundingRect();
    if (combined.size() < 3 || resultBounds.width() < 1.0 || resultBounds.height() < 1.0)
    {
        mEditor->deselectAll();
    }
    else
    {
        selectMan->setSelection(combined, roundPixels);
    }

    mScribbleArea->updateFrame();
}
