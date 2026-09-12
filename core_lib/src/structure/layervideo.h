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
#include <QList>
#include <QMap>
#include <QSet>
#include "keyframe.h"
#include "layer.h"

class QAudioOutput;
class QMediaPlayer;
class QWidget;
class QThread;
class VideoDecodeWorker;

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

// 参考视频层:链接式导入,friction 式按需解码管线——
//   帧画面 = VideoDecodeWorker 工作线程 libav 解码 + 帧号缓存(LRU,MB 上限),
//   播放时前瞻预取、scrub 时精确按需解;声音 = QMediaPlayer 纯音频跟随时间轴。
// 解码/转换永不发生在 GUI 线程,播放消费的是缓存里现成的 QImage。
// 不进编辑/导出管线(纯参考)。
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

    // 工程帧(相对入点)→ 视频帧号映射;fps 一致时为恒等
    static int videoFrameIndexForRel(int rel, double videoFps, double projectFps);

    /** 显示缩放(1.0=原始尺寸);静态属性不参与关键帧 */
    qreal videoScale() const { return mScale; }
    void setVideoScale(qreal scale) { mScale = qBound(0.05, scale, 8.0); }

    /** 显示位移(画布世界坐标,0=居中);静态属性不参与关键帧 */
    QPointF videoOffset() const { return mOffset; }
    void setVideoOffset(const QPointF& offset) { mOffset = offset; }

    /** 声音开关(false=静音,音轨不随播放出声);静态属性不参与关键帧 */
    bool videoMuted() const { return mVideoMuted; }
    void setVideoMuted(bool muted);

    // 时间轴同步(Editor::scrubTo 每帧驱动):
    // 音频=QMediaPlayer 区间内起播/失步纠偏;帧=以当前视频帧号为中心开预取窗。
    // playing=工程播放态(预取窗更大),停止/scrub 也照常请求(scrub 精确解)。
    void syncToFrame(int frameNumber, double projectFps, bool playing);

    // 画布取当前帧:精确命中回缓存,未命中回"最近的在前帧"(解码追上后异步刷新)。
    QImage currentFrameImage() const;

    // 解码组件可用性(进程级 ffmpeg DLL 已加载);失败原因供画布占位提示
    bool isDecoderAvailable() const;
    QString decoderHint() const;

    // 解码器异步出帧后经该目标触发画布重画(invalidateCanvasCache 槽优先,
    // 找不到槽回退 update);setObject 每帧重新挂接。
    void attachRepaintTarget(QWidget* target);

private:
    void ensureAudioPlayer();
    void ensureDecoder();
    void teardownDecoder();
    void requestWindow(int centerIdx, bool playing);
    void onFrameDecoded(int frameIndex, const QImage& image);
    void onDecoderOpened(bool ok, const QString& error, int frameCount,
                         double streamFps, int width, int height);
    void trimCache();

    QString mFilePath;
    double mVideoFps = 24.0;
    qreal mScale = 1.0;
    QPointF mOffset;
    bool mVideoMuted = false;

    // 音频(QMediaPlayer 只接 QAudioOutput,不再接视频 sink)
    QMediaPlayer* mPlayer = nullptr;
    QAudioOutput* mAudioOutput = nullptr;
    bool mSyncStarted = false;

    // 帧解码管线(Layer 非 QObject,worker/线程/桥手动管理生命周期)
    VideoDecodeWorker* mWorker = nullptr;
    QThread* mWorkerThread = nullptr;
    QObject* mBridge = nullptr;      // worker→GUI 队列信号的接收上下文
    bool mDecoderReady = false;
    QString mDecoderError;
    int mFrameCount = 0;
    int mCurVideoIdx = 0;

    QMap<int, QImage> mFrameCache;   // 视频帧号 → 解码结果
    QList<int> mFrameLru;            // 驱逐序(头=最旧)
    QSet<int> mPending;              // 已排队未回帧(GUI 侧镜像)
    qint64 mCacheBytes = 0;

    QWidget* mRepaintTarget = nullptr;
};

#endif
