/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef LAYERVIDEO_H
#define LAYERVIDEO_H

#include <QImage>
#include "keyframe.h"
#include "layer.h"

class QAudioOutput;
class QMediaPlayer;
class QVideoSink;
class QWidget;

// 时间轴占位:pos=入点帧,length=时长帧数。视频本身链接外部文件不拷贝,
// 数据(路径/帧率)挂在层上,clip 只是把区间暴露给时间轴的拖动/绘制。
class VideoClip : public KeyFrame
{
public:
    explicit VideoClip() = default;
    KeyFrame* clone() const override
    {
        VideoClip* c = new VideoClip;
        c->setPos(pos());
        c->setLength(length());
        c->setLengthExplicit(true);
        return c;
    }
};

// 参考视频层:链接式导入,QMediaPlayer+QVideoSink 进程内解码,
// 跟随时间轴在画布上预览播放。不进编辑/导出管线(纯参考)。
class LayerVideo : public Layer
{
    Q_DECLARE_TR_FUNCTIONS(LayerVideo)
public:
    explicit LayerVideo(int id);
    ~LayerVideo() override;

    QDomElement createDomElement(QDomDocument& doc) const override;
    void loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep) override;

    // Layer 纯虚契约:参考视频无文件负载/无帧替换;createKeyFrame 不被
    // 正常流程触达(Editor 层已挡加帧),给出守恒实现防抽象实例化失败
    Status saveKeyFrameFile(KeyFrame*, QString) override { return Status::SAFE; }
    void replaceKeyFrame(const KeyFrame*) override {}
    KeyFrame* createKeyFrame(int position) override
    {
        VideoClip* clip = new VideoClip;
        clip->setPos(position);
        clip->setLengthExplicit(true);
        return clip;
    }

    // 设置视频源(绝对路径)。探测信息由导入方给出:视频自身帧率+总帧数。
    // 文件存在时才创建解码器;缺文件时保持占位层(画布画占位框)。
    void setVideoSource(const QString& absoluteFilePath, double videoFps, int durationFrames, int inFrame = 1);
    QString videoPath() const { return mFilePath; }
    double videoFps() const { return mVideoFps; }
    bool isFileMissing() const;

    /** 显示缩放(1.0=原始尺寸);静态属性不参与关键帧 */
    qreal videoScale() const { return mScale; }
    void setVideoScale(qreal scale) { mScale = qBound(0.05, scale, 8.0); }

    /** 显示位移(画布世界坐标,0=居中);静态属性不参与关键帧 */
    QPointF videoOffset() const { return mOffset; }
    void setVideoOffset(const QPointF& offset) { mOffset = offset; }

    /** 声音开关(false=静音,音轨不随播放出声);静态属性不参与关键帧 */
    bool videoMuted() const { return mVideoMuted; }
    void setVideoMuted(bool muted);

    // 时间轴同步(pull 模式,Editor::scrubTo 每帧驱动):
    // 区间外暂停;区间内首次起步/失步超阈值才 setPosition——
    // 播放中让解码器自由前进,不逐帧 seek。
    // playing=工程播放态:停止(false)时暂停并复位起步标记,
    // 否则工程停止后参考视频(含声音)会继续自己播。
    void syncToFrame(int frameNumber, double projectFps, bool playing);

    // 画布取当前帧:sink 最近一帧,startTime 未变时直接回缓存(省转换)。
    QImage currentFrameImage() const;

    // 解码器异步出帧后需要有人触发画布重画;sink→target 的连接在
    // 任一方销毁时自动断开,层析构无需清理。
    void attachRepaintTarget(QWidget* target);

private:
    void ensurePlayer();

    QString mFilePath;
    double mVideoFps = 24.0;
    qreal mScale = 1.0;
    QPointF mOffset;
    bool mVideoMuted = false;
    QMediaPlayer* mPlayer = nullptr;
    QVideoSink* mSink = nullptr;
    QAudioOutput* mAudioOutput = nullptr;
    mutable QImage mLastFrame;
    mutable qint64 mLastFrameStartTime = -1;
    bool mSyncStarted = false;
    bool mRepaintHooked = false;
};

#endif
