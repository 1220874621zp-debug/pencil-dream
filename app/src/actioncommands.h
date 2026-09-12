/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef COMMANDCENTER_H
#define COMMANDCENTER_H

#include <QObject>
#include <QImage>
#include "pencilerror.h"
#include "filetype.h"

class Editor;
class QWidget;
class ExportMovieDialog;


class ActionCommands : public QObject
{
    Q_OBJECT

public:
    explicit ActionCommands(QWidget* parent);
    virtual ~ActionCommands();

    void setCore(Editor* e) { mEditor = e; }

    // file
    Status importAnimatedImage();
    Status importReferenceVideo();
    Status importSound(FileType type);
    Status exportMovie(bool isGif = false);
    Status exportImageSequence();
    Status exportImage();
    Status exportGif();

    // edit
    void flipSelectionX();
    void flipSelectionY();
    void selectAll();
    void deselectAll();

    // view
    void ZoomIn();
    void ZoomOut();
    void rotateClockwise();
    void rotateCounterClockwise();

    // Animation
    void PlayStop();
    void GotoNextFrame();
    void GotoPrevFrame();
    void GotoNextKeyFrame();
    void GotoPrevKeyFrame();
    Status addNewKey();

    void resetAllTools();

    /** Will insert a keyframe at the current position and push connected frames to the right */
    Status insertKeyFrameAtCurrentPosition();
    void removeKey();
    void duplicateLayer();
    void duplicateKey();
    void moveFrameForward();
    void moveFrameBackward();
    void removeSelectedFrames();
    void reverseSelectedFrames();
    void addExposureToSelectedFrames();
    void subtractExposureFromSelectedFrames();

    // Layer
    Status addNewBitmapLayer();
    Status addNewColorizeLayer();
    Status addNewCameraLayer();
    Status addNewSoundLayer();
    Status deleteCurrentLayer();
    Status mergeLayerDown();
    /** 镂空检测：一键检测并填充当前帧线稿的封闭镂空/细缝。
     *  连续点击循环切换填充方向：自动最近邻 → 方向一（左/上侧）→ 方向二（右/下侧），
     *  每次都先还原到循环前快照再重填；中途画过画/撤销过/换帧换层则自动开新一轮。 */
    Status fillHolesOnCurrentFrame();
    void changeKeyframeLineColor();
    void changeallKeyframeLineColor();

    void setLayerVisibilityIndex(int index);

    // Help
    void help();
    void quickGuide();
    void website();
    void forum();
    void discord();
    void reportbug();
    void checkForUpdates();
    void openTemporaryDirectory();
    void about();

signals:
    /** 镂空检测循环状态变化（含下一击行为说明），用于按钮 tooltip 实时反馈 */
    void holeFillStatusChanged(const QString& tip);

private:
    void showSoundClipWarningIfNeeded();

    void exposeSelectedFrames(int offset);

    Status convertSoundToWav(const QString& filePath);

    // 导出/导入需要 ffmpeg 时先确保其可用；缺失则弹窗引导下载并选择。
    // 返回 true = ffmpeg 可用（可能刚由用户设置），false = 用户取消。
    bool ensureFFmpegAvailable();

    Editor* mEditor = nullptr;
    QWidget* mParent = nullptr;

    bool mSuppressSoundWarning = false;

    // 镂空检测循环：同层同帧且画面仍是上轮结果 → 连点换方向重填
    bool mHoleCycleActive = false;
    int mHoleCycleMode = 0;
    int mHoleCycleLayerId = -1;
    int mHoleCycleFramePos = -1;
    QImage mHoleCycleBefore;      // 循环前快照（每轮"改前"状态）
    QImage mHoleCycleLastResult;  // 上轮填充结果（点击时比对判定循环是否延续）
};

#endif // COMMANDCENTER_H
