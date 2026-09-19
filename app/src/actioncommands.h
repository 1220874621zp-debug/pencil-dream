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
#include "pencilerror.h"
#include "filetype.h"

struct ColorToAlphaParams;
struct LayerSplitParams;
struct AutoShadowParams;

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
    Status importMovieVideo();
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
    /** 镂空检测：一键检测并填充当前帧线稿的封闭镂空/细缝（各像素取最近不透明像素的颜色） */
    Status fillHolesOnCurrentFrame();
    /** 颜色转透明度（Krita Color to Alpha 移植）：接近目标色的像素转透明、按感知色差渐变；
        allKeyFrames=false 只处理当前显示帧背后的关键帧，=true 处理图层全部关键帧 */
    Status applyColorToAlpha(const ColorToAlphaParams& params, bool allKeyFrames);
    /** 自动上阴影（CSP Shading Assist 程序化近似）：径向渐变光场+色调分离+阻塞，
        给平涂画面叠赛璐璐阴影；allKeyFrames=false 只处理当前显示帧背后的关键帧，
        =true 处理图层全部关键帧；逐帧双快照精确撤销 */
    Status applyAutoShadow(const AutoShadowParams& params, bool allKeyFrames);
    /** 拆分图层颜色（Krita Split Layer 移植）：按颜色把图层拆成多个新图层；
        allKeyFrames=true 时同色跨帧归同一层；单步撤销 */
    Status splitLayerByColor(const LayerSplitParams& params, bool allKeyFrames);
    /** 跨帧传播填色：把填色层当前帧的色点块匹配搬运到后续帧块（缺帧自动补）并批量平涂 */
    Status propagateColorizeStrokes();
    /** 清除帧：清空当前选中图层画布上的所有像素（保留帧结构，可撤销） */
    Status clearCurrentLayerCanvas();
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
};

#endif // COMMANDCENTER_H
