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

#include "videodecodeworker.h"

#include <QDebug>
#include <QFileInfo>
#include <QtMath>

#include "util/avruntime.h"

// decodeTowards 的 seek/前进/越界接受策略移植自 friction 的
// VideoFrameLoader::readFrame/seek(GPLv3 仓库,算法思想复用,代码重写)。

namespace
{
    // 距目标超过该帧数才 seek(否则顺序读包更快)
    int seekAheadFrames(double fps) { return qMax(12, qRound(fps * 1.5)); }
}

VideoDecodeWorker::VideoDecodeWorker(const QString& filePath, double videoFps, QObject* parent)
    : QObject(parent)
    , mFilePath(filePath)
    , mVideoFps(videoFps > 0.0 ? videoFps : 24.0)
{
}

VideoDecodeWorker::~VideoDecodeWorker()
{
    releaseContexts();
}

void VideoDecodeWorker::releaseContexts()
{
    const AvRuntime& av = avRuntime();
    if (mSwsCtx) { av.sws_freeContext(mSwsCtx); mSwsCtx = nullptr; }
    if (mFrame) { av.frame_free(&mFrame); mFrame = nullptr; }
    if (mPacket) { av.packet_free(&mPacket); mPacket = nullptr; }
    if (mCodecCtx) { av.codec_free_context(&mCodecCtx); mCodecCtx = nullptr; }
    if (mFmtCtx) { av.format_close_input(&mFmtCtx); mFmtCtx = nullptr; }
    mStream = nullptr;
    mOpened = false;
}

QString VideoDecodeWorker::avErrorText(int errnum) const
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    if (avRuntime().strerror(errnum, buf, sizeof(buf)) == 0 && buf[0] != '\0')
    {
        return QString::fromUtf8(buf);
    }
    return QStringLiteral("错误码 %1").arg(errnum);
}

void VideoDecodeWorker::open()
{
    const AvRuntime& av = avRuntime();
    if (!av.ok)
    {
        emit openFinished(false, av.errorMessage, 0, 0.0, 0, 0);
        return;
    }
    if (!QFileInfo::exists(mFilePath))
    {
        emit openFinished(false, QStringLiteral("文件不存在"), 0, 0.0, 0, 0);
        return;
    }

    int ret = av.format_open_input(&mFmtCtx, mFilePath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0)
    {
        mFmtCtx = nullptr; // open_input 失败时 ffmpeg 已自行释放上下文
        emit openFinished(false, QStringLiteral("无法打开视频: %1").arg(avErrorText(ret)), 0, 0.0, 0, 0);
        return;
    }
    ret = av.format_find_stream_info(mFmtCtx, nullptr);
    if (ret < 0)
    {
        releaseContexts();
        emit openFinished(false, QStringLiteral("无法读取流信息: %1").arg(avErrorText(ret)), 0, 0.0, 0, 0);
        return;
    }

    const AVCodec* decoder = nullptr;
    mVideoStreamIndex = av.find_best_stream(mFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (mVideoStreamIndex < 0 || decoder == nullptr)
    {
        releaseContexts();
        emit openFinished(false, QStringLiteral("无视频轨"), 0, 0.0, 0, 0);
        return;
    }
    mStream = mFmtCtx->streams[mVideoStreamIndex];

    mCodecCtx = av.codec_alloc_context3(decoder);
    // 参考视频逐帧 seek 用不上帧级多线程,单线程解码足够跑赢播放消耗
    mCodecCtx->thread_count = 1;
    const int p2cRet = av.codec_parameters_to_context(mCodecCtx, mStream->codecpar);
    const int open2Ret = av.codec_open2(mCodecCtx, decoder, nullptr);
    if (p2cRet < 0 || open2Ret < 0)
    {
        releaseContexts();
        emit openFinished(false, QStringLiteral("解码器初始化失败"), 0, 0.0, 0, 0);
        return;
    }

    mPacket = av.packet_alloc();
    mFrame = av.frame_alloc();

    // 流真实帧率:avg_frame_rate 优先(VFR 流更准),退化 r_frame_rate,再退化导入探测值
    double streamFps = av_q2d(mStream->avg_frame_rate);
    if (streamFps <= 0.0 || qIsNaN(streamFps)) { streamFps = av_q2d(mStream->r_frame_rate); }
    if (streamFps <= 0.0 || qIsNaN(streamFps)) { streamFps = mVideoFps; }
    mVideoFps = streamFps;

    int frameCount = int(mStream->nb_frames);
    if (frameCount <= 0 && mStream->duration > 0)
    {
        const qint64 durationUs = av.rescale_q(mStream->duration, mStream->time_base, AVRational{1, AV_TIME_BASE});
        frameCount = qRound(durationUs * streamFps / 1000000.0);
    }
    if (frameCount <= 0) { frameCount = 1; }

    mOpened = true;
    mLastDecodedFrame = INT_MIN;
    emit openFinished(true, QString(), frameCount, streamFps, mCodecCtx->width, mCodecCtx->height);
}

void VideoDecodeWorker::requestFrames(const QList<int>& frames, bool reset)
{
    if (reset)
    {
        mPending.clear();
        mPendingSet.clear();
    }
    for (int f : frames)
    {
        if (!mPendingSet.contains(f))
        {
            mPending.append(f);
            mPendingSet.insert(f);
        }
    }
    while (!mAbort.load() && !mPending.isEmpty())
    {
        const int target = mPending.first();
        const bool reached = mOpened && decodeTowards(target);
        // target 可能已被途中顺路投递(decodeTowards 内部 deliver 会移除);
        // 仍在队列时才在此出队,避免重复解码
        const int idx = mPending.indexOf(target);
        if (idx >= 0)
        {
            mPending.removeAt(idx);
            mPendingSet.remove(target);
        }
        Q_UNUSED(reached);
    }
}

bool VideoDecodeWorker::decodeTowards(int targetFrame)
{
    const AvRuntime& av = avRuntime();
    if (!mOpened || mVideoFps <= 0.0) { return false; }

    // 目标在当前位置之前,或太远:seek 到目标前方的关键帧
    if (targetFrame < mLastDecodedFrame ||
            targetFrame > mLastDecodedFrame + seekAheadFrames(mVideoFps))
    {
        seekNear(targetFrame, 0);
    }

    int seekTry = 1;
    while (!mAbort.load())
    {
        const int readRet = av.read_frame(mFmtCtx, mPacket);
        if (readRet < 0)
        {
            return false; // EOF 或读失败:目标当作不可得
        }
        if (mPacket->stream_index != mVideoStreamIndex)
        {
            av.packet_unref(mPacket);
            continue;
        }
        av.codec_send_packet(mCodecCtx, mPacket);
        av.packet_unref(mPacket);

        bool sawFrame = false;
        while (!mAbort.load())
        {
            const int recRet = av.codec_receive_frame(mCodecCtx, mFrame);
            if (recRet == AVERROR(EAGAIN)) { break; }
            if (recRet == AVERROR_EOF) { return sawFrame; }
            if (recRet < 0) { return false; }
            sawFrame = true;

            const int curr = frameIndexFromPts(mFrame->best_effort_timestamp);
            mLastDecodedFrame = curr;
            if (mPendingSet.contains(curr))
            {
                deliverCurrentFrame(curr);
            }
            if (curr == targetFrame) { return true; }
            if (curr > targetFrame)
            {
                // 已越过目标:先重试 seek,穷尽后接受最近可用帧
                if (seekTry <= 2)
                {
                    seekNear(targetFrame, seekTry++);
                    break; // 跳出排空循环,回外层重新读包
                }
                return true;
            }
        }
    }
    return false;
}

void VideoDecodeWorker::seekNear(int targetFrame, int tryN)
{
    const AvRuntime& av = avRuntime();
    const AVRational tb = mStream->time_base;
    const AVRational msTb = AVRational{1, 1000};
    const qint64 tsms = qMax<qint64>(0, qFloor((targetFrame - tryN) * 1000.0 / mVideoFps));
    const qint64 tm = av.rescale_q(tsms, msTb, tb);
    if (tm <= 0)
    {
        av.format_seek_file(mFmtCtx, mVideoStreamIndex, INT64_MIN, 0, 0, 0);
    }
    else
    {
        // 回退一秒余量,落点略早于目标再前进,吸收关键帧间距
        const qint64 tsms0 = qMax<qint64>(0, qFloor((targetFrame - mVideoFps - tryN) * 1000.0 / mVideoFps));
        const qint64 tm0 = av.rescale_q(tsms0, msTb, tb);
        if (av.format_seek_file(mFmtCtx, mVideoStreamIndex, tm0, tm, tm, AVSEEK_FLAG_FRAME) < 0)
        {
            av.format_seek_file(mFmtCtx, mVideoStreamIndex, INT64_MIN, 0, INT64_MAX, 0);
        }
    }
    av.codec_flush_buffers(mCodecCtx);
    mLastDecodedFrame = INT_MIN;
}

int VideoDecodeWorker::frameIndexFromPts(int64_t pts) const
{
    if (pts == AV_NOPTS_VALUE)
    {
        return mLastDecodedFrame == INT_MIN ? 0 : mLastDecodedFrame + 1;
    }
    const qint64 us = avRuntime().rescale_q(pts, mStream->time_base, AVRational{1, AV_TIME_BASE});
    const qreal frameApprox = us / 1000000.0 * mVideoFps;
    const int frameRound = qRound(frameApprox);
    // 复刻 friction:四舍五入偏出 0.4 以上时取前一帧
    return (frameRound - frameApprox > 0.4) ? frameRound - 1 : frameRound;
}

void VideoDecodeWorker::deliverCurrentFrame(int frameIndex)
{
    const AvRuntime& av = avRuntime();

    if (mSwsCtx == nullptr || mSwsW != mFrame->width || mSwsH != mFrame->height ||
            mSwsSrcFmt != mFrame->format)
    {
        if (mSwsCtx) { av.sws_freeContext(mSwsCtx); mSwsCtx = nullptr; }
        mSwsCtx = av.sws_getContext(mFrame->width, mFrame->height,
                                    static_cast<AVPixelFormat>(mFrame->format),
                                    mFrame->width, mFrame->height,
                                    AV_PIX_FMT_BGRA, SWS_BICUBIC,
                                    nullptr, nullptr, nullptr);
        mSwsW = mFrame->width;
        mSwsH = mFrame->height;
        mSwsSrcFmt = mFrame->format;
    }
    if (mSwsCtx == nullptr) { return; }

    // 小端平台 BGRA 字节序与 Format_ARGB32_Premultiplied 内存布局一致,
    // sws 直接写入 QImage 数据,零中间拷贝
    QImage image(mFrame->width, mFrame->height, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) { return; }
    uint8_t* dst[4] = { image.bits(), nullptr, nullptr, nullptr };
    const int dstStride[4] = { image.bytesPerLine(), 0, 0, 0 };
    av.sws_scale(mSwsCtx, mFrame->data, mFrame->linesize, 0, mFrame->height, dst, dstStride);

    const int idx = mPending.indexOf(frameIndex);
    if (idx >= 0) { mPending.removeAt(idx); }
    mPendingSet.remove(frameIndex);
    emit frameDecoded(frameIndex, image);
}
