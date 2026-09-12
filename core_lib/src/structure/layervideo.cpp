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

#include "layervideo.h"

#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QThread>
#include <QWidget>

#include "util/avruntime.h"
#include "videodecodeworker.h"

namespace
{
    // 播放预取窗口(帧):顺序软解远快于实时消费,约 1.5 秒前瞻足够吸收抖动
    constexpr int PLAY_PREFETCH = 36;
    // scrub 前瞻(帧):拖动方向不确定,小窗足够
    constexpr int SCRUB_PREFETCH = 8;
    // 当前帧身后保留(帧):循环/小幅回拖免重解
    constexpr int KEEP_BEHIND = 12;
    // 帧缓存字节上限(1080p ≈ 8.3MB/帧 → 约 46 帧;超限 LRU 逐出,重解廉价)
    constexpr qint64 CACHE_CAP_BYTES = 384LL * 1024 * 1024;
}

LayerVideo::LayerVideo(int id) : Layer(id, Layer::MOVIE)
{
    setName(tr("参考视频"));
}

LayerVideo::~LayerVideo()
{
    // 顺序敏感:先拆信号桥(投递中的队列回调随桥作废),再停线程,最后释资源
    delete mBridge;
    mBridge = nullptr;
    if (mWorker) { mWorker->requestAbort(); }
    if (mWorkerThread)
    {
        mWorkerThread->quit();
        mWorkerThread->wait(3000);
    }
    delete mWorker;
    delete mWorkerThread;
    delete mPlayer;
    delete mAudioOutput;
}

void LayerVideo::setVideoSource(const QString& absoluteFilePath, double videoFps, int durationFrames, int inFrame)
{
    mFilePath = absoluteFilePath;
    mVideoFps = (videoFps > 0.0) ? videoFps : 24.0;
    mSyncStarted = false;
    mCurVideoIdx = 0;
    mFrameCount = qMax(1, durationFrames);

    teardownDecoder();

    if (mPlayer)
    {
        delete mPlayer;
        mPlayer = nullptr;
        delete mAudioOutput;
        mAudioOutput = nullptr;
    }

    // 替换/删除旧的占位 clip,新建于入点(调用方随后可能再 setPos 挪入点)。
    // removeKeyFrame 有"最后一帧不可删"的 TVP 守卫,单 clip 层会拒绝删除,
    // 换成 takeKeyFrame(无最后帧限制,所有权移交后手动释放)。
    while (keyFrameCount() > 0)
    {
        delete takeKeyFrame(firstKeyFramePosition());
    }
    VideoClip* clip = new VideoClip;
    clip->setPos(qMax(1, inFrame));
    clip->setLength(qMax(1, durationFrames));
    clip->setLengthExplicit(true);
    loadKey(clip);

    ensureAudioPlayer();
    ensureDecoder();
}

void LayerVideo::ensureAudioPlayer()
{
    if (mPlayer || mFilePath.isEmpty() || !QFileInfo::exists(mFilePath))
    {
        return;
    }
    // Layer 非 QObject,player 不能挂父对象;由本层析构负责释放。
    // 帧画面不再走播放器(专用解码线程),只留纯音频输出
    mPlayer = new QMediaPlayer;
    mAudioOutput = new QAudioOutput;
    mAudioOutput->setMuted(mVideoMuted);
    mPlayer->setAudioOutput(mAudioOutput);
    mPlayer->setSource(QUrl::fromLocalFile(mFilePath));
}

void LayerVideo::ensureDecoder()
{
    if (mWorker || mFilePath.isEmpty() || !QFileInfo::exists(mFilePath))
    {
        return;
    }
    if (!avRuntime().ok)
    {
        mDecoderError = avRuntime().errorMessage;
        return;
    }
    mDecoderError.clear();
    mDecoderReady = false;
    mFrameCache.clear();
    mFrameLru.clear();
    mPending.clear();
    mCacheBytes = 0;

    mBridge = new QObject; // GUI 线程裸对象;接收 worker 队列信号,本层析构先行删除
    mWorkerThread = new QThread;
    mWorkerThread->setObjectName(QStringLiteral("video-decode"));
    mWorker = new VideoDecodeWorker(mFilePath, mVideoFps);
    mWorker->moveToThread(mWorkerThread);
    QObject::connect(mWorker, &VideoDecodeWorker::frameDecoded, mBridge,
                     [this](int frame, const QImage& image) { onFrameDecoded(frame, image); });
    QObject::connect(mWorker, &VideoDecodeWorker::openFinished, mBridge,
                     [this](bool ok, const QString& error, int frameCount, double streamFps, int width, int height)
    {
        onDecoderOpened(ok, error, frameCount, streamFps, width, height);
    });
    mWorkerThread->start();
    QMetaObject::invokeMethod(mWorker, "open", Qt::QueuedConnection);
}

void LayerVideo::teardownDecoder()
{
    delete mBridge;
    mBridge = nullptr;
    if (mWorker) { mWorker->requestAbort(); }
    if (mWorkerThread)
    {
        mWorkerThread->quit();
        mWorkerThread->wait(3000);
    }
    delete mWorker;
    mWorker = nullptr;
    delete mWorkerThread;
    mWorkerThread = nullptr;
    mDecoderReady = false;
    mFrameCache.clear();
    mFrameLru.clear();
    mPending.clear();
    mCacheBytes = 0;
    mCurVideoIdx = 0;
}

void LayerVideo::onDecoderOpened(bool ok, const QString& error, int frameCount,
                                 double streamFps, int width, int height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    if (!ok)
    {
        mDecoderReady = false;
        mDecoderError = error;
        return;
    }
    mDecoderReady = true;
    mDecoderError.clear();
    mFrameCount = qMax(1, frameCount);
    if (streamFps > 0.0)
    {
        mVideoFps = streamFps;
    }
    // 预热入点画面
    requestWindow(mCurVideoIdx, false);
}

void LayerVideo::onFrameDecoded(int frameIndex, const QImage& image)
{
    mPending.remove(frameIndex);
    if (image.isNull()) { return; }

    if (!mFrameCache.contains(frameIndex))
    {
        mCacheBytes += image.sizeInBytes();
    }
    mFrameCache.insert(frameIndex, image);
    mFrameLru.removeAll(frameIndex);
    mFrameLru.append(frameIndex);
    trimCache();

    // 画布帧级缓存(QPixmapCache)可能已合入旧视频帧,必须作废才会显示新帧;
    // ScribbleArea::invalidateCanvasCache 是 public slot,按名调用,
    // 其他目标回退 update()
    if (mRepaintTarget)
    {
        if (!QMetaObject::invokeMethod(mRepaintTarget, "invalidateCanvasCache"))
        {
            mRepaintTarget->update();
        }
    }
}

void LayerVideo::trimCache()
{
    while (mCacheBytes > CACHE_CAP_BYTES && mFrameLru.count() > 1)
    {
        const int victim = mFrameLru.takeFirst();
        const auto it = mFrameCache.find(victim);
        if (it != mFrameCache.end())
        {
            mCacheBytes -= it.value().sizeInBytes();
            mFrameCache.erase(it);
        }
    }
}

void LayerVideo::requestWindow(int centerIdx, bool playing)
{
    if (!mDecoderReady || mFrameCount <= 0) { return; }
    const int fwd = playing ? PLAY_PREFETCH : SCRUB_PREFETCH;
    const int from = qMax(0, centerIdx - KEEP_BEHIND);
    const int to = qMin(mFrameCount - 1, centerIdx + fwd);
    // scrub 跳变:队列里存在明显不属于新窗口的帧 → 整队重置重排
    bool reset = false;
    for (int f : mPending)
    {
        if (f < from - 8 || f > to + 8) { reset = true; break; }
    }
    if (reset) { mPending.clear(); }

    QList<int> wanted;
    for (int f = from; f <= to; ++f)
    {
        if (!mFrameCache.contains(f) && !mPending.contains(f))
        {
            wanted.append(f);
        }
    }
    if (wanted.isEmpty()) { return; }
    const int centerPos = wanted.indexOf(centerIdx);
    if (centerPos > 0) { wanted.move(centerPos, 0); }
    mPending.unite(QSet<int>(wanted.cbegin(), wanted.cend()));

    QMetaObject::invokeMethod(mWorker, [worker = mWorker, wanted, reset]()
    {
        worker->requestFrames(wanted, reset);
    }, Qt::QueuedConnection);
}

bool LayerVideo::isFileMissing() const
{
    return mFilePath.isEmpty() || !QFileInfo::exists(mFilePath);
}

bool LayerVideo::isDecoderAvailable() const
{
    return avRuntime().ok;
}

QString LayerVideo::decoderHint() const
{
    if (!avRuntime().ok) { return avRuntime().errorMessage; }
    return mDecoderError;
}

void LayerVideo::setVideoMuted(bool muted)
{
    mVideoMuted = muted;
    if (mAudioOutput) { mAudioOutput->setMuted(muted); }
}

int LayerVideo::videoFrameIndexForRel(int rel, double videoFps, double projectFps)
{
    if (projectFps <= 0.0 || videoFps <= 0.0)
    {
        return rel;
    }
    return qRound(rel * videoFps / projectFps);
}

void LayerVideo::syncToFrame(int frameNumber, double projectFps, bool playing)
{
    VideoClip* clip = (keyFrameCount() > 0)
                      ? static_cast<VideoClip*>(getKeyFrameAt(firstKeyFramePosition()))
                      : nullptr;
    if (clip == nullptr) { return; }

    const qint64 rel = frameNumber - clip->pos();
    const bool inRange = rel >= 0 && rel < clip->length();

    // —— 音频:区间内起播,失步超阈值才纠偏;区间外/工程停止即暂停
    if (mPlayer)
    {
        if (playing && inRange)
        {
            const double pf = projectFps > 0.0 ? projectFps : mVideoFps;
            const qint64 targetMs = qRound(rel * 1000.0 / pf);
            if (!mSyncStarted)
            {
                mPlayer->setPosition(targetMs);
                mPlayer->play();
                mPlayer->setPlaybackRate(1.0); // 帧号映射已吸收 fps 差,音频原速
                mSyncStarted = true;
            }
            else if (qAbs(targetMs - mPlayer->position()) > 300)
            {
                mPlayer->setPosition(targetMs);
            }
        }
        else if (mPlayer->playbackState() == QMediaPlayer::PlayingState)
        {
            mPlayer->pause();
            mSyncStarted = false;
        }
    }

    // —— 帧画面:以当前视频帧号为中心开窗(播放大窗/scrub 小窗,停止也跟手)
    if (mDecoderReady && inRange)
    {
        mCurVideoIdx = qBound(0, videoFrameIndexForRel(int(rel), mVideoFps,
                                                        projectFps > 0.0 ? projectFps : mVideoFps),
                              qMax(0, mFrameCount - 1));
        requestWindow(mCurVideoIdx, playing);
    }
}

QImage LayerVideo::currentFrameImage() const
{
    if (mFrameCache.isEmpty()) { return QImage(); }
    const auto exact = mFrameCache.constFind(mCurVideoIdx);
    if (exact != mFrameCache.constEnd()) { return exact.value(); }

    // 未命中:回最近的在前帧(解码异步追上后会刷新),全在后面则取最近后帧
    auto it = mFrameCache.lowerBound(mCurVideoIdx);
    if (it != mFrameCache.constEnd() && it.key() == mCurVideoIdx) { return it.value(); }
    if (it != mFrameCache.constBegin()) { --it; }
    return it.value();
}

void LayerVideo::attachRepaintTarget(QWidget* target)
{
    if (target != nullptr) { mRepaintTarget = target; }
}

QDomElement LayerVideo::createDomElement(QDomDocument& doc) const
{
    QDomElement layerElem = createBaseDomElement(doc);
    layerElem.setAttribute("src", mFilePath);
    layerElem.setAttribute("fps", QString::number(mVideoFps, 'f', 6));
    layerElem.setAttribute("scale", QString::number(mScale, 'f', 4));
    layerElem.setAttribute("offsetX", QString::number(mOffset.x(), 'f', 2));
    layerElem.setAttribute("offsetY", QString::number(mOffset.y(), 'f', 2));
    layerElem.setAttribute("muted", mVideoMuted ? "1" : "0");

    foreachKeyFrame([&doc, &layerElem](KeyFrame* keyFrame)
    {
        QDomElement clipTag = doc.createElement("videoClip");
        clipTag.setAttribute("frame", keyFrame->pos());
        clipTag.setAttribute("length", keyFrame->length());
        layerElem.appendChild(clipTag);
    });
    return layerElem;
}

void LayerVideo::loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep)
{
    Q_UNUSED(dataDirPath)
    loadBaseDomElement(element);

    const double fps = element.attribute("fps", "24").toDouble();
    int duration = 1;

    QDomNode tag = element.firstChild();
    while (!tag.isNull())
    {
        QDomElement e = tag.toElement();
        if (!e.isNull() && e.tagName() == "videoClip")
        {
            duration = qMax(1, e.attribute("length").toInt());
            VideoClip* clip = new VideoClip;
            clip->setPos(qMax(1, e.attribute("frame").toInt()));
            clip->setLength(duration);
            clip->setLengthExplicit(true);
            loadKey(clip);
        }
        tag = tag.nextSibling();
    }

    mFilePath = element.attribute("src");
    mVideoFps = (fps > 0.0) ? fps : 24.0;
    mScale = element.attribute("scale", "1.0").toDouble();
    if (mScale <= 0.0) { mScale = 1.0; }
    mOffset = QPointF(element.attribute("offsetX", "0").toDouble(),
                      element.attribute("offsetY", "0").toDouble());
    mVideoMuted = (element.attribute("muted", "0") == "1");
    mSyncStarted = false;
    mFrameCount = qMax(1, duration);
    mCurVideoIdx = 0;
    ensureAudioPlayer();
    ensureDecoder();

    if (progressStep) { progressStep(); }
}
