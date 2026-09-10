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

#ifndef SOUNDCLIP_H
#define SOUNDCLIP_H

#include <memory>
#include <QVector>
#include "keyframe.h"

class SoundPlayer;

class SoundClip : public KeyFrame
{
public:
    explicit SoundClip();
    explicit SoundClip(const SoundClip&);
    ~SoundClip() override;
    SoundClip& operator=(const SoundClip& a);

    SoundClip* clone() const override;

    Status init(const QString& strSoundFile);
    bool isValid() const;

    void setSoundClipName(const QString& sName) { mOriginalSoundClipName = sName; }
    QString soundClipName() const { return mOriginalSoundClipName; }

    void attachPlayer(SoundPlayer* player);
    void detachPlayer();
    SoundPlayer* player() const;

    void play();
    void playFromPosition(int frameNumber, int fps);
    void pause();
    void stop();

    int64_t duration() const;
    void setDuration(const int64_t& duration);

    void updateLength(int fps);

    /// 峰值包络：整段音频按等长桶取多声道最大幅度（0..1），供时间轴
    /// 波形绘制；首次调用时解析 WAV 文件并缓存，非 WAV/损坏文件返回空
    const QVector<qreal>& waveformPeaks();

private:
    std::shared_ptr<SoundPlayer> mPlayer;

    QString mOriginalSoundClipName;

    QVector<qreal> mWaveformPeaks;
    bool mWaveformParsed = false;

    // Duration in seconds.
    // This is stored to update the length of the frame when the FPS changes.
    int64_t mDuration = 0;
};

#endif // SOUNDCLIP_H
