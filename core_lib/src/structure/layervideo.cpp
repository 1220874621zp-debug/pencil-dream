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
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>

LayerVideo::LayerVideo(int id) : Layer(id, Layer::MOVIE)
{
    setName(tr("参考视频"));
}

LayerVideo::~LayerVideo()
{
    // sink/audioOutput 均不挂父对象,统一手动释放
    delete mPlayer;
    delete mSink;
    delete mAudioOutput;
}

void LayerVideo::setVideoSource(const QString& absoluteFilePath, double videoFps, int durationFrames, int inFrame)
{
    mFilePath = absoluteFilePath;
    mVideoFps = (videoFps > 0.0) ? videoFps : 24.0;
    mSyncStarted = false;
    mLastFrameStartTime = -1;

    if (mPlayer)
    {
        delete mPlayer;
        mPlayer = nullptr;
        delete mSink;
        mSink = nullptr;
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

    ensurePlayer();
}

void LayerVideo::attachRepaintTarget(QWidget* target)
{
    if (mSink == nullptr || mRepaintHooked || target == nullptr) { return; }
    // 出帧即请求重画(异步解码延迟一帧内可见);sink/目标任一销毁自动断连
    // QWidget::update 有多个重载,取成员指针有歧义,用 lambda 调无参版
    QObject::connect(mSink, &QVideoSink::videoFrameChanged, target, [target](const QVideoFrame&)
    {
        target->update();
    });
    mRepaintHooked = true;
}

void LayerVideo::ensurePlayer()
{
    if (mPlayer || mFilePath.isEmpty() || !QFileInfo::exists(mFilePath))
    {
        return;
    }
    // Layer 非 QObject,player/sink 不能挂父对象;由本层析构负责释放。
    mPlayer = new QMediaPlayer;
    // 参考视频出声:音画同步由 QMediaPlayer 内部保证;
    // 音量跟随系统,想静音就用系统/层可见性
    mAudioOutput = new QAudioOutput;
    mPlayer->setAudioOutput(mAudioOutput);
    mSink = new QVideoSink; // 不挂父:换源时 player 重建,子对象会被连带删除造成双重释放
    mPlayer->setVideoSink(mSink);
    mPlayer->setSource(QUrl::fromLocalFile(mFilePath));
}

bool LayerVideo::isFileMissing() const
{
    return mFilePath.isEmpty() || !QFileInfo::exists(mFilePath);
}

void LayerVideo::syncToFrame(int frameNumber, double projectFps, bool playing)
{
    if (mPlayer == nullptr) { return; }

    // 工程停止播放:立即暂停并复位起步标记,下次播放从准确位置起步
    if (!playing)
    {
        if (mPlayer->playbackState() == QMediaPlayer::PlayingState)
        {
            mPlayer->pause();
        }
        mSyncStarted = false;
        return;
    }

    VideoClip* clip = (keyFrameCount() > 0)
        ? static_cast<VideoClip*>(getKeyFrameAt(firstKeyFramePosition()))
        : nullptr;
    if (clip == nullptr) { return; }

    const qint64 rel = frameNumber - clip->pos();
    if (rel < 0 || rel >= clip->length())
    {
        // 时间轴出了视频区间:停住等回来,画面保留最后一帧
        if (mPlayer->playbackState() != QMediaPlayer::PausedState)
        {
            mPlayer->pause();
            mSyncStarted = false;
        }
        return;
    }

    const qint64 targetMs = qRound(rel * 1000.0 / mVideoFps);
    if (!mSyncStarted)
    {
        mPlayer->setPosition(targetMs);
        mPlayer->play();
        // 帧率失配(视频 29.97 vs 工程 24)时校准播放速率,否则缓慢漂移
        if (projectFps > 0.0 && mVideoFps > 0.0)
        {
            mPlayer->setPlaybackRate(qBound(0.1, projectFps / mVideoFps, 10.0));
        }
        mSyncStarted = true;
        return;
    }

    // 自由播放中只纠明显失步(比如视频比工程 fps 更快累积的漂移)
    if (qAbs(targetMs - mPlayer->position()) > 300)
    {
        mPlayer->setPosition(targetMs);
    }
}

QImage LayerVideo::currentFrameImage() const
{
    if (mSink == nullptr) { return QImage(); }
    const QVideoFrame frame = mSink->videoFrame();
    if (!frame.isValid()) { return mLastFrame; }
    if (frame.startTime() != mLastFrameStartTime)
    {
        mLastFrame = frame.toImage();
        mLastFrameStartTime = frame.startTime();
    }
    return mLastFrame;
}

QDomElement LayerVideo::createDomElement(QDomDocument& doc) const
{
    QDomElement layerElem = createBaseDomElement(doc);
    layerElem.setAttribute("src", mFilePath);
    layerElem.setAttribute("fps", QString::number(mVideoFps, 'f', 6));
    layerElem.setAttribute("scale", QString::number(mScale, 'f', 4));
    layerElem.setAttribute("offsetX", QString::number(mOffset.x(), 'f', 2));
    layerElem.setAttribute("offsetY", QString::number(mOffset.y(), 'f', 2));

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
    mSyncStarted = false;
    mLastFrameStartTime = -1;
    ensurePlayer();

    if (progressStep) { progressStep(); }
}
