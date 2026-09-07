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

#include "timeline.h"

#include <QWidget>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QMessageBox>
#include <QLabel>
#include <QWheelEvent>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QSet>
#include <algorithm>

#include "editor.h"
#include "keyframe.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "timecontrols.h"
#include "timelinecells.h"
#include "tvptoolsdialog.h"
#include "layerbitmap.h"
#include "bitmapimage.h"


TimeLine::TimeLine(QWidget* parent) : BaseDockWidget(parent)
{
}

void TimeLine::initUI()
{
    Q_ASSERT(editor() != nullptr);

    setWindowTitle(tr("Timeline", "Subpanel title"));

    QWidget* timeLineContent = new QWidget(this);

    mLayerList = new TimeLineCells(this, editor(), TIMELINE_CELL_TYPE::Layers);
    mTracks = new TimeLineCells(this, editor(), TIMELINE_CELL_TYPE::Tracks);

    mHScrollbar = new QScrollBar(Qt::Horizontal);
    mVScrollbar = new QScrollBar(Qt::Vertical);
    mVScrollbar->setMinimum(0);
    mVScrollbar->setMaximum(1);
    mVScrollbar->setPageStep(1);

    QWidget* leftWidget = new QWidget();
    leftWidget->setMinimumWidth(120);
    QWidget* rightWidget = new QWidget();

    QWidget* leftToolBar = new QWidget();
    leftToolBar->setFixedHeight(42);
    QWidget* rightToolBar = new QWidget();
    rightToolBar->setFixedHeight(42);

    // --- left widget ---
    // --------- layer buttons ---------
    QToolBar* layerButtons = new QToolBar(this);
    layerButtons->setIconSize(QSize(22,22));
    QLabel* layerLabel = new QLabel(tr("Layers:"));
    layerLabel->setIndent(5);

    QToolButton* addLayerButton = new QToolButton(this);
    addLayerButton->setIcon(QIcon(":icons/themes/playful/timeline/layer-add.svg"));
    addLayerButton->setToolTip(tr("Add Layer"));
    addLayerButton->setIconSize(QSize(26, 26));
    addLayerButton->setMinimumSize(QSize(34, 34));

    mLayerDeleteButton = new QToolButton(this);
    mLayerDeleteButton->setIcon(QIcon(":icons/themes/playful/timeline/layer-remove.svg"));
    mLayerDeleteButton->setToolTip(tr("Delete Layer"));
    mLayerDeleteButton->setIconSize(QSize(26, 26));
    mLayerDeleteButton->setMinimumSize(QSize(34, 34));

    QToolButton* duplicateLayerButton = new QToolButton(this);
    duplicateLayerButton->setIcon(QIcon(":icons/themes/playful/timeline/layer-duplicate.svg"));
    duplicateLayerButton->setToolTip(tr("Duplicate Layer"));
    duplicateLayerButton->setIconSize(QSize(26, 26));
    duplicateLayerButton->setMinimumSize(QSize(34, 34));

    // TVP "copy clear": duplicate the layer structure above the original
    // with blank frames (cleanup / trace-over workflow)
    QToolButton* copyClearButton = new QToolButton(this);
    copyClearButton->setText(tr("复制清空"));
    copyClearButton->setToolTip(tr("在原图层上方复制一个同结构图层，关键帧内容全部为空白（清稿/描线用）"));
    copyClearButton->setMinimumSize(QSize(38, 30));

    // TVP global toggles: every layer on -> every layer off (and back)
    QToolButton* allVisibleButton = new QToolButton(this);
    allVisibleButton->setText(tr("可见"));
    allVisibleButton->setToolTip(tr("全部图层可见性切换（全开→全关，有关→全开）"));
    allVisibleButton->setMinimumSize(QSize(38, 30));

    QToolButton* allLockedButton = new QToolButton(this);
    allLockedButton->setText(tr("锁定"));
    allLockedButton->setToolTip(tr("全部图层锁定切换（全解锁→全锁，有锁→全解锁）"));
    allLockedButton->setMinimumSize(QSize(38, 30));

    // TVP satellite tools dropdown: right beside the lock toggle
    QToolButton* toolsButton = new QToolButton(this);
    toolsButton->setText(tr("工具"));
    toolsButton->setToolTip(tr("口型同步 / 调色板提取 / 对位中割 / 视频抽帧"));
    toolsButton->setMinimumSize(QSize(38, 34));
    QMenu* toolsMenu = new QMenu(this);
    QAction* lipsyncAct = toolsMenu->addAction(tr("口型同步切换器"));
    QAction* paletteAct = toolsMenu->addAction(tr("调色板提取"));
    QAction* inbetweenAct = toolsMenu->addAction(tr("对位中割参考"));
    QAction* videoAct = toolsMenu->addAction(tr("视频抽帧中割"));
    toolsButton->setMenu(toolsMenu);
    toolsButton->setPopupMode(QToolButton::InstantPopup);

    layerButtons->addWidget(layerLabel);
    layerButtons->addWidget(addLayerButton);
    layerButtons->addWidget(mLayerDeleteButton);
    layerButtons->addWidget(duplicateLayerButton);
    layerButtons->addWidget(copyClearButton);
    layerButtons->addSeparator();
    layerButtons->addWidget(allVisibleButton);
    layerButtons->addWidget(allLockedButton);
    layerButtons->addWidget(toolsButton);
    layerButtons->setFixedHeight(42);

    QHBoxLayout* leftToolBarLayout = new QHBoxLayout();
    leftToolBarLayout->setContentsMargins(0, 0, 0, 0);
    leftToolBarLayout->addWidget(layerButtons);
    leftToolBar->setLayout(leftToolBarLayout);

    QAction* newBitmapLayerAct = new QAction(QIcon(":icons/themes/playful/timeline/cell-bitmap.svg"), tr("New Bitmap Layer"), this);
    QAction* newSoundLayerAct = new QAction(QIcon(":icons/themes/playful/timeline/cell-sound.svg"), tr("New Sound Layer"), this);
    QAction* newCameraLayerAct = new QAction(QIcon(":icons/themes/playful/timeline/cell-camera.svg"), tr("New Camera Layer"), this);

    QMenu* layerMenu = new QMenu(tr("Layer", "Timeline add-layer menu"), this);
    layerMenu->addAction(newBitmapLayerAct);
    layerMenu->addAction(newSoundLayerAct);
    layerMenu->addAction(newCameraLayerAct);
    addLayerButton->setMenu(layerMenu);
    addLayerButton->setPopupMode(QToolButton::InstantPopup);

    QGridLayout* leftLayout = new QGridLayout();
    leftLayout->addWidget(leftToolBar, 0, 0);
    leftLayout->addWidget(mLayerList, 1, 0);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    leftWidget->setLayout(leftLayout);

    // --- right widget ---
    // --------- key buttons ---------
    QToolBar* timelineButtons = new QToolBar(this);
    timelineButtons->setIconSize(QSize(22,22));
    QLabel* keyLabel = new QLabel(tr("Keys:"));
    keyLabel->setIndent(5);

    QToolButton* addKeyButton = new QToolButton(this);
    addKeyButton->setIcon(QIcon(":icons/themes/playful/timeline/frame-add.svg"));
    addKeyButton->setToolTip(tr("Add Frame"));
    addKeyButton->setIconSize(QSize(26, 26));
    addKeyButton->setMinimumSize(QSize(34, 34));

    QToolButton* removeKeyButton = new QToolButton(this);
    removeKeyButton->setIcon(QIcon(":icons/themes/playful/timeline/frame-remove.svg"));
    removeKeyButton->setToolTip(tr("Remove Frame"));
    removeKeyButton->setIconSize(QSize(26, 26));
    removeKeyButton->setMinimumSize(QSize(34, 34));

    QToolButton* duplicateKeyButton = new QToolButton(this);
    duplicateKeyButton->setIcon(QIcon(":icons/themes/playful/timeline/frame-duplicate.svg"));
    duplicateKeyButton->setToolTip(tr("Duplicate Frame"));
    duplicateKeyButton->setIconSize(QSize(26, 26));
    duplicateKeyButton->setMinimumSize(QSize(34, 34));

    QToolButton* holdOneButton = new QToolButton(this);
    holdOneButton->setText(QString("×1"));
    holdOneButton->setToolTip(tr("Hold 1 frame per key"));
    holdOneButton->setMinimumSize(QSize(34, 34));

    QToolButton* holdTwoButton = new QToolButton(this);
    holdTwoButton->setText(QString("×2"));
    holdTwoButton->setToolTip(tr("Hold 2 frames per key"));
    holdTwoButton->setMinimumSize(QSize(34, 34));

    QToolButton* holdThreeButton = new QToolButton(this);
    holdThreeButton->setText(QString("×3"));
    holdThreeButton->setToolTip(tr("Hold 3 frames per key"));
    holdThreeButton->setMinimumSize(QSize(34, 34));

    QToolButton* holdFourButton = new QToolButton(this);
    holdFourButton->setText(QString("×4"));
    holdFourButton->setToolTip(tr("Hold 4 frames per key"));
    holdFourButton->setMinimumSize(QSize(34, 34));

    // TVP loop-clone: repeat the selected frames (or the whole layer) N times
    QSpinBox* loopCloneSpin = new QSpinBox(this);
    loopCloneSpin->setRange(1, 99);
    loopCloneSpin->setValue(3);
    loopCloneSpin->setToolTip(tr("循环克隆次数"));
    loopCloneSpin->setFixedWidth(52);
    mLoopCloneSpin = loopCloneSpin;

    QToolButton* loopCloneButton = new QToolButton(this);
    loopCloneButton->setText(tr("循环"));
    loopCloneButton->setToolTip(tr("循环克隆帧：把选中的帧（未选中则整层）按原间隔重复指定次数"));
    loopCloneButton->setMinimumSize(QSize(38, 34));

    QSlider* zoomSlider = new QSlider(this);
    zoomSlider->setRange(6, 120);
    zoomSlider->setFixedWidth(74);
    zoomSlider->setValue(mTracks->getFrameSize());
    zoomSlider->setToolTip(tr("Adjust frame width"));
    zoomSlider->setOrientation(Qt::Horizontal);
    zoomSlider->setFocusPolicy(Qt::TabFocus);

    // zoom slider to the LEFT of the keyframe buttons (TVP)
    timelineButtons->addWidget(zoomSlider);
    timelineButtons->addSeparator();
    timelineButtons->addWidget(keyLabel);
    timelineButtons->addWidget(addKeyButton);
    timelineButtons->addWidget(removeKeyButton);
    timelineButtons->addWidget(duplicateKeyButton);
    timelineButtons->addSeparator();
    timelineButtons->addWidget(holdOneButton);
    timelineButtons->addWidget(holdTwoButton);
    timelineButtons->addWidget(holdThreeButton);
    timelineButtons->addWidget(holdFourButton);
    timelineButtons->addSeparator();
    timelineButtons->addWidget(loopCloneButton);
    timelineButtons->addWidget(loopCloneSpin);
    timelineButtons->setFixedHeight(42);

    // --------- Time controls ---------
    mTimeControls = new TimeControls(this);
    mTimeControls->setIconSize(QSize(22,22));
    mTimeControls->setEditor(editor());
    mTimeControls->initUI();
    updateLength();

    QHBoxLayout* rightToolBarLayout = new QHBoxLayout();
    rightToolBarLayout->addWidget(timelineButtons);
    // playback buttons centered, fps/speed pushed to the right end (TVP)
    rightToolBarLayout->addStretch();
    rightToolBarLayout->addWidget(mTimeControls->transportBar());
    rightToolBarLayout->addStretch();
    rightToolBarLayout->addWidget(mTimeControls->fpsBar());
    rightToolBarLayout->setContentsMargins(0, 0, 0, 0);
    rightToolBarLayout->setSpacing(0);
    rightToolBar->setLayout(rightToolBarLayout);

    QGridLayout* rightLayout = new QGridLayout();
    rightLayout->addWidget(rightToolBar, 0, 0);
    rightLayout->addWidget(mTracks, 1, 0);
    rightLayout->setRowStretch(1, 1);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightWidget->setLayout(rightLayout);

    // --- Splitter ---
    QSplitter* splitter = new QSplitter(this);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setSizes(QList<int>() << 100 << 600);


    QGridLayout* lay = new QGridLayout();
    lay->addWidget(splitter, 0, 0);
    lay->addWidget(mVScrollbar, 0, 1);
    lay->addWidget(mHScrollbar, 1, 0);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    timeLineContent->setLayout(lay);
    setWidget(timeLineContent);

    mScrollingStoppedTimer = new QTimer();
    mScrollingStoppedTimer->setSingleShot(true);

    setWindowFlags(Qt::WindowStaysOnTopHint);

    connect(mHScrollbar, &QScrollBar::valueChanged, mTracks, &TimeLineCells::hScrollChange);
    connect(mTracks, &TimeLineCells::offsetChanged, mHScrollbar, &QScrollBar::setValue);
    connect(mVScrollbar, &QScrollBar::valueChanged, mTracks, &TimeLineCells::vScrollChange);
    connect(mVScrollbar, &QScrollBar::valueChanged, mLayerList, &TimeLineCells::vScrollChange);
    connect(mVScrollbar, &QScrollBar::valueChanged, this, &TimeLine::onScrollbarValueChanged);
    connect(mScrollingStoppedTimer, &QTimer::timeout, mLayerList, &TimeLineCells::onScrollingVerticallyStopped);

    connect(splitter, &QSplitter::splitterMoved, this, &TimeLine::updateLength);

    connect(addKeyButton, &QToolButton::clicked, this, &TimeLine::insertKeyClick);
    connect(removeKeyButton, &QToolButton::clicked, this, &TimeLine::removeKeyClick);
    connect(duplicateLayerButton, &QToolButton::clicked, this , &TimeLine::duplicateLayerClick);
    connect(duplicateKeyButton, &QToolButton::clicked, this, &TimeLine::duplicateKeyClick);
    connect(holdOneButton, &QToolButton::clicked, this, [this]() { applyHoldLength(1); });
    connect(holdTwoButton, &QToolButton::clicked, this, [this]() { applyHoldLength(2); });
    connect(holdThreeButton, &QToolButton::clicked, this, [this]() { applyHoldLength(3); });
    connect(holdFourButton, &QToolButton::clicked, this, [this]() { applyHoldLength(4); });
    connect(loopCloneButton, &QToolButton::clicked, this, &TimeLine::cloneLoopFrames);
    connect(copyClearButton, &QToolButton::clicked, this, &TimeLine::duplicateLayerCleared);
    connect(lipsyncAct, &QAction::triggered, this, &TimeLine::showLipsyncDialog);
    connect(paletteAct, &QAction::triggered, this, &TimeLine::showPaletteExtractDialog);
    connect(inbetweenAct, &QAction::triggered, this, &TimeLine::showInbetweenRefsDialog);
    connect(videoAct, &QAction::triggered, this, &TimeLine::showVideoExtractDialog);

    // TVP global toggles: unanimous state flips, mixed state resolves to "all on"
    connect(allVisibleButton, &QToolButton::clicked, this, [this]()
    {
        Object* obj = editor()->object();
        bool anyInvisible = false;
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            if (!obj->getLayer(i)->visible()) { anyInvisible = true; break; }
        }
        const bool makeVisible = anyInvisible;
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            obj->getLayer(i)->setVisible(makeVisible);
        }
        emit editor()->updateTimeLine();
        editor()->getScribbleArea()->update();
        updateContent();
    });
    connect(allLockedButton, &QToolButton::clicked, this, [this]()
    {
        Object* obj = editor()->object();
        bool anyLocked = false;
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            if (obj->getLayer(i)->locked()) { anyLocked = true; break; }
        }
        const bool makeLocked = !anyLocked;
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            obj->getLayer(i)->setLocked(makeLocked);
        }
        emit editor()->updateTimeLine();
        updateContent();
    });
    connect(zoomSlider, &QSlider::valueChanged, mTracks, &TimeLineCells::setFrameSize);
    // wheel-driven scaling keeps the slider and the layer list in sync
    connect(mTracks, &TimeLineCells::frameSizeChanged, zoomSlider, &QSlider::setValue);
    connect(mTracks, &TimeLineCells::layerHeightChanged, mLayerList, &TimeLineCells::setLayerHeight);
    // collapse state stays in sync between the layer list and the track view
    connect(mLayerList, &TimeLineCells::layerCollapsedChanged, mTracks, &TimeLineCells::setLayerCollapsed);
    connect(mTracks, &TimeLineCells::layerCollapsedChanged, mLayerList, &TimeLineCells::setLayerCollapsed);

    connect(mTimeControls, &TimeControls::soundToggled, this, &TimeLine::soundClick);
    connect(mTimeControls, &TimeControls::fpsChanged, this, &TimeLine::fpsChanged);
    connect(mTimeControls, &TimeControls::fpsChanged, this, &TimeLine::updateLength);
    connect(mTimeControls, &TimeControls::playButtonTriggered, this, &TimeLine::playButtonTriggered);
    connect(editor(), &Editor::scrubbed, mTimeControls, &TimeControls::updateTimecodeLabel);
    connect(mTimeControls, &TimeControls::fpsChanged, mTimeControls, &TimeControls::setFps);
    connect(this, &TimeLine::fpsChanged, mTimeControls, &TimeControls::setFps);

    connect(newBitmapLayerAct, &QAction::triggered, this, &TimeLine::newBitmapLayer);
    connect(newSoundLayerAct, &QAction::triggered, this, &TimeLine::newSoundLayer);
    connect(newCameraLayerAct, &QAction::triggered, this, &TimeLine::newCameraLayer);
    connect(mLayerDeleteButton, &QPushButton::clicked, this, &TimeLine::deleteCurrentLayerClick);

    connect(mLayerList, &TimeLineCells::mouseMovedY, mLayerList, &TimeLineCells::setMouseMoveY);
    connect(mLayerList, &TimeLineCells::mouseMovedY, mTracks, &TimeLineCells::setMouseMoveY);
    connect(mTracks, &TimeLineCells::lengthChanged, this, &TimeLine::updateLength);
    connect(mTracks, &TimeLineCells::selectionChanged, this, &TimeLine::selectionChanged);
    connect(mTracks, &TimeLineCells::insertNewKeyFrame, this, &TimeLine::insertKeyClick);

    connect(editor(), &Editor::scrubbed, this, &TimeLine::updateFrame);
    connect(editor(), &Editor::frameModified, this, &TimeLine::updateContent);
    connect(editor(), &Editor::framesModified, this, &TimeLine::updateContent);

    LayerManager* layer = editor()->layers();
    connect(layer, &LayerManager::layerCountChanged, this, &TimeLine::updateLayerNumber);
    connect(layer, &LayerManager::currentLayerChanged, this, &TimeLine::onCurrentLayerChanged);
    mNumLayers = layer->count();

    scrubbing = false;
}

void TimeLine::showLipsyncDialog()
{
    if (mLipsyncDialog == nullptr) { mLipsyncDialog = new LipsyncDialog(editor(), this); }
    mLipsyncDialog->show();
    mLipsyncDialog->raise();
    mLipsyncDialog->activateWindow();
}

void TimeLine::showPaletteExtractDialog()
{
    if (mPaletteDialog == nullptr) { mPaletteDialog = new PaletteExtractDialog(editor(), this); }
    mPaletteDialog->show();
    mPaletteDialog->raise();
    mPaletteDialog->activateWindow();
}

void TimeLine::showInbetweenRefsDialog()
{
    if (mInbetweenDialog == nullptr) { mInbetweenDialog = new InbetweenRefsDialog(editor(), this); }
    mInbetweenDialog->show();
    mInbetweenDialog->raise();
    mInbetweenDialog->activateWindow();
}

void TimeLine::showVideoExtractDialog()
{
    if (mVideoDialog == nullptr) { mVideoDialog = new VideoExtractDialog(editor(), this); }
    mVideoDialog->show();
    mVideoDialog->raise();
    mVideoDialog->activateWindow();
}

QWidget* TimeLine::playbackBottomBar() const
{
    return mTimeControls->bottomBar();
}

void TimeLine::updateUI()
{
    updateContent();
}

void TimeLine::updateUICached()
{
    mLayerList->update();
    mTracks->update();
}

/** Extends the timeline frame length if necessary
 *
 *  @param[in] frame The new animation length
 */
void TimeLine::extendLength(int frame)
{
    int currentLength = mTracks->getFrameLength();
    if(frame > (currentLength * 0.75))
    {
        int newLength = static_cast<int>(std::max(frame, currentLength) * 1.5);

        if (newLength > 9999)
            newLength = 9999;

        mTracks->setFrameLength(newLength);
        updateLength();
    }
}

void TimeLine::resizeEvent(QResizeEvent*)
{
    updateLayerView();
}

void TimeLine::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        mHScrollbar->event(event);
    }
    else
    {
        mVScrollbar->event(event);
    }
}

void TimeLine::onScrollbarValueChanged()
{
    // After the scrollbar has been updated, prepare to trigger stopped event
    mScrollingStoppedTimer->start(150);
}

void TimeLine::updateFrame(int frameNumber)
{
    Q_ASSERT(mTracks);


    mTracks->updateFrame(mLastUpdatedFrame);
    mTracks->updateFrame(frameNumber);

    mLastUpdatedFrame = frameNumber;
}

void TimeLine::updateLayerView()
{
    int pageDisplay = (mTracks->height() - mTracks->getOffsetY()) / mTracks->getLayerHeight();

    mVScrollbar->setMinimum(0);
    mVScrollbar->setMaximum(qMax(0, mNumLayers - pageDisplay));
    updateContent();
}

void TimeLine::updateLayerNumber(int numberOfLayers)
{
    mNumLayers = numberOfLayers;
    updateLayerView();
}

void TimeLine::updateLength()
{
    int frameLength = mTracks->getFrameLength();
    mHScrollbar->setMaximum(qMax(0, frameLength - mTracks->width() / mTracks->getFrameSize()));
    mTimeControls->updateLength(frameLength);
    updateContent();
}

void TimeLine::updateContent()
{
    mLayerList->updateContent();
    mTracks->updateContent();
    update();
}

void TimeLine::setLoop(bool loop)
{
    mTimeControls->setLoop(loop);
}

void TimeLine::setPlaying(bool isPlaying)
{
    Q_UNUSED(isPlaying)
    mTimeControls->updatePlayState();
}

void TimeLine::setRangeState(bool range)
{
    mTimeControls->setRangeState(range);
}

int TimeLine::getRangeLower()
{
    return mTimeControls->getRangeLower();
}

int TimeLine::getRangeUpper()
{
    return mTimeControls->getRangeUpper();
}

void TimeLine::onObjectLoaded()
{
    mTimeControls->updateUI();
    updateLayerNumber(editor()->layers()->count());
}

void TimeLine::onCurrentLayerChanged()
{
    updateVerticalScrollbarPosition();
    mLayerDeleteButton->setEnabled(editor()->layers()->canDeleteLayer(editor()->currentLayerIndex()));
}

void TimeLine::updateVerticalScrollbarPosition()
{
    // invert index so 0 is at the top
    int idx = mNumLayers - editor()->currentLayerIndex() - 1;
    // number of visible layers
    int height = mNumLayers - mVScrollbar->maximum();
    // scroll bar position/offset
    int pos = mVScrollbar->value();

    if (idx < pos) // above visible area
    {
        mVScrollbar->setValue(idx);
    }
    else if (idx >= pos + height) // below visible area
    {
        mVScrollbar->setValue(idx - height + 1);
    }
}

/** Redistributes keyframes so each one holds n frames (TVP "set duration").
 *  Selected frames (>=2) are re-spaced from the leftmost one; unselected
 *  keyframes caught inside the re-spaced span are pushed after it, keeping
 *  their original spacing (TVP bridge collision push). The rearrangement goes
 *  through a parking zone so no frame is ever destroyed, and the whole
 *  operation is one undoable step that also resets trimmed (explicit) lengths
 *  back to auto: after hold-N the spacing alone is the exposure.
 */
void TimeLine::applyHoldLength(int n)
{
    if (n < 1) { return; }

    Layer* layer = editor()->layers()->currentLayer();
    if (layer == nullptr || layer->type() == Layer::SOUND) { return; }

    QList<int> all;
    layer->foreachKeyFrame([&all](KeyFrame* key) { all.append(key->pos()); });
    std::sort(all.begin(), all.end());

    QList<int> selected = layer->getSelectedFramesByPos();
    std::sort(selected.begin(), selected.end());
    if (selected.count() < 2) { selected = all; }
    if (selected.count() < 2) { return; }

    const int start = selected.first();

    // Equally spaced targets: the first frame keeps its spot
    QVector<QPair<int, int>> moves; // oldPos -> newPos
    for (int i = 0; i < selected.count(); i++)
    {
        moves.append(qMakePair(selected[i], start + i * n));
    }
    const int tailNew = start + (selected.count() - 1) * n;

    // Collision push: unselected keys that would end up inside the re-spaced
    // span (and everything behind the first one hit) are appended after the
    // span, preserving their original relative spacing
    QSet<int> selSet(selected.cbegin(), selected.cend());
    bool pushing = false;
    int cursor = tailNew;
    int prevOld = selected.last();
    for (int pos : all)
    {
        if (selSet.contains(pos) || pos <= start) { continue; }
        if (!pushing && pos > tailNew) { break; } // safely beyond the span
        pushing = true;
        int target = cursor + (pos - prevOld);
        target = qMax(target, cursor + 1); // keep the sequence monotonic
        moves.append(qMakePair(pos, target));
        cursor = target;
        prevOld = pos;
    }

    bool anyMove = false;
    for (const auto& move : moves)
    {
        if (move.first != move.second) { anyMove = true; break; }
    }
    if (!anyMove) { return; }

    editor()->beginLayerLayoutEdit(layer);

    // Parking-lot rearrangement: stash every moving frame far beyond the
    // layer, then drop each one on its target. Targets are unique and never
    // occupied by a frame that stays behind, so nothing is overwritten.
    const int parkBase = layer->getMaxKeyFramePosition() + moves.count() + 1000;

    for (int i = 0; i < moves.count(); i++)
    {
        if (moves[i].first == moves[i].second) { continue; }
        KeyFrame* key = layer->takeKeyFrame(moves[i].first);
        Q_ASSERT(key != nullptr);
        layer->addKeyFrame(parkBase + i, key);
    }
    for (int i = 0; i < moves.count(); i++)
    {
        if (moves[i].first == moves[i].second) { continue; }
        KeyFrame* key = layer->takeKeyFrame(parkBase + i);
        Q_ASSERT(key != nullptr);
        layer->addKeyFrame(moves[i].second, key);
    }

    // spacing is the exposure now: drop stale explicit (trimmed) lengths
    for (int i = 0; i < selected.count(); i++)
    {
        KeyFrame* key = layer->getKeyFrameAt(start + i * n);
        if (key != nullptr)
        {
            key->setLengthExplicit(false);
            key->setLength(1);
        }
    }

    layer->deselectAll();
    for (int i = 0; i < selected.count(); i++)
    {
        layer->setFrameSelected(start + i * n, true);
    }

    editor()->endLayerLayoutEdit(tr("一拍 %1").arg(n));

    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->framesModified();
    updateContent();
    editor()->updateFrame();
}

/** TVP "copy clear": duplicates the current bitmap layer right above
 *  itself, keeping the keyframe structure (positions and exposure lengths)
 *  but replacing every frame with a blank one. One undo step. */
void TimeLine::duplicateLayerCleared()
{
    Layer* source = editor()->layers()->currentLayer();
    if (source == nullptr || source->type() != Layer::BITMAP || source->locked()) { return; }

    const int sourceIndex = editor()->layers()->currentLayerIndex();

    QList<KeyFrameLayoutEntry> structure;
    source->foreachKeyFrame([&structure](KeyFrame* key)
    {
        KeyFrameLayoutEntry entry;
        entry.pos = key->pos();
        entry.length = key->length();
        entry.lengthExplicit = key->isLengthExplicit();
        structure.append(entry);
    });

    LayerBitmap* copy = editor()->layers()->createBitmapLayer(tr("%1_清空").arg(source->name()));
    if (copy == nullptr) { return; }

    // park the copy directly above its source (canvas stacking = higher index)
    const int copyIndex = editor()->layers()->count() - 1;
    editor()->object()->moveLayer(copyIndex, sourceIndex + 1);

    editor()->beginLayerLayoutEdit(copy);
    // the fresh layer ships with a default keyframe at position 1
    KeyFrame* defaultKey = editor()->takeLayerKeyFrame(copy, 1);
    delete defaultKey;
    for (const KeyFrameLayoutEntry& entry : structure)
    {
        QImage blank(1, 1, QImage::Format_ARGB32_Premultiplied);
        blank.fill(Qt::transparent);
        BitmapImage* blankImage = new BitmapImage(QPoint(0, 0), blank);
        copy->addKeyFrame(entry.pos, blankImage);
        blankImage->setLength(entry.length);
        blankImage->setLengthExplicit(entry.lengthExplicit);
    }
    editor()->endLayerLayoutEdit(tr("复制图层并清空"));

    editor()->layers()->setCurrentLayer(sourceIndex);
    emit editor()->updateTimeLine();
    editor()->getScribbleArea()->onLayerChanged();
    emit editor()->framesModified();
    editor()->layers()->notifyAnimationLengthChanged();
    updateContent();
}

/** TVP loop-clone: repeats the selected frames (or every frame of the layer
 *  when nothing is selected) N times, keeping the original spacing. The whole
 *  duplication is a single undo step. */
void TimeLine::cloneLoopFrames()
{
    Layer* layer = editor()->layers()->currentLayer();
    if (layer == nullptr || layer->type() == Layer::SOUND || layer->locked()) { return; }

    const int loops = mLoopCloneSpin ? mLoopCloneSpin->value() : 3;
    if (loops < 1) { return; }

    QList<int> positions;
    if (layer->hasAnySelectedFrames())
    {
        positions = layer->getSelectedFramesByPos();
        std::sort(positions.begin(), positions.end());
    }
    if (positions.isEmpty())
    {
        layer->foreachKeyFrame([&positions](KeyFrame* key) { positions.append(key->pos()); });
        std::sort(positions.begin(), positions.end());
    }
    if (positions.isEmpty()) { return; }

    const int first = positions.first();
    const int span = positions.last() - first + 1;

    editor()->beginLayerLayoutEdit(layer);

    for (int loop = 1; loop <= loops; ++loop)
    {
        for (int pos : positions)
        {
            const int newPos = pos + span * loop;
            if (layer->keyExists(newPos)) { continue; }
            KeyFrame* key = layer->getKeyFrameAt(pos);
            if (key == nullptr) { continue; }
            layer->addKeyFrame(newPos, key->clone());
        }
    }

    layer->deselectAll();
    editor()->endLayerLayoutEdit(tr("循环克隆 ×%1").arg(loops));

    editor()->scrubTo(first + span);
    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->framesModified();
    updateContent();
}
