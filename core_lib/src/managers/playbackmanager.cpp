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

#include "playbackmanager.h"

#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <QSettings>
#include "object.h"
#include "editor.h"
#include "layersound.h"
#include "layermanager.h"
#include "soundclip.h"
#include "toolmanager.h"


PlaybackManager::PlaybackManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

PlaybackManager::~PlaybackManager()
{
    delete mElapsedTimer;
}

bool PlaybackManager::init()
{
    mTimer = new QTimer(this);
    mTimer->setTimerType(Qt::PreciseTimer);

    mFlipTimer = new QTimer(this);
    mFlipTimer->setTimerType(Qt::PreciseTimer);

    mScrubTimer = new QTimer(this);
    mScrubTimer->setTimerType(Qt::PreciseTimer);
    mSoundclipsToPLay.clear();

    QSettings settings (PENCIL2D, PENCIL2D);
    mFps = settings.value(SETTING_FPS).toInt();
    mMsecSoundScrub = settings.value(SETTING_SOUND_SCRUB_MSEC).toInt();
    if (mMsecSoundScrub == 0) { mMsecSoundScrub = 100; }
    mSoundScrub = settings.value(SETTING_SOUND_SCRUB_ACTIVE).toBool();
    mPlaybackSpeed = qBound(0.25, settings.value(SETTING_PLAYBACK_SPEED, 1.0).toDouble(), 4.0);

    mElapsedTimer = new QElapsedTimer;
    connect(mTimer, &QTimer::timeout, this, &PlaybackManager::timerTick);
    connect(mFlipTimer, &QTimer::timeout, this, &PlaybackManager::flipTimerTick);
    return true;
}

Status PlaybackManager::load(Object* o)
{
    const ObjectData* data = o->data();

    mIsLooping = data->isLooping();
    mIsRangedPlayback = data->isRangedPlayback();
    mMarkInFrame = data->getMarkInFrameNumber();
    mMarkOutFrame = data->getMarkOutFrameNumber();
    mFps = data->getFrameRate();

    updateStartFrame();
    updateEndFrame();

    return Status::OK;
}

Status PlaybackManager::save(Object* o)
{
    ObjectData* data = o->data();
    data->setLooping(mIsLooping);
    data->setRangedPlayback(mIsRangedPlayback);
    data->setMarkInFrameNumber(mMarkInFrame);
    data->setMarkOutFrameNumber(mMarkOutFrame);
    data->setFrameRate(mFps);
    data->setCurrentFrame(editor()->currentFrame());
    return Status::OK;
}

bool PlaybackManager::isPlaying()
{
    return (mTimer->isActive() || mFlipTimer->isActive());
}

void PlaybackManager::play()
{
    updateStartFrame();
    updateEndFrame();

    int frame = editor()->currentFrame();
    if (frame >= mEndFrame || frame < mStartFrame)
    {
        editor()->scrubTo(mStartFrame);
        frame = editor()->currentFrame();
    }

    mListOfActiveSoundFrames.clear();
    // Check for any sounds we should start playing part-way through.
    mCheckForSoundsHalfway = true;
    playSounds(frame);

    mTimer->setInterval(qMax(1, qRound(playbackInterval())));
    mTimer->start();

    // for error correction, please ref skipFrame()
    mPlayingFrameCounter = 1;
    mElapsedTimer->start();

    // reset real-time fps statistics
    mFpsWindowStartMsec = 0;
    mFpsWindowFrameCount = 0;

    emit playStateChanged(true);
}

void PlaybackManager::stop()
{
    mTimer->stop();
    stopSounds();
    emit playStateChanged(false);
}

void PlaybackManager::playFlipRoll()
{
    if (isPlaying()) { return; }

    int start = editor()->currentFrame();
    int tmp = start;
    mFlipList.clear();
    QSettings settings(PENCIL2D, PENCIL2D);
    mFlipRollMax = settings.value(SETTING_FLIP_ROLL_DRAWINGS).toInt();
    for (int i = 0; i < mFlipRollMax; i++)
    {
        int prev = editor()->layers()->currentLayer()->getPreviousKeyFramePosition(tmp);
        if (prev < tmp)
        {
            mFlipList.prepend(prev);
            tmp = prev;
        }
    }
    if (mFlipList.isEmpty()) { return; }

    // run the roll...
    mFlipRollInterval = settings.value(SETTING_FLIP_ROLL_MSEC).toInt();
    mFlipList.append(start);
    mFlipTimer->setInterval(mFlipRollInterval);

    editor()->scrubTo(mFlipList[0]);
    mFlipTimer->start();
    emit playStateChanged(true);
}

void PlaybackManager::playFlipInBetween()
{
    if (isPlaying()) { return; }

    LayerManager* layerMgr = editor()->layers();
    int start = editor()->currentFrame();

    int prev = layerMgr->currentLayer()->getPreviousKeyFramePosition(start);
    int next = layerMgr->currentLayer()->getNextKeyFramePosition(start);

    if (prev < start && next > start &&
            layerMgr->currentLayer()->keyExists(prev) &&
            layerMgr->currentLayer()->keyExists(next))
    {
        mFlipList.clear();
        mFlipList.append(prev);
        mFlipList.append(prev);
        mFlipList.append(start);
        mFlipList.append(next);
        mFlipList.append(next);
        mFlipList.append(start);
    }
    else
    {
        return;
    }
    // run the flip in-between...
    QSettings settings(PENCIL2D, PENCIL2D);
    mFlipInbetweenInterval = settings.value(SETTING_FLIP_INBETWEEN_MSEC).toInt();

    mFlipTimer->setInterval(mFlipInbetweenInterval);
    editor()->scrubTo(mFlipList[0]);
    mFlipTimer->start();
    emit playStateChanged(true);
}

void PlaybackManager::playScrub(int frame)
{
    if (!mSoundScrub || !mSoundclipsToPLay.isEmpty()) {return; }

    auto layerMan = editor()->layers();
    for (int i = 0; i < layerMan->count(); i++)
    {
        Layer* layer = layerMan->getLayer(i);
        if (layer->type() == Layer::SOUND && layer->visible())
        {
            KeyFrame* key = layer->getKeyFrameWhichCovers(frame);
            if (key != nullptr)
            {
                SoundClip* clip = static_cast<SoundClip*>(key);
                mSoundclipsToPLay.append(clip);
            }
        }
    }

    if (mSoundclipsToPLay.isEmpty()) { return; }

    mScrubTimer->singleShot(mMsecSoundScrub, this, &PlaybackManager::stopScrubPlayback);
    for (int i = 0; i < mSoundclipsToPLay.count(); i++)
    {
        mSoundclipsToPLay.at(i)->playFromPosition(frame, mFps);
    }
}

void PlaybackManager::setFps(int fps)
{
    if (mFps != fps)
    {
        mFps = fps;
        QSettings settings (PENCIL2D, PENCIL2D);
        settings.setValue(SETTING_FPS, fps);
        emit fpsChanged(mFps);

        // Update key-frame lengths of sound layers,
        // since the length depends on fps.
        for (int i = 0; i < object()->getLayerCount(); ++i)
        {
            Layer* layer = object()->getLayer(i);
            if (layer->type() == Layer::SOUND)
            {
                auto soundLayer = dynamic_cast<LayerSound *>(layer);
                soundLayer->updateFrameLengths(mFps);
            }
        }
    }
}

qreal PlaybackManager::playbackInterval() const
{
    Q_ASSERT(mFps > 0);
    return 1000.0 / (mFps * mPlaybackSpeed);
}

void PlaybackManager::setPlaybackSpeed(qreal speed)
{
    const qreal clamped = qBound(0.25, speed, 4.0);

    if (qFuzzyCompare(mPlaybackSpeed, clamped))
    {
        return;
    }

    mPlaybackSpeed = clamped;

    if (mTimer->isActive())
    {
        // Restart the timer with the new interval (QTimer::setInterval
        // restarts an active timer). The pacing references must be reset
        // too, otherwise the error correction in skipFrame() and the
        // drop-frame catch-up in timerTick() would misinterpret the
        // frames played so far as a huge lag.
        mTimer->setInterval(qMax(1, qRound(playbackInterval())));
        mPlayingFrameCounter = 1;
        mElapsedTimer->start();

        mFpsWindowStartMsec = 0;
        mFpsWindowFrameCount = 0;
    }
}

void PlaybackManager::playSounds(int frame)
{
    // If sound is turned off, don't play anything.
    if (!mIsPlaySound)
    {
        return;
    }

    std::vector< LayerSound* > kSoundLayers;
    for (int i = 0; i < object()->getLayerCount(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (layer->type() == Layer::SOUND)
        {
            kSoundLayers.push_back(static_cast<LayerSound*>(layer));
        }
    }

    for (LayerSound* layer : kSoundLayers)
    {
        KeyFrame* key = layer->getLastKeyFrameAtPosition(frame);

        if (!layer->visible())
        {
            continue;
        }

        if (key != nullptr)
        {
            // add keyframe position to list
            if (key->pos() + key->length() >= frame)
            {
                if (!mListOfActiveSoundFrames.contains(key->pos()))
                {
                    mListOfActiveSoundFrames.append(key->pos());
                }
            }
        }

        if (mCheckForSoundsHalfway)
        {
            // Check for sounds which we should start playing from part-way through.
            for (int i = 0; i < mListOfActiveSoundFrames.count(); i++)
            {
                int listPosition = mListOfActiveSoundFrames.at(i);
                if (layer->keyExistsWhichCovers(listPosition))
                {
                    key = layer->getKeyFrameWhichCovers(listPosition);
                    SoundClip* clip = static_cast<SoundClip*>(key);
                    clip->playFromPosition(frame, mFps);
                }
            }
        }
        else if (layer->keyExists(frame))
        {
            key = layer->getKeyFrameAt(frame);
            SoundClip* clip = static_cast<SoundClip*>(key);

            clip->play();

            // save the position of our active sound frame
            mActiveSoundFrame = frame;
        }

        if (frame >= mEndFrame)
        {
            if (layer->keyExists(mActiveSoundFrame))
            {
                key = layer->getKeyFrameWhichCovers(mActiveSoundFrame);
                SoundClip* clip = static_cast<SoundClip*>(key);
                clip->stop();

                // make sure list is cleared on end
                if (!mListOfActiveSoundFrames.isEmpty())
                    mListOfActiveSoundFrames.clear();
            }
        }
    }

    // Set flag to false, since this check should only be done when
    // starting play-back, or when looping.
    mCheckForSoundsHalfway = false;
}

/**
 * @brief PlaybackManager::skipFrame()
 * Small errors accumulate while playing animation
 * If the error is greater than a frame interval, skip a frame
 */
bool PlaybackManager::skipFrame()
{
    // uncomment these debug outputs to see what happens
    //float expectedTime = (mPlayingFrameCounter) * (1000.f / mFps);
    //qDebug("Expected:  %.2f ms", expectedTime);
    //qDebug("Actual:    %d   ms", mElapsedTimer->elapsed());

    int t = qRound((mPlayingFrameCounter - 1) * playbackInterval());
    if (mElapsedTimer->elapsed() < t)
    {
        qDebug() << "skip";
        return true;
    }

    ++mPlayingFrameCounter;
    return false;
}

void PlaybackManager::stopSounds()
{
    std::vector<LayerSound*> kSoundLayers;

    for (int i = 0; i < object()->getLayerCount(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (layer->type() == Layer::SOUND)
        {
            kSoundLayers.push_back(static_cast<LayerSound*>(layer));
        }
    }

    for (LayerSound* layer : kSoundLayers)
    {
        layer->foreachKeyFrame([](KeyFrame* key)
        {
            SoundClip* clip = static_cast<SoundClip*>(key);
            clip->stop();
        });
    }
}

void PlaybackManager::stopScrubPlayback()
{
    for (int i = 0; i < mSoundclipsToPLay.count(); i++)
    {
        mSoundclipsToPLay.at(i)->pause();
    }
    mSoundclipsToPLay.clear();
}

void PlaybackManager::timerTick()
{
    int currentFrame = editor()->currentFrame();

    // reach the end
    if (currentFrame >= mEndFrame)
    {
        if (mIsLooping)
        {
            editor()->scrubTo(mStartFrame);
            mCheckForSoundsHalfway = true;
        }
        else
        {
            stop();
        }
        return;
    }

    if (skipFrame())
        return;

    // Number of frames to advance this tick. In drop-frames mode we catch up
    // when rendering falls behind, otherwise we always advance a single frame.
    int framesToAdvance = 1;
    if (mDropFrames)
    {
        const qreal interval = playbackInterval();
        const int playedFrames = mPlayingFrameCounter - 1;
        const qreal lagMsec = mElapsedTimer->elapsed() - playedFrames * interval;
        if (lagMsec > 2.0 * interval)
        {
            framesToAdvance += static_cast<int>(lagMsec / interval);
        }
    }

    editor()->scrubTo(qMin(currentFrame + framesToAdvance, mEndFrame));

    // keep skipFrame()'s error correction in sync with the frames played
    mPlayingFrameCounter += framesToAdvance - 1;

    // real-time fps statistics, emitted every 500 ms
    ++mFpsWindowFrameCount;
    const qint64 nowMsec = mElapsedTimer->elapsed();
    const qint64 windowElapsed = nowMsec - mFpsWindowStartMsec;
    if (windowElapsed >= 500)
    {
        emit fpsMeasured(mFpsWindowFrameCount * 1000.0 / windowElapsed);
        mFpsWindowStartMsec = nowMsec;
        mFpsWindowFrameCount = 0;
    }

    int newFrame = editor()->currentFrame();
    playSounds(newFrame);
}

void PlaybackManager::flipTimerTick()
{
    if (mFlipList.count() < 2 || editor()->currentFrame() != mFlipList[0])
    {
        mFlipTimer->stop();
        editor()->scrubTo(mFlipList.last());
        emit playStateChanged(false);
    }
    else
    {
        editor()->scrubTo(mFlipList[1]);
        mFlipList.removeFirst();
    }
}

void PlaybackManager::setLooping(bool isLoop)
{
    if (mIsLooping != isLoop)
    {
        mIsLooping = isLoop;
        emit loopStateChanged(mIsLooping);
    }
}

void PlaybackManager::enableRangedPlayback(bool b)
{
    if (mIsRangedPlayback != b)
    {
        mIsRangedPlayback = b;

        updateStartFrame();
        updateEndFrame();

        emit rangedPlaybackStateChanged(mIsRangedPlayback);
    }
}

void PlaybackManager::setRangedStartFrame(int frame)
{
    mMarkInFrame = frame;
    updateStartFrame();
}

void PlaybackManager::setRangedEndFrame(int frame)
{
    mMarkOutFrame = frame;
    updateEndFrame();
}

void PlaybackManager::updateStartFrame()
{
    mStartFrame = (mIsRangedPlayback) ? mMarkInFrame : 1;
}

void PlaybackManager::updateEndFrame()
{
    int projectLength = editor()->layers()->animationLength();
    mEndFrame = (mIsRangedPlayback) ? mMarkOutFrame : projectLength;
}

void PlaybackManager::enableSound(bool b)
{
    mIsPlaySound = b;

    if (!mIsPlaySound)
    {
        stopSounds();

        // If, during playback, the sound is turned on again,
        // check for sounds partway through.
        mCheckForSoundsHalfway = true;
    }
}
