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
#ifndef TVPTOOLSDIALOG_H
#define TVPTOOLSDIALOG_H

#include <QDialog>
#include <QImage>

class Editor;
class Layer;
class QSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTimer;
class QProgressBar;
class QTemporaryDir;
class QProcess;

/** 口型同步切换器：口型图层 (A/E/I/O/U/...) → 点击在当前帧插入对应口型 */
class LipsyncDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LipsyncDialog(Editor* editor, QWidget* parent = nullptr);

private slots:
    void refreshPhonemes();
    void insertPhoneme(const QString& phoneme);
    void clearCurrentFrame();

private:
    Layer* targetLayer();

    Editor* mEditor = nullptr;
    QWidget* mPhonemeGrid = nullptr;
};

/** 调色板提取：从图片量化主色 → 生成色块图层 */
class PaletteExtractDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PaletteExtractDialog(Editor* editor, QWidget* parent = nullptr);

private slots:
    void pickImageAndExtract();

private:
    Editor* mEditor = nullptr;
    QSpinBox* mCountSpin = nullptr;
    QLabel* mPreviewLabel = nullptr;
    QList<QRgb> mLastColors;
};

/** 视频抽帧中割：ffmpeg 从视频抽帧预览并导入为时间轴图层 */
class VideoExtractDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VideoExtractDialog(Editor* editor, QWidget* parent = nullptr);
    ~VideoExtractDialog() override;

private slots:
    void browseVideo();
    void probeVideo();
    void sliderMoved(int frame);
    void fetchFinished();
    void togglePlay();
    void importFrames();

private:
    void showFrame(int frame);
    void requestFrame(int frame);
    QImage renderImportImage(int frame, const QSize& targetSize);

    Editor* mEditor = nullptr;
    QLineEdit* mFfmpegEdit = nullptr;
    QLineEdit* mVideoEdit = nullptr;
    QSlider* mSlider = nullptr;
    QLabel* mFrameLabel = nullptr;
    QLabel* mPreview = nullptr;
    QPushButton* mPlayButton = nullptr;
    QSpinBox* mStartSpin = nullptr;
    QSpinBox* mEndSpin = nullptr;
    QPushButton* mImportButton = nullptr;
    QProgressBar* mProgress = nullptr;
    QTimer* mPlayTimer = nullptr;
    QProcess* mProcess = nullptr;
    QTemporaryDir* mTempDir = nullptr;

    double mFps = 12.0;
    double mDuration = 0.0;
    int mFrameCount = 0;
    int mCurrentFrame = 0;
    int mPendingFrame = -1;

    QMap<int, QImage> mFrameCache;
    QList<int> mCacheOrder;
};

#endif // TVPTOOLSDIALOG_H
