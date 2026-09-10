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

#include "soundclip.h"

#include <QFile>
#include <QMediaPlayer>
#include <QtEndian>
#include <QtMath>
#include "soundplayer.h"

namespace
{
    // 解码单个采样为 [-1,1]（小端主机，memcpy 取值即可）
    qreal decodeSample(quint16 formatTag, quint16 bits, const uchar* p)
    {
        if (formatTag == 3) // IEEE float
        {
            if (bits == 32) { float v; memcpy(&v, p, 4); return v; }
            if (bits == 64) { double v; memcpy(&v, p, 8); return v; }
            return 0;
        }
        if (formatTag == 1) // PCM int
        {
            switch (bits)
            {
            case 8:  return (static_cast<qint32>(p[0]) - 128) / 128.0;
            case 16: { qint16 v; memcpy(&v, p, 2); return v / 32768.0; }
            case 24: { qint32 v = p[0] | (p[1] << 8) | (p[2] << 16); if (v & 0x800000) v -= 0x1000000; return v / 8388608.0; }
            case 32: { qint32 v; memcpy(&v, p, 4); return v / 2147483648.0; }
            default: return 0;
            }
        }
        return 0;
    }

    // 流式解析 RIFF/WAV，整段音频按 bucketCount 等长桶取多声道最大幅度。
    // 仅支持非压缩 PCM 与 IEEE float（导入链恒转 WAV，正常都满足）；
    // 头部损坏或格式不支持返回空。
    QVector<qreal> wavPeakEnvelope(const QString& filePath, int bucketCount)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) { return {}; }

        char riff[12];
        if (file.read(riff, 12) != 12
            || qstrncmp(riff, "RIFF", 4) != 0
            || qstrncmp(riff + 8, "WAVE", 4) != 0) { return {}; }

        quint16 formatTag = 0, channels = 0, bits = 0, blockAlign = 0;
        qint64 dataOffset = -1, dataSize = 0;
        bool fmtFound = false;

        while (!file.atEnd() && !(fmtFound && dataOffset >= 0))
        {
            char chunk[8];
            if (file.read(chunk, 8) != 8) { break; }
            const quint32 size = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(chunk + 4));

            if (qstrncmp(chunk, "fmt ", 4) == 0)
            {
                QByteArray fmt = file.read(size);
                if (fmt.size() < 16) { return {}; }
                const uchar* f = reinterpret_cast<const uchar*>(fmt.constData());
                formatTag = qFromLittleEndian<quint16>(f);
                channels = qFromLittleEndian<quint16>(f + 2);
                blockAlign = qFromLittleEndian<quint16>(f + 12);
                bits = qFromLittleEndian<quint16>(f + 14);
                // WAVE_FORMAT_EXTENSIBLE：真实格式在 SubFormat GUID 前 2 字节
                if (formatTag == 0xFFFE && fmt.size() >= 40)
                {
                    formatTag = qFromLittleEndian<quint16>(f + 24);
                }
                if (channels == 0 || blockAlign == 0) { return {}; }
                fmtFound = true;
                if (size % 2 == 1) { file.seek(file.pos() + 1); } // 字对齐补位
            }
            else if (qstrncmp(chunk, "data", 4) == 0)
            {
                dataOffset = file.pos();
                dataSize = size;
                if (!fmtFound)
                {
                    // data 先于 fmt 出现：记下位置继续找 fmt，回头再扫数据
                    file.seek(file.pos() + size + (size % 2));
                }
            }
            else
            {
                file.seek(file.pos() + size + (size % 2));
            }
        }

        if (!fmtFound || dataOffset < 0) { return {}; }
        if ((formatTag != 1 && formatTag != 3) || bits == 0 || bits % 8 != 0) { return {}; }

        const int bytesPerSample = bits / 8;
        const int samplesPerFrame = qMin<int>(channels, blockAlign / bytesPerSample);
        if (samplesPerFrame <= 0) { return {}; }

        const qint64 totalFrames = dataSize / blockAlign;
        if (totalFrames <= 0) { return {}; }

        file.seek(dataOffset);
        QVector<qreal> peaks(bucketCount, 0.0);
        QByteArray buffer;
        qint64 frameIndex = 0;
        while (frameIndex < totalFrames)
        {
            const qint64 want = qMin<qint64>(1 << 20, (totalFrames - frameIndex) * blockAlign);
            buffer = file.read(want);
            if (buffer.isEmpty()) { break; }

            int offset = 0;
            while (offset + blockAlign <= buffer.size() && frameIndex < totalFrames)
            {
                const uchar* base = reinterpret_cast<const uchar*>(buffer.constData()) + offset;
                qreal framePeak = 0;
                for (int s = 0; s < samplesPerFrame; ++s)
                {
                    framePeak = qMax(framePeak, qAbs(decodeSample(formatTag, bits, base + s * bytesPerSample)));
                }
                const int bucket = static_cast<int>(frameIndex * bucketCount / totalFrames);
                peaks[bucket] = qMax(peaks[bucket], framePeak);
                ++frameIndex;
                offset += blockAlign;
            }
        }
        return peaks;
    }
}

SoundClip::SoundClip()
{
    // Sound clips always cover exactly their own length
    setLengthExplicit(true);
}

SoundClip::SoundClip(const SoundClip& s2) : KeyFrame(s2)
{
    mOriginalSoundClipName = s2.mOriginalSoundClipName;
}

SoundClip::~SoundClip()
{
    //QFile::remove( fileName() );
}

SoundClip& SoundClip::operator=(const SoundClip& a)
{
    if (this == &a)
    {
        return *this; // a self-assignment
    }

    KeyFrame::operator=(a);
    mOriginalSoundClipName = a.mOriginalSoundClipName;
    return *this;
}

SoundClip* SoundClip::clone() const
{
    // Question: need to copy the file?
    // The audio files are not allowed to be edited in Pencil2D, it should be file for now.
    return new SoundClip(*this);
}

Status SoundClip::init(const QString& strSoundFile)
{
    if (strSoundFile.isEmpty())
    {
        return Status::FAIL;
    }
    setFileName(strSoundFile);
    return Status::OK;
}

bool SoundClip::isValid() const
{
    if (fileName().isEmpty())
    {
        return false;
    }

    if (!mPlayer)
    {
        return false;
    }

    return true;
}

void SoundClip::attachPlayer(SoundPlayer* player)
{
    Q_ASSERT( player != nullptr );
    mPlayer.reset(player);
}

void SoundClip::detachPlayer()
{
    mPlayer.reset();
}

void SoundClip::play()
{
    if (mPlayer)
    {
        mPlayer->play();
    }
}

void SoundClip::playFromPosition(int frameNumber, int fps)
{
    int framesIntoSound = frameNumber;
    if (pos() > 1)
    {
        framesIntoSound = frameNumber - pos();
    }
    qreal msPerFrame = 1000.0 / fps;
    qint64 msIntoSound = qRound(framesIntoSound * msPerFrame);
    if (mPlayer)
    {
        mPlayer->setMediaPlayerPosition(msIntoSound);
        mPlayer->play();
    }
}

void SoundClip::pause()
{
    if (mPlayer)
    {
        mPlayer->pause();
    }
}

void SoundClip::stop()
{
    if (mPlayer)
    {
        mPlayer->stop();
    }
}

int64_t SoundClip::duration() const
{
    return mDuration;
}

void SoundClip::setDuration(const int64_t& duration)
{
    mDuration = duration;
}

void SoundClip::updateLength(int fps)
{
    setLength(qCeil(mDuration * fps / 1000.0));
}

const QVector<qreal>& SoundClip::waveformPeaks()
{
    if (!mWaveformParsed)
    {
        mWaveformPeaks = wavPeakEnvelope(fileName(), 2000);
        mWaveformParsed = true;
    }
    return mWaveformPeaks;
}
