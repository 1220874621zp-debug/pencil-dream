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
#ifndef TIMECONTROL_H
#define TIMECONTROL_H

#include <QWidget>
#include <QToolBar>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>

class Editor;
class PreferenceManager;
class TimeLine;

class TimeControls : public QToolBar
{
    Q_OBJECT

public:
    TimeControls(TimeLine* parent = nullptr);
    void initUI();
    void updateUI();
    void setEditor(Editor* editor);
    void setFps(int value);
    void setLoop(bool);
    void setRangeState(bool);

    /** Jump/play/loop cluster: centered in the timeline toolbar. */
    QWidget* transportBar() const { return mTransportBar; }
    /** FPS / playback speed / live fps label: right end of the toolbar. */
    QWidget* fpsBar() const { return mFpsBar; }
    /** Playback-range row hosted in the main status bar (right end, TVP). */
    QWidget* bottomBar() const { return mBottomBar; }

    void updateLength(int frameLength);
    void updatePlayState();

    int getRangeLower() { return mPlaybackRangeCheckBox->isChecked() ? mLoopStartSpinBox->value() : -1; }
    int getRangeUpper() { return mPlaybackRangeCheckBox->isChecked() ? mLoopEndSpinBox->value() : -1; }

signals:
    void soundToggled(bool);
    void soundScrubToggled(bool);
    void fpsChanged(int);
    void playButtonTriggered();

public slots:
    /// Work-around in case the FPS spin-box "valueChanged" signal doesn't work.
    void onFpsEditingFinished();
    void updateTimecodeLabel(int frame);

private:
    void makeConnections();

    void playButtonClicked();
    void jumpToStartButtonClicked();
    void jumpToEndButtonClicked();
    void loopButtonClicked(bool bChecked);
    void playbackRangeClicked(bool bChecked);
    void loopStartValueChanged(int);
    void loopEndValueChanged(int);
    void updateSoundScrubIcon(bool soundScrubEnabled);
    void playbackSpeedChanged(int index);
    void updateFpsLabel(qreal actualFps);

    void noTimecodeText();
    void onlyFramesText();
    void sffText();
    void smpteText();

private:
    QPushButton* mPlayButton = nullptr;
    QPushButton* mJumpToEndButton = nullptr;
    QPushButton* mJumpToStartButton = nullptr;
    QPushButton* mLoopButton = nullptr;
    QPushButton* mTransparencyGridButton = nullptr;
    QPushButton* mSoundButton = nullptr;
    QPushButton* mSoundScrubButton = nullptr;
    QSpinBox*    mFpsBox = nullptr;
    QComboBox*   mPlaybackSpeedBox = nullptr;
    QLabel*      mFpsLabel = nullptr;
    QCheckBox*   mPlaybackRangeCheckBox = nullptr;
    QSpinBox*    mLoopStartSpinBox = nullptr;
    QSpinBox*    mLoopEndSpinBox = nullptr;
    QToolButton* mTimecodeSelect = nullptr;
    QLabel*      mTimecodeLabel = nullptr;
    QAction*     mNoTimecodeAction = nullptr;
    QAction*     mOnlyFramesAction = nullptr;
    QAction*     mSmpteAction = nullptr;
    QAction*     mSffAction = nullptr;

    // split bars: transport (jump/play/loop) is centered in the timeline
    // toolbar, fps/speed sits at its right end, the range row lives in the
    // main status bar (TVP layout)
    QWidget* mTransportBar = nullptr;
    QWidget* mFpsBar = nullptr;
    QWidget* mBottomBar = nullptr;

    QIcon mStartIcon;
    QIcon mStopIcon;
    QIcon mLoopIcon;
    QIcon mSoundIcon;
    QIcon mSoundScrubIcon;
    QIcon mJumpToEndIcon;
    QIcon mJumpToStartIcon;

    TimeLine* mTimeline = nullptr;
    Editor* mEditor = nullptr;
    int mFps = 12;
    int mTimecodeLabelEnum;
    bool mFpsWarned = false; // 实测帧率告警色状态（仅在翻转时重设样式表）
};

#endif
