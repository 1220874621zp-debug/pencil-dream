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

#include "timelinecells.h"

#include <QApplication>
#include <QResizeEvent>
#include <QInputDialog>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QDebug>

#include "camerapropertiesdialog.h"
#include "theme.h"
#include "editor.h"
#include "keyframe.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "soundclip.h"
#include "soundmanager.h"
#include "undoredomanager.h"
#include "timeline.h"

#include "cameracontextmenu.h"

TimeLineCells::TimeLineCells(TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE type) : QWidget(parent)
{
    mTimeLine = parent;
    mEditor = editor;
    mPrefs = editor->preference();
    mType = type;

    mFrameLength = mPrefs->getInt(SETTING::TIMELINE_SIZE);
    mFontSize = mPrefs->getInt(SETTING::LABEL_FONT_SIZE);
    mFrameSize = mPrefs->getInt(SETTING::FRAME_SIZE);
    mbShortScrub = mPrefs->isOn(SETTING::SHORT_SCRUB);
    mDrawFrameNumber = mPrefs->isOn(SETTING::DRAW_LABEL);

    setMinimumSize(500, 4 * mLayerHeight);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);

    connect(mPrefs, &PreferenceManager::optionChanged, this, &TimeLineCells::loadSetting);
}

TimeLineCells::~TimeLineCells()
{
    delete mCache;
}

void TimeLineCells::loadSetting(SETTING setting)
{
    switch (setting)
    {
    case SETTING::TIMELINE_SIZE:
        mFrameLength = mPrefs->getInt(SETTING::TIMELINE_SIZE);
        mTimeLine->updateLength();
        break;
    case SETTING::LABEL_FONT_SIZE:
        mFontSize = mPrefs->getInt(SETTING::LABEL_FONT_SIZE);
        break;
    case SETTING::FRAME_SIZE:
        mFrameSize = mPrefs->getInt(SETTING::FRAME_SIZE);
        mTimeLine->updateLength();
        break;
    case SETTING::SHORT_SCRUB:
        mbShortScrub = mPrefs->isOn(SETTING::SHORT_SCRUB);
        break;
    case SETTING::DRAW_LABEL:
        mDrawFrameNumber = mPrefs->isOn(SETTING::DRAW_LABEL);
        break;
    default:
        break;
    }

    updateContent();
}

int TimeLineCells::getFrameNumber(int x) const
{
    return mFrameOffset + 1 + (x - mOffsetX) / mFrameSize;
}

int TimeLineCells::getFrameX(int frameNumber) const
{
    return mOffsetX + (frameNumber - mFrameOffset) * mFrameSize;
}

void TimeLineCells::setFrameSize(int size)
{
    mFrameSize = size;
    mPrefs->set(SETTING::FRAME_SIZE, mFrameSize);
    updateContent();
}

int TimeLineCells::getLayerNumber(int y) const
{
    int layerNumber = mLayerOffset + (y - mOffsetY) / mLayerHeight;

    int totalLayerCount = mEditor->object()->getLayerCount();

    // Layers numbers are displayed in descending order
    // The last row is layer 0
    if (layerNumber <= totalLayerCount)
        layerNumber = (totalLayerCount - 1) - layerNumber;
    else
        layerNumber = 0;

    if (y < mOffsetY)
    {
        layerNumber = -1;
    }

    if (layerNumber >= totalLayerCount)
    {
        layerNumber = totalLayerCount;
    }

    //If the mouse release event if fired with mouse off the frame of the application
    // mEditor->object()->getLayerCount() doesn't return the correct value.
    if (layerNumber < -1)
    {
        layerNumber = -1;
    }
    return layerNumber;
}

int TimeLineCells::getInbetweenLayerNumber(int y) const {
    int layerNumber = getLayerNumber(y);
    // Round the layer number towards the drag start
    if(layerNumber != mFromLayer) {
        if(mMouseMoveY > 0 && y < getLayerY(layerNumber) + mLayerHeight / 2) {
            layerNumber++;
        }
        else if(mMouseMoveY < 0 && y > getLayerY(layerNumber) + mLayerHeight / 2) {
            layerNumber--;
        }
    }
    return layerNumber;
}

int TimeLineCells::getLayerY(int layerNumber) const
{
    return mOffsetY + (mEditor->object()->getLayerCount() - 1 - layerNumber - mLayerOffset)*mLayerHeight;
}

void TimeLineCells::updateFrame(int frameNumber)
{
    int x = getFrameX(frameNumber);
    update(x - mFrameSize, 0, mFrameSize + 1, height());
}

void TimeLineCells::updateContent()
{
    mRedrawContent = true;
    update();
}

bool TimeLineCells::didDetachLayer() const {
    return abs(mMouseMoveY) > mLayerDetachThreshold;
}

void TimeLineCells::showCameraMenu(QPoint pos)
{
    int frameNumber = getFrameNumber(pos.x());

    const Layer* curLayer = mEditor->layers()->currentLayer();
    Q_ASSERT(curLayer);

    // only show menu if on camera layer and key exists
    if (curLayer->type() != Layer::CAMERA || !curLayer->keyExists(frameNumber))
    {
        return;
    }

    mHighlightFrameEnabled = true;
    mHighlightedFrame = frameNumber;

    CameraContextMenu menu(frameNumber, static_cast<const LayerCamera*>(curLayer));

    menu.connect(&menu, &CameraContextMenu::aboutToHide, this, [=] {
        mHighlightFrameEnabled = false;
        mHighlightedFrame = -1;
        update();

        KeyFrame* key = curLayer->getKeyFrameAt(frameNumber);
        if (key->isModified()) {
            emit mEditor->frameModified(frameNumber);
        }
    });

    // Update needs to happen before executing menu, otherwise paint event might be postponed
    update();

    menu.exec(mapToGlobal(pos));
}

void TimeLineCells::drawContent()
{
    if (mCache == nullptr)
    {
        // allocate at physical resolution so text stays sharp on high-DPI displays
        const qreal dpr = devicePixelRatioF();
        mCache = new QPixmap(size() * dpr);
        mCache->setDevicePixelRatio(dpr);
        if (mCache->isNull())
        {
            // fail to create cache
            return;
        }
    }

    QPainter painter(mCache);

    // grey background of the view
    const QPalette palette = QApplication::palette();
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Base));
    painter.drawRect(QRect(0, 0, width(), height()));

    const int widgetWidth = width();

    // Draw non-current layers
    const Object* object = mEditor->object();
    Q_ASSERT(object != nullptr);
    for (int i = 0; i < object->getLayerCount(); i++)
    {
        if (i == mEditor->layers()->currentLayerIndex())
        {
            continue;
        }
        const Layer* layeri = object->getLayer(i);

        if (layeri != nullptr)
        {
            const int layerY = getLayerY(i);
            switch (mType)
            {
            case TIMELINE_CELL_TYPE::Tracks:
                paintTrack(painter, layeri, mOffsetX,
                           layerY, widgetWidth - mOffsetX,
                           mLayerHeight, false, mFrameSize);
                break;

            case TIMELINE_CELL_TYPE::Layers:
                paintLabel(painter, layeri, 0,
                           layerY, widgetWidth - 1,
                           mLayerHeight, false, mEditor->layerVisibility());
                break;
            }
        }
    }

    // Draw current layer
    const Layer* currentLayer = mEditor->layers()->currentLayer();
    if (didDetachLayer())
    {
        int layerYMouseMove = getLayerY(mEditor->layers()->currentLayerIndex()) + mMouseMoveY;
        if (mType == TIMELINE_CELL_TYPE::Tracks)
        {
            paintTrack(painter, currentLayer,
                       mOffsetX, layerYMouseMove,
                       widgetWidth - mOffsetX, mLayerHeight,
                       true, mFrameSize);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers)
        {
            paintLabel(painter, currentLayer,
                       0, layerYMouseMove,
                       widgetWidth - 1, mLayerHeight, true, mEditor->layerVisibility());

            paintLayerGutter(painter);
        }
    }
    else
    {
        if (mType == TIMELINE_CELL_TYPE::Tracks)
        {
            paintTrack(painter,
                       currentLayer,
                       mOffsetX,
                       getLayerY(mEditor->layers()->currentLayerIndex()),
                       widgetWidth - mOffsetX,
                       mLayerHeight,
                       true,
                       mFrameSize);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers)
        {
            paintLabel(painter,
                       currentLayer,
                       0,
                       getLayerY(mEditor->layers()->currentLayerIndex()),
                       widgetWidth - 1,
                       mLayerHeight,
                       true,
                       mEditor->layerVisibility());
        }
    }

    // --- draw track bar background
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Base));
    painter.drawRect(QRect(0, 0, width() - 1, mOffsetY - 1));

    // --- draw bottom line splitter for track bar
    painter.setPen(palette.color(QPalette::Mid));
    painter.drawLine(0, mOffsetY - 2, width() - 1, mOffsetY - 2);

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        // --- draw circle
        painter.setPen(palette.color(QPalette::Text));
        if (mEditor->layerVisibility() == LayerVisibility::CURRENTONLY)
        {
            painter.setBrush(palette.color(QPalette::Base));
        }
        else if (mEditor->layerVisibility() == LayerVisibility::RELATED)
        {
            QColor color = palette.color(QPalette::Text);
            color.setAlpha(128);
            painter.setBrush(color);
        }
        else if (mEditor->layerVisibility() == LayerVisibility::ALL)
        {
            painter.setBrush(palette.brush(QPalette::Text));
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawEllipse(6, 4, 9, 9);
        painter.setRenderHint(QPainter::Antialiasing, false);
    }
    else if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        paintTicks(painter, palette);

        for (int i = 0; i < object->getLayerCount(); i++) {
            paintSelectedFrames(painter, object->getLayer(i), i);
        }
    }
    mRedrawContent = false;
}

void TimeLineCells::paintTicks(QPainter& painter, const QPalette& palette) const
{
    painter.setPen(palette.color(QPalette::Text));
    painter.setBrush(palette.brush(QPalette::Text));
    int fps = mEditor->playback()->fps();
    for (int i = mFrameOffset; i < mFrameOffset + (width() - mOffsetX) / mFrameSize; i++)
    {
        // line x pos + some offset
        const int lineX = getFrameX(i) + 1;
        if (i + 1 >= mTimeLine->getRangeLower() && i < mTimeLine->getRangeUpper())
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette.color(QPalette::Highlight));

            painter.drawRect(lineX, 1, mFrameSize + 1, 2);

            painter.setPen(palette.color(QPalette::Text));
            painter.setBrush(palette.brush(QPalette::Text));
        }

        // Draw large tick at fps mark
        if (i % fps == 0 || i % fps == fps / 2)
        {
            painter.drawLine(lineX, 1, lineX, 5);
        }
        else // draw small tick
        {
            painter.drawLine(lineX, 1, lineX, 3);
        }
        if (i == 0 || i % fps == fps - 1)
        {
            int incr = (i < 9) ? 4 : 0; // poor man’s text centering
            painter.drawText(QPoint(lineX + incr, 17), QString::number(i + 1));
        }
    }
}

void TimeLineCells::paintTrack(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, int frameSize) const
{
    const QPalette palette = QApplication::palette();
    QColor col;
    // Color each track according to the layer type
    if (layer->type() == Layer::BITMAP) col = Theme::LayerBitmap;
    if (layer->type() == Layer::SOUND) col = Theme::LayerSound;
    if (layer->type() == Layer::CAMERA) col = Theme::LayerCamera;
    // Dim invisible layers
    if (!layer->visible()) col.setAlpha(64);

    painter.save();
    painter.setBrush(col);
    painter.setPen(QPen(QBrush(Theme::Border), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRoundedRect(QRectF(x, y - 1, width, height), 4.0, 4.0);

    if (!layer->visible())
    {
        painter.restore();
        return;
    }

    // Changes the appearance if selected
    if (selected)
    {
        paintSelection(painter, x, y, width, height);
    }
    else
    {
        painter.save();
        QLinearGradient linearGrad(QPointF(0, y), QPointF(0, y + height));
        linearGrad.setColorAt(0, QColor(255,255,255,70));
        linearGrad.setColorAt(1, QColor(0,0,0,0));
        painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        painter.setBrush(linearGrad);
        painter.drawRect(x, y - 1, width, height);
        painter.restore();
    }

    paintFrames(painter, col, layer, y, height, selected, frameSize);

    painter.restore();
}

int TimeLineCells::blockLengthFor(const Layer* layer, const KeyFrame* key) const
{
    if (mTrimming && key->pos() == mTrimKeyPos)
    {
        return mTrimPreviewLength;
    }
    const int end = layer->getBlockEnd(key);
    if (end < 0)
    {
        return 1; // open-ended hold (last keyframe, auto length): single cell
    }
    return qMax(1, end - key->pos());
}

void TimeLineCells::paintFrames(QPainter& painter, QColor trackCol, const Layer* layer, int y, int height, bool selected, int frameSize) const
{
    painter.setPen(QPen(QBrush(Theme::TimelineFrameBorder), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    int recTop = y + 1;
    int standardWidth = frameSize - 2;

    int recHeight = height - 4;

    const QList<int> selectedFrames = layer->getSelectedFramesByPos();

    if (layer->type() == Layer::CAMERA)
    {
        // Camera keyframes are interpolated: keep the classic single-cell look
        layer->foreachKeyFrame([&](KeyFrame* key)
        {
            int framePos = key->pos();
            int recWidth = standardWidth;
            int recLeft = getFrameX(framePos) - recWidth;

            if (selectedFrames.contains(framePos)) {
                return;
            }

            if (selected)
            {
                painter.setBrush(QColor(trackCol.red(), trackCol.green(), trackCol.blue(), 150));
            }
            else
            {
                painter.setBrush(Theme::TimelineFrameFill);
            }

            painter.drawRoundedRect(QRectF(recLeft, recTop, recWidth, recHeight), 3.0, 3.0);
        });
        return;
    }

    // Bitmap & sound layers render Dreams-style exposure blocks:
    // the block spans from the keyframe position to the start of the next keyframe
    // (auto length) or exactly its trimmed length (explicit).
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        int framePos = key->pos();
        int recLeft = getFrameX(framePos) - standardWidth;

        // Selected frames are painted separately
        if (selectedFrames.contains(framePos)) {
            return;
        }

        int blockLen = blockLengthFor(layer, key);
        int recWidth = standardWidth + (blockLen - 1) * frameSize;

        if (selected)
        {
            painter.setBrush(QColor(trackCol.red(), trackCol.green(), trackCol.blue(), 150));
        }
        else
        {
            painter.setBrush(Theme::TimelineFrameFill);
        }

        painter.drawRoundedRect(QRectF(recLeft, recTop, recWidth, recHeight), 3.0, 3.0);

        // Keyframe marker at the block start
        painter.setPen(Qt::NoPen);
        painter.setBrush(trackCol);
        painter.drawEllipse(QRectF(recLeft + 3.0, recTop + recHeight / 2.0 - 2.0, 4.0, 4.0));
        painter.setPen(QPen(QBrush(Theme::TimelineFrameBorder), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    });
}

void TimeLineCells::paintCurrentFrameBorder(QPainter &painter, int recLeft, int recTop, int recWidth, int recHeight) const
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Theme::TimelineCurrentFrameBorder, 2));
    painter.drawRoundedRect(QRectF(recLeft, recTop, recWidth, recHeight), 3.0, 3.0);
    painter.restore();
}

void TimeLineCells::paintFrameCursorOnCurrentLayer(QPainter &painter, int recTop, int recWidth, int recHeight) const
{
    int recLeft = getFrameX(mFramePosMoveX) - recWidth;

    painter.save();
    const QPalette palette = QApplication::palette();
    // Don't fill
    painter.setBrush(Qt::NoBrush);
    // paint border
    QColor penColor = palette.color(QPalette::WindowText);
    penColor.setAlpha(127);
    painter.setPen(penColor);
    painter.drawRect(recLeft, recTop, recWidth, recHeight);
    painter.restore();
}

void TimeLineCells::paintHighlightedFrame(QPainter& painter, int framePos, int recTop, int recWidth, int recHeight) const
{
    int recLeft = getFrameX(framePos) - recWidth;

    painter.save();
    const QPalette palette = QApplication::palette();
    painter.setBrush(palette.color(QPalette::Window));
    painter.setPen(palette.color(QPalette::WindowText));

    // Draw a rect slighly smaller than the frame
    painter.drawRect(recLeft+1, recTop+1, recWidth-1, recHeight-1);
    painter.restore();
}

void TimeLineCells::paintSelectedFrames(QPainter& painter, const Layer* layer, const int layerIndex) const
{
    const QList<int> selectedFrames = layer->getSelectedFramesByPos();
    if (selectedFrames.isEmpty()) { return; }

    const int standardWidth = mFrameSize - 2;
    const int recHeight = mLayerHeight - 4;

    // The moving preview follows the drag: normally on this row, on the drop row for cross-layer drags
    bool previewing = mMovingFrames && layerIndex == mCurrentLayerNumber;
    int previewRow = layerIndex;
    if (previewing && mDropTargetLayer != -1)
    {
        previewRow = mDropTargetLayer;
    }
    const int recTop = getLayerY(previewRow) + 1;
    const int lift = previewing ? -4 : 0;

    // Horizontal offset (in frames) of the preview position
    int dx = 0;
    if (previewing)
    {
        const int posUnderCursor = getFrameNumber(mMousePressX);
        dx = mFramePosMoveX - posUnderCursor;
        if (mDropTargetLayer != -1)
        {
            dx += mDropShiftFrames;
        }
    }

    painter.save();
    painter.setBrush(Theme::TimelineSelectedFrameFill);
    painter.setPen(QPen(QBrush(Theme::Accent), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    // Merge consecutive blocks (block end == next selected start) into one rounded run
    int i = 0;
    while (i < selectedFrames.count())
    {
        const int runStart = selectedFrames[i];
        int runEndExclusive = runStart + 1;

        while (i < selectedFrames.count())
        {
            const int pos = selectedFrames[i];
            const KeyFrame* key = layer->getKeyFrameAt(pos);
            const int len = (layer->type() == Layer::CAMERA || key == nullptr) ? 1 : blockLengthFor(layer, key);
            runEndExclusive = pos + len;
            if (i + 1 < selectedFrames.count() && selectedFrames[i + 1] == runEndExclusive)
            {
                ++i; // contiguous block, extend the run
            }
            else
            {
                break;
            }
        }
        ++i;

        const int blockLen = runEndExclusive - runStart;
        const int recWidth = standardWidth + (blockLen - 1) * mFrameSize;
        const int recLeft = getFrameX(runStart + dx) - standardWidth;
        painter.drawRoundedRect(QRectF(recLeft, recTop + lift, recWidth, recHeight), 3.0, 3.0);

        // One marker dot per selected keyframe inside the run
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::Accent);
        for (int framePos : selectedFrames)
        {
            if (framePos >= runStart && framePos < runEndExclusive)
            {
                painter.drawEllipse(QRectF(getFrameX(framePos + dx) - standardWidth + 3.0,
                                           recTop + lift + recHeight / 2.0 - 2.0, 4.0, 4.0));
            }
        }
        painter.setPen(QPen(QBrush(Theme::Accent), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Theme::TimelineSelectedFrameFill);
    }

    painter.restore();
}

void TimeLineCells::paintLabel(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, LayerVisibility layerVisibility) const
{
    const QPalette palette = QApplication::palette();

    // Row background: rounded card, selected rows get a subtle raised tone
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (selected)
    {
        painter.setBrush(Theme::TimelineRowAlternate);
    }
    else
    {
        painter.setBrush(palette.color(QPalette::Base));
    }
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(x + 2, y + 1, width - 4, height - 2), 5.0, 5.0);

    // Selected rows carry an accent side bar
    if (selected)
    {
        painter.setBrush(Theme::Accent);
        painter.drawRoundedRect(QRectF(x + 2, y + 6, 3.0, height - 12), 1.5, 1.5);
    }

    if (!layer->visible())
    {
        painter.setBrush(palette.color(QPalette::Base));
    }
    else
    {
        if ((layerVisibility == LayerVisibility::ALL) || selected)
        {
            painter.setBrush(palette.color(QPalette::Text));
        }
        else if (layerVisibility == LayerVisibility::CURRENTONLY)
        {
            painter.setBrush(palette.color(QPalette::Base));
        }
        else if (layerVisibility == LayerVisibility::RELATED)
        {
            QColor color = palette.color(QPalette::Text);
            color.setAlpha(128);
            painter.setBrush(color);
        }
    }
    if (selected)
    {
        painter.setPen(palette.color(QPalette::HighlightedText));
    }
    else
    {
        painter.setPen(palette.color(QPalette::Text));
    }
    painter.drawEllipse(QRectF(x + 10, y + (height - 9) / 2.0, 9.0, 9.0));

    if (layer->type() == Layer::BITMAP) painter.drawPixmap(QPoint(28, y + (height - 20) / 2), QPixmap(":icons/themes/playful/timeline/cell-bitmap.svg").scaledToHeight(18, Qt::SmoothTransformation));
    if (layer->type() == Layer::SOUND) painter.drawPixmap(QPoint(28, y + (height - 20) / 2), QPixmap(":icons/themes/playful/timeline/cell-sound.svg").scaledToHeight(18, Qt::SmoothTransformation));
    if (layer->type() == Layer::CAMERA) painter.drawPixmap(QPoint(28, y + (height - 20) / 2), QPixmap(":icons/themes/playful/timeline/cell-camera.svg").scaledToHeight(18, Qt::SmoothTransformation));

    if (selected)
    {
        painter.setPen(Theme::AccentHover);
    }
    else
    {
        painter.setPen(palette.color(QPalette::Text));
    }
    painter.drawText(QPoint(52, y + height / 2 + 4), layer->name());
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void TimeLineCells::paintSelection(QPainter& painter, int x, int y, int width, int height) const
{
    QLinearGradient linearGrad(QPointF(0, y), QPointF(0, y + height));
    linearGrad.setColorAt(0, QColor(255, 255, 255, 60));
    linearGrad.setColorAt(1, QColor(255, 255, 255, 0));
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Overlay);
    painter.setBrush(linearGrad);
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, y, width, height - 1);
    painter.restore();
}

void TimeLineCells::paintLayerGutter(QPainter& painter) const
{
    painter.setPen(Theme::Accent);
    if (mMouseMoveY > mLayerDetachThreshold)
    {
        painter.drawRect(0, getLayerY(getInbetweenLayerNumber(mEndY))+mLayerHeight, width(), 2);
    }
    else
    {
        painter.drawRect(0, getLayerY(getInbetweenLayerNumber(mEndY)), width(), 2);
    }
}

void TimeLineCells::paintOnionSkin(QPainter& painter) const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }

    int frameNumber = mEditor->currentFrame();

    int prevOnionSkinCount = mEditor->preference()->getInt(SETTING::ONION_PREV_FRAMES_NUM);
    int nextOnionSkinCount = mEditor->preference()->getInt(SETTING::ONION_NEXT_FRAMES_NUM);

    bool isAbsolute = (mEditor->preference()->getString(SETTING::ONION_TYPE) == "absolute");

    if (mEditor->preference()->isOn(SETTING::PREV_ONION) && prevOnionSkinCount > 0)
    {
        int onionFrameNumber = frameNumber;
        if (isAbsolute)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber+1, true);
        }
        onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute);
        int onionPosition = 0;

        while (onionPosition < prevOnionSkinCount && onionFrameNumber > 0)
        {
            painter.setBrush(QColor(128, 128, 128, 128));
            painter.setPen(Qt::NoPen);
            QRect onionRect;
            onionRect.setTopLeft(QPoint(getFrameX(onionFrameNumber - 1), 0));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), height()));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), 23));
            painter.drawRect(onionRect);

            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute);
            onionPosition++;
        }
    }

    if (mEditor->preference()->isOn(SETTING::NEXT_ONION) && nextOnionSkinCount > 0) {

        int onionFrameNumber = layer->getNextFrameNumber(frameNumber, isAbsolute);
        int onionPosition = 0;

        while (onionPosition < nextOnionSkinCount && onionFrameNumber > 0)
        {
            painter.setBrush(QColor(128, 128, 128, 128));
            painter.setPen(Qt::NoPen);
            QRect onionRect;
            onionRect.setTopLeft(QPoint(getFrameX(onionFrameNumber - 1), 0));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), height()));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), 23));
            painter.drawRect(onionRect);

            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, isAbsolute);
            onionPosition++;
        }
    }
}

void TimeLineCells::paintEvent(QPaintEvent*)
{
    const QPalette palette = QApplication::palette();
    QPainter painter(this);

    bool isPlaying = mEditor->playback()->isPlaying();
    if (mCache == nullptr || mRedrawContent || trackScrubber())
    {
        drawContent();
    }
    if (mCache)
    {
        painter.drawPixmap(QPoint(0, 0), *mCache);
    }

    if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        if (!isPlaying) {
            paintOnionSkin(painter);
        }

        int currentFrame = mEditor->currentFrame();
        Layer* currentLayer = mEditor->layers()->currentLayer();
        KeyFrame* keyFrame = currentLayer->getKeyFrameWhichCovers(currentFrame);
        if (keyFrame != nullptr)
        {
            int blockLen = (currentLayer->type() == Layer::CAMERA) ? 1 : blockLengthFor(currentLayer, keyFrame);
            int recWidth = mFrameSize - 2 + (blockLen - 1) * mFrameSize;
            int recLeft = getFrameX(keyFrame->pos()) - (mFrameSize - 2);
            paintCurrentFrameBorder(painter, recLeft, getLayerY(mEditor->currentLayerIndex()) + 1, recWidth, mLayerHeight - 4);
        }

        if (!mMovingFrames && mLayerPosMoveY != -1 && mLayerPosMoveY == mEditor->currentLayerIndex())
        {
            // This is terrible but well...
            int recTop = getLayerY(mLayerPosMoveY) + 1;
            int standardWidth = mFrameSize - 2;
            int recHeight = mLayerHeight - 4;

            if (mHighlightFrameEnabled)
            {
                paintHighlightedFrame(painter, mHighlightedFrame, recTop, standardWidth, recHeight);
            }
            if (currentLayer->visible())
            {
                paintFrameCursorOnCurrentLayer(painter, recTop, standardWidth, recHeight);
            }
        }

        // --- draw the position of the current frame
        if (currentFrame > mFrameOffset)
        {
            QColor scrubColor = Theme::TimelinePlayhead;
            scrubColor.setAlpha(160);
            painter.setBrush(scrubColor);
            painter.setPen(Qt::NoPen);

            int currentFrameStartX = getFrameX(currentFrame - 1) + 1;
            int currentFrameEndX = getFrameX(currentFrame);
            QRect scrubRect;
            scrubRect.setTopLeft(QPoint(currentFrameStartX, 0));
            scrubRect.setBottomRight(QPoint(currentFrameEndX, height()));
            if (mbShortScrub)
            {
                scrubRect.setBottomRight(QPoint(currentFrameEndX, 23));
            }
            painter.save();

            bool mouseUnderScrubber = currentFrame == mFramePosMoveX;
            if (mouseUnderScrubber) {
                QRect smallScrub = QRect(QPoint(currentFrameStartX, 0), QPoint(currentFrameEndX, 23));
                QPen pen = scrubColor;
                pen.setWidth(2);
                painter.setPen(pen);
                painter.drawRect(smallScrub);
                painter.setBrush(Qt::NoBrush);
            }
            painter.drawRect(scrubRect);
            painter.restore();

            painter.setPen(palette.color(QPalette::HighlightedText));
            int incr = (currentFrame < 10) ? 4 : 0;
            painter.drawText(QPoint(currentFrameStartX + incr, 17),
                             QString::number(currentFrame));
        }
    }
}

void TimeLineCells::resizeEvent(QResizeEvent* event)
{
    clearCache();
    updateContent();
    event->accept();
    emit lengthChanged(getFrameLength());
}

bool TimeLineCells::event(QEvent* event)
{
    if (event->type() == QEvent::Leave) {
        onDidLeaveWidget();
    }

    return QWidget::event(event);
}

void TimeLineCells::mousePressEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());
    mCurrentLayerNumber = layerNumber;

    mMousePressX = event->pos().x();
    mFromLayer = mToLayer = layerNumber;

    mStartY = event->pos().y();
    mStartLayerNumber = layerNumber;
    mEndY = event->pos().y();

    mStartFrameNumber = frameNumber;
    mLastFrameNumber = mStartFrameNumber;

    mCanMoveFrame = false;
    mMovingFrames = false;

    mCanBoxSelect = false;
    mBoxSelecting = false;

    mClickSelecting = false;

    mWholeLayerMode = false;
    mDropTargetLayer = -1;
    mDropShiftFrames = 0;
    mTrimming = false;
    mTrimKeyPos = -1;

    primaryButton = event->button();

    switch (mType)
    {
    case TIMELINE_CELL_TYPE::Layers:
        if (layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
        {
            if (event->pos().x() < 15)
            {
                mEditor->switchVisibilityOfLayer(layerNumber);
            }
            else if (mEditor->currentLayerIndex() != layerNumber)
            {
                mEditor->layers()->setCurrentLayer(layerNumber);
                mEditor->layers()->currentLayer()->deselectAll();
            }
        }
        if (layerNumber == -1)
        {
            if (event->pos().x() < 15)
            {
                if (event->button() == Qt::LeftButton) {
                    mEditor->increaseLayerVisibilityIndex();
                } else if (event->button() == Qt::RightButton) {
                    mEditor->decreaseLayerVisibilityIndex();
                }
            }
        }
        break;
    case TIMELINE_CELL_TYPE::Tracks:
        if (event->button() == Qt::MiddleButton)
        {
            mLastFrameNumber = getFrameNumber(event->pos().x());
        }
        else
        {
            // Dreams-style trim: grabbing the right edge of a bitmap block adjusts its length
            if (event->button() == Qt::LeftButton && layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
            {
                int trimPos = hitTestTrimHandle(event->pos());
                if (trimPos > 0)
                {
                    Layer* trimLayer = mEditor->object()->getLayer(layerNumber);
                    KeyFrame* trimKey = trimLayer->getKeyFrameAt(trimPos);
                    if (trimKey != nullptr)
                    {
                        if (mEditor->currentLayerIndex() != layerNumber)
                        {
                            mEditor->layers()->currentLayer()->deselectAll();
                            mEditor->layers()->setCurrentLayer(layerNumber);
                            emit mEditor->selectedFramesChanged();
                        }
                        mTrimming = true;
                        mTrimKeyPos = trimPos;
                        int trimEnd = trimLayer->getBlockEnd(trimKey);
                        mTrimOriginalLength = (trimEnd > 0) ? (trimEnd - trimPos) : 1;
                        mTrimPreviewLength = mTrimOriginalLength;
                        updateContent();
                        break;
                    }
                }
            }

            if (frameNumber == mEditor->currentFrame() && mStartY < 20)
            {
                if (mEditor->playback()->isPlaying())
                {
                    mEditor->playback()->stop();
                }
                mTimeLine->scrubbing = true;
            }
            else
            {
                if ((layerNumber != -1) && layerNumber < mEditor->object()->getLayerCount())
                {
                    int previousLayerNumber = mEditor->layers()->currentLayerIndex();

                    if (previousLayerNumber != layerNumber)
                    {
                        Layer *previousLayer = mEditor->object()->getLayer(previousLayerNumber);
                        previousLayer->deselectAll();
                        emit mEditor->selectedFramesChanged();
                        mEditor->layers()->setCurrentLayer(layerNumber);
                    }

                    Layer *currentLayer = mEditor->object()->getLayer(layerNumber);

                    // Check if we are using the alt key
                    if (event->modifiers() == Qt::AltModifier)
                    {
                        // If it is the case, we select everything that is after the selected frame
                        mClickSelecting = true;
                        mCanMoveFrame = true;

                        currentLayer->selectAllFramesAfter(frameNumber);
                        emit mEditor->selectedFramesChanged();
                    }
                    // Check if we are clicking on a non selected frame
                    else if (!currentLayer->isFrameSelected(frameNumber))
                    {
                        // If it is the case, we select it if it is the left button...
                        mCanBoxSelect = true;
                        mClickSelecting = true;
                        if (event->button() == Qt::LeftButton)
                        {

                            if (event->modifiers() == Qt::ControlModifier)
                            {
                                // Add/remove from already selected
                                currentLayer->toggleFrameSelected(frameNumber, true);
                                emit mEditor->selectedFramesChanged();
                            }
                            else if (event->modifiers() == Qt::ShiftModifier)
                            {
                                // Select a range from the last selected
                                currentLayer->extendSelectionTo(frameNumber);
                                emit mEditor->selectedFramesChanged();
                            }
                            else
                            {
                                // Only select if left button clicked
                                currentLayer->toggleFrameSelected(frameNumber, false);
                                emit mEditor->selectedFramesChanged();
                            }
                        }

                        // ... or we show the camera context menu, if it is the right button
                        if (event->button() == Qt::RightButton)
                        {
                            showCameraMenu(event->pos());
                        }

                    }
                    else
                    {
                        // If selected they can also be interpolated
                        if (event->button() == Qt::RightButton)
                        {
                            showCameraMenu(event->pos());
                        }
                        // We clicked on a selected frame, we can move it
                        mCanMoveFrame = true;
                    }

                    if (currentLayer->hasAnySelectedFrames()) {
                        emit selectionChanged();
                    }

                    mTimeLine->updateContent();
                }
                else
                {
                    if (frameNumber > 0)
                    {
                        if (mEditor->playback()->isPlaying())
                        {
                            mEditor->playback()->stop();
                        }
                        if (mEditor->playback()->getSoundScrubActive() && mLastScrubFrame != frameNumber)
                        {
                            mEditor->playback()->playScrub(frameNumber);
                            mLastScrubFrame = frameNumber;
                        }

                        mEditor->scrubTo(frameNumber);

                        mTimeLine->scrubbing = true;
                        qDebug("Scrub to %d frame", frameNumber);
                    }
                }
            }
        }
        break;
    }
}

void TimeLineCells::mouseMoveEvent(QMouseEvent* event)
{
    mMouseMoveX = event->pos().x();
    mFramePosMoveX = getFrameNumber(mMouseMoveX);
    mLayerPosMoveY = getLayerNumber(event->pos().y());

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        if (event->buttons() & Qt::LeftButton ) {
            mEndY = event->pos().y();
            emit mouseMovedY(mEndY - mStartY);
        }
    }
    else if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        if (mTrimming)
        {
            Layer* layer = mEditor->layers()->getLayer(mCurrentLayerNumber);
            KeyFrame* key = (layer != nullptr) ? layer->getKeyFrameAt(mTrimKeyPos) : nullptr;
            if (key != nullptr)
            {
                // The block cannot swallow the next keyframe
                int nextPos = layer->getNextKeyFramePosition(mTrimKeyPos);
                int maxLen = (nextPos > mTrimKeyPos) ? (nextPos - mTrimKeyPos)
                                                     : (mFrameLength - mTrimKeyPos + 1);
                int newLen = mFramePosMoveX - mTrimKeyPos + 1;
                mTrimPreviewLength = qBound(1, newLen, maxLen);
                updateContent();
            }
            return;
        }

        if (primaryButton == Qt::MiddleButton)
        {
            mFrameOffset = qMin(qMax(0, mFrameLength - width() / getFrameSize()), qMax(0, mFrameOffset + mLastFrameNumber - mFramePosMoveX));
            update();
            emit offsetChanged(mFrameOffset);
        }
        else
        {
            if (mTimeLine->scrubbing)
            {
                if (mEditor->playback()->getSoundScrubActive() && mLastScrubFrame != mFramePosMoveX)
                {
                    mEditor->playback()->playScrub(mFramePosMoveX);
                    mLastScrubFrame = mFramePosMoveX;
                }
                mEditor->scrubTo(mFramePosMoveX);
            }
            else
            {
                if (event->buttons() & Qt::LeftButton) {
                    if (mStartLayerNumber != -1 && mStartLayerNumber < mEditor->object()->getLayerCount())
                    {
                        Layer *currentLayer = mEditor->object()->getLayer(mStartLayerNumber);

                        // Check if the frame we clicked was selected
                        if (mCanMoveFrame) {

                            // Ctrl + drag = Dreams-style whole-track grab: every frame of the layer follows
                            if ((event->modifiers() & Qt::ControlModifier) && !mWholeLayerMode)
                            {
                                mWholeLayerMode = true;
                                currentLayer->selectAllFramesAfter(currentLayer->firstKeyFramePosition());
                                emit mEditor->selectedFramesChanged();
                            }

                            // If it is the case, we move the selected frames in the layer
                            mMovingFrames = true;

                            // Vertical drag onto another row of the same type = cross-layer move
                            mDropTargetLayer = -1;
                            mDropShiftFrames = 0;
                            if (mLayerPosMoveY >= 0 && mLayerPosMoveY < mEditor->object()->getLayerCount()
                                && mLayerPosMoveY != mCurrentLayerNumber)
                            {
                                Layer* srcLayer = mEditor->object()->getLayer(mCurrentLayerNumber);
                                Layer* tgtLayer = mEditor->object()->getLayer(mLayerPosMoveY);
                                if (srcLayer != nullptr && tgtLayer != nullptr && srcLayer->type() == tgtLayer->type())
                                {
                                    mDropTargetLayer = mLayerPosMoveY;

                                    // Find the smallest shift so the whole selection lands on free spots
                                    const QList<int> sel = srcLayer->selectedKeyFramesPositions();
                                    const int posUnderCursor = getFrameNumber(mMousePressX);
                                    const int dx = mFramePosMoveX - posUnderCursor;
                                    int shift = 0;
                                    auto collides = [&sel, tgtLayer, dx](int s) {
                                        for (int p : sel) {
                                            int np = p + dx + s;
                                            if (np < 1 || tgtLayer->keyExists(np)) { return true; }
                                        }
                                        return false;
                                    };
                                    while (collides(shift) && shift < mFrameLength) { shift++; }
                                    mDropShiftFrames = shift;
                                }
                            }
                        }
                        else if (mCanBoxSelect)
                        {
                            // Otherwise, we do a box select
                            mBoxSelecting = true;

                            currentLayer->deselectAll();
                            currentLayer->setFrameSelected(mStartFrameNumber, true);
                            currentLayer->extendSelectionTo(mFramePosMoveX);
                            emit mEditor->selectedFramesChanged();
                        }
                        mLastFrameNumber = mFramePosMoveX;
                        updateContent();
                    }
                }
                else if (event->buttons() == Qt::NoButton)
                {
                    // Hover feedback: resize cursor over a block edge
                    if (hitTestTrimHandle(event->pos()) > 0)
                    {
                        setCursor(Qt::SizeHorCursor);
                    }
                    else
                    {
                        setCursor(Qt::ArrowCursor);
                    }
                }
                update();
            }
        }
    }
}

void TimeLineCells::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != primaryButton) return;

    int frameNumber = getFrameNumber(event->pos().x());
    if (frameNumber < 1) frameNumber = 1;
    int layerNumber = getLayerNumber(event->pos().y());

    if (mType == TIMELINE_CELL_TYPE::Tracks && mCurrentLayerNumber != -1 && primaryButton != Qt::MiddleButton)
    {
        // We should affect the current layer based on what's selected, not where the mouse currently is.
        Layer* currentLayer = mEditor->layers()->getLayer(mCurrentLayerNumber);
        Q_ASSERT(currentLayer);

        if (mTrimming)
        {
            mTrimming = false;
            KeyFrame* trimKey = currentLayer->getKeyFrameAt(mTrimKeyPos);
            if (trimKey != nullptr && mTrimPreviewLength != mTrimOriginalLength)
            {
                // BitmapReplaceCommand snapshots the redo state at the current frame,
                // so the scrubber must sit on the trimmed block before recording
                mEditor->scrubTo(mTrimKeyPos);
                SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
                trimKey->setLength(mTrimPreviewLength);
                trimKey->setLengthExplicit(true);
                currentLayer->markFrameAsDirty(mTrimKeyPos);
                mEditor->undoRedo()->record(saveStateId, tr("Trim Frame"));
            }
            mTrimKeyPos = -1;
            mEditor->layers()->notifyAnimationLengthChanged();
            emit mEditor->framesModified();
            updateContent();
        }
        else if (mMovingFrames && mDropTargetLayer != -1 && mDropTargetLayer != mCurrentLayerNumber)
        {
            // Vertical drag onto another track: carry the selected frames over
            moveSelectedFramesAcrossLayers(mCurrentLayerNumber, mDropTargetLayer);
            mEditor->layers()->notifyAnimationLengthChanged();
            emit mEditor->framesModified();
            updateContent();
        }
        else if (mMovingFrames)
        {
            int posUnderCursor = getFrameNumber(mMousePressX);
            int offset = frameNumber - posUnderCursor;

            if (currentLayer->canMoveSelectedFramesToOffset(offset)) {
                SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MOVE);
                UserSaveState userState;
                userState.moveFramesState = MoveFramesSaveState(offset, currentLayer->selectedKeyFramesPositions());
                mEditor->undoRedo()->addUserState(saveStateId, userState);

                currentLayer->moveSelectedFrames(offset);
                mEditor->undoRedo()->record(saveStateId, tr("Move Frames"));
            }
            mEditor->layers()->notifyAnimationLengthChanged();
            emit mEditor->framesModified();
            updateContent();
        }
        else if (!mTimeLine->scrubbing && !mMovingFrames && !mClickSelecting && !mBoxSelecting)
        {
            // De-selecting if we didn't move, scrub nor select anything
            bool multipleSelection = (event->modifiers() == Qt::ControlModifier);

            // Add/remove from already selected
            currentLayer->toggleFrameSelected(frameNumber, multipleSelection);
            emit mEditor->selectedFramesChanged();
            updateContent();
        }
    }
    if (mType == TIMELINE_CELL_TYPE::Layers && !mScrollingVertically && layerNumber != mStartLayerNumber && mStartLayerNumber != -1 && layerNumber != -1)
    {
        mToLayer = getInbetweenLayerNumber(event->pos().y());
        if (mToLayer != mFromLayer && mToLayer > -1 && mToLayer < mEditor->layers()->count())
        {
            // Bubble the from layer up or down to the to layer
            if (mToLayer < mFromLayer) // bubble up
            {
                for (int i = mFromLayer - 1; i >= mToLayer; i--)
                    mEditor->swapLayers(i, i + 1);
            }
            else // bubble down
            {
                for (int i = mFromLayer + 1; i <= mToLayer; i++)
                    mEditor->swapLayers(i, i - 1);
            }
        }
    }

    if (mType == TIMELINE_CELL_TYPE::Layers && event->button() == Qt::LeftButton)
    {
        emit mouseMovedY(0);
    }

    primaryButton = Qt::NoButton;
    mEndY = mStartY;
    mTimeLine->scrubbing = false;
    mMovingFrames = false;
    mWholeLayerMode = false;
    mDropTargetLayer = -1;
    mDropShiftFrames = 0;
}

void TimeLineCells::mouseDoubleClickEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());

    // -- short scrub --
    if (event->pos().y() < 20 && (mType != TIMELINE_CELL_TYPE::Layers || event->pos().x() >= 15))
    {
        mPrefs->set(SETTING::SHORT_SCRUB, !mbShortScrub);
    }

    // -- layer --
    Layer* layer = mEditor->object()->getLayer(layerNumber);
    if (layer && event->buttons() & Qt::LeftButton)
    {
        if (mType == TIMELINE_CELL_TYPE::Tracks && (layerNumber != -1) && (frameNumber > 0) && layerNumber < mEditor->object()->getLayerCount())
        {
            if (!layer->keyExistsWhichCovers(frameNumber))
            {
                mEditor->scrubTo(frameNumber);
            }

            // The release event will toggle the frame on again, so we make sure it gets
            // deselected now instead.
            layer->setFrameSelected(frameNumber, true);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers && event->pos().x() >= 15)
        {
            editLayerProperties(layer);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimeLineCells::editLayerProperties(Layer *layer) const
{
    if (layer->type() != Layer::CAMERA)
    {
        editLayerName(layer);
        return;
    }

    auto cameraLayer = dynamic_cast<LayerCamera*>(layer);
    Q_ASSERT(cameraLayer);
    editLayerProperties(cameraLayer);
}

void TimeLineCells::editLayerProperties(LayerCamera* cameraLayer) const
{
    QRegularExpression regex("([\\x{FFEF}-\\x{FFFF}])+");

    CameraPropertiesDialog dialog(cameraLayer->name(), cameraLayer->getViewRect().width(),
                                  cameraLayer->getViewRect().height());
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    QString name = dialog.getName().replace(regex, "");

    if (!name.isEmpty())
    {
        mEditor->layers()->renameLayer(cameraLayer, name);
    }
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(SETTING_FIELD_W, dialog.getWidth());
    settings.setValue(SETTING_FIELD_H, dialog.getHeight());
    cameraLayer->setViewRect(QRect(-dialog.getWidth() / 2, -dialog.getHeight() / 2, dialog.getWidth(), dialog.getHeight()));
    mEditor->view()->forceUpdateViewTransform();
}

void TimeLineCells::editLayerName(Layer* layer) const
{
    QRegularExpression regex("([\\x{FFEF}-\\x{FFFF}])+");

    bool ok;
    QString name = QInputDialog::getText(nullptr, tr("Layer Properties"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         layer->name(), &ok);
    name.replace(regex, "");
    if (!ok || name.isEmpty())
    {
        return;
    }

    mEditor->layers()->renameLayer(layer, name);
}

void TimeLineCells::hScrollChange(int x)
{
    mFrameOffset = x;
    updateContent();
}

int TimeLineCells::hitTestTrimHandle(const QPoint& pos) const
{
    if (mType != TIMELINE_CELL_TYPE::Tracks) { return -1; }

    const int layerNumber = getLayerNumber(pos.y());
    if (layerNumber < 0 || layerNumber >= mEditor->object()->getLayerCount()) { return -1; }

    Layer* layer = mEditor->object()->getLayer(layerNumber);
    if (layer == nullptr || layer->type() != Layer::BITMAP) { return -1; }

    const int frameNumber = getFrameNumber(pos.x());
    KeyFrame* key = layer->getKeyFrameWhichCovers(frameNumber);
    if (key == nullptr) { return -1; }

    int blockEnd = layer->getBlockEnd(key);
    if (blockEnd < 0) { blockEnd = key->pos() + 1; } // open-ended hold: single-cell block

    // The visual right edge of the block is the right border of its last frame cell
    const int edgeX = getFrameX(blockEnd - 1);
    return (qAbs(pos.x() - edgeX) <= 4) ? key->pos() : -1;
}

void TimeLineCells::moveSelectedFramesAcrossLayers(int sourceIndex, int targetIndex)
{
    Layer* source = mEditor->layers()->getLayer(sourceIndex);
    Layer* target = mEditor->layers()->getLayer(targetIndex);
    if (source == nullptr || target == nullptr || source == target) { return; }
    if (source->type() != target->type()) { return; }

    const QList<int> positions = source->selectedKeyFramesPositions();
    if (positions.isEmpty()) { return; }

    mEditor->backup(tr("Move Frames to Layer"));

    const int posUnderCursor = getFrameNumber(mMousePressX);
    int dx = mFramePosMoveX - posUnderCursor + mDropShiftFrames;

    // Keep every frame inside the timeline
    const int minPos = positions.first();
    if (minPos + dx < 1) { dx = 1 - minPos; }

    target->deselectAll();

    // Take the selected frames out of the source layer; ownership travels with them
    QVector<QPair<int, KeyFrame*>> taken;
    for (int pos : positions)
    {
        KeyFrame* key = source->takeKeyFrame(pos);
        if (key != nullptr)
        {
            taken.append(qMakePair(pos, key));
        }
    }

    for (const auto& pair : taken)
    {
        const int newPos = pair.first + dx;
        KeyFrame* key = pair.second;

        if (target->keyExists(newPos))
        {
            // Should not happen (shift avoided collisions); put it back as a fallback
            source->addKeyFrame(pair.first, key);
            continue;
        }

        target->addKeyFrame(newPos, key);
        if (target->type() == Layer::SOUND)
        {
            auto soundClip = static_cast<SoundClip*>(key);
            mEditor->sound()->loadSound(soundClip, soundClip->fileName());
        }
        target->setFrameSelected(newPos, true);
    }

    source->deselectAll();
}

void TimeLineCells::vScrollChange(int x)
{
    mLayerOffset = x;
    mScrollingVertically = true;
    updateContent();
}

void TimeLineCells::onScrollingVerticallyStopped()
{
    mScrollingVertically = false;
}

void TimeLineCells::setMouseMoveY(int x)
{
    mMouseMoveY = x;
    updateContent();
}

bool TimeLineCells::trackScrubber()
{
    if (mType != TIMELINE_CELL_TYPE::Tracks ||
        (mPrevFrame == mEditor->currentFrame() && !mEditor->playback()->isPlaying()))
    {
        return false;
    }
    mPrevFrame = mEditor->currentFrame();

    if (mEditor->currentFrame() <= mFrameOffset)
    {
        // Move the timeline back if the scrubber is offscreen to the left
        mFrameOffset = mEditor->currentFrame() - 1;
        emit offsetChanged(mFrameOffset);
        return true;
    }
    else if (width() < (mEditor->currentFrame() - mFrameOffset + 1) * mFrameSize)
    {
        // Move timeline forward if the scrubber is offscreen to the right
        if (mEditor->playback()->isPlaying())
            mFrameOffset = mFrameOffset + ((mEditor->currentFrame() - mFrameOffset) / 2);
        else
            mFrameOffset = mEditor->currentFrame() - width() / mFrameSize;
        emit offsetChanged(mFrameOffset);
        return true;
    }
    return false;
}

void TimeLineCells::onDidLeaveWidget()
{
    // Reset last known frame pos to avoid wrong UI states when leaving the widget
    mFramePosMoveX = 0;
    update();
}
