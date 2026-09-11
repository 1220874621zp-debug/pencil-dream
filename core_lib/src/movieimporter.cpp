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
#include "movieimporter.h"

#include <QDebug>
#include <QTemporaryDir>
#include <QProcess>
#include <QRegularExpression>
#include <QtMath>
#include <QTime>
#include <QFileInfo>

#include "movieexporter.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "soundmanager.h"

#include "soundclip.h"
#include "bitmapimage.h"

#include "util.h"
#include "editor.h"

MovieImporter::MovieImporter(QObject* parent) : QObject(parent)
{
}

MovieImporter::~MovieImporter()
{
}

Status MovieImporter::run(const QString &filePath, int fps, FileType type,
                          std::function<void(int)> progress,
                          std::function<void(QString)> progressMessage,
                          std::function<bool()> askPermission)
{
    if (mCanceled) return Status::CANCELED;

    Status status = Status::OK;
    DebugDetails dd;

    STATUS_CHECK(verifyFFmpegExists())

    mTempDir = new QTemporaryDir();
    if (!mTempDir->isValid())
    {
        status = Status::FAIL;
        status.setTitle(tr("Error creating folder"));
        status.setDescription(tr("Unable to create a temporary folder, cannot import video."));
        dd << QString("Path: ").append(mTempDir->path())
           << QString("Error: ").append(mTempDir->errorString());
        status.setDetails(dd);
        return status;
    }
    mEditor->addTemporaryDir(mTempDir);

    if (type != FileType::SOUND)
    {
        Status st = Status::FAIL;
        st.setTitle(tr("Unknown error"));
        st.setDescription(tr("不支持的导入类型:此路径只处理音频导入。"));
        return st;
    }


    return importMovieAudio(filePath, [&progress, this](int prog) -> bool
    {
        progress(prog); return !mCanceled;
    });
}
Status MovieImporter::importMovieAudio(const QString& filePath, std::function<bool(int)> progress)
{
    Layer* layer = mEditor->layers()->currentLayer();

    Status status = Status::OK;
    if (layer->type() != Layer::SOUND)
    {
        status = Status::FAIL;
        status.setTitle(tr("Sound only"));
        status.setDescription(tr("You need to be on a sound layer to import the audio"));
        return status;
    }

    int currentFrame = mEditor->currentFrame();

    if (layer->keyExists(currentFrame))
    {
        SoundClip* key = static_cast<SoundClip*>(layer->getKeyFrameAt(currentFrame));
        if (!key->fileName().isEmpty())
        {
            status = Status::FAIL;
            status.setTitle(tr("Move to an empty frame"));
            status.setDescription(tr("A frame already exists on frame: %1 Move the scrubber to a empty position on the timeline and try again").arg(currentFrame));
            return status;
        }
        layer->removeKeyFrame(currentFrame);
    }

    QString audioPath = QDir(mTempDir->path()).filePath("audio.wav");

    QStringList args{ "-i", filePath, "-map_metadata", "-1", "-flags", "bitexact", "-fflags", "bitexact", audioPath };

    status = MovieExporter::executeFFmpeg(ffmpegLocation(), args, [&progress, this] (int frame) {
        Q_UNUSED(frame)
        progress(50); return !mCanceled;
    });

    if(mCanceled) return Status::CANCELED;
    progress(90);

    Q_ASSERT(!layer->keyExists(currentFrame));

    SoundClip* key = new SoundClip;
    layer->addKeyFrame(currentFrame, key);

    key->setSoundClipName(QFileInfo(filePath).fileName()); // keep the original file name
    Status st = mEditor->sound()->loadSound(key, audioPath);

    if (!st.ok())
    {
        layer->removeKeyFrame(currentFrame);
        return st;
    }

    return Status::OK;
}


Status MovieImporter::verifyFFmpegExists()
{
    QString ffmpegPath = ffmpegLocation();
    if (!QFile::exists(ffmpegPath))
    {
        Status status = Status::ERROR_FFMPEG_NOT_FOUND;
        status.setTitle(tr("未找到 FFmpeg"));
        status.setDescription(tr("请在 首选项 → 文件 页设置 FFmpeg 路径（可从 https://www.gyan.dev/ffmpeg/builds/ 下载），或把 ffmpeg 放进程序目录的 plugins 文件夹后重试。"));
        return status;
    }
    return Status::OK;
}
