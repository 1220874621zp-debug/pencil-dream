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

#ifndef VIDEODECODEWORKER_H
#define VIDEODECODEWORKER_H

#include <QImage>
#include <QObject>
#include <QSet>
#include <atomic>
#include <climits>

// libav 前向声明:头文件不引入 ffmpeg 头(避免把第三方头告警传染给包含方),
// 实现文件经 util/avruntime.h 取完整类型。
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVPacket;
struct AVFrame;
struct SwsContext;

// 参考视频解码工作线程(friction VideoFrameLoader 同款思路,移植到 QThread+libav 动态加载):
// 每个视频层一根线程,串行消费帧号队列;GUI 线程永不触碰解码上下文,
// 解码结果以 QImage(隐式共享,跨线程拷贝廉价)经队列信号回投。
//
// 帧号=视频自身帧序号(0 起),与 pts 的换算沿用 friction 的 best_effort_timestamp 算法。
class VideoDecodeWorker : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecodeWorker(const QString& filePath, double videoFps, QObject* parent = nullptr);
    ~VideoDecodeWorker() override;

    // 跨线程直调:析构方先置中止,再 quit/wait 线程
    void requestAbort() { mAbort = true; }

public slots:
    // 打开文件并建解码上下文(在 worker 线程执行)
    void open();
    // 帧号增量入队;reset=true 丢弃现有队列(scrub 跳变后重定序)
    void requestFrames(const QList<int>& frames, bool reset);

signals:
    void frameDecoded(int frameIndex, const QImage& image);
    void openFinished(bool ok, const QString& error, int frameCount,
                      double streamFps, int width, int height);

private:
    bool decodeTowards(int targetFrame);
    void seekNear(int targetFrame, int tryN);
    int frameIndexFromPts(int64_t pts) const;
    void deliverCurrentFrame(int frameIndex);
    void releaseContexts();
    QString avErrorText(int errnum) const;

    const QString mFilePath;
    double mVideoFps;              // 导入探测值;open 后以流真实值为准
    std::atomic<bool> mAbort { false };

    // libav 上下文(仅 worker 线程访问)
    AVFormatContext* mFmtCtx = nullptr;
    AVCodecContext* mCodecCtx = nullptr;
    AVStream* mStream = nullptr;
    AVPacket* mPacket = nullptr;
    AVFrame* mFrame = nullptr;
    SwsContext* mSwsCtx = nullptr;
    int mVideoStreamIndex = -1;
    int mSwsW = 0, mSwsH = 0, mSwsSrcFmt = -1;
    int mLastDecodedFrame = INT_MIN;
    bool mOpened = false;

    QList<int> mPending;
    QSet<int> mPendingSet;
};

#endif // VIDEODECODEWORKER_H
