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
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QDebug>
#include <QWheelEvent>
#include <QTimer>
#include <algorithm>
#include <QThreadPool>
#include <QRunnable>
#include "layerbitmap.h"
#include "colorizeimage.h"
#include "bitmapimage.h"

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
#include "scribblearea.h"
#include "undoredomanager.h"
#include "layerlayoutcommand.h"
#include "timeline.h"

#include "cameracontextmenu.h"

TimeLineCells::TimeLineCells(TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE type) : QWidget(parent)
{
    mTimeLine = parent;
    mEditor = editor;
    mPrefs = editor->preference();
    // frame contents changed -> thumbnails must be regenerated
    connect(mEditor, &Editor::framesModified, this, [this]()
    {
        mThumbCache.clear();
        mThumbLru.clear();
        mThumbQueue.clear();
        mThumbQueued.clear();
    });
    // 图层多选变化 -> 整列重绘
    connect(mEditor->layers(), &LayerManager::layerSelectionChanged, this, [this]()
    {
        updateContent();
    });
    // async thumbnail batches (TVP-style: paint reads the cache only)
    mThumbTimer = new QTimer(this);
    mThumbTimer->setSingleShot(true);
    connect(mThumbTimer, &QTimer::timeout, this, &TimeLineCells::processThumbQueue);
    mType = type;

    mFrameLength = mPrefs->getInt(SETTING::TIMELINE_SIZE);
    mFontSize = mPrefs->getInt(SETTING::LABEL_FONT_SIZE);
    mFrameSize = mPrefs->getInt(SETTING::FRAME_SIZE);
    mbShortScrub = mPrefs->isOn(SETTING::SHORT_SCRUB);
    mDrawFrameNumber = mPrefs->isOn(SETTING::DRAW_LABEL);

    // tracks need horizontal room; the layer column must not force the
    // splitter open — 500px here kept the divider far right of the layer
    // toolbar (it has to sit right beside the tools dropdown instead)
    if (type == TIMELINE_CELL_TYPE::Tracks)
    {
        setMinimumSize(500, 4 * mLayerHeight);
    }
    else
    {
        setMinimumSize(120, 4 * mLayerHeight);
    }
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
    const int row = rowIndexAtY(y);
    if (row < 0)
    {
        return -1;
    }
    if (row >= mRows.size())
    {
        return mEditor->object()->getLayerCount();
    }
    const Object::TimelineRowRef& r = mRows.at(row);
    if (r.isHeader)
    {
        // 组头行映射为组内栈序最大成员（视觉上紧贴组头下方那层）
        const QList<int> members = mEditor->object()->layerGroupMemberIndices(r.groupId);
        return members.isEmpty() ? -1 : members.last();
    }
    return mEditor->object()->getIndex(r.layer);
}

// ---- 行模型几何：组头行 + 图层行共用 ----

void TimeLineCells::rebuildRows() const
{
    const Object* obj = mEditor->object();
    // 对象身份必须参与戳值：加载/恢复换 Object 时新旧代数可能同为 0，
    // 不加指针会让 mRows 继续引用已销毁旧对象的层（启动即 UAF）
    const quint64 stamp = reinterpret_cast<quint64>(obj) * 31ULL
                        + static_cast<quint64>(obj->layerStructureGeneration()) * 1000003ULL
                        + static_cast<quint64>(obj->layerGroupGeneration());
    if (stamp == mRowsStamp)
    {
        return;
    }
    mRows = obj->buildTimelineRows();
    mRowsStamp = stamp;
    mRowPrefixHeights.clear(); // 行集变了，前缀和一并失效
}

void TimeLineCells::rebuildRowPrefix() const
{
    mRowPrefixHeights.clear();
    mRowPrefixHeights.reserve(mRows.size() + 1);
    mRowPrefixHeights.append(0);
    for (int i = 0; i < mRows.size(); ++i)
    {
        mRowPrefixHeights.append(mRowPrefixHeights.last() + rowHeightAt(i));
    }
}

int TimeLineCells::rowHeightAt(int rowIndex) const
{
    rebuildRows();
    if (rowIndex < 0 || rowIndex >= mRows.size())
    {
        return mLayerHeight;
    }
    const Object::TimelineRowRef& r = mRows.at(rowIndex);
    if (r.isHeader)
    {
        // 组头行与普通图层同高（收起组就是一条全高轨道，仅显示帧范围带）
        return mLayerHeight;
    }
    return mCollapsedLayerIds.contains(r.layer->id()) ? 18 : mLayerHeight;
}

int TimeLineCells::rowYAt(int rowIndex) const
{
    rebuildRows();
    if (mRowPrefixHeights.size() != mRows.size() + 1)
    {
        rebuildRowPrefix();
    }
    const int n = mRows.size();
    if (rowIndex < 0 || rowIndex >= n) { return mOffsetY; }
    // 屏幕顶可见行 = 索引 n-1-mLayerOffset，y 向下随索引递减而增大：
    // rowYAt(r) = mOffsetY + sum_{i in (r, n-1-mLayerOffset]} h(i)
    const int topExclusive = n - mLayerOffset;
    return mOffsetY + (mRowPrefixHeights.at(topExclusive) - mRowPrefixHeights.at(rowIndex + 1));
}

int TimeLineCells::rowIndexAtY(int y) const
{
    rebuildRows();
    if (y < mOffsetY)
    {
        return -1;
    }
    const int n = mRows.size();
    int row = n - 1 - mLayerOffset;
    int yy = mOffsetY;
    while (row >= 0 && yy + rowHeightAt(row) <= y)
    {
        yy += rowHeightAt(row);
        --row;
    }
    return row;
}

int TimeLineCells::rowIndexOfLayer(int layerNumber) const
{
    rebuildRows();
    Layer* layer = mEditor->object()->getLayer(layerNumber);
    if (layer == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < mRows.size(); ++i)
    {
        if (!mRows.at(i).isHeader && mRows.at(i).layer == layer)
        {
            return i;
        }
    }
    return -1; // 收起组内的成员没有自己的行
}

int TimeLineCells::visualRowFromTop(int layerNumber) const
{
    const int row = rowIndexOfLayer(layerNumber);
    if (row < 0) { return -1; }
    // mRows 按栈序递增，屏幕从上往下反之（顶行 = 末尾索引）
    return mRows.size() - 1 - row;
}

int TimeLineCells::getInbetweenLayerNumber(int y) const {
    int layerNumber = getLayerNumber(y);
    // Round the layer number towards the drag start
    if(layerNumber != mFromLayer) {
        const int half = rowHeightOf(layerNumber) / 2;
        if(mMouseMoveY > 0 && y < getLayerY(layerNumber) + half) {
            layerNumber++;
        }
        else if(mMouseMoveY < 0 && y > getLayerY(layerNumber) + half) {
            layerNumber--;
        }
    }
    return layerNumber;
}

int TimeLineCells::rowHeightOf(int layerNumber) const
{
    const int row = rowIndexOfLayer(layerNumber);
    return (row >= 0) ? rowHeightAt(row) : mLayerHeight;
}

bool TimeLineCells::isLayerCollapsed(int layerNumber) const
{
    Layer* l = mEditor->object()->getLayer(layerNumber);
    return l != nullptr && mCollapsedLayerIds.contains(l->id());
}

void TimeLineCells::toggleLayerCollapsed(int layerNumber)
{
    Layer* l = mEditor->object()->getLayer(layerNumber);
    if (l == nullptr) return;
    qDebug() << "[ui] layer" << layerNumber << "collapse toggle";
    const int id = l->id();
    const bool nowCollapsed = !mCollapsedLayerIds.contains(id);
    setLayerCollapsed(id, nowCollapsed);
    emit layerCollapsedChanged(id, nowCollapsed);
}

void TimeLineCells::setLayerCollapsed(int layerId, bool collapsed)
{
    if (collapsed)
        mCollapsedLayerIds.insert(layerId);
    else
        mCollapsedLayerIds.remove(layerId);
    mRowPrefixHeights.clear(); // 行高变了，前缀和失效
    clearCache();
    updateContent();
    update();
}

int TimeLineCells::getLayerY(int layerNumber) const
{
    const int row = rowIndexOfLayer(layerNumber);
    if (row >= 0)
    {
        return rowYAt(row);
    }
    // 收起组内的成员：借用其组头行的位置（当前层指示等用途）
    Layer* layer = mEditor->object()->getLayer(layerNumber);
    if (layer != nullptr && layer->groupId() >= 0)
    {
        rebuildRows();
        for (int i = 0; i < mRows.size(); ++i)
        {
            if (mRows.at(i).isHeader && mRows.at(i).groupId == layer->groupId())
            {
                return rowYAt(i);
            }
        }
    }
    return mOffsetY;
}

void TimeLineCells::updateFrame(int frameNumber)
{
    int x = getFrameX(frameNumber);
    update(x - mFrameSize, 0, mFrameSize + 1, height());
}

void TimeLineCells::updateContent()
{
    // note: thumbnails are NOT dropped here — scrolling must not regenerate
    // them; only real frame modifications invalidate the thumbnail cache
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

    // Draw non-current rows (group headers + layers, shared row model)
    const Object* object = mEditor->object();
    Q_ASSERT(object != nullptr);
    rebuildRows();
    const int currentIdx = mEditor->layers()->currentLayerIndex();
    const bool groupDrag = (mGroupDragId >= 0) && didDetachLayer();
    const int viewH = height(); // 行级视口裁剪用
    for (int r = 0; r < mRows.size(); r++)
    {
        const Object::TimelineRowRef& rowRef = mRows.at(r);
        if (rowRef.isHeader)
        {
            if (groupDrag && rowRef.groupId == mGroupDragId) { continue; } // 拖动中单独绘制
            const int rowY = rowYAt(r);
            const int rowH = rowHeightAt(r);
            if (rowY + rowH <= mOffsetY || rowY >= viewH) { continue; } // 视口外
            if (mType == TIMELINE_CELL_TYPE::Layers)
            {
                paintGroupHeader(painter, rowRef.groupId, 0, rowY, widgetWidth - 1, rowH);
            }
            else
            {
                paintGroupTrack(painter, rowRef.groupId, rowY, rowH);
            }
            continue;
        }
        const int i = object->getIndex(rowRef.layer);
        if (i == currentIdx)
        {
            continue;
        }
        const int rowH = rowHeightAt(r);
        const int rowY = rowYAt(r) + ((groupDrag && rowRef.groupId == mGroupDragId) ? mMouseMoveY : 0);
        if (rowY + rowH <= mOffsetY || rowY >= viewH) { continue; } // 视口外整行跳过

        if (rowRef.layer != nullptr)
        {
            const bool rowSelected = mEditor->layers()->isLayerSelected(rowRef.layer);
            switch (mType)
            {
            case TIMELINE_CELL_TYPE::Tracks:
                paintTrack(painter, rowRef.layer, mOffsetX,
                           rowY, widgetWidth - mOffsetX,
                           rowH, rowSelected, mFrameSize);
                break;

            case TIMELINE_CELL_TYPE::Layers:
                paintLabel(painter, rowRef.layer, 0,
                           rowY, widgetWidth - 1,
                           rowH, rowSelected, mEditor->layerVisibility());
                break;
            }
        }
    }

    // 拖拽成组落点高亮：悬停在目标行中心区时画强调框（替代插入线的反馈）
    if (mType == TIMELINE_CELL_TYPE::Layers && didDetachLayer() && mGroupDragId < 0)
    {
        int hoverRow = -1;
        if (groupDropHoverRow(hoverRow))
        {
            const int hy = rowYAt(hoverRow);
            const int hh = rowHeightAt(hoverRow);
            const bool isHeaderRow = mRows.at(hoverRow).isHeader;
            painter.setRenderHint(QPainter::Antialiasing, true);
            QColor fill = Theme::Accent;
            fill.setAlpha(36);
            painter.setBrush(fill);
            painter.setPen(QPen(Theme::Accent, 2));
            painter.drawRoundedRect(QRectF(3, hy + 1.0, widgetWidth - 6, hh - 2.0), 5.0, 5.0);
            painter.setRenderHint(QPainter::Antialiasing, false);
            // 落点含义提示角标
            painter.setPen(Theme::AccentHover);
            painter.drawText(QRect(6, hy - 1, widgetWidth - 12, 14), Qt::AlignRight,
                             isHeaderRow ? tr("加入此组") : tr("成组"));
        }
    }

    // Draw current layer（收起组内的当前层不绘制；组拖动时它随组偏移）
    const Layer* currentLayer = mEditor->layers()->currentLayer();
    const int currentRow = rowIndexOfLayer(currentIdx);
    if (currentRow < 0)
    {
        // 当前层藏在收起的组里：跳过绘制（组头行已在上面画过）
    }
    else if (didDetachLayer() && mGroupDragId < 0)
    {
        int layerYMouseMove = getLayerY(mEditor->layers()->currentLayerIndex()) + mMouseMoveY;
        if (mType == TIMELINE_CELL_TYPE::Tracks)
        {
            paintTrack(painter, currentLayer,
                       mOffsetX, layerYMouseMove,
                       widgetWidth - mOffsetX,
                       rowHeightOf(mEditor->layers()->currentLayerIndex()),
                       true, mFrameSize);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers)
        {
            paintLabel(painter, currentLayer,
                       0, layerYMouseMove,
                       widgetWidth - 1,
                       rowHeightOf(mEditor->layers()->currentLayerIndex()),
                       true, mEditor->layerVisibility());

            int gutterRow = -1;
            if (!groupDropHoverRow(gutterRow))
            {
                paintLayerGutter(painter); // 中心区悬停时不画插入线，改画落点高亮
            }
        }
    }
    else if (didDetachLayer() && mGroupDragId >= 0)
    {
        // 整组拖动：当前层（组顶成员）随组偏移，组头行单独补画
        const int groupYOff = mMouseMoveY;
        const int baseY = getLayerY(mEditor->layers()->currentLayerIndex()) + groupYOff;
        if (mType == TIMELINE_CELL_TYPE::Tracks)
        {
            paintTrack(painter, currentLayer,
                       mOffsetX, baseY,
                       widgetWidth - mOffsetX,
                       rowHeightOf(mEditor->layers()->currentLayerIndex()),
                       true, mFrameSize);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers)
        {
            paintLabel(painter, currentLayer,
                       0, baseY,
                       widgetWidth - 1,
                       rowHeightOf(mEditor->layers()->currentLayerIndex()),
                       true, mEditor->layerVisibility());
            paintLayerGutter(painter);
        }
        // 补画拖动中的组头
        rebuildRows();
        for (int r = 0; r < mRows.size(); r++)
        {
            if (mRows.at(r).isHeader && mRows.at(r).groupId == mGroupDragId)
            {
                const int headerY = rowYAt(r) + groupYOff;
                if (mType == TIMELINE_CELL_TYPE::Layers)
                {
                    paintGroupHeader(painter, mGroupDragId, 0, headerY, widgetWidth - 1, mLayerHeight);
                }
                else
                {
                    paintGroupTrack(painter, mGroupDragId, headerY, mLayerHeight);
                }
            }
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
                       rowHeightOf(mEditor->layers()->currentLayerIndex()),
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
                       rowHeightOf(mEditor->layers()->currentLayerIndex()),
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
        // TVP numbering: every 5th frame, or every frame when cells are wide
        if (i == mFrameOffset || (i + 1) % 5 == 0 || mFrameSize > 35)
        {
            int incr = (i < 9) ? 4 : 0; // poor man's text centering
            painter.drawText(QPoint(lineX + incr, 17), QString::number(i + 1));
        }
    }
}

void TimeLineCells::paintCollapsedTrack(QPainter& painter, const Layer* layer, int x, int y, int width) const
{
    QColor col;
    if (layer->type() == Layer::BITMAP) col = Theme::LayerBitmap;
    if (layer->type() == Layer::COLORIZE) col = Theme::LayerBitmap;
    if (layer->type() == Layer::SOUND) col = Theme::LayerSound;
    if (layer->type() == Layer::CAMERA) col = Theme::LayerCamera;
    painter.setPen(Qt::NoPen);
    painter.setBrush(col);
    painter.drawRoundedRect(QRectF(x + 1.0, y + 7.0, width - 2.0, 4.0), 2.0, 2.0);
}

void TimeLineCells::paintTrack(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, int frameSize) const
{
    if (height <= 20)
    {
        paintCollapsedTrack(painter, layer, x, y, width);
        if (selected)
        {
            // 瘦行选中：同款 accent 红框（细行收窄描边）
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Theme::Accent, 1.5));
            painter.drawRoundedRect(QRectF(x + 2.0, y + 1.0, width - 4.0, height - 2.0), 4.0, 4.0);
            painter.setRenderHint(QPainter::Antialiasing, false);
        }
        return;
    }
    const QPalette palette = QApplication::palette();
    QColor col;
    // the layer type color only feeds the block cards / accents, the track
    // itself stays dark (TVP) so the label color reads clearly
    if (layer->type() == Layer::BITMAP) col = Theme::LayerBitmap;
    if (layer->type() == Layer::COLORIZE) col = Theme::LayerBitmap;
    if (layer->type() == Layer::SOUND) col = Theme::LayerSound;
    if (layer->type() == Layer::CAMERA) col = Theme::LayerCamera;

    painter.save();
    // dark track base
    painter.setBrush(layer->visible() ? QColor(0x17, 0x17, 0x1B) : QColor(0x11, 0x11, 0x14));
    painter.setPen(QPen(QBrush(Theme::Border), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRoundedRect(QRectF(x, y - 1, width, height), 4.0, 4.0);
    if (layer->colorIndex() >= 0 && layer->colorIndex() < 8 && layer->visible())
    {
        // TVP: the label color washes over the track; the black block cards
        // sit on top and the color reads between them
        QColor tint = Theme::LayerLabelColors[layer->colorIndex()];
        tint.setAlpha(180);
        painter.setPen(Qt::NoPen);
        painter.setBrush(tint);
        painter.drawRoundedRect(QRectF(x + 2.0, y + 1.0, width - 4.0, height - 2.0), 3.0, 3.0);
    }

    if (!layer->visible())
    {
        painter.restore();
        return;
    }

    // Changes the appearance if selected
    if (selected)
    {
        paintSelection(painter, x, y, width, height);
        // 选中层轨道：与拖拽成组落点同款 accent 红框
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Theme::Accent, 2));
        painter.drawRoundedRect(QRectF(x + 2, y + 1.0, width - 4, height - 2.0), 5.0, 5.0);
        painter.setRenderHint(QPainter::Antialiasing, false);
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
    if (mTrimming && layer == mTrimLayer && key->pos() == mTrimKeyPos)
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

int TimeLineCells::hitTestPlusHandle(const QPoint& pos) const
{
    if (headerGroupIdAt(pos) >= 0) { return -1; } // 组头行不可新建帧
    const int layerIndex = getLayerNumber(pos.y());
    if (layerIndex < 0 || layerIndex >= mEditor->object()->getLayerCount()) return -1;
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    if (!layer->isBitmapKind() || layer->locked()) return -1;
    if (isLayerCollapsed(layerIndex)) return -1; // 折叠行不画把手也不响应

    int lastPos = -1;
    layer->foreachKeyFrame([&](KeyFrame* k) { lastPos = qMax(lastPos, k->pos()); });
    if (lastPos < 0) return -1;

    KeyFrame* key = layer->getKeyFrameAt(lastPos);
    const int standardWidth = mFrameSize - 2;
    const int recLeft = getFrameX(lastPos) - standardWidth;
    const int blockLen = blockLengthFor(layer, key);
    const int recWidth = standardWidth + (blockLen - 1) * mFrameSize;
    const int y = getLayerY(layerIndex);
    // TVP geometry: last block's top-right corner, 14px handle zone
    const int edge = recLeft + recWidth - 2; // visual right edge
    return (pos.x() >= edge - 14 && pos.x() <= edge + 4
            && pos.y() >= y + 2 && pos.y() <= y + 18) ? layerIndex : -1;
}

// 缩略图缓存键：layerId 高 32 位 | framePos 低 32 位（原来每块每次重绘都
// 拼 "%1_%2" 字符串做哈希键）
static inline qint64 thumbCacheKey(int layerId, int framePos)
{
    return (static_cast<qint64>(layerId) << 32) | static_cast<quint32>(framePos);
}

QPixmap TimeLineCells::thumbnailFor(const Layer* layer, int framePos) const
{
    const qint64 key = thumbCacheKey(layer->id(), framePos);
    const auto it = mThumbCache.constFind(key);
    if (it != mThumbCache.constEnd())
    {
        // LRU touch：挪到末尾（容量上限 400，线性移动代价可忽略）
        mThumbLru.removeOne(key);
        mThumbLru.append(key);
        return it.value();
    }

    // cache miss: queue for async generation, the placeholder shows meanwhile
    if (!mThumbQueued.contains(key))
    {
        mThumbQueued.insert(key);
        mThumbQueue.append({ layer->id(), framePos });
        if (!mThumbTimer->isActive())
            mThumbTimer->start(30);
    }
    return QPixmap();
}

void TimeLineCells::processThumbQueue()
{
    if (mType != TIMELINE_CELL_TYPE::Tracks)
    {
        mThumbQueue.clear();
        mThumbQueued.clear();
        return;
    }

    int generated = 0;
    while (!mThumbQueue.isEmpty() && generated < 8)
    {
        const ThumbRequest request = mThumbQueue.takeFirst();
        const qint64 key = thumbCacheKey(request.layerId, request.framePos);
        mThumbQueued.remove(key);
        if (mThumbCache.contains(key)) { continue; }

        const Layer* layer = mEditor->layers()->findLayerById(request.layerId);
        if (layer == nullptr) { continue; }

        QPixmap thumb;
        LayerBitmap* bitmapLayer = const_cast<LayerBitmap*>(dynamic_cast<const LayerBitmap*>(layer));
        if (bitmapLayer != nullptr)
        {
            BitmapImage* img = bitmapLayer->getBitmapImageAtFrame(request.framePos);
            if (img == nullptr)
                img = bitmapLayer->getLastBitmapImageAtFrame(request.framePos);
            if (img != nullptr && !img->image()->isNull())
            {
                // crop to actual content, then letterbox into a 16:9 card
                const QImage src = img->image()->copy(img->bounds());
                QImage card(160, 90, QImage::Format_ARGB32_Premultiplied);
                card.fill(Qt::transparent);
                QPainter cp(&card);
                const QSize scaled = src.size().scaled(160, 90, Qt::KeepAspectRatio);
                const int dx = (160 - scaled.width()) / 2;
                const int dy = (90 - scaled.height()) / 2;
                cp.drawImage(QRect(dx, dy, scaled.width(), scaled.height()), src);
                cp.end();
                thumb = QPixmap::fromImage(card);
            }
        }
        mThumbCache.insert(key, thumb);
        mThumbLru.append(key);
        // LRU 驱逐最久未用（原来 erase(begin()) 是任意序，可能踢掉正在显示的）
        while (mThumbCache.size() > 400 && !mThumbLru.isEmpty())
        {
            mThumbCache.remove(mThumbLru.takeFirst());
        }
        ++generated;
    }

    if (!mThumbQueue.isEmpty())
        mThumbTimer->start(30);
    else
        update();
}

void TimeLineCells::paintPlusPreview(QPainter& painter) const
{
    if (!mPlusCreating || mPlusPreviewCount <= 0) return;
    Layer* layer = mEditor->object()->getLayer(mCurrentLayerNumber);
    if (layer == nullptr || !layer->isBitmapKind()) return;

    int lastPos = -1;
    layer->foreachKeyFrame([&](KeyFrame* k) { lastPos = qMax(lastPos, k->pos()); });
    if (lastPos < 0) return;
    KeyFrame* key = layer->getKeyFrameAt(lastPos);
    const int standardWidth = mFrameSize - 2;
    const int recLeft = getFrameX(lastPos) - standardWidth;
    const int blockLen = blockLengthFor(layer, key);
    const int recWidth = standardWidth + (blockLen - 1) * mFrameSize;
    const int y = getLayerY(mCurrentLayerNumber);

    const qreal startX = recLeft + recWidth + 2.0;
    const qreal w = mPlusPreviewCount * mFrameSize;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen dash(Theme::Accent, 1.6, Qt::DashLine);
    painter.setPen(dash);
    painter.setBrush(QColor(0xE8, 0x38, 0x5A, 36));
    painter.drawRoundedRect(QRectF(startX, y + 2.0, w, mLayerHeight - 6.0), 6.0, 6.0);
    painter.setPen(Theme::AccentHover);
    painter.drawText(QRectF(startX, y + 2.0, w, mLayerHeight - 6.0),
                     Qt::AlignCenter, QString("+%1f").arg(mPlusPreviewCount));
    painter.restore();
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
        const int viewW = width();
        layer->foreachKeyFrame([&](KeyFrame* key)
        {
            int framePos = key->pos();
            int recWidth = standardWidth;
            int recLeft = getFrameX(framePos) - recWidth;
            if (recLeft >= viewW || recLeft + recWidth < 0) { return; } // 视口外

            if (selectedFrames.contains(framePos)) {
                return;
            }

            // uniform black block base (TVP): the current layer is marked by
            // the track background, not by recoloring its blocks
            painter.setBrush(Theme::TimelineFrameFill);

            painter.drawRoundedRect(QRectF(recLeft, recTop, recWidth, recHeight), 3.0, 3.0);
        });
        return;
    }

    // Bitmap & sound layers render TVPaint-style exposure blocks:
    // a wide card per keyframe spanning its exposure length, with a
    // thumbnail zone, frame number, trim-handle bar and a trailing "+".
    //
    // sheet numbering: blocks are sheets of drawing paper, so a block shows
    // its ordinal among the layer's keyframes (1, 2, 3...) instead of the
    // timeline frame position. mKeyFrames 降序迭代（begin=最大 pos）：
    // 第 idx 个块的图纸号 = 总数 - idx；lastPos 同一趟顺手记录
    // （原来是 lastPos 扫描 + 收集排序两趟，现合并为一趟）
    QHash<int, int> sheetNumber;
    int lastPos = -1;
    {
        const int total = layer->keyFrameCount();
        int idx = 0;
        layer->foreachKeyFrame([&](KeyFrame* key)
        {
            sheetNumber.insert(key->pos(), total - idx);
            lastPos = qMax(lastPos, key->pos()); // 防御：不硬靠迭代序取末块
            ++idx;
        });
    }
    const int viewW = width(); // 块级视口裁剪用

    auto paintOneBlock = [&](KeyFrame* key)
    {
        int framePos = key->pos();
        int recLeft = getFrameX(framePos) - standardWidth;
        // live ripple: while trimming, later blocks follow the drag in real time
        if (mTrimming && layer == mTrimLayer && framePos > mTrimKeyPos)
            recLeft += mTrimRippleOffset * mFrameSize;

        // Selected frames are normally drawn as regular cards with a
        // border-only highlight on top (paintSelectedFrames). While moving,
        // the card at the original spot is hidden so the floating outline
        // reads as the block being carried.
        if (selectedFrames.contains(framePos) && mMovingFrames) {
            return;
        }

        int blockLen = blockLengthFor(layer, key);
        int recWidth = standardWidth + (blockLen - 1) * frameSize;
        if (recLeft >= viewW || recLeft + recWidth < 0) { return; } // 视口外

        // 声音块走独立外观（参考图：深灰胶囊+波形），无缩略图/图纸号/"+"
        if (layer->type() == Layer::SOUND)
        {
            paintSoundWaveform(painter, static_cast<SoundClip*>(key), recLeft, recTop, recWidth, recHeight);
            // 右缘 trim 把手指示与位图块一致
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0x9A, 0x9A, 0xA4));
            painter.drawRoundedRect(QRectF(recLeft + recWidth - 2.0, recTop + 6.0, 2.0, recHeight - 12.0), 1.0, 1.0);
            painter.setPen(QPen(QBrush(Theme::TimelineFrameBorder), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            return;
        }

        // uniform black block base (TVP): selection is border-only
        painter.setBrush(Theme::TimelineFrameFill);

        painter.drawRoundedRect(QRectF(recLeft, recTop, recWidth, recHeight), 6.0, 6.0);

        // thumbnail card zone inside the block (TVP proportions: the white
        // card covers ~45% of the row height, not the whole block face)
        const qreal thumbH = qRound(recHeight * 0.47);
        const qreal thumbW = qMin(static_cast<qreal>(recWidth) - 8.0, thumbH * 16.0 / 9.0);
        if (thumbW > 14.0 && thumbH > 10.0)
        {
            // TVP THUMB_BG: a white thumbnail base is always visible and the
            // (transparent-letterboxed) frame image sits on top of it
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0xE8, 0xE8, 0xEA));
            painter.drawRoundedRect(QRectF(recLeft + 4.0, recTop + 4.0, thumbW, thumbH), 4.0, 4.0);
            const QPixmap thumb = thumbnailFor(layer, framePos);
            if (!thumb.isNull())
            {
                painter.drawPixmap(QRectF(recLeft + 4.0, recTop + 4.0, thumbW, thumbH), thumb, QRectF(thumb.rect()));
            }
        }

        // sheet number at block bottom center (ordinal, not frame position)
        painter.setPen(selected ? QColor(0xE8, 0xE8, 0xEA) : QColor(0x8A, 0x8A, 0x90));
        painter.drawText(QRectF(recLeft, recTop + recHeight - 17.0, static_cast<qreal>(recWidth), 14.0),
                         Qt::AlignCenter, QString::number(sheetNumber.value(framePos, framePos)));

        // right-edge trim handle indicator: 2px, flush against the edge
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x9A, 0x9A, 0xA4));
        painter.drawRoundedRect(QRectF(recLeft + recWidth - 2.0, recTop + 6.0, 2.0, recHeight - 12.0), 1.0, 1.0);

        // 智能填色块：着色待更新标记（琥珀点，右下角）
        if (layer->type() == Layer::COLORIZE)
        {
            auto colorizeFrame = static_cast<ColorizeImage*>(key);
            if (colorizeFrame->needsUpdate() ||
                colorizeFrame->computedStructureGeneration() != mEditor->object()->layerStructureGeneration())
            {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0xF5, 0x9E, 0x0B));
                painter.drawEllipse(QRectF(recLeft + recWidth - 12.0, recTop + recHeight - 12.0, 5.0, 5.0));
            }
        }

        // TVP create handle: "+" on the top-right of the trailing block;
        // drag it out to create consecutive new frames
        if (framePos == lastPos)
        {
            const QPointF c(recLeft + recWidth - 9.0, recTop + 7.0);
            painter.setBrush(Theme::PanelRaised);
            painter.setPen(QPen(trackCol, 1.2));
            painter.drawEllipse(c, 7.0, 7.0);
            painter.setPen(QPen(trackCol, 1.4));
            painter.drawLine(QPointF(c.x() - 3.6, c.y()), QPointF(c.x() + 3.6, c.y()));
            painter.drawLine(QPointF(c.x(), c.y() - 3.6), QPointF(c.x(), c.y() + 3.6));
        }

        // separator line towards the next consecutive block (TVP-style)
        {
            const int nextPos = layer->getNextKeyFramePosition(framePos);
            if (nextPos == framePos + blockLen)
            {
                QColor sep = Theme::TimelineFrameBorder;
                sep.setAlpha(200);
                painter.setPen(QPen(sep, 1.0));
                const qreal sepX = recLeft + recWidth - 1.0;
                painter.drawLine(QPointF(sepX, recTop + 4.0), QPointF(sepX, recTop + recHeight - 4.0));
            }
        }

        painter.setPen(QPen(QBrush(Theme::TimelineFrameBorder), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    };

    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        // the block being trimmed is painted last so its growth stays visible
        if (mTrimming && layer == mTrimLayer && key->pos() == mTrimKeyPos) { return; }
        paintOneBlock(key);
    });
    if (mTrimming && layer == mTrimLayer)
    {
        KeyFrame* trimKey = layer->getKeyFrameAt(mTrimKeyPos);
        if (trimKey != nullptr) { paintOneBlock(trimKey); }
    }
}

void TimeLineCells::paintCurrentFrameBorder(QPainter &painter, int recLeft, int recTop, int recWidth, int recHeight) const
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Theme::TimelineCurrentFrameBorder, 2));
    painter.drawRoundedRect(QRectF(recLeft, recTop, recWidth, recHeight), 3.0, 3.0);
    painter.restore();
}

// 声音块：黑色圆角底（与位图块同款），顶部音频名称，下方白色细柱波形
void TimeLineCells::paintSoundWaveform(QPainter& painter, SoundClip* clip, int recLeft, int recTop, int recWidth, int recHeight) const
{
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::TimelineFrameFill);
    painter.drawRoundedRect(QRectF(recLeft + 1.0, recTop + 1.0, recWidth - 2.0, recHeight - 2.0), 6.0, 6.0);

    // 音频名称：左上一行，过长中段省略
    const QString clipName = clip->soundClipName();
    if (!clipName.isEmpty())
    {
        const QRectF nameRect(recLeft + 8.0, recTop + 3.0, recWidth - 16.0, 13.0);
        const QString shown = QFontMetrics(painter.font()).elidedText(clipName, Qt::ElideMiddle, qMax<qreal>(20.0, nameRect.width()));
        painter.setPen(QColor(0xE8, 0xE8, 0xEA));
        painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, shown);
    }

    // 波形：名称下方剩余区域，白柱上下对称
    const qreal waveTop = recTop + 17.0;
    const qreal waveBottom = recTop + recHeight - 4.0;
    const qreal centerY = (waveTop + waveBottom) / 2.0;
    const qreal maxBar = qMax(1.0, (waveBottom - waveTop) / 2.0 - 2.0);
    const QVector<qreal>& peaks = clip->waveformPeaks();
    painter.setPen(QPen(QColor(0xFF, 0xFF, 0xFF), 1.0));

    if (peaks.isEmpty())
    {
        // 无波形数据（非 WAV 或解码失败）：中线占位
        painter.drawLine(QPointF(recLeft + 5.0, centerY), QPointF(recLeft + recWidth - 5.0, centerY));
        painter.restore();
        return;
    }

    const int peakCount = peaks.size();
    const qreal innerLeft = recLeft + 4.0;
    const qreal innerW = recWidth - 8.0;
    const int barStep = 3; // 2px 柱 + 1px 间隙
    for (int x = 0; innerW > 6 && x + barStep <= static_cast<int>(innerW); x += barStep)
    {
        // 像素区间映射到桶区间，取区间内最大峰值
        const int b0 = qBound(0, static_cast<int>(x * peakCount / innerW), peakCount - 1);
        const int b1 = qBound(0, static_cast<int>((x + barStep) * peakCount / innerW), peakCount - 1);
        qreal peak = 0;
        for (int b = b0; b <= b1; ++b) { peak = qMax(peak, peaks[b]); }
        const qreal h = qMax(1.0, peak * maxBar);
        const qreal bx = innerLeft + x + 1.0;
        painter.drawLine(QPointF(bx, centerY - h), QPointF(bx, centerY + h));
    }
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
    if (layerRowHidden(layer)) { return; } // 收起组内的层不画选中高亮
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
    // 视口外整层跳过
    if (recTop + recHeight <= mOffsetY || recTop >= height()) { return; }

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
    // TVP-style selection: highlight the border only, the block's own card
    // (thumbnail, frame number) stays visible underneath. While dragging, a
    // translucent ghost keeps the carried outline readable. The brush must be
    // set explicitly in both cases — a stale brush from the block painting
    // leaked a white/blue fill over the selection.
    QColor ghostTint = Theme::Accent;
    ghostTint.setAlpha(30);
    painter.setBrush(previewing ? QBrush(ghostTint) : Qt::NoBrush);
    const QBrush selectionFill = painter.brush();
    painter.setPen(QPen(QBrush(Theme::Accent), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

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
        if (recLeft >= width() || recLeft + recWidth < 0) { continue; } // 视口外
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
        painter.setPen(QPen(QBrush(Theme::Accent), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(selectionFill);
    }

    painter.restore();
}

void TimeLineCells::drawCollapseTriangle(QPainter& painter, const Layer* layer, int x, int y, int width, int height) const
{
    Q_UNUSED(layer)
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x8A, 0x8A, 0x90));
    const QPointF c(x + width - 13.0, y + height / 2.0);
    QPolygonF tri2;
    // collapsed -> right-pointing, expanded -> down-pointing
    if (height <= 20)
    {
        tri2 << QPointF(c.x() - 3.0, c.y() - 4.0) << QPointF(c.x() + 4.0, c.y()) << QPointF(c.x() - 3.0, c.y() + 4.0);
    }
    else
    {
        tri2 << QPointF(c.x() - 4.0, c.y() - 3.0) << QPointF(c.x() + 4.0, c.y() - 3.0) << QPointF(c.x(), c.y() + 4.0);
    }
    painter.drawPolygon(tri2);
    painter.restore();
}

bool TimeLineCells::rowHasInlineControls(int rowWidth) const
{
    // controls sit on their own line under the name: slider(64) + %(38) +
    // lock(16) + clip(16) anchored at x=52, ending around x=194
    return rowWidth >= 205;
}

QRect TimeLineCells::opacitySliderRect(int rowWidth) const
{
    Q_UNUSED(rowWidth)
    // bottom-left cluster, slider aligned under the name text (name starts at x=52)
    return QRect(52, 0, 64, 0);
}

QRect TimeLineCells::lockIconRect(int rowWidth) const
{
    Q_UNUSED(rowWidth)
    // right after the percentage label: 52 + 64 slider + 4 gap + 34 label + 4 gap
    return QRect(158, 0, 16, 0);
}

QRect TimeLineCells::clipIconRect(int rowWidth) const
{
    Q_UNUSED(rowWidth)
    // right after the padlock: 158 + 16 lock + 4 gap
    return QRect(178, 0, 16, 0);
}

QPixmap TimeLineCells::cachedRowIcon(const QString& key, const std::function<QPixmap()>& make) const
{
    const auto it = mRowIconCache.constFind(key);
    if (it != mRowIconCache.constEnd()) { return it.value(); }
    const QPixmap pix = make();
    mRowIconCache.insert(key, pix);
    return pix;
}

void TimeLineCells::paintLabel(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, LayerVisibility layerVisibility) const
{
    const QPalette palette = QApplication::palette();

    if (height <= 20) // collapsed row: color chip + name + expand triangle
    {
        if (layer->colorIndex() >= 0 && layer->colorIndex() < 8)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::LayerLabelColors[layer->colorIndex()]);
            painter.drawRoundedRect(QRectF(x + 1.0, y + 5.0, 4.0, height - 10), 2.0, 2.0);
        }
        painter.setPen(selected ? Theme::AccentHover : QColor(0x8A, 0x8A, 0x90));
        painter.drawText(QPoint(x + 12, y + height - 6), layer->name());
        drawCollapseTriangle(painter, layer, x, y, width, height);
        return;
    }
    drawCollapseTriangle(painter, layer, x, y, width, height);

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

    // TVP-style 8-color label bar (click cycles through colors)
    if (layer->colorIndex() >= 0 && layer->colorIndex() < 8)
    {
        painter.setBrush(Theme::LayerLabelColors[layer->colorIndex()]);
        painter.drawRoundedRect(QRectF(x + 0.5, y + 4, 5.0, height - 8), 2.0, 2.0);
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
    // two-line row: name line in the upper third, inline controls below
    const int nameCenterY = y + qRound(height * 0.32);
    painter.drawEllipse(QRectF(x + 10, nameCenterY - 4.5, 9.0, 9.0));

    // 行类型图标：SVG 按 DPR 一次性栅格化后缓存（原来每行每次重绘都重新
    // 从资源加载并缩放；顺便补上 devicePixelRatio，高 DPI 下不再模糊）
    const char* typeIconRes = nullptr;
    if (layer->type() == Layer::BITMAP || layer->type() == Layer::COLORIZE) typeIconRes = ":icons/themes/playful/timeline/cell-bitmap.svg";
    else if (layer->type() == Layer::SOUND) typeIconRes = ":icons/themes/playful/timeline/cell-sound.svg";
    else if (layer->type() == Layer::CAMERA) typeIconRes = ":icons/themes/playful/timeline/cell-camera.svg";
    if (typeIconRes != nullptr)
    {
        const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
        const QString iconKey = QStringLiteral("type:%1@%2").arg(QLatin1String(typeIconRes)).arg(dpr);
        const QPixmap typeIcon = cachedRowIcon(iconKey, [typeIconRes, dpr]() {
            QPixmap scaled = QPixmap(QLatin1String(typeIconRes))
                                 .scaledToHeight(qMax(1, qRound(18 * dpr)), Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(dpr);
            return scaled;
        });
        painter.drawPixmap(QPoint(28, nameCenterY - 9), typeIcon);
    }

    if (selected)
    {
        painter.setPen(Theme::AccentHover);
    }
    else
    {
        painter.setPen(palette.color(QPalette::Text));
    }
    // the name owns the full top line (controls live on the line below)
    const int nameRight = width - 30;
    const QString shownName = QFontMetrics(painter.font()).elidedText(layer->name(), Qt::ElideMiddle, qMax(20, nameRight - 52));
    painter.drawText(QPoint(52, nameCenterY + 5), shownName);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // --- TVP inline row controls: opacity slider, percentage, lock toggle ---
    // rendered on their own line under the name; skipped on very short rows
    if (!rowHasInlineControls(width) || height < 40) { return; }

    const int sliderY = y + qRound(height * 0.72);
    const QRect slider = opacitySliderRect(width);

    // percentage label right-aligned before the slider
    painter.setPen(selected ? Theme::AccentHover : QColor(0x8A, 0x8A, 0x90));
    painter.drawText(QRect(slider.right() + 4, sliderY - 9, 34, 18),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("%1%").arg(qRound(layer->opacity() * 100)));

    // rounded track + knob
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x3A, 0x3A, 0x40));
    painter.drawRoundedRect(QRectF(slider.x(), sliderY - 2.5, slider.width(), 5.0), 2.5, 2.5);
    const qreal knobX = slider.x() + layer->opacity() * (slider.width() - 10) + 5.0;
    painter.setBrush(layer->opacity() > 0.0 ? Theme::Accent : QColor(0x66, 0x66, 0x6E));
    painter.drawEllipse(QPointF(knobX, sliderY), 5.0, 5.0);

    // padlock toggle (closed = locked)
    const QRect lock = lockIconRect(width);
    const QPointF lockC(lock.x() + 7.5, sliderY);
    QColor lockColor = layer->locked() ? QColor(0xE8, 0xB4, 0x30) : QColor(0x66, 0x66, 0x6E);
    painter.setBrush(lockColor);
    painter.drawRect(QRectF(lockC.x() - 6.0, lockC.y() - 1.0, 12.0, 8.0));
    painter.setPen(QPen(lockColor, 1.6));
    painter.setBrush(Qt::NoBrush);
    if (layer->locked())
    {
        painter.drawArc(QRectF(lockC.x() - 4.0, lockC.y() - 7.0, 8.0, 8.0), 180 * 16, -180 * 16);
    }
    else
    {
        painter.drawArc(QRectF(lockC.x() - 4.0, lockC.y() - 7.0, 8.0, 9.0), 180 * 16, -160 * 16);
    }

    // clipping-mask toggle: Krita inherit-alpha glyph (swoosh, strike = off), tinted by state
    const QRect clipR = clipIconRect(width);
    const bool clipActive = layer->clipMask() && layer->type() == Layer::BITMAP;
    QColor clipColor = clipActive ? Theme::Accent : QColor(0x66, 0x66, 0x6E);
    if (layer->type() != Layer::BITMAP)
    {
        // bitmap-only feature: keep the control visible but inert
        clipColor = QColor(0x3A, 0x3A, 0x40);
    }
    // 染色结果按 状态+颜色 缓存（原来每行每次重绘都建 QPixmap+QPainter 现染）
    const bool clipOn = layer->clipMask();
    const QString clipKey = QStringLiteral("clip:%1:%2").arg(clipOn).arg(clipColor.rgba());
    const QPixmap clipTinted = cachedRowIcon(clipKey, [clipOn, clipColor]() {
        QPixmap clipPix(clipOn ? ":/icons/themes/playful/timeline/clip-on.svg"
                               : ":/icons/themes/playful/timeline/clip-off.svg");
        if (clipPix.isNull()) { return QPixmap(); }
        QPixmap tinted(clipPix.size());
        tinted.fill(Qt::transparent);
        QPainter tp(&tinted);
        tp.drawPixmap(0, 0, clipPix);
        tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tp.fillRect(tinted.rect(), clipColor);
        tp.end();
        return tinted;
    });
    if (!clipTinted.isNull())
    {
        painter.drawPixmap(QPointF(clipR.x(), sliderY - 8.0), clipTinted);
    }

    // loop-mode badge: only drawn when the layer is set to Cycle/PingPong
    // (right-click layer row -> 循环模式); slot sits right after the clip icon.
    // Cycle reuses the playback loop button icon (control-loop.svg); PingPong
    // keeps a hand-drawn double-arrow glyph to stay distinguishable
    if (layer->isBitmapKind() && layer->loopMode() != Layer::LoopMode::None)
    {
        const QColor loopColor = Theme::Accent;
        const QRect loopR(clipR.right() + 4, 0, 16, 0);
        if (layer->loopMode() == Layer::LoopMode::Cycle)
        {
            // QIcon 按请求尺寸+设备DPR直接矢量栅格化：QPixmap(svg路径)对
            // width=100% 的源图先按回退尺寸栅格化再 scaled 是两次有损，
            // 高DPI屏还会被最近邻拉伸出锯齿
            // 源图形四周有内边距，16px 渲染显出来偏小；放大到 20px 并按槽位中心对齐
            // 染色结果按 DPR+颜色 缓存（原来每行每次重绘都现染）
            const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
            const QString loopKey = QStringLiteral("loopcycle:%1:%2").arg(dpr).arg(loopColor.rgba());
            const QPixmap loopTinted = cachedRowIcon(loopKey, [dpr, loopColor]() {
                QIcon loopIcon(":/icons/themes/playful/controls/control-loop.svg");
                QPixmap loopPix = loopIcon.pixmap(QSize(20, 20), dpr);
                if (loopPix.isNull()) { return QPixmap(); }
                QPixmap tinted(loopPix.size());
                tinted.setDevicePixelRatio(loopPix.devicePixelRatio());
                tinted.fill(Qt::transparent);
                QPainter tp(&tinted);
                tp.drawPixmap(0, 0, loopPix);
                tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
                tp.fillRect(tinted.rect(), loopColor);
                tp.end();
                return tinted;
            });
            if (!loopTinted.isNull())
            {
                painter.drawPixmap(QPointF(loopR.x() + 8.0 - loopTinted.width() / (2.0 * loopTinted.devicePixelRatio()),
                                           sliderY - loopTinted.height() / (2.0 * loopTinted.devicePixelRatio())),
                                   loopTinted);
            }
        }
        else
        {
            // ping-pong: two opposing horizontal arrows（占满同款 16px 槽位）
            const QPointF loopC(loopR.x() + 8.0, sliderY);
            painter.setPen(QPen(loopColor, 1.8));
            painter.setBrush(loopColor);
            const qreal halfW = 5.5;
            painter.drawLine(QPointF(loopC.x() - halfW, loopC.y() - 3.5),
                             QPointF(loopC.x() + halfW, loopC.y() - 3.5));
            QPolygonF headRight;
            headRight << QPointF(loopC.x() + halfW + 3.5, loopC.y() - 3.5)
                      << QPointF(loopC.x() + halfW - 1.0, loopC.y() - 6.0)
                      << QPointF(loopC.x() + halfW - 1.0, loopC.y() - 1.0);
            painter.drawPolygon(headRight);
            painter.drawLine(QPointF(loopC.x() + halfW, loopC.y() + 3.5),
                             QPointF(loopC.x() - halfW, loopC.y() + 3.5));
            QPolygonF headLeft;
            headLeft << QPointF(loopC.x() - halfW - 3.5, loopC.y() + 3.5)
                     << QPointF(loopC.x() - halfW + 1.0, loopC.y() + 1.0)
                     << QPointF(loopC.x() - halfW + 1.0, loopC.y() + 6.0);
            painter.drawPolygon(headLeft);
        }
    }
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

bool TimeLineCells::groupDropHoverRow(int& rowOut) const
{
    // 拖拽单层悬停在另一层/组头行的中心 40% 区 = 成组/入组落点（与释放判定同公式）
    if (mType != TIMELINE_CELL_TYPE::Layers || !didDetachLayer() || mGroupDragId >= 0)
    {
        return false;
    }
    Layer* fromObj = mEditor->object()->getLayer(mFromLayer);
    if (fromObj == nullptr || !fromObj->isGroupable())
    {
        return false;
    }
    const int row = rowIndexAtY(mEndY);
    if (row < 0 || row >= mRows.size())
    {
        return false;
    }
    const int y = rowYAt(row);
    const int h = rowHeightAt(row);
    if (mEndY < y + h * 0.225 || mEndY > y + h * 0.775)
    {
        return false;
    }
    if (mRows.at(row).isHeader)
    {
        rowOut = row;
        return true; // 组头中心区 = 加入该组
    }
    Layer* target = mRows.at(row).layer;
    if (target == fromObj || !target->isGroupable())
    {
        return false;
    }
    rowOut = row;
    return true;
}

int TimeLineCells::headerGroupIdAt(const QPoint& pos) const
{
    const int row = rowIndexAtY(pos.y());
    if (row < 0 || row >= mRows.size())
    {
        return -1;
    }
    return mRows.at(row).isHeader ? mRows.at(row).groupId : -1;
}

bool TimeLineCells::layerRowHidden(const Layer* layer) const
{
    if (layer == nullptr || layer->groupId() < 0)
    {
        return false;
    }
    const LayerGroupInfo* info = mEditor->object()->layerGroupInfo(layer->groupId());
    return info != nullptr && info->collapsed;
}

QPair<int, int> TimeLineCells::groupFrameExtent(int groupId) const
{
    int minPos = 0;
    int maxEnd = 0;
    bool anyKey = false;
    const QList<int> members = mEditor->object()->layerGroupMemberIndices(groupId);
    for (int index : members)
    {
        Layer* layer = mEditor->object()->getLayer(index);
        layer->foreachKeyFrame([&](KeyFrame* key)
        {
            if (!anyKey) { minPos = key->pos(); maxEnd = key->pos() + 1; anyKey = true; }
            minPos = qMin(minPos, key->pos());
            int end = layer->getBlockEnd(key);
            if (end < 0) { end = key->pos() + 1; } // 开放末块
            maxEnd = qMax(maxEnd, end);
        });
    }
    if (!anyKey) { return qMakePair(0, 0); }
    return qMakePair(minPos, maxEnd);
}

void TimeLineCells::paintGroupHeader(QPainter& painter, int groupId, int x, int y, int width, int height) const
{
    const QPalette palette = QApplication::palette();
    const LayerGroupInfo* info = mEditor->object()->layerGroupInfo(groupId);
    if (info == nullptr) { return; }
    const QList<int> members = mEditor->object()->layerGroupMemberIndices(groupId);

    painter.setRenderHint(QPainter::Antialiasing, true);

    // 组头卡片底
    painter.setPen(Qt::NoPen);
    QColor base = palette.color(QPalette::Base);
    base = QColor(qBound(0, base.red() - 12, 255), qBound(0, base.green() - 10, 255), qBound(0, base.blue() - 16, 255));
    painter.setBrush(base);
    painter.drawRoundedRect(QRectF(x + 2, y + 1, width - 4, height - 2), 5.0, 5.0);

    // 组强调色条
    painter.setBrush(Theme::Accent);
    painter.drawRoundedRect(QRectF(x + 2, y + 4, 3.0, height - 8), 1.5, 1.5);

    // 展开箭头（▶ 收起 / ▼ 展开），与层的折叠开关同区（右端）
    const qreal cx = width - 14;
    const qreal cy = y + height / 2.0;
    QPolygonF arrow;
    if (info->collapsed)
    {
        arrow << QPointF(cx - 3, cy - 4) << QPointF(cx + 4, cy) << QPointF(cx - 3, cy + 4);
    }
    else
    {
        arrow << QPointF(cx - 4, cy - 3) << QPointF(cx + 4, cy - 3) << QPointF(cx, cy + 4);
    }
    painter.setBrush(palette.color(QPalette::Text));
    painter.drawPolygon(arrow);

    // 组眼睛（x<30 区，同层行热区）：组可见=实心圆，不可见=空心
    painter.setPen(QPen(palette.color(QPalette::Text), 1.4));
    painter.setBrush(info->visible ? palette.color(QPalette::Text) : Qt::NoBrush);
    painter.drawEllipse(QPointF(14.5, cy), 4.5, 4.5);
    if (!info->visible)
    {
        painter.drawLine(QPointF(10.5, cy + 4.0), QPointF(18.5, cy - 4.0));
    }

    // 组锁（眼旁）
    const qreal lockX = 26.0;
    QColor lockColor = info->locked ? QColor(0xE8, 0xB4, 0x30) : QColor(0x66, 0x66, 0x6E);
    painter.setPen(QPen(lockColor, 1.4));
    painter.setBrush(lockColor);
    painter.drawRect(QRectF(lockX, cy - 1.0, 7.0, 5.5));
    painter.setBrush(Qt::NoBrush);
    if (info->locked)
    {
        painter.drawArc(QRectF(lockX + 1.2, cy - 5.2, 4.6, 4.6), 180 * 16, -180 * 16);
    }
    else
    {
        painter.drawArc(QRectF(lockX + 1.2, cy - 5.2, 4.6, 5.2), 180 * 16, -160 * 16);
    }

    // 组名 + 成员数
    const bool currentInside = (mEditor->layers()->currentLayerIndex() >= 0
                                && members.contains(mEditor->layers()->currentLayerIndex()));
    painter.setPen(currentInside ? Theme::AccentHover : palette.color(QPalette::Text));
    const QString label = QStringLiteral("%1  ×%2").arg(info->name).arg(members.size());
    painter.drawText(QPoint(52, y + height / 2 + 5), label);
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void TimeLineCells::paintGroupTrack(QPainter& painter, int groupId, int y, int height) const
{
    const QPalette palette = QApplication::palette();
    const LayerGroupInfo* info = mEditor->object()->layerGroupInfo(groupId);
    if (info == nullptr) { return; }

    // 组行底色（比普通轨道略深）
    QColor base = palette.color(QPalette::Base);
    base = QColor(qBound(0, base.red() - 10, 255), qBound(0, base.green() - 8, 255), qBound(0, base.blue() - 14, 255));
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(mOffsetX, y, width() - mOffsetX, height);

    // 成员帧范围带：组内所有关键帧的最小/最大范围
    const QPair<int, int> extent = groupFrameExtent(groupId);
    if (extent.second > extent.first)
    {
        const int left = getFrameX(extent.first - 1) + 2;
        const int right = getFrameX(extent.second - 1);
        QColor band = Theme::Accent;
        band.setAlpha(info->visible ? 70 : 28);
        painter.setBrush(band);
        painter.drawRoundedRect(QRectF(left, y + 3.0, qMax(6.0, static_cast<qreal>(right - left)), height - 6.0), 3.0, 3.0);

        // 端点小竖线
        painter.setBrush(info->visible ? Theme::Accent : QColor(0x66, 0x66, 0x6E));
        painter.drawRect(QRectF(left, y + 2.0, 2.0, height - 4.0));
        painter.drawRect(QRectF(right - 2.0, y + 2.0, 2.0, height - 4.0));
    }

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
        // selected frames highlighted on the ruler (TVP-style)
        Layer* rulerLayer = mEditor->layers()->currentLayer();
        if (rulerLayer != nullptr && rulerLayer->hasAnySelectedFrames())
        {
            painter.setPen(Qt::NoPen);
            QColor hl = Theme::Accent;
            hl.setAlpha(40);
            painter.setBrush(hl);
            const int standardWidth = mFrameSize - 2;
            for (int framePos : rulerLayer->getSelectedFramesByPos())
            {
                const int x = getFrameX(framePos) - standardWidth;
                painter.drawRect(QRect(x, 1, mFrameSize + 1, 4));
            }
        }

        // "+" handle drag-create preview
        paintPlusPreview(painter);
    }

    if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        if (!isPlaying) {
            paintOnionSkin(painter);
        }

        int currentFrame = mEditor->currentFrame();
        Layer* currentLayer = mEditor->layers()->currentLayer();
        KeyFrame* keyFrame = currentLayer->getKeyFrameWhichCovers(currentFrame);
        // 当前层行可见才画当前帧红框：折叠组内成员经getLayerY兜底会把框画到组轨道行上
        if (keyFrame != nullptr && rowIndexOfLayer(mEditor->currentLayerIndex()) >= 0)
        {
            int blockLen = (currentLayer->type() == Layer::CAMERA) ? 1 : blockLengthFor(currentLayer, keyFrame);
            int recWidth = mFrameSize - 2 + (blockLen - 1) * mFrameSize;
            int recLeft = getFrameX(keyFrame->pos()) - (mFrameSize - 2);
            paintCurrentFrameBorder(painter, recLeft, getLayerY(mEditor->currentLayerIndex()) + 1, recWidth, mLayerHeight - 4);
        }

        if (!mMovingFrames && mLayerPosMoveY != -1 && mLayerPosMoveY == mEditor->currentLayerIndex()
            && rowIndexOfLayer(mLayerPosMoveY) >= 0)
        {
            // This is terrible but well...
            int recTop = getLayerY(mLayerPosMoveY) + 1;
            int standardWidth = mFrameSize - 2;
            int recHeight = mLayerHeight - 4;

            if (mHighlightFrameEnabled)
            {
                paintHighlightedFrame(painter, mHighlightedFrame, recTop, standardWidth, recHeight);
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

void TimeLineCells::setLayerHeight(int h)
{
    mLayerHeight = qBound(26, h, 110);
    mRowPrefixHeights.clear(); // 行高变了，前缀和失效
    clearCache();
    updateContent();
    update();
}

void TimeLineCells::wheelEvent(QWheelEvent* event)
{
    // Track view only: wheel scales the row height, Alt+wheel scales the frame width
    if (mType != TIMELINE_CELL_TYPE::Tracks)
    {
        QWidget::wheelEvent(event);
        return;
    }
    const int delta = event->angleDelta().y();
    if (event->modifiers() & Qt::AltModifier)
    {
        // TVP: the frame under the viewport center stays anchored while zooming
        const qreal centerFrame = mFrameOffset + (width() / 2.0) / mFrameSize;
        const int newSize = qBound(6, mFrameSize + (delta > 0 ? 4 : -4), 120);
        if (newSize != mFrameSize)
        {
            setFrameSize(newSize);
            int newOffset = qRound(centerFrame - (width() / 2.0) / newSize);
            const int maxOffset = qMax(0, mFrameLength - width() / newSize);
            newOffset = qBound(0, newOffset, maxOffset);
            if (newOffset != mFrameOffset)
            {
                mFrameOffset = newOffset;
                emit offsetChanged(newOffset);
            }
            emit frameSizeChanged(newSize);
        }
    }
    else
    {
        const int newHeight = qBound(26, mLayerHeight + (delta > 0 ? 8 : -8), 110);
        if (newHeight != mLayerHeight)
        {
            setLayerHeight(newHeight);
            emit layerHeightChanged(newHeight);
        }
    }
    event->accept();
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
    mTrimLayer = nullptr;
    mTrimKeyPos = -1;
    mTrimRippleOffset = 0;
    mPlusCreating = false;
    mPlusPreviewCount = 0;

    primaryButton = event->button();

    switch (mType)
    {
    case TIMELINE_CELL_TYPE::Layers:
        mOpacityDragLayer = -1;
        mGroupDragId = -1;

        // ---- 组头行优先拦截（箭头/眼睛/锁/选中/整组拖动/右键菜单） ----
        if (const int hgid = headerGroupIdAt(event->pos()); hgid >= 0)
        {
            const LayerGroupInfo* hinfo = mEditor->object()->layerGroupInfo(hgid);
            if (hinfo == nullptr) { break; }
            if (event->button() == Qt::RightButton)
            {
                showGroupHeaderMenu(event->pos(), hgid);
                break;
            }
            if (event->pos().x() > width() - 24)
            {
                mEditor->layers()->toggleGroupCollapsed(hgid);
            }
            else if (event->pos().x() < 30)
            {
                mEditor->layers()->setGroupVisible(hgid, !hinfo->visible);
                mEditor->getScribbleArea()->update();
            }
            else if (event->pos().x() < 38)
            {
                mEditor->layers()->setGroupLocked(hgid, !hinfo->locked);
            }
            else
            {
                // 组头主体：选中组顶成员并启动整组拖动
                const QList<int> members = mEditor->object()->layerGroupMemberIndices(hgid);
                if (!members.isEmpty())
                {
                    mEditor->layers()->setCurrentLayer(members.last());
                    mEditor->layers()->currentLayer()->deselectAll();
                    if (event->modifiers() & Qt::ShiftModifier)
                    {
                        mEditor->layers()->selectLayerRange(members.last());
                    }
                    else
                    {
                        mEditor->layers()->selectSingleLayer(members.last());
                    }
                    mGroupDragId = hgid;
                    mFromLayer = members.last();
                }
            }
            break;
        }

        // ---- 图层右键：成组入口菜单 ----
        if (event->button() == Qt::RightButton
            && layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
        {
            Layer* menuLayer = mEditor->object()->getLayer(layerNumber);
            if (menuLayer != nullptr && menuLayer->isGroupable())
            {
                showLayerGroupMenu(event->pos(), layerNumber);
                break;
            }
        }

        if (layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
        {
            Layer* hitLayer = mEditor->object()->getLayer(layerNumber);
            const int rowY = getLayerY(layerNumber);
            const int rowH = rowHeightOf(layerNumber);
            const bool expandedRow = rowH > 20;

            // TVP inline controls: lock toggle and opacity slider (own line under the name)
            if (expandedRow && rowH >= 40 && rowHasInlineControls(width()))
            {
                const int ctrlY = rowY + qRound(rowH * 0.72);
                const QRect lock = lockIconRect(width()).adjusted(0, ctrlY - 11, 0, ctrlY + 11);
                if (lock.contains(event->pos().x(), event->pos().y()))
                {
                    hitLayer->setLocked(!hitLayer->locked());
                    qDebug() << "[ui] layer" << layerNumber << "locked ->" << hitLayer->locked();
                    updateContent();
                    break;
                }
                const QRect clip = clipIconRect(width()).adjusted(0, ctrlY - 11, 0, ctrlY + 11);
                if (clip.contains(event->pos().x(), event->pos().y()))
                {
                    if (hitLayer->type() == Layer::BITMAP)
                    {
                        hitLayer->setClipMask(!hitLayer->clipMask());
                        qDebug() << "[ui] layer" << layerNumber << "clipMask ->" << hitLayer->clipMask();
                        // clipping changes how pre/post layers composite: drop all caches
                        mEditor->getScribbleArea()->onLayerChanged();
                        updateContent();
                    }
                    break;
                }
                const QRect slider = opacitySliderRect(width()).adjusted(0, ctrlY - 11, 0, ctrlY + 11);
                if (slider.contains(event->pos().x(), event->pos().y()))
                {
                    mOpacityDragLayer = layerNumber;
                    const qreal value = qBound(0.0, static_cast<qreal>(event->pos().x() - slider.x()) / slider.width(), 1.0);
                    hitLayer->setOpacity(value);
                    mEditor->getScribbleArea()->update();
                    updateContent();
                    break;
                }
            }

            if (event->pos().x() < 9)
            {
                // cycle the 8-color label: -1 -> 0 -> ... -> 7 -> -1
                Layer* labelLayer = mEditor->object()->getLayer(layerNumber);
                labelLayer->setColorIndex((labelLayer->colorIndex() + 2) % 9 - 1);
                qDebug() << "[ui] layer" << layerNumber << "label color ->" << labelLayer->colorIndex();
                mTimeLine->updateContent(); // both the layer list and the track tint
            }
            else if (event->pos().x() > width() - 24)
            {
                toggleLayerCollapsed(layerNumber);
            }
            else if (event->pos().x() < 30)
            {
                qDebug() << "[ui] layer" << layerNumber << "toggle visible";
                mEditor->switchVisibilityOfLayer(layerNumber);
            }
            else
            {
                if (mEditor->currentLayerIndex() != layerNumber)
                {
                    mEditor->layers()->setCurrentLayer(layerNumber);
                    mEditor->layers()->currentLayer()->deselectAll();
                }
                if (event->modifiers() & Qt::ShiftModifier)
                {
                    mEditor->layers()->selectLayerRange(layerNumber);
                }
                else
                {
                    mEditor->layers()->selectSingleLayer(layerNumber);
                }
            }
        }
        if (layerNumber == -1)
        {
            if (event->pos().x() < 30)
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
        // 组头行轨道区：仅换当前帧（不选层、不可编辑帧）
        if (headerGroupIdAt(event->pos()) >= 0)
        {
            if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
            {
                const int fn = getFrameNumber(event->pos().x());
                if (fn >= 1) { mEditor->scrubTo(fn); }
            }
            break;
        }
        if (event->button() == Qt::MiddleButton)
        {
            mLastFrameNumber = getFrameNumber(event->pos().x());
        }
        else
        {
            // TVP create handle (checked before resize, matching tvp_timeline)
            if (event->button() == Qt::LeftButton)
            {
                const int plusLayer = hitTestPlusHandle(event->pos());
                if (plusLayer != -1)
                {
                    if (mEditor->currentLayerIndex() != plusLayer)
                    {
                        mEditor->layers()->currentLayer()->deselectAll();
                        mEditor->layers()->setCurrentLayer(plusLayer);
                    }
                    mPlusCreating = true;
                    mPlusPreviewCount = 0;
                    qDebug() << "[ui] create-drag start: layer" << plusLayer;
                    update();
                    break;
                }
            }

            // Dreams-style trim: grabbing the right edge of a bitmap block
            // adjusts its length (after the create handle, TVP order)
            if (event->button() == Qt::LeftButton && layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
            {
                int trimPos = hitTestTrimHandle(event->pos());
                if (trimPos > 0)
                {
                    Layer* trimLayer = mEditor->object()->getLayer(layerNumber);
                    KeyFrame* trimKey = trimLayer->getKeyFrameAt(trimPos);
                    if (trimKey != nullptr)
                    {
                        qDebug() << "[ui] trim-drag start: block" << trimPos << "layer" << layerNumber;
                        if (mEditor->currentLayerIndex() != layerNumber)
                        {
                            mEditor->layers()->currentLayer()->deselectAll();
                            mEditor->layers()->setCurrentLayer(layerNumber);
                            emit mEditor->selectedFramesChanged();
                        }
                        mTrimming = true;
                        mTrimLayer = trimLayer;
                        mTrimKeyPos = trimPos;
                        int trimEnd = trimLayer->getBlockEnd(trimKey);
                        mTrimOriginalLength = (trimEnd > 0) ? (trimEnd - trimPos) : 1;
                        mTrimPreviewLength = mTrimOriginalLength;
                        updateContent();
                        break;
                    }
                }
            }



            if (frameNumber == mEditor->currentFrame() && mStartY < mOffsetY)
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
                        qDebug() << "[ui] tracks click: select frame" << frameNumber << "layer" << layerNumber
                                 << "mods" << event->modifiers();
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
                        qDebug() << "[ui] tracks click: drag selected frames at" << frameNumber << "layer" << layerNumber;
                        mCanMoveFrame = true;
                    }

                    if (currentLayer->hasAnySelectedFrames()) {
                        emit selectionChanged();
                    }

                    // TVP: clicking/selecting a block moves the playhead onto it
                    if (event->button() == Qt::LeftButton)
                    {
                        mEditor->scrubTo(frameNumber);
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
                        qDebug() << "[ui] scrub to" << frameNumber;
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

    if (mPlusCreating && mType == TIMELINE_CELL_TYPE::Tracks)
    {
        Layer* layer = mEditor->object()->getLayer(mCurrentLayerNumber);
        if (layer != nullptr && layer->isBitmapKind())
        {
            int lastPos = -1;
            layer->foreachKeyFrame([&](KeyFrame* k) { lastPos = qMax(lastPos, k->pos()); });
            if (lastPos >= 0)
            {
                const int blockLen = blockLengthFor(layer, layer->getKeyFrameAt(lastPos));
                const int endFrame = lastPos + blockLen;
                const int n = qMax(0, getFrameNumber(event->pos().x()) - endFrame + 1);
                if (n != mPlusPreviewCount)
                {
                    mPlusPreviewCount = n;
                    update();
                }
            }
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    mFramePosMoveX = getFrameNumber(mMouseMoveX);
    mLayerPosMoveY = getLayerNumber(event->pos().y());

    // TVP-style hover feedback: <-> over block edges, + over the create handle
    if (mType == TIMELINE_CELL_TYPE::Tracks && primaryButton == Qt::NoButton && !mTrimming && !mPlusCreating)
    {
        Qt::CursorShape shape = Qt::ArrowCursor;
        if (hitTestPlusHandle(event->pos()) != -1)
            shape = Qt::CrossCursor;
        else if (hitTestTrimHandle(event->pos()) != -1)
            shape = Qt::SizeHorCursor;
        else if (event->pos().y() < mOffsetY)
            shape = Qt::PointingHandCursor;
        if (cursor().shape() != shape)
            setCursor(shape);
    }

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        if (mOpacityDragLayer != -1)
        {
            // live opacity drag on the layer row
            Layer* layer = mEditor->object()->getLayer(mOpacityDragLayer);
            if (layer != nullptr)
            {
                const QRect slider = opacitySliderRect(width());
                const qreal value = qBound(0.0, static_cast<qreal>(event->pos().x() - slider.x()) / slider.width(), 1.0);
                layer->setOpacity(value);
                mEditor->getScribbleArea()->update();
                update();
            }
            QWidget::mouseMoveEvent(event);
            return;
        }
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
                // no clamp against the next keyframe: the release-side ripple
                // pushes subsequent keyframes away, so let the preview grow
                const int maxLen = mFrameLength - mTrimKeyPos + 1;
                int newLen = mFramePosMoveX - mTrimKeyPos + 1;
                mTrimPreviewLength = qBound(1, newLen, maxLen);
                mTrimRippleOffset = mTrimPreviewLength - mTrimOriginalLength;
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
                            qDebug() << "[ui] frames drag begin";;

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
        if (currentLayer == nullptr)
        {
            // 手势进行中途图层被删除（快捷键等）：丢弃手势状态，落到末尾统一复位
            mTrimming = false;
            mTrimLayer = nullptr;
            mTrimKeyPos = -1;
            mPlusCreating = false;
            mPlusPreviewCount = 0;
        }
        else if (mPlusCreating)
        {
            mPlusCreating = false;
            const int n = mPlusPreviewCount;
            mPlusPreviewCount = 0;
            if (n > 0)
            {
                Layer* layer = mEditor->layers()->getLayer(mCurrentLayerNumber);
                if (layer != nullptr && layer->isBitmapKind())
                {
                    int lastPos = -1;
                    layer->foreachKeyFrame([&](KeyFrame* k) { lastPos = qMax(lastPos, k->pos()); });
                    const int blockLen = (lastPos >= 0) ? blockLengthFor(layer, layer->getKeyFrameAt(lastPos)) : 1;
                    const int startFrame = (lastPos >= 0) ? lastPos + blockLen : getFrameNumber(mMousePressX);
                    qDebug() << "[ui] plus-create" << n << "frames from" << startFrame;

                    // one transaction = one undo step for the whole batch
                    mEditor->beginLayerLayoutEdit(layer);
                    for (int i = 0; i < n; i++)
                    {
                        layer->addNewKeyFrameAt(startFrame + i);
                    }
                    mEditor->endLayerLayoutEdit(tr("新建 %1 帧").arg(n));
                    mEditor->scrubTo(startFrame + n - 1);

                    mEditor->layers()->notifyAnimationLengthChanged();
                    emit mEditor->framesModified();
                }
            }
            updateContent();
        }
        else if (mTrimming)
        {
            mTrimming = false;
            KeyFrame* trimKey = currentLayer->getKeyFrameAt(mTrimKeyPos);
            if (trimKey != nullptr && mTrimPreviewLength != mTrimOriginalLength && !currentLayer->locked())
            {
                // Layout transaction: one undo step restores the new length,
                // the explicit flag, ripple moves AND materialized frames
                mEditor->beginLayerLayoutEdit(currentLayer);
                const int delta = mTrimPreviewLength - mTrimOriginalLength;

                QList<int> laterPos;
                currentLayer->foreachKeyFrame([&](KeyFrame* k)
                {
                    if (k->pos() > mTrimKeyPos) laterPos << k->pos();
                });
                const bool isTrailingBlock = laterPos.isEmpty();

                if (isTrailingBlock && delta > 0)
                {
                    // Trailing block: extend the exposure (explicit block
                    // length) — one wide block, no materialized keyframes
                    trimKey->setLength(mTrimPreviewLength);
                    trimKey->setLengthExplicit(true);
                }
                else if (isTrailingBlock && delta < 0)
                {
                    // Shrinking the trailing block removes the materialized
                    // frames inside the vacated span (undo restores them);
                    // legacy projects may still carry materialized blanks
                    const int prevEnd = mTrimKeyPos + mTrimOriginalLength;
                    const int newEnd = mTrimKeyPos + mTrimPreviewLength;
                    for (int f = newEnd; f < prevEnd; ++f)
                    {
                        mEditor->takeLayerKeyFrame(currentLayer, f); // stays alive in the transaction
                    }
                    trimKey->setLength(mTrimPreviewLength);
                    trimKey->setLengthExplicit(true);
                }
                else
                {
                    // Middle block: TVP-style bidirectional ripple — trimming
                    // inserts or removes time, later keyframes follow the drag
                    // either way. grow -> move far ones first; shrink -> near first
                    if (delta != 0)
                    {
                        if (delta > 0)
                            std::sort(laterPos.begin(), laterPos.end(), std::greater<int>());
                        else
                            std::sort(laterPos.begin(), laterPos.end());
                        for (int p : laterPos)
                            currentLayer->moveKeyFrame(p, delta);
                    }
                    trimKey->setLength(mTrimPreviewLength);
                    trimKey->setLengthExplicit(true);
                }
                qDebug() << "[ui] trim-drag end: block" << mTrimKeyPos
                         << "len" << mTrimOriginalLength << "->" << mTrimPreviewLength
                         << (isTrailingBlock ? "(trailing)" : "(ripple)");
                currentLayer->markFrameAsDirty(mTrimKeyPos);
                mEditor->endLayerLayoutEdit(tr("拉伸帧块"));
                // 仅真变更才通知：空点击若发射 framesModified 会白清缩略图缓存
                mEditor->layers()->notifyAnimationLengthChanged();
                emit mEditor->framesModified();
            }
            mTrimKeyPos = -1;
            mTrimLayer = nullptr;
            updateContent();
        }
        else if (mMovingFrames && mDropTargetLayer != -1 && mDropTargetLayer != mCurrentLayerNumber)
        {
            // Vertical drag onto another track: carry the selected frames over
            // （预检冲突时整批放弃并返回 false，不落任何变更）
            if (moveSelectedFramesAcrossLayers(mCurrentLayerNumber, mDropTargetLayer))
            {
                mEditor->layers()->notifyAnimationLengthChanged();
                emit mEditor->framesModified();
            }
            updateContent();
        }
        else if (mMovingFrames)
        {
            int posUnderCursor = getFrameNumber(mMousePressX);
            int offset = frameNumber - posUnderCursor;

            // offset==0（点了没拖动）跳过：否则事务会推入一个空撤销步
            if (offset != 0 && !currentLayer->locked() && currentLayer->canMoveSelectedFramesToOffset(offset)) {
                // Layout transaction: one undo step for the whole move, plus
                // TVP gap absorption for the vacated spots
                const QList<int> vacated = currentLayer->selectedKeyFramesPositions();
                mEditor->beginLayerLayoutEdit(currentLayer);

                qDebug() << "[ui] frames moved by" << offset;
                currentLayer->moveSelectedFrames(offset);
                currentLayer->absorbGapsAt(vacated);

                mEditor->endLayerLayoutEdit(tr("移动帧"));
                mEditor->layers()->notifyAnimationLengthChanged();
                emit mEditor->framesModified();
            }
            updateContent();
        }
        else if (!mTimeLine->scrubbing && !mMovingFrames && !mClickSelecting && !mBoxSelecting)
        {
            // De-selecting if we didn't move, scrub nor select anything
            bool multipleSelection = (event->modifiers() == Qt::ControlModifier);

            qDebug() << "[ui] tracks release: toggle frame" << frameNumber << "multi" << multipleSelection;
            // Add/remove from already selected
            currentLayer->toggleFrameSelected(frameNumber, multipleSelection);
            emit mEditor->selectedFramesChanged();
            updateContent();
        }
    }
    if (mType == TIMELINE_CELL_TYPE::Layers && mStartLayerNumber != -1)
    {
        qDebug() << "[ui] layer-release: start=" << mStartLayerNumber << "at=" << layerNumber
                 << "scroll=" << mScrollingVertically << "moveY=" << mMouseMoveY
                 << "groupDrag=" << mGroupDragId;
        if (!mScrollingVertically)
        {
        if (mGroupDragId >= 0 && didDetachLayer())
        {
            // ---- 整组拖动提交：块移动（moveLayerGroup 自带单步撤销） ----
            const int toIdx = getInbetweenLayerNumber(event->pos().y());
            if (toIdx != mFromLayer && toIdx > -1 && toIdx <= mEditor->layers()->count())
            {
                mEditor->layers()->moveLayerGroup(mGroupDragId, qMin(toIdx, mEditor->layers()->count() - 1));
            }
            mGroupDragId = -1;
        }
        else if (layerNumber != -1 && layerNumber != mStartLayerNumber)
        {
            Layer* fromLayerObj = mEditor->object()->getLayer(mFromLayer);
            const bool altOut = (event->modifiers() & Qt::AltModifier) && fromLayerObj != nullptr
                                && fromLayerObj->groupId() >= 0;

            // 落点判定先行：成组/入组只看落点行与中心区，与插入目标 mToLayer 无关。
            // getInbetweenLayerNumber 是插入语义（向拖拽起点方向取整），向上拖时会把
            // 目标层取整回起点层，用它门禁成组判定会把成组一起短路掉（探针实锤）。
            const int dropRow = rowIndexAtY(event->pos().y());
            Layer* dropTarget = (dropRow >= 0 && dropRow < mRows.size() && !mRows.at(dropRow).isHeader)
                                ? mRows.at(dropRow).layer : nullptr;
            const int rowY2 = (dropRow >= 0) ? rowYAt(dropRow) : 0;
            const int rowH2 = (dropRow >= 0) ? rowHeightAt(dropRow) : 0;
            const bool inCenterZone = dropRow >= 0 && dropRow < mRows.size()
                                      && event->pos().y() >= rowY2 + rowH2 * 0.225
                                      && event->pos().y() <= rowY2 + rowH2 * 0.775;
            const bool ontoCenter = inCenterZone && dropTarget != nullptr && dropTarget != fromLayerObj
                                    && fromLayerObj != nullptr && fromLayerObj->isGroupable()
                                    && dropTarget->isGroupable();
            const bool ontoHeader = inCenterZone && dropRow >= 0 && dropRow < mRows.size()
                                    && mRows.at(dropRow).isHeader
                                    && fromLayerObj != nullptr && fromLayerObj->isGroupable();

            qDebug() << "[ui] layer-release: dropRow=" << dropRow
                     << "target=" << (dropTarget ? dropTarget->name() : QString("null"))
                     << "ontoCenter=" << ontoCenter << "ontoHeader=" << ontoHeader;

            if (ontoHeader)
            {
                // ---- 组头中心区 = 加入该组（插到组块上方紧邻位 b+1） ----
                const auto groupsBefore2 = LayerOrderCommand::captureGroups(mEditor->object());
                const QList<int> orderBefore2 = mEditor->object()->layerIdOrder();

                const int joinGid = mRows.at(dropRow).groupId;
                const QList<int> joinMembers = mEditor->object()->layerGroupMemberIndices(joinGid);
                if (!joinMembers.isEmpty() && fromLayerObj->groupId() != joinGid)
                {
                    const int insertPos = joinMembers.last() + 1; // 紧贴组块上沿
                    mEditor->object()->moveLayer(mFromLayer, insertPos);
                    fromLayerObj->setGroupId(joinGid);
                    mEditor->object()->repairLayerGroupContiguity();
                    mEditor->undoRedo()->pushUndoCommand(new LayerOrderCommand(
                        mEditor, orderBefore2, mEditor->object()->layerIdOrder(), tr("加入图层组"),
                        nullptr, groupsBefore2, LayerOrderCommand::captureGroups(mEditor->object())));
                    mEditor->layers()->setCurrentLayer(mEditor->object()->getIndex(fromLayerObj));
                    emit mEditor->updateTimeLine();
                    mEditor->getScribbleArea()->onLayerChanged();
                }
            }
            else if (ontoCenter)
            {
                // ---- 拖到层上（中心）= 成组/入组，单步撤销 ----
                qDebug() << "[ui] group-drop: 成组/入组" << fromLayerObj->name() << "->" << dropTarget->name();
                const auto groupsBefore = LayerOrderCommand::captureGroups(mEditor->object());
                const QList<int> orderBefore = mEditor->object()->layerIdOrder();

                const int targetIdx = mEditor->object()->getIndex(dropTarget);
                mEditor->object()->moveLayer(mFromLayer, targetIdx);
                const int newIdx = mEditor->object()->getIndex(fromLayerObj);
                if (dropTarget->groupId() >= 0)
                {
                    fromLayerObj->setGroupId(dropTarget->groupId());
                }
                else
                {
                    const int gid = mEditor->object()->createLayerGroup(
                        tr("组 %1").arg(mEditor->object()->layerGroups().size() + 1));
                    dropTarget->setGroupId(gid);
                    fromLayerObj->setGroupId(gid);
                }
                mEditor->object()->repairLayerGroupContiguity();
                mEditor->undoRedo()->pushUndoCommand(new LayerOrderCommand(
                    mEditor, orderBefore, mEditor->object()->layerIdOrder(), tr("图层成组"),
                    nullptr, groupsBefore, LayerOrderCommand::captureGroups(mEditor->object())));
                mEditor->layers()->setCurrentLayer(newIdx);
                emit mEditor->updateTimeLine();
                mEditor->getScribbleArea()->onLayerChanged();
            }
            else
            {
                // ---- 常规插入式重排（含 Alt 出组），单步撤销；mToLayer 只管这一分支 ----
                mToLayer = getInbetweenLayerNumber(event->pos().y());
                qDebug() << "[ui] layer-release: insert to=" << mToLayer << "from=" << mFromLayer;
                if (mToLayer != mFromLayer && mToLayer > -1 && mToLayer < mEditor->layers()->count())
                {
                    const auto groupsBefore = LayerOrderCommand::captureGroups(mEditor->object());
                    const QList<int> orderBefore = mEditor->object()->layerIdOrder();
                    if (mEditor->object()->moveLayer(mFromLayer, mToLayer))
                    {
                        const int newIdx = mEditor->object()->getIndex(fromLayerObj);
                        if (altOut)
                        {
                            fromLayerObj->setGroupId(-1);
                        }
                        else
                        {
                            // 插进别组中间（两侧同组）= 自动入组，保持连续性不变式
                            Layer* below = mEditor->object()->getLayer(newIdx - 1);
                            Layer* above = mEditor->object()->getLayer(newIdx + 1);
                            if (below != nullptr && above != nullptr)
                            {
                                const int bg = below->groupId();
                                if (bg >= 0 && above->groupId() == bg
                                    && fromLayerObj->groupId() != bg && fromLayerObj->isGroupable())
                                {
                                    fromLayerObj->setGroupId(bg);
                                }
                            }
                        }
                        mEditor->object()->repairLayerGroupContiguity();
                        const QList<int> orderAfter = mEditor->object()->layerIdOrder();
                        mEditor->undoRedo()->pushUndoCommand(
                            new LayerOrderCommand(mEditor, orderBefore, orderAfter,
                                                  altOut ? tr("移出图层组") : tr("重排图层"),
                                                  nullptr, groupsBefore,
                                                  LayerOrderCommand::captureGroups(mEditor->object())));
                        mEditor->layers()->setCurrentLayer(mToLayer);
                        emit mEditor->updateTimeLine();
                        mEditor->getScribbleArea()->onLayerChanged();
                    }
                }
            }
        }
        mGroupDragId = -1;
        }
    }

    if (mType == TIMELINE_CELL_TYPE::Layers && event->button() == Qt::LeftButton)
    {
        if (mOpacityDragLayer != -1)
        {
            // finished an opacity drag: invalidate frame caches and thumbnails
            emit mEditor->frameModified(mEditor->currentFrame());
            mOpacityDragLayer = -1;
            updateContent();
        }
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
    if (event->pos().y() < mOffsetY && (mType != TIMELINE_CELL_TYPE::Layers || event->pos().x() >= 15))
    {
        mPrefs->set(SETTING::SHORT_SCRUB, !mbShortScrub);
    }

    // -- 组头双击：重命名组 --
    if (const int dgid = headerGroupIdAt(event->pos()); dgid >= 0)
    {
        if ((mType == TIMELINE_CELL_TYPE::Layers) && (event->buttons() & Qt::LeftButton))
        {
            const LayerGroupInfo* dinfo = mEditor->object()->layerGroupInfo(dgid);
            if (dinfo != nullptr)
            {
                bool ok = false;
                QString name = QInputDialog::getText(nullptr, tr("重命名图层组"),
                                                     tr("组名："), QLineEdit::Normal, dinfo->name, &ok);
                if (ok && !name.isEmpty())
                {
                    mEditor->layers()->renameGroup(dgid, name);
                }
            }
        }
        QWidget::mouseDoubleClickEvent(event);
        return;
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

void TimeLineCells::showGroupHeaderMenu(QPoint pos, int groupId)
{
    const LayerGroupInfo* info = mEditor->object()->layerGroupInfo(groupId);
    if (info == nullptr) { return; }

    QMenu menu(this);
    QAction* toggleAction = menu.addAction(info->collapsed ? tr("展开组") : tr("收起组"));
    QAction* renameAction = menu.addAction(tr("重命名组"));
    menu.addSeparator();
    QAction* dissolveAction = menu.addAction(tr("解散组"));

    QAction* chosen = menu.exec(mapToGlobal(pos));
    if (chosen == toggleAction)
    {
        mEditor->layers()->toggleGroupCollapsed(groupId);
    }
    else if (chosen == renameAction)
    {
        bool ok = false;
        QString name = QInputDialog::getText(nullptr, tr("重命名图层组"),
                                             tr("组名："), QLineEdit::Normal, info->name, &ok);
        if (ok && !name.isEmpty())
        {
            mEditor->layers()->renameGroup(groupId, name);
        }
    }
    else if (chosen == dissolveAction)
    {
        mEditor->layers()->dissolveGroup(groupId);
    }
}

void TimeLineCells::showLayerGroupMenu(QPoint pos, int layerIndex)
{
    Layer* layer = mEditor->object()->getLayer(layerIndex);
    if (layer == nullptr || !layer->isGroupable()) { return; }

    QMenu menu(this);
    QAction* createAction = nullptr;
    QAction* groupSelectedAction = nullptr;
    QAction* leaveAction = nullptr;
    QAction* dissolveAction = nullptr;

    // 多选（含本层且全可入组）时提供批量成组
    const QList<int> selection = mEditor->layers()->selectedLayerIds();
    bool allGroupable = selection.contains(layer->id());
    for (int i = 0; i < mEditor->object()->getLayerCount() && allGroupable; ++i)
    {
        Layer* l = mEditor->object()->getLayer(i);
        if (selection.contains(l->id()) && !l->isGroupable())
        {
            allGroupable = false;
        }
    }
    if (selection.size() >= 2 && allGroupable)
    {
        groupSelectedAction = menu.addAction(tr("将选中 %1 个图层成组").arg(selection.size()));
        menu.addSeparator();
    }

    if (layer->groupId() < 0)
    {
        createAction = menu.addAction(tr("新建组（包含“%1”）").arg(layer->name()));
    }
    else
    {
        leaveAction = menu.addAction(tr("移出组"));
        dissolveAction = menu.addAction(tr("解散所在组"));
    }

    menu.addSeparator();
    QAction* deleteLayerAction = menu.addAction(tr("删除图层…"));

    // 循环模式（TVP式）：开放尾块区域的取帧回绕；显式尾块 = 播完即不受影响
    menu.addSeparator();
    QMenu* loopMenu = menu.addMenu(tr("循环模式"));
    QAction* loopHoldAction = loopMenu->addAction(tr("保持（默认）"));
    QAction* loopCycleAction = loopMenu->addAction(tr("循环"));
    QAction* loopPingPongAction = loopMenu->addAction(tr("往复循环"));
    loopHoldAction->setCheckable(true);
    loopCycleAction->setCheckable(true);
    loopPingPongAction->setCheckable(true);
    loopHoldAction->setChecked(layer->loopMode() == Layer::LoopMode::None);
    loopCycleAction->setChecked(layer->loopMode() == Layer::LoopMode::Cycle);
    loopPingPongAction->setChecked(layer->loopMode() == Layer::LoopMode::PingPong);

    QAction* chosen = menu.exec(mapToGlobal(pos));
    Layer::LoopMode newLoopMode = layer->loopMode();
    if (chosen == loopHoldAction)
    {
        newLoopMode = Layer::LoopMode::None;
    }
    else if (chosen == loopCycleAction)
    {
        newLoopMode = Layer::LoopMode::Cycle;
    }
    else if (chosen == loopPingPongAction)
    {
        newLoopMode = Layer::LoopMode::PingPong;
    }

    if (newLoopMode != layer->loopMode())
    {
        layer->setLoopMode(newLoopMode);
        qDebug() << "[ui] layer" << layerIndex << "loopMode ->" << static_cast<int>(newLoopMode);
        // 取帧方式变化影响整段显示与渲染缓存
        mEditor->getScribbleArea()->onLayerChanged();
        updateContent();
        return;
    }

    if (chosen == deleteLayerAction)
    {
        // 与工具栏删除按钮同链：确认弹窗 + 相机守卫都在 ActionCommands
        mEditor->layers()->setCurrentLayer(layerIndex);
        Q_EMIT deleteLayerRequested(layerIndex);
        return;
    }

    if (chosen == groupSelectedAction)
    {
        mEditor->layers()->groupSelectedLayers();
    }
    else if (chosen == createAction)
    {
        mEditor->layers()->createGroupWithLayer(layerIndex);
    }
    else if (chosen == leaveAction)
    {
        mEditor->layers()->removeLayerFromGroup(layerIndex);
    }
    else if (chosen == dissolveAction)
    {
        mEditor->layers()->dissolveGroup(layer->groupId());
    }
}

void TimeLineCells::hScrollChange(int x)
{
    mFrameOffset = x;
    updateContent();
}

int TimeLineCells::hitTestTrimHandle(const QPoint& pos) const
{
    if (mType != TIMELINE_CELL_TYPE::Tracks) { return -1; }
    if (headerGroupIdAt(pos) >= 0) { return -1; } // 组头行不可 trim

    const int layerNumber = getLayerNumber(pos.y());
    if (layerNumber < 0 || layerNumber >= mEditor->object()->getLayerCount()) { return -1; }

    Layer* layer = mEditor->object()->getLayer(layerNumber);
    if (layer == nullptr || !layer->isBitmapKind() || layer->locked()) { return -1; }
    if (isLayerCollapsed(layerNumber)) { return -1; } // 折叠行不画把手也不响应

    const int frameNumber = getFrameNumber(pos.x());
    KeyFrame* key = layer->getKeyFrameWhichCovers(frameNumber);
    if (key == nullptr)
    {
        // gap between blocks: dragging inside the gap stretches the preceding block
        key = layer->getLastKeyFrameAtPosition(frameNumber);
        if (key == nullptr) { return -1; }
    }

    int blockEnd = layer->getBlockEnd(key);
    if (blockEnd < 0) { blockEnd = key->pos() + 1; } // open-ended hold: single-cell block

    // The block card spans [getFrameX(pos-1)+2, getFrameX(blockEnd-1)]:
    // its visual right edge is the right border of the LAST covered frame's
    // cell — NOT getFrameX(blockEnd), which sits a full cell further right
    const int edgeX = getFrameX(blockEnd - 1);
    const int nextPos = layer->getNextKeyFramePosition(key->pos());
    const bool nearEdge = qAbs(pos.x() - edgeX) <= 7;
    // 空隙内可拖动拉伸前块，但只认紧贴块右缘一个单元格内的区域；
    // 更远的空白留给点击定位播放头（否则整段空隙都点不动播放头）
    const bool inGap = frameNumber >= blockEnd && (nextPos < 0 || frameNumber < nextPos)
                       && pos.x() <= edgeX + mFrameSize;
    if (nearEdge || inGap) { return key->pos(); }

    // TVP seam between adjacent blocks: hovering the start edge of a block
    // whose predecessor ends right there is the same boundary as the
    // predecessor's end edge — the <-> cursor and the trim drag act on the
    // preceding block
    if (frameNumber == key->pos())
    {
        // left border of this block's first cell == predecessor's visual edge
        const int leftBorderX = getFrameX(key->pos()) - mFrameSize;
        if (qAbs(pos.x() - leftBorderX) <= 9)
        {
            const int prevPos = layer->getPreviousKeyFramePosition(key->pos());
            if (prevPos > 0 && prevPos < key->pos())
            {
                KeyFrame* prevKey = layer->getKeyFrameAt(prevPos);
                if (prevKey != nullptr && layer->getBlockEnd(prevKey) == key->pos())
                {
                    return prevPos;
                }
            }
        }
    }
    return -1;
}

bool TimeLineCells::moveSelectedFramesAcrossLayers(int sourceIndex, int targetIndex)
{
    Layer* source = mEditor->layers()->getLayer(sourceIndex);
    Layer* target = mEditor->layers()->getLayer(targetIndex);
    if (source == nullptr || target == nullptr || source == target) { return false; }
    if (source->type() != target->type()) { return false; }
    if (source->locked() || target->locked()) { return false; } // locked layers reject edits

    const QList<int> positions = source->selectedKeyFramesPositions();
    if (positions.isEmpty()) { return false; }

    const int posUnderCursor = getFrameNumber(mMousePressX);
    int dx = mFramePosMoveX - posUnderCursor + mDropShiftFrames;

    // Keep every frame inside the timeline
    const int minPos = positions.first();
    if (minPos + dx < 1) { dx = 1 - minPos; }

    // 预检：任何落点被占（避让位移超出搜索范围时可能发生）则整批放弃，
    // 绝不半移半留——部分移动会把一次拖拽静默拆到两层
    for (int pos : positions)
    {
        if (target->keyExists(pos + dx))
        {
            qDebug() << "[ui] cross-layer drop aborted: target occupied at" << pos + dx;
            return false;
        }
    }

    // Two layers take part in the transaction: one undo step restores both
    mEditor->beginLayerLayoutEdit(source);
    mEditor->addLayerToLayoutEdit(target);

    target->deselectAll();

    // Take the selected frames out of the source layer; ownership travels with them
    QVector<QPair<int, KeyFrame*>> taken;
    for (int pos : positions)
    {
        KeyFrame* key = mEditor->takeLayerKeyFrame(source, pos);
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
            // Unreachable after the pre-check; keep as a defensive fallback
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

    // the vacated span in the source layer is absorbed by its previous block
    source->absorbGapsAt(positions);

    source->deselectAll();

    mEditor->endLayerLayoutEdit(tr("跨层移动帧"));
    return true;
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
