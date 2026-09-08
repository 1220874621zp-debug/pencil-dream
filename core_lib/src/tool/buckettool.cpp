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
#include "buckettool.h"

#include <QPixmap>
#include <QPainter>
#include <QSettings>
#include <climits>
#include "pointerevent.h"

#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "colormanager.h"
#include "toolmanager.h"
#include "viewmanager.h"
#include "editor.h"
#include "scribblearea.h"


BucketTool::BucketTool(QObject* parent) : BaseTool(parent)
{
}

void BucketTool::loadSettings()
{
    mPropertyUsed[BucketToolProperties::COLORTOLERANCE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::COLORTOLERANCE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLEXPAND_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLEXPAND_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLMODE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::CLOSEGAP_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FEATHER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::ANTIALIASING_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::GROWSTOPDARKEST_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::REGIONMODE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::BOUNDARYCOLOR_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::DRAGMODE_VALUE] = { Layer::BITMAP };

    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    QHash<int, PropertyInfo> info;

    info[BucketToolProperties::COLORTOLERANCE_VALUE] = { 1, 100, 32 };
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = false;
    // The expand value is now a signed grow/shrink amount (Krita parity)
    info[BucketToolProperties::FILLEXPAND_VALUE] = { -40, 40, 2 };
    info[BucketToolProperties::FILLEXPAND_ENABLED] = true;
    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = { 0, 1, 0 };
    info[BucketToolProperties::FILLMODE_VALUE] = { 0, 2, 0 };
    info[BucketToolProperties::CLOSEGAP_VALUE] = { 0, 32, 0 };
    info[BucketToolProperties::FEATHER_VALUE] = { 0, 40, 0 };
    info[BucketToolProperties::ANTIALIASING_ENABLED] = false;
    info[BucketToolProperties::GROWSTOPDARKEST_ENABLED] = false;
    info[BucketToolProperties::REGIONMODE_VALUE] = { 0, 3, 0 };
    // A QRgb does not always fit a positive int, hence the full int range
    info[BucketToolProperties::BOUNDARYCOLOR_VALUE] = { INT_MIN, INT_MAX, static_cast<int>(QColor(Qt::black).rgba()) };
    info[BucketToolProperties::DRAGMODE_VALUE] = { 0, 2, 0 };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(BucketToolProperties::COLORTOLERANCE_VALUE, pencilSettings.value("Tolerance", 32).toInt());
        toolProperties().setBaseValue(BucketToolProperties::COLORTOLERANCE_ENABLED, pencilSettings.value("BucketToleranceEnabled", false).toBool());
        toolProperties().setBaseValue(BucketToolProperties::FILLEXPAND_VALUE, pencilSettings.value("BucketFillExpand", 2).toInt());
        toolProperties().setBaseValue(BucketToolProperties::FILLEXPAND_ENABLED, pencilSettings.value("BucketFillExpandEnabled", true).toBool());
        toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, pencilSettings.value("BucketFillReferenceMode", 0).toInt());
        toolProperties().setBaseValue(BucketToolProperties::FILLMODE_VALUE, pencilSettings.value("FillMode", 0).toInt());

        pencilSettings.remove("Tolerance");
        pencilSettings.remove("BucketToleranceEnabled");
        pencilSettings.remove("BucketFillExpand");
        pencilSettings.remove("BucketFillExpandEnabled");
        pencilSettings.remove("BucketFillReferenceMode");
        pencilSettings.remove("FillMode");
    }
}

QCursor BucketTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":icons/general/cursor-bucket.svg"), -1, 17);
    }
    else
    {
        return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
    }
}

void BucketTool::pointerPressEvent(PointerEvent* event)
{
    mInterpolator.pointerPressEvent(event);

    Layer* targetLayer = mEditor->layers()->currentLayer();

    if (targetLayer->type() != Layer::BITMAP) { return; }

    LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());

    mUndoSaveState = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);

    // Krita parity: holding Shift for this click fills similar regions
    // everywhere, holding Alt fills the active selection; the configured
    // option stays untouched.
    int regionModeOverride = -1;
    if (event->modifiers() & Qt::ShiftModifier) {
        regionModeOverride = static_cast<int>(FillRegionMode::Similar);
    } else if (event->modifiers() & Qt::AltModifier) {
        regionModeOverride = static_cast<int>(FillRegionMode::Selection);
    }

    mBitmapBucket = BitmapBucket(mEditor,
                                 mEditor->color()->frontColor(),
                                 layerCam ? layerCam->getViewAtFrame(mEditor->currentFrame()).inverted().mapRect(layerCam->getViewRect()) : QRect(),
                                 getCurrentPoint(),
                                 mSettings,
                                 regionModeOverride);

    // Because we can change layer to on the fly, but we do not act reactively
    // on it, it's necessary to invalidate layer cache on press event.
    // Otherwise, the cache will be drawn until a move event has been initiated.
    mScribbleArea->invalidatePainterCaches();

    qDebug() << "[bucket] press layer=" << targetLayer->name()
             << "frame" << mEditor->currentFrame()
             << "pt=" << getCurrentPoint()
             << "cam=" << (layerCam ? layerCam->getViewAtFrame(mEditor->currentFrame()).inverted().mapRect(layerCam->getViewRect()) : QRect())
             << "color=" << mEditor->color()->frontColor();
}

void BucketTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (event->buttons() & Qt::LeftButton)
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer->type() == Layer::BITMAP)
        {
            paintBitmap();
            mFilledOnMove = true;
        }
    }
}

void BucketTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);

    Layer* layer = editor()->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (event->button() == Qt::LeftButton)
    {
        // Backup of bitmap image is more complicated now and has therefore been moved to bitmap code
        if (layer->type() == Layer::BITMAP && !mFilledOnMove)
        {
            paintBitmap();
        }
    }
    mFilledOnMove = false;
}

void BucketTool::paintBitmap()
{
    mBitmapBucket.paint(getCurrentPoint(), [this](BucketState progress, int layerIndex, int frameIndex)
    {
        if (progress == BucketState::WillFillTarget)
        {
            mEditor->backup(layerIndex, frameIndex, typeName());
        }
        else if (progress == BucketState::DidFillTarget)
        {
            mEditor->undoRedo()->record(mUndoSaveState, typeName());
            mEditor->setModified(layerIndex, frameIndex);
        }
    });
}

void BucketTool::setColorTolerance(int tolerance)
{
    toolProperties().setBaseValue(BucketToolProperties::COLORTOLERANCE_VALUE, tolerance);
    emit toleranceChanged(tolerance);
}

void BucketTool::setColorToleranceEnabled(bool enabled)
{
    toolProperties().setBaseValue(BucketToolProperties::COLORTOLERANCE_ENABLED, enabled);
    emit toleranceEnabledChanged(enabled);
}

void BucketTool::setFillExpand(int fillExpandValue)
{
    toolProperties().setBaseValue(BucketToolProperties::FILLEXPAND_VALUE, fillExpandValue);
    emit fillExpandChanged(fillExpandValue);
}

void BucketTool::setFillExpandEnabled(bool enabled)
{
    toolProperties().setBaseValue(BucketToolProperties::FILLEXPAND_ENABLED, enabled);
    emit fillExpandEnabledChanged(enabled);
}

void BucketTool::setFillReferenceMode(int referenceMode)
{
    toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, referenceMode);
    emit fillReferenceModeChanged(referenceMode);
}

void BucketTool::setFillMode(int mode)
{
    toolProperties().setBaseValue(BucketToolProperties::FILLMODE_VALUE, mode);
    emit fillModeChanged(mode);
}

void BucketTool::setCloseGap(int closeGap)
{
    toolProperties().setBaseValue(BucketToolProperties::CLOSEGAP_VALUE, closeGap);
    emit closeGapChanged(closeGap);
}

void BucketTool::setFeather(int feather)
{
    toolProperties().setBaseValue(BucketToolProperties::FEATHER_VALUE, feather);
    emit featherChanged(feather);
}

void BucketTool::setAntiAliasingEnabled(bool enabled)
{
    toolProperties().setBaseValue(BucketToolProperties::ANTIALIASING_ENABLED, enabled);
    emit antiAliasingEnabledChanged(enabled);
}

void BucketTool::setGrowStopDarkestEnabled(bool enabled)
{
    toolProperties().setBaseValue(BucketToolProperties::GROWSTOPDARKEST_ENABLED, enabled);
    emit growStopDarkestEnabledChanged(enabled);
}

void BucketTool::setRegionMode(int mode)
{
    toolProperties().setBaseValue(BucketToolProperties::REGIONMODE_VALUE, mode);
    emit regionModeChanged(mode);
}

void BucketTool::setBoundaryColor(const QColor& color)
{
    toolProperties().setBaseValue(BucketToolProperties::BOUNDARYCOLOR_VALUE,
                                  PropertyInfo(static_cast<int>(color.rgba())));
    emit boundaryColorChanged(color);
}

void BucketTool::setDragMode(int mode)
{
    toolProperties().setBaseValue(BucketToolProperties::DRAGMODE_VALUE, mode);
    emit dragModeChanged(mode);
}

QPointF BucketTool::getCurrentPoint() const
{
    return mEditor->view()->mapScreenToCanvas(getCurrentPixel());
}

QPointF BucketTool::getCurrentPixel() const
{
    return mInterpolator.getCurrentPixel();
}

