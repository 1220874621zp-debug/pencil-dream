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

#include "polylinetool.h"

#include <QSettings>
#include <QPainterPath>
#include "editor.h"
#include "scribblearea.h"

#include "layermanager.h"
#include "colormanager.h"
#include "undoredomanager.h"
#include "pointerevent.h"
#include "layerbitmap.h"


namespace
{
    // The following helpers build the polyline preview path geometry.

    QPainterPath straightPathFromPoints(const QList<QPointF>& points)
    {
        QPainterPath path;
        path.moveTo(points.first());
        for (int i = 1; i < points.size(); i++)
        {
            path.lineTo(points.at(i));
        }
        return path;
    }

    QPainterPath smoothPathFromPoints(const QList<QPointF>& points)
    {
        QPainterPath path;
        path.moveTo(points.first());

        QList<QPointF> c1;
        QList<QPointF> c2;
        QList<QPointF> vertex;
        for (int i = 1; i < points.size(); i++)
        {
            c1.append(points.at(i));
            c2.append(points.at(i));
            vertex.append(points.at(i));
        }

        const int n = vertex.size();
        QPointF c2old;
        for (int p = 0; p < n - 1; p++)
        {
            QPointF D = vertex.at(p);
            QPointF Dprev = (p == 0) ? points.first() : vertex.at(p - 1);
            QPointF Dnext = vertex.at(p + 1);
            qreal L1 = qAbs(D.x() - Dprev.x()) + qAbs(D.y() - Dprev.y());
            qreal L2 = qAbs(D.x() - Dnext.x()) + qAbs(D.y() - Dnext.y());

            QPointF tangentVec = 0.4 * (Dnext - Dprev);
            QPointF c1Point, c2Point;
            if (((D - Dprev).x() * (D - Dnext).x() + (D - Dprev).y() * (D - Dnext).y()) / (1.0 * L1 * L2) < 0)
            {
                // smooth point
                c1Point = D - tangentVec * (L1 + 0.0) / (L1 + L2);
                c2Point = D + tangentVec * (L2 + 0.0) / (L1 + L2);
            }
            else
            {
                // sharp point
                c1Point = 0.6 * D + 0.4 * Dprev;
                c2Point = 0.6 * D + 0.4 * Dnext;
            }

            if (p == 0)
            {
                c2old = 0.5 * (vertex.at(0) + c1Point);
            }

            c1[p] = c2old;
            c2[p] = c1Point;
            c2old = c2Point;
        }
        if (n > 2)
        {
            c1[n - 1] = c2old;
            c2[n - 1] = 0.5 * (c2old + vertex.at(n - 1));
        }

        for (int i = 0; i < n; i++)
        {
            path.cubicTo(c1.at(i), c2.at(i), vertex.at(i));
        }
        return path;
    }
}

PolylineTool::PolylineTool(QObject* parent) : StrokeTool(parent)
{
}

ToolType PolylineTool::type() const
{
    return POLYLINE;
}

void PolylineTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP };
    mPropertyUsed[PolylineToolProperties::CLOSEDPATH_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[PolylineToolProperties::BEZIERPATH_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::ANTI_ALIASING_ENABLED] = { Layer::BITMAP };

    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    QHash<int, PropertyInfo> info;

    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 8.0 };
    info[PolylineToolProperties::CLOSEDPATH_ENABLED] = false;
    info[PolylineToolProperties::BEZIERPATH_ENABLED] = false;
    info[StrokeToolProperties::ANTI_ALIASING_ENABLED] = true;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, pencilSettings.value("polylineWidth", 8.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::ANTI_ALIASING_ENABLED, pencilSettings.value("brushAA", true).toBool());
        toolProperties().setBaseValue(PolylineToolProperties::CLOSEDPATH_ENABLED, pencilSettings.value("closedPolylinePath", false).toBool());

        pencilSettings.remove("polylineWidth");
        pencilSettings.remove("brushAA");
        pencilSettings.remove("closedPolylinePath");
    }

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
}

bool PolylineTool::leavingThisTool()
{
    StrokeTool::leavingThisTool();
    if (mPoints.size() > 0)
    {
        cancelPolyline();
    }
    return true;
}

bool PolylineTool::isActive() const
{
    return !mPoints.isEmpty();
}

QCursor PolylineTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void PolylineTool::clearToolData()
{
    if (mPoints.empty()) {
        return;
    }

    mPoints.clear();
    emit isActiveChanged(POLYLINE, false);

    // Clear the in-progress polyline from the bitmap buffer.
    mScribbleArea->clearDrawingBuffer();
    mScribbleArea->updateFrame();
}

void PolylineTool::pointerPressEvent(PointerEvent* event)
{
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    Layer* layer = mEditor->layers()->currentLayer();

    if (event->button() == Qt::LeftButton)
    {
        if (layer->isBitmapKind())
        {
            mScribbleArea->handleDrawingOnEmptyFrame();

            mPoints << getCurrentPoint();
            emit isActiveChanged(POLYLINE, true);
        }
    }

    StrokeTool::pointerPressEvent(event);
}

void PolylineTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->isBitmapKind())
    {
        drawPolyline(mPoints, getCurrentPoint());
    }

    StrokeTool::pointerMoveEvent(event);
}

void PolylineTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    StrokeTool::pointerReleaseEvent(event);
}

void PolylineTool::pointerDoubleClickEvent(PointerEvent* event)
{
    mInterpolator.pointerPressEvent(event);


    if (mPoints.size() > 0) {
        if (mPoints.last() != getCurrentPoint()) {
            // include the current point before ending the line.
            mPoints << getCurrentPoint();
        }
        SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
        mEditor->backup(typeName());
        endPolyline(mPoints);
        mEditor->undoRedo()->record(saveStateId, typeName());
    }
}

void PolylineTool::removeLastPolylineSegment()
{
    if (mPoints.size() > 1)
    {
        mPoints.removeLast();
        drawPolyline(mPoints, getCurrentPoint());
    }
    else if (mPoints.size() == 1)
    {
        cancelPolyline();
        clearToolData();
    }
}

bool PolylineTool::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Control:
        mClosedPathOverrideEnabled = true;
        drawPolyline(mPoints, getCurrentPoint());
        return true;
        break;

    case Qt::Key_Return:
        if (mPoints.size() > 0)
        {
            // include the current point before ending the line.
            mPoints << getCurrentPoint();
            SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
            endPolyline(mPoints);
            mEditor->undoRedo()->record(saveStateId, typeName());
            return true;
        }
        break;
    case Qt::Key_Backspace:
        if (mPoints.size() > 0)
        {
            removeLastPolylineSegment();
            return true;
        }
    case Qt::Key_Escape:
        if (mPoints.size() > 0)
        {
            cancelPolyline();
            return true;
        }
        break;

    default:
        break;
    }

    return BaseTool::keyPressEvent(event);
}

bool PolylineTool::keyReleaseEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Control:
        mClosedPathOverrideEnabled = false;
        drawPolyline(mPoints, getCurrentPoint());
        return true;
        break;

    default:
        break;
    }

    return BaseTool::keyReleaseEvent(event);
}

void PolylineTool::drawPolyline(QList<QPointF> points, QPointF endPoint)
{
    if (points.size() > 0)
    {
        QPen pen(mEditor->color()->frontColor(),
                 mSettings.width(),
                 Qt::SolidLine,
                 Qt::RoundCap,
                 Qt::RoundJoin);

        QPainterPath tempPath;
        if (mSettings.bezierPathEnabled())
        {
            tempPath = smoothPathFromPoints(points);
        }
        else
        {
            tempPath = straightPathFromPoints(points);
        }
        tempPath.lineTo(endPoint);

        // Ctrl key inverts closed behavior while held (XOR)
        if ((mSettings.closedPathEnabled() == !mClosedPathOverrideEnabled) && points.size() > 1)
        {
            tempPath.closeSubpath();
        }

        mScribbleArea->drawPolyline(tempPath, pen, mSettings.AntiAliasingEnabled());
    }
}


void PolylineTool::cancelPolyline()
{
    clearToolData();
}

void PolylineTool::endPolyline(QList<QPointF> points)
{
    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->isBitmapKind())
    {
        drawPolyline(points, points.last());
    }

    mScribbleArea->endStroke();
    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());

    clearToolData();
}

void PolylineTool::setUseBezier(bool useBezier)
{
    toolProperties().setBaseValue(PolylineToolProperties::BEZIERPATH_ENABLED, useBezier);
    emit bezierPathEnabledChanged(useBezier);
}

void PolylineTool::setClosePath(bool closePath)
{
    toolProperties().setBaseValue(PolylineToolProperties::CLOSEDPATH_ENABLED, closePath);
    emit closePathChanged(closePath);
}
