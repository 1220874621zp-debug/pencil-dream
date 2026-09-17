/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "actioncommands.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QStandardPaths>
#include <QFileDialog>
#include <QSet>
#include <limits>

#include "pencildef.h"
#include "editor.h"
#include "object.h"
#include "viewmanager.h"
#include "layermanager.h"
#include "undoredomanager.h"
#include "undoredocommand.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "soundmanager.h"
#include "playbackmanager.h"
#include "colormanager.h"
#include "selectionmanager.h"
#include "preferencemanager.h"
#include "util.h"
#include "app_util.h"

#include "layercamera.h"
#include "layersound.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "layercolorize.h"
#include "colorizeimage.h"
#include "colorizeupdatemanager.h"
#include "holefiller.h"
#include "colortoalpha.h"
#include "layersplitter.h"
#include "colordistance.h"
#include "colorref.h"
#include "layerlayoutcommand.h"
#include "soundclip.h"
#include "camera.h"

#include "importimageseqdialog.h"
#include "importpositiondialog.h"
#include "movieimporter.h"
#include "layervideo.h"
#include <QProcess>
#include <QRegularExpression>
#include "movieexporter.h"
#include "filedialog.h"
#include "exportmoviedialog.h"
#include "exportimagedialog.h"
#include "aboutdialog.h"
#include "doubleprogressdialog.h"
#include "checkupdatesdialog.h"
#include "errordialog.h"


ActionCommands::ActionCommands(QWidget* parent) : QObject(parent)
{
    mParent = parent;
}

ActionCommands::~ActionCommands() {}

bool ActionCommands::ensureFFmpegAvailable()
{
    if (QFile::exists(ffmpegLocation()))
    {
        return true;
    }

    QMessageBox box(mParent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("未找到 FFmpeg"));
    box.setText(tr("导出视频/GIF、导入视频或音频需要 FFmpeg 程序，但未能找到可用的 FFmpeg。"));
    box.setInformativeText(tr("请先下载 FFmpeg（https://www.gyan.dev/ffmpeg/builds/ ），然后点击“浏览”选择 ffmpeg 程序。\n"
                              "也可以稍后在 首选项 → 文件 页设置路径，或把 ffmpeg 放进程序目录的 plugins 文件夹。"));
    QPushButton* browseButton = box.addButton(tr("浏览..."), QMessageBox::AcceptRole);
    box.addButton(tr("取消"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != browseButton)
    {
        return false;
    }

#ifdef _WIN32
    const QString filter = tr("可执行程序 (*.exe);;所有文件 (*)");
#else
    const QString filter = tr("所有文件 (*)");
#endif
    const QString selected = QFileDialog::getOpenFileName(mParent, tr("选择 FFmpeg 程序"), QString(), filter);
    if (selected.isEmpty())
    {
        return false;
    }

    mEditor->preference()->set(SETTING::FFMPEG_PATH, selected);
    return true;
}

Status ActionCommands::importAnimatedImage()
{
    ImportImageSeqDialog fileDialog(mParent, ImportExportDialog::Import, FileType::ANIMATED_IMAGE);
    fileDialog.exec();
    if (fileDialog.result() != QDialog::Accepted)
    {
        return Status::CANCELED;
    }
    int frameSpacing = fileDialog.getSpace();
    QString strImgFileLower = fileDialog.getFilePath();

    ImportPositionDialog positionDialog(mEditor, mParent);
    positionDialog.exec();
    if (positionDialog.result() != QDialog::Accepted)
    {
        return Status::CANCELED;
    }

    // Show a progress dialog, as this could take a while if the gif is huge
    QProgressDialog progressDialog(tr("Importing Animated Image..."), tr("Abort"), 0, 100, mParent);
    hideQuestionMark(progressDialog);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.show();

    Status st = mEditor->importAnimatedImage(strImgFileLower, frameSpacing, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
        QApplication::processEvents();
    }, [&progressDialog]() {
        return progressDialog.wasCanceled();
    });

    progressDialog.setValue(100);
    progressDialog.close();

    if (!st.ok())
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html());
        errorDialog.exec();
        return Status::SAFE;
    }

    return Status::OK;
}

Status ActionCommands::importReferenceVideo()
{
    // 参考视频:链接式导入,进程内解码跟随时间轴预览,不拆帧、不进导出。
    // ffprobe 只在导入时探测一次时长/帧率。
    if (!ensureFFmpegAvailable())
    {
        return Status::SAFE;
    }

    QString filePath = FileDialog::getOpenFileName(mParent, FileType::MOVIE);
    if (filePath.isEmpty())
    {
        return Status::FAIL;
    }

    double duration = 0.0;
    double videoFps = 0.0;
    if (!QFileInfo::exists(ffprobeLocation()))
    {
        QMessageBox::information(mParent, tr("导入参考视频"),
            tr("未找到 ffprobe，将用 ffmpeg 解析视频时长。\n"
               "如需更快更准的探测，可把 ffprobe.exe 放到 ffmpeg 同目录（plugins 文件夹或首选项所设路径）。"));
    }
    QProcess probe(this);
    // 注意 -show_entries 只认最后一次(选项覆盖),多 section 用冒号合并
    probe.start(ffprobeLocation(), { "-v", "error", "-select_streams", "v:0",
                                     "-show_entries", "stream=r_frame_rate,duration:format=duration",
                                     "-of", "default=noprint_wrappers=1", filePath });
    if (probe.waitForStarted(3000) && probe.waitForFinished(10000))
    {
        const QString out = probe.readAllStandardOutput();
        QRegularExpression durRx("duration=([0-9.]+)");
        auto it = durRx.globalMatch(out);
        while (it.hasNext())
        {
            const double v = it.next().captured(1).toDouble();
            if (v > 0.0 && (duration <= 0.0 || v < duration)) { duration = v; }
        }
        QRegularExpression fpsRx("r_frame_rate=(\\d+)/(\\d+)");
        const auto fpsMatch = fpsRx.match(out);
        if (fpsMatch.hasMatch() && fpsMatch.captured(2).toInt() > 0)
        {
            videoFps = fpsMatch.captured(1).toDouble() / fpsMatch.captured(2).toDouble();
        }
    }
    if (duration <= 0.0)
    {
        // 兜底:环境里只有 ffmpeg 单文件(gyan.dev 默认只下 ffmpeg.exe)没有
        // ffprobe 时,从 ffmpeg -i 的 stderr 抓 Duration 行(音频导入同依赖级)
        QProcess ffmpegProc(this);
        ffmpegProc.setProcessChannelMode(QProcess::MergedChannels);
        ffmpegProc.start(ffmpegLocation(), { "-i", filePath });
        if (ffmpegProc.waitForStarted(3000) && ffmpegProc.waitForFinished(10000))
        {
            const QString out = ffmpegProc.readAll();
            QRegularExpression durRx("Duration:\\s*(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)");
            const auto m = durRx.match(out);
            if (m.hasMatch())
            {
                duration = m.captured(1).toInt() * 3600 + m.captured(2).toInt() * 60 + m.captured(3).toDouble();
            }
        }
    }
    if (duration <= 0.0)
    {
        QMessageBox::warning(mParent, tr("导入参考视频"),
                             tr("无法解析视频时长:请确认文件完好,且 ffmpeg/ffprobe 可正常执行(看首选项→文件的 ffmpeg 路径)。"));
        return Status::FAIL;
    }
    if (videoFps <= 0.0) { videoFps = mEditor->playback()->fps(); }

    const int frames = qMax(1, qRound(duration * videoFps));
    const QFileInfo info(filePath);
    LayerVideo* layer = mEditor->layers()->createVideoLayer(info.completeBaseName());
    // 入点固定帧1(不以时间指针为入点)
    layer->setVideoSource(info.absoluteFilePath(), videoFps, frames, 1);

    mEditor->layers()->notifyAnimationLengthChanged();
    mEditor->getScribbleArea()->update();
    return Status::OK;
}

Status ActionCommands::importSound(FileType type)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        Q_ASSERT(layer);
        return Status::FAIL;
    }

    if (layer->type() != Layer::SOUND)
    {
        // 自动创建声音层作为导入目标（不再弹问询/命名框）
        mEditor->layers()->createSoundLayer(mEditor->layers()->nameSuggestLayer(tr("Sound Layer", "Default name on creating a sound layer")));
    }

    layer = mEditor->layers()->currentLayer();
    Q_ASSERT(layer->type() == Layer::SOUND);

    // Adding key before getting file name just to make sure the keyframe can be insterted
    SoundClip* key = static_cast<SoundClip*>(mEditor->addNewKey());

    if (key == nullptr)
    {
        // Probably tried to modify a hidden layer or something like that
        // Let Editor handle the warnings
        return Status::SAFE;
    }

    QString strSoundFile = FileDialog::getOpenFileName(mParent, type);

    Status st = Status::FAIL;

    if (strSoundFile.isEmpty())
    {
        st = Status::CANCELED;
    }
    else
    {
        // Convert even if it already is a WAV file to strip metadata that the
        // DirectShow media player backend on Windows can't handle
        st = convertSoundToWav(strSoundFile);
    }

    if (!st.ok())
    {
        mEditor->removeKey();
        emit mEditor->layers()->currentLayerChanged(mEditor->layers()->currentLayerIndex()); // trigger timeline repaint.
    } else {
        showSoundClipWarningIfNeeded();
    }

    return st;
}

Status ActionCommands::convertSoundToWav(const QString& filePath)
{
    if (!ensureFFmpegAvailable())
    {
        return Status::ERROR_FFMPEG_NOT_FOUND;
    }

    QProgressDialog progressDialog(tr("Importing sound..."), tr("Abort"), 0, 100, mParent);
    hideQuestionMark(progressDialog);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.show();

    MovieImporter importer(this);
    importer.setCore(mEditor);

    Status st = importer.run(filePath, mEditor->playback()->fps(), FileType::SOUND, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
        QApplication::processEvents();
    }, [](QString progressMessage) {
        Q_UNUSED(progressMessage)
        // Not needed
    }, []() {
        return true;
    });

    connect(&progressDialog, &QProgressDialog::canceled, &importer, &MovieImporter::cancel);

    if (!st.ok() && st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
    }
    return st;
}

Status ActionCommands::exportGif()
{
    // exporting gif
    return exportMovie(true);
}

Status ActionCommands::exportMovie(bool isGif)
{
    if (!ensureFFmpegAvailable())
    {
        return Status::SAFE;
    }

    FileType fileType = (isGif) ? FileType::GIF : FileType::MOVIE;

    int clipCount = mEditor->sound()->soundClipCount();
    if (fileType == FileType::MOVIE && clipCount >= MovieExporter::MAX_SOUND_FRAMES)
    {
        ErrorDialog errorDialog(tr("Something went wrong"), tr("You currently have a total of %1 sound clips. Due to current limitations, you will be unable to export any animation exceeding %2 sound clips. We recommend splitting up larger projects into multiple smaller project to stay within this limit.").arg(clipCount).arg(MovieExporter::MAX_SOUND_FRAMES), QString(), mParent);
        errorDialog.exec();
        return Status::FAIL;
    }

    ExportMovieDialog* dialog = new ExportMovieDialog(mParent, ImportExportDialog::Export, fileType);
    OnScopeExit(dialog->deleteLater());

    dialog->init();

    std::vector< std::pair<QString, QSize> > camerasInfo;
    auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
    for (LayerCamera* i : cameraLayers)
    {
        camerasInfo.push_back(std::make_pair(i->name(), i->getViewSize()));
    }

    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer->type() == Layer::CAMERA)
    {
        QString strName = currLayer->name();
        auto it = std::find_if(camerasInfo.begin(), camerasInfo.end(),
            [strName](std::pair<QString, QSize> p)
        {
            return p.first == strName;
        });

        Q_ASSERT(it != camerasInfo.end());

        std::swap(camerasInfo[0], *it);
    }

    dialog->setCamerasInfo(camerasInfo);

    int lengthWithSounds = mEditor->layers()->animationLength(true);
    int length = mEditor->layers()->animationLength(false);

    dialog->setDefaultRange(1, length, lengthWithSounds);
    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }
    QString strMoviePath = dialog->getFilePath();

    ExportMovieDesc desc;
    desc.strFileName = strMoviePath;
    desc.startFrame = dialog->getStartFrame();
    desc.endFrame = dialog->getEndFrame();
    desc.fps = mEditor->playback()->fps();
    desc.exportSize = dialog->getExportSize();
    desc.strCameraName = dialog->getSelectedCameraName();
    desc.loop = dialog->getLoop();
    desc.alpha = dialog->getTransparency();

    DoubleProgressDialog progressDlg(mParent);
    progressDlg.setWindowModality(Qt::WindowModal);
    progressDlg.setWindowTitle(tr("Exporting movie"));
    Qt::WindowFlags eFlags = Qt::Dialog | Qt::WindowTitleHint;
    progressDlg.setWindowFlags(eFlags);
    progressDlg.show();

    MovieExporter ex;

    connect(&progressDlg, &DoubleProgressDialog::canceled, [&ex]
    {
        ex.cancel();
    });

    // The start points and length for the current minor operation segment on the major progress bar
    float minorStart, minorLength;

    Status st = ex.run(mEditor->object(), desc,
        [&progressDlg, &minorStart, &minorLength](float f, float final)
        {
            progressDlg.major->setValue(f);

            minorStart = f;
            minorLength = qMax(0.f, final - minorStart);

            QApplication::processEvents();
        },
        [&progressDlg, &minorStart, &minorLength](float f) {
            progressDlg.minor->setValue(f);

            progressDlg.major->setValue(minorStart + f * minorLength);

            QApplication::processEvents();
        },
        [&progressDlg](QString s) {
            progressDlg.setStatus(s);
            QApplication::processEvents();
        }
    );

    if (st.ok())
    {
        if (QFile::exists(strMoviePath))
        {
            if (isGif) {
                auto btn = QMessageBox::question(mParent, "Pencil2D",
                                                 tr("Finished. Open file location?"));

                if (btn == QMessageBox::Yes)
                {
                    QString path = dialog->getAbsolutePath();
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                }
                return Status::OK;
            }
            auto btn = QMessageBox::question(mParent, "Pencil2D",
                                             tr("Finished. Open movie now?", "When movie export done."));
            if (btn == QMessageBox::Yes)
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(strMoviePath));
            }
        }
        else
        {
            ErrorDialog errorDialog(tr("Unknown export error"), tr("The export did not produce any errors, however we can't find the output file. Your export may not have completed successfully."), QString(), mParent);
            errorDialog.exec();
        }
    }
    else if(st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
    }

    return st;
}

Status ActionCommands::exportImageSequence()
{
    auto dialog = new ExportImageDialog(mParent, FileType::IMAGE_SEQUENCE);
    OnScopeExit(dialog->deleteLater());

    dialog->init();

    std::vector< std::pair<QString, QSize> > camerasInfo;
    auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
    for (LayerCamera* i : cameraLayers)
    {
        camerasInfo.push_back(std::make_pair(i->name(), i->getViewSize()));
    }

    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer->type() == Layer::CAMERA)
    {
        QString strName = currLayer->name();
        auto it = std::find_if(camerasInfo.begin(), camerasInfo.end(),
            [strName](std::pair<QString, QSize> p)
        {
            return p.first == strName;
        });

        Q_ASSERT(it != camerasInfo.end());
        std::swap(camerasInfo[0], *it);
    }
    dialog->setCamerasInfo(camerasInfo);

    int lengthWithSounds = mEditor->layers()->animationLength(true);
    int length = mEditor->layers()->animationLength(false);

    dialog->setDefaultRange(1, length, lengthWithSounds);

    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }

    QString strFilePath = dialog->getFilePath();
    QSize exportSize = dialog->getExportSize();
    QString exportFormat = dialog->getExportFormat();
    bool exportKeyframesOnly = dialog->getExportKeyframesOnly();
    bool useTransparency = dialog->getTransparency();
    int startFrame = dialog->getStartFrame();
    int endFrame  = dialog->getEndFrame();

    QString sCameraLayerName = dialog->getCameraLayerName();
    LayerCamera* cameraLayer = static_cast<LayerCamera*>(mEditor->layers()->findLayerByName(sCameraLayerName, Layer::CAMERA));

    // Show a progress dialog, as this can take a while if you have lots of frames.
    QProgressDialog progress(tr("Exporting image sequence..."), tr("Abort"), 0, 100, mParent);
    hideQuestionMark(progress);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    Status st = mEditor->object()->exportFrames(startFrame, endFrame,
                                    cameraLayer,
                                    exportSize,
                                    strFilePath,
                                    exportFormat,
                                    useTransparency,
                                    exportKeyframesOnly,
                                    mEditor->layers()->currentLayer()->name(),
                                    true,
                                    &progress,
                                    100);

    if (!st.ok())
    {
        ErrorDialog errorDialog(tr("Something went wrong"), tr("Unable to export one or more images in the image sequence."), st.details().html(), mParent);
        errorDialog.exec();
        return st;
    }

    progress.close();

    return Status::OK;
}

Status ActionCommands::exportImage()
{
    // Options
    auto dialog = new ExportImageDialog(mParent, FileType::IMAGE);
    OnScopeExit(dialog->deleteLater())

    dialog->init();

    std::vector< std::pair<QString, QSize> > camerasInfo;
    auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
    for (LayerCamera* i : cameraLayers)
    {
        camerasInfo.push_back(std::make_pair(i->name(), i->getViewSize()));
    }

    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer->type() == Layer::CAMERA)
    {
        QString strName = currLayer->name();
        auto it = std::find_if(camerasInfo.begin(), camerasInfo.end(),
            [strName](std::pair<QString, QSize> p)
        {
            return p.first == strName;
        });

        Q_ASSERT(it != camerasInfo.end());
        std::swap(camerasInfo[0], *it);
    }
    dialog->setCamerasInfo(camerasInfo);

    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }

    QString filePath = dialog->getFilePath();
    QSize exportSize = dialog->getExportSize();
    QString exportFormat = dialog->getExportFormat();
    bool useTransparency = dialog->getTransparency();

    QString extension = "";
    QString formatStr = exportFormat;
    if (formatStr == "PNG" || formatStr == "png")
    {
        exportFormat = "PNG";
        extension = ".png";
    }
    if (formatStr == "JPG" || formatStr == "jpg" || formatStr == "JPEG" || formatStr == "jpeg")
    {
        exportFormat = "JPG";
        extension = ".jpg";
        useTransparency = false; // JPG doesn't support transparency, so we have to include the background
    }
    if (formatStr == "TIFF" || formatStr == "tiff" || formatStr == "TIF" || formatStr == "tif")
    {
        exportFormat = "TIFF";
        extension = ".tiff";
    }
    if (formatStr == "BMP" || formatStr == "bmp")
    {
        exportFormat = "BMP";
        extension = ".bmp";
        useTransparency = false;
    }
    if (formatStr == "WEBP" || formatStr == "webp") {
        exportFormat = "WEBP";
        extension = ".webp";
    }
    if (!filePath.endsWith(extension, Qt::CaseInsensitive))
    {
        filePath += extension;
    }

    // Export
    QString sCameraLayerName = dialog->getCameraLayerName();
    LayerCamera* cameraLayer = static_cast<LayerCamera*>(mEditor->layers()->findLayerByName(sCameraLayerName, Layer::CAMERA));

    QTransform view = cameraLayer->getViewAtFrame(mEditor->currentFrame());

    Status st = mEditor->object()->exportIm(mEditor->currentFrame(),
                                           view,
                                           cameraLayer->getViewSize(),
                                           exportSize,
                                           filePath,
                                           exportFormat,
                                           true,
                                           useTransparency);

    if (!st.ok())
    {
        ErrorDialog errorDialog(tr("Something went wrong"), tr("Unable to export image."), st.details().html(), mParent);
        errorDialog.exec();
        return st;
    }
    return Status::OK;
}

void ActionCommands::flipSelectionX()
{
    bool flipVertical = false;
    mEditor->flipSelection(flipVertical);
}

void ActionCommands::flipSelectionY()
{
    bool flipVertical = true;
    mEditor->flipSelection(flipVertical);
}

void ActionCommands::selectAll()
{
    mEditor->selectAll();
}

void ActionCommands::deselectAll()
{
    mEditor->deselectAll();
}

void ActionCommands::ZoomIn()
{
    mEditor->view()->scaleUp();
}

void ActionCommands::ZoomOut()
{
    mEditor->view()->scaleDown();
}

void ActionCommands::rotateClockwise()
{
    // Rotation direction is inverted if view is flipped either vertically or horizontally
    const float delta = mEditor->view()->isFlipHorizontal() == !mEditor->view()->isFlipVertical() ? -15.f : 15.f;
    mEditor->view()->rotateRelative(delta);
}

void ActionCommands::rotateCounterClockwise()
{
    // Rotation direction is inverted if view is flipped either vertically or horizontally
    const float delta = mEditor->view()->isFlipHorizontal() == !mEditor->view()->isFlipVertical() ? 15.f : -15.f;
    mEditor->view()->rotateRelative(delta);
}

void ActionCommands::PlayStop()
{
    PlaybackManager* playback = mEditor->playback();
    if (playback->isPlaying())
    {
        playback->stop();
    }
    else
    {
        playback->play();
    }
}

void ActionCommands::GotoNextFrame()
{
    mEditor->scrubForward();
}

void ActionCommands::GotoPrevFrame()
{
    mEditor->scrubBackward();
}

void ActionCommands::GotoNextKeyFrame()
{
    mEditor->scrubNextKeyFrame();
}

void ActionCommands::GotoPrevKeyFrame()
{
    mEditor->scrubPreviousKeyFrame();
}

Status ActionCommands::addNewKey()
{
    // Sound keyframes should not be empty, so we try to import a sound instead
    if (mEditor->layers()->currentLayer()->type() == Layer::SOUND)
    {
        return importSound(FileType::SOUND);
    }

    KeyFrame* key = mEditor->addNewKey();
    Camera* cam = dynamic_cast<Camera*>(key);
    if (cam)
    {
        mEditor->view()->forceUpdateViewTransform();
    }

    return Status::OK;
}

void ActionCommands::exposeSelectedFrames(int offset)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer->locked()) { return; }

    bool hasSelectedFrames = currentLayer->hasAnySelectedFrames();

    // Functionality to be able to expose the current frame without selecting
    // A:
    KeyFrame* key = currentLayer->getLastKeyFrameAtPosition(mEditor->currentFrame());
    if (!hasSelectedFrames) {

        if (key == nullptr) { return; }
        currentLayer->setFrameSelected(key->pos(), true);
    }

    // One transaction = one undo step for the whole exposure change
    const QList<int> oldPositions = currentLayer->getSelectedFramesByPos();
    mEditor->beginLayerLayoutEdit(currentLayer);
    currentLayer->setExposureForSelectedFrames(offset);
    // TVP gap absorption for the spots the frames moved away from
    currentLayer->absorbGapsAt(oldPositions);
    mEditor->endLayerLayoutEdit(offset > 0 ? tr("增加曝光") : tr("减少曝光"));

    emit mEditor->updateTimeLine();
    emit mEditor->framesModified();

    // Remember to deselect frame again so we don't show it being visually selected.
    // B:
    if (!hasSelectedFrames) {
        currentLayer->setFrameSelected(key->pos(), false);
    }
}

void ActionCommands::addExposureToSelectedFrames()
{
    exposeSelectedFrames(1);
}

void ActionCommands::subtractExposureFromSelectedFrames()
{
    exposeSelectedFrames(-1);
}

Status ActionCommands::insertKeyFrameAtCurrentPosition()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer == nullptr) { return Status::SAFE; }
    if (currentLayer->locked())
    {
        mEditor->getScribbleArea()->showLayerLockedWarning();
        return Status::SAFE;
    }
    if (!currentLayer->visible())
    {
        mEditor->getScribbleArea()->showLayerNotVisibleWarning();
        return Status::SAFE;
    }

    const int currentPosition = mEditor->currentFrame();

    // One transaction = one undo step covering the exposure shift AND the new frame
    mEditor->beginLayerLayoutEdit(currentLayer);
    currentLayer->insertExposureAt(currentPosition);
    const bool added = currentLayer->addNewKeyFrameAt(currentPosition);
    mEditor->endLayerLayoutEdit(tr("Insert frame"));

    if (added)
    {
        mEditor->scrubTo(currentPosition);
        emit mEditor->frameModified(currentPosition);
        mEditor->layers()->notifyAnimationLengthChanged();
        if (currentLayer->type() == Layer::CAMERA)
        {
            mEditor->view()->forceUpdateViewTransform();
        }
    }
    return Status::OK;
}

void ActionCommands::removeSelectedFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();

    if (!currentLayer->hasAnySelectedFrames() || currentLayer->locked()) { return; }

    const QList<int> positions = currentLayer->selectedKeyFramesPositions();

    // Non-sound layers always keep their last keyframe
    const bool keepLastFrame = (currentLayer->type() != Layer::SOUND
                                && positions.count() >= currentLayer->keyFrameCount());

    // One transaction = one undo step that restores every deleted frame
    mEditor->beginLayerLayoutEdit(currentLayer);
    int removed = 0;
    for (int pos : positions) {
        if (keepLastFrame && removed >= positions.count() - 1) { break; }
        if (mEditor->takeLayerKeyFrame(currentLayer, pos) != nullptr) { removed++; }
    }
    // TVP gap absorption: the block before each vacated spot takes over
    currentLayer->absorbGapsAt(positions);
    currentLayer->deselectAll();
    mEditor->endLayerLayoutEdit(tr("删除选中帧"));

    mEditor->layers()->notifyLayerChanged(currentLayer);
    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();
}

void ActionCommands::reverseSelectedFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer->locked()) { return; }

    mEditor->beginLayerLayoutEdit(currentLayer);
    const bool reversed = currentLayer->reverseOrderOfSelection();
    mEditor->endLayerLayoutEdit(tr("Reverse frames"));

    if (!reversed) {
        return;
    }

    if (currentLayer->type() == Layer::CAMERA) {
        mEditor->view()->forceUpdateViewTransform();
    }
    emit mEditor->framesModified();
};

void ActionCommands::removeKey()
{
    mEditor->removeKey();
}

void ActionCommands::duplicateLayer()
{
    LayerManager* layerMgr = mEditor->layers();
    Layer* fromLayer = layerMgr->currentLayer();
    if (fromLayer->type() == Layer::MOVIE) { return; } // 参考视频层不参与复制
    int currFrame = mEditor->currentFrame();

    Layer* toLayer = layerMgr->createLayer(fromLayer->type(), tr("%1 (copy)", "Default duplicate layer name").arg(fromLayer->name()));
    fromLayer->foreachKeyFrame([&] (KeyFrame* key) {
        key = key->clone();
        toLayer->addOrReplaceKeyFrame(key->pos(), key);
        if (toLayer->type() == Layer::SOUND)
        {
            mEditor->sound()->processSound(static_cast<SoundClip*>(key));
        }
    });
    if (!fromLayer->keyExists(1)) {
        toLayer->removeKeyFrame(1);
    }
    mEditor->scrubTo(currFrame);
}

void ActionCommands::duplicateKey()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->locked()) return;
    if (layer->type() == Layer::MOVIE) return; // 参考视频层无帧复制
    if (!layer->visible())
    {
        mEditor->getScribbleArea()->showLayerNotVisibleWarning();
        return;
    }

    KeyFrame* key = layer->getKeyFrameAt(mEditor->currentFrame());
    if (key == nullptr) return;

    // Duplicating a selected keyframe is not handled properly.
    // The desired behavior is to clear selection anyway so we just do that.
    deselectAll();

    KeyFrame* dupKey = key->clone();

    int nextEmptyFrame = mEditor->currentFrame() + 1;
    // 空位看"该格有无 key 起点"而非覆盖：auto 块覆盖查询是开放延伸（末块
    // cover=INT_MAX），当覆盖当占用会无限找下去（复制帧必卡死）；在 auto
    // 覆盖区插入 key 是合法操作，块会被下一 key 自然截断。
    while (layer->keyExists(nextEmptyFrame))
    {
        nextEmptyFrame += 1;
    }

    mEditor->beginLayerLayoutEdit(layer);
    layer->addKeyFrame(nextEmptyFrame, dupKey);
    mEditor->endLayerLayoutEdit(tr("Duplicate frame"));
    mEditor->scrubTo(nextEmptyFrame);
    emit mEditor->frameModified(nextEmptyFrame);

    if (layer->type() == Layer::SOUND)
    {
        mEditor->sound()->processSound(dynamic_cast<SoundClip*>(dupKey));
        showSoundClipWarningIfNeeded();
    }

    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->layers()->currentLayerChanged(mEditor->layers()->currentLayerIndex()); // trigger timeline repaint.
}

void ActionCommands::moveFrameForward()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer && !layer->locked())
    {
        mEditor->beginLayerLayoutEdit(layer);
        const bool moved = layer->moveKeyFrame(mEditor->currentFrame(), 1);
        mEditor->endLayerLayoutEdit(tr("Move frame forward"));
        if (moved)
        {
            mEditor->scrubForward();
        }
    }
    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();
}

void ActionCommands::moveFrameBackward()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer && !layer->locked())
    {
        mEditor->beginLayerLayoutEdit(layer);
        const bool moved = layer->moveKeyFrame(mEditor->currentFrame(), -1);
        mEditor->endLayerLayoutEdit(tr("Move frame backward"));
        if (moved)
        {
            mEditor->scrubBackward();
        }
    }
    emit mEditor->framesModified();
}

Status ActionCommands::addNewBitmapLayer()
{
    bool ok;
    QString text = QInputDialog::getText(nullptr, tr("Layer Properties"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         mEditor->layers()->nameSuggestLayer(tr("Bitmap Layer")), &ok);
    if (ok && !text.isEmpty())
    {
        mEditor->layers()->createBitmapLayer(text);
    }
    return Status::OK;
}

Status ActionCommands::addNewColorizeLayer()
{
    // 静默建层（用户拍板）：直接用建议名，不弹命名对话框打断流程
    mEditor->layers()->createColorizeLayer(mEditor->layers()->nameSuggestLayer(tr("Colorize Layer")));
    return Status::OK;
}

Status ActionCommands::addNewCameraLayer()
{
    bool ok;
    QString text = QInputDialog::getText(nullptr, tr("Layer Properties", "A popup when creating a new layer"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         mEditor->layers()->nameSuggestLayer(tr("Camera Layer")), &ok);
    if (ok && !text.isEmpty())
    {
        mEditor->layers()->createCameraLayer(text);
    }
    return Status::OK;
}

Status ActionCommands::addNewSoundLayer()
{
    // 自动用建议名建层（不弹命名框），随即直接进入音频导入
    Layer* layer = mEditor->layers()->createSoundLayer(mEditor->layers()->nameSuggestLayer(tr("Sound Layer")));
    if (layer != nullptr)
    {
        mEditor->layers()->setCurrentLayer(layer);
    }
    return importSound(FileType::SOUND);
}

Status ActionCommands::deleteCurrentLayer()
{
    LayerManager* layerMgr = mEditor->layers();
    QString strLayerName = layerMgr->currentLayer()->name();

    if (!layerMgr->canDeleteLayer(mEditor->currentLayerIndex())) {
        return Status::CANCELED;
    }

    int ret = QMessageBox::warning(mParent,
                                   tr("Delete Layer", "Windows title of Delete current layer pop-up."),
                                   tr("Are you sure you want to delete layer: %1? This cannot be undone.").arg(strLayerName),
                                   QMessageBox::Ok | QMessageBox::Cancel,
                                   QMessageBox::Ok);
    if (ret == QMessageBox::Ok)
    {
        Status st = layerMgr->deleteLayer(mEditor->currentLayerIndex());
        if (st == Status::ERROR_NEED_AT_LEAST_ONE_CAMERA_LAYER)
        {
            QMessageBox::information(mParent, "",
                                     tr("Please keep at least one camera layer in project", "text when failed to delete camera layer"));
        }
    }
    return Status::OK;
}

Status ActionCommands::mergeLayerDown()
{
    LayerManager* layerMgr = mEditor->layers();
    Layer* upper = layerMgr->currentLayer();
    if (upper == nullptr)
    {
        return Status::FAIL;
    }

    const QString tipTitle = tr("向下合并图层");
    if (upper->type() != Layer::BITMAP)
    {
        QMessageBox::information(mParent, tipTitle, tr("只有位图图层可以向下合并。"));
        return Status::CANCELED;
    }
    const int upperIndex = layerMgr->currentLayerIndex();
    if (upperIndex <= 0)
    {
        QMessageBox::information(mParent, tipTitle, tr("当前图层下方没有可合并的图层。"));
        return Status::CANCELED;
    }
    Layer* lower = layerMgr->getLayer(upperIndex - 1);
    if (lower == nullptr || lower->type() != Layer::BITMAP)
    {
        QMessageBox::information(mParent, tipTitle, tr("下方图层不是位图图层，无法合并。"));
        return Status::CANCELED;
    }
    if (lower->locked())
    {
        QMessageBox::information(mParent, tipTitle, tr("下方图层已锁定，无法合并。"));
        return Status::CANCELED;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        mParent, tipTitle,
        tr("将把“%1”并入下方“%2”，并删除“%1”。\n此操作不可撤销，是否继续？").arg(upper->name(), lower->name()),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Ok)
    {
        return Status::CANCELED;
    }

    // 合并会改动下层像素并删除上层，撤销栈里的旧状态会引用已删层的帧 → 清栈保安全
    mEditor->undoRedo()->clearStack();

    const Status st = layerMgr->mergeBitmapLayerDown(upperIndex);
    if (!st.ok())
    {
        QMessageBox::information(mParent, tipTitle, tr("合并失败。"));
        return st;
    }

    mEditor->getScribbleArea()->onLayerChanged();
    emit mEditor->framesModified();
    return Status::OK;
}

Status ActionCommands::fillHolesOnCurrentFrame()
{
    const QString tipTitle = tr("镂空检测");

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        return Status::FAIL;
    }
    if (!layer->isBitmapKind())
    {
        QMessageBox::information(mParent, tipTitle, tr("镂空检测只能在位图族图层（位图/填色）上使用。"));
        return Status::CANCELED;
    }
    if (layer->locked())
    {
        QMessageBox::information(mParent, tipTitle, tr("图层“%1”已锁定，无法填充。").arg(layer->name()));
        return Status::CANCELED;
    }

    // 与画布落笔同源：循环层编辑的是显示帧背后的关键帧（所见即所编辑）
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* bitmap = static_cast<BitmapImage*>(
        bitmapLayer->getKeyFrameWhichCovers(bitmapLayer->displayFrameFor(mEditor->currentFrame())));
    if (bitmap == nullptr)
    {
        QMessageBox::information(mParent, tipTitle, tr("当前帧没有可处理的位图内容。"));
        return Status::CANCELED;
    }

    QImage* img = bitmap->image();
    Q_CHECK_PTR(img);

    const SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    const int filled = HoleFiller::fillHoles(*img);
    if (filled == 0)
    {
        // 无变化不进撤销栈
        QMessageBox::information(mParent, tipTitle, tr("未检测到封闭镂空。"));
        return Status::OK;
    }

    mEditor->setModified(mEditor->currentLayerIndex(), mEditor->currentFrame());
    mEditor->undoRedo()->record(saveStateId, tr("镂空检测填充", "Undo step text"));
    return Status::OK;
}

Status ActionCommands::applyColorToAlpha(const ColorToAlphaParams& params, bool allKeyFrames)
{
    const QString tipTitle = tr("颜色转为透明度");

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        return Status::FAIL;
    }
    if (!layer->isBitmapKind())
    {
        QMessageBox::information(mParent, tipTitle, tr("颜色转为透明度只能在位图族图层（位图/填色）上使用。"));
        return Status::CANCELED;
    }
    if (layer->locked())
    {
        QMessageBox::information(mParent, tipTitle, tr("图层“%1”已锁定，无法处理。").arg(layer->name()));
        return Status::CANCELED;
    }

    auto bitmapLayer = static_cast<LayerBitmap*>(layer);

    if (!allKeyFrames)
    {
        // 与画布落笔同源：循环层编辑的是显示帧背后的关键帧（所见即所编辑）
        BitmapImage* bitmap = static_cast<BitmapImage*>(
            bitmapLayer->getKeyFrameWhichCovers(bitmapLayer->displayFrameFor(mEditor->currentFrame())));
        if (bitmap == nullptr)
        {
            QMessageBox::information(mParent, tipTitle, tr("当前帧没有可处理的位图内容。"));
            return Status::CANCELED;
        }

        const SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
        QImage* img = bitmap->image();
        Q_CHECK_PTR(img);
        const int changed = ColorToAlpha::apply(*img, params);
        if (changed == 0)
        {
            // 无变化不进撤销栈
            QMessageBox::information(mParent, tipTitle, tr("没有符合条件的像素，图像未改变。"));
            return Status::OK;
        }
        bitmap->setModified(true);
        // 数据失效传实际关键帧 pos（循环层显示帧≠数据帧）
        mEditor->setModified(mEditor->layers()->currentLayerIndex(), bitmap->pos());
        mEditor->undoRedo()->record(saveStateId, tr("颜色转为透明度", "Undo step text"));
        return Status::OK;
    }

    // 批量：图层全部关键帧，单状态单步撤销
    QVector<BitmapImage*> bitmaps;
    bitmapLayer->foreachKeyFrame([&](KeyFrame* key) {
        bitmaps.append(static_cast<BitmapImage*>(key));
    });

    QProgressDialog progress(tr("正在处理颜色转为透明度…"), tr("取消"), 0, bitmaps.size(), mParent);
    progress.setWindowTitle(tipTitle);
    progress.setWindowModality(Qt::WindowModal);

    const SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    int changedFrames = 0;
    int done = 0;
    bool canceled = false;

    for (BitmapImage* bitmap : bitmaps)
    {
        QImage* img = bitmap->image();
        if (img != nullptr && ColorToAlpha::apply(*img, params) > 0)
        {
            bitmap->setModified(true);
            mEditor->setModified(mEditor->layers()->currentLayerIndex(), bitmap->pos());
            ++changedFrames;
        }
        ++done;
        progress.setValue(done);
        QApplication::processEvents();
        if (progress.wasCanceled())
        {
            canceled = true;
            break;
        }
    }

    if (changedFrames == 0)
    {
        QMessageBox::information(mParent, tipTitle,
                                 canceled ? tr("已取消，没有帧被处理。") : tr("没有符合条件的像素，图像未改变。"));
        return Status::OK;
    }

    mEditor->undoRedo()->record(saveStateId, tr("颜色转为透明度（全部关键帧）", "Undo step text"));
    if (canceled)
    {
        QMessageBox::information(mParent, tipTitle,
                                 tr("完成 %1/%2 帧，已取消。已处理的帧可 Ctrl+Z 撤销。").arg(done).arg(bitmaps.size()));
    }
    return Status::OK;
}

Status ActionCommands::splitLayerByColor(const LayerSplitParams& params, bool allKeyFrames)
{
    const QString tipTitle = tr("拆分图层颜色");

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        return Status::FAIL;
    }
    if (!layer->isBitmapKind())
    {
        QMessageBox::information(mParent, tipTitle, tr("拆分图层颜色只能在位图族图层（位图/填色）上使用。"));
        return Status::CANCELED;
    }
    if (layer->locked())
    {
        QMessageBox::information(mParent, tipTitle, tr("图层“%1”已锁定，无法处理。").arg(layer->name()));
        return Status::CANCELED;
    }

    auto bitmapLayer = static_cast<LayerBitmap*>(layer);

    // 收集要处理的源关键帧（pos + 位图）
    struct FrameRef { int pos = 0; BitmapImage* bitmap = nullptr; };
    QVector<FrameRef> frames;
    if (allKeyFrames)
    {
        bitmapLayer->foreachKeyFrame([&](KeyFrame* key) {
            frames.append({ key->pos(), static_cast<BitmapImage*>(key) });
        });
        if (frames.isEmpty())
        {
            QMessageBox::information(mParent, tipTitle, tr("当前图层没有关键帧。"));
            return Status::CANCELED;
        }
    }
    else
    {
        // 与画布落笔同源：循环层编辑的是显示帧背后的关键帧（所见即所编辑）
        BitmapImage* bitmap = static_cast<BitmapImage*>(
            bitmapLayer->getKeyFrameWhichCovers(bitmapLayer->displayFrameFor(mEditor->currentFrame())));
        if (bitmap == nullptr)
        {
            QMessageBox::information(mParent, tipTitle, tr("当前帧没有可处理的位图内容。"));
            return Status::CANCELED;
        }
        frames.append({ bitmap->pos(), bitmap });
    }

    Object* object = mEditor->object();
    const int sourceIndex = mEditor->layers()->currentLayerIndex();
    int nextInsertIndex = sourceIndex + 1; // 新层插源层上方（索引大者渲染在上）

    // 拆分前分组快照（撤销用；须在任何结构改动前捕获）
    const LayerOrderCommand::GroupSnapshot undoGroups = LayerOrderCommand::captureGroups(object);

    // 色板最接近色名（ΔE≤30 命中，否则空）
    const auto paletteNameFor = [this, object](const QRgb key) -> QString {
        const int count = object->getColorCount();
        if (count == 0) { return QString(); }
        const ColorDistance::LabF keyLab = ColorDistance::rgbToLab(key);
        QString best;
        double bestDist = 30.0;
        for (int i = 0; i < count; ++i)
        {
            const ColorRef ref = object->getColor(i);
            if (!ref.color.isValid()) { continue; }
            const double dist = ColorDistance::deltaE(ColorDistance::rgbToLab(ref.color.rgb()), keyLab);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = ref.name;
            }
        }
        return best;
    };

    QProgressDialog progress(tr("正在拆分图层颜色…"), tr("取消"), 0, frames.size(), mParent);
    progress.setWindowTitle(tipTitle);
    progress.setWindowModality(Qt::WindowModal);
    if (frames.size() < 2)
    {
        progress.setMinimumDuration(std::numeric_limits<int>::max()); // 单帧不弹进度框
    }

    LayerSplitter splitter(params);
    QVector<LayerBitmap*> bucketLayers; // 桶索引 → 新层（惰性建）
    QSet<QString> usedSwatchNames;      // 色板名去重（多个桶命中同一色名时后者回退色值）
    bool aborted = false;
    QString abortReason;
    int done = 0;

    for (const FrameRef& frame : frames)
    {
        QImage* img = frame.bitmap->image();
        if (img != nullptr)
        {
            if (!splitter.processFrame(*img))
            {
                aborted = true;
                abortReason = tr("颜色种类超过上限（256），已中止。建议调大“颜色模糊度”后重试。");
                break;
            }

            // 分发当前帧各桶画布到对应新层（与源帧同坐标系 topLeft）
            for (int bIdx = 0; bIdx < splitter.bucketCount(); ++bIdx)
            {
                QImage piece = splitter.takeBucketImage(bIdx);
                if (piece.isNull())
                    continue;

                while (bucketLayers.size() <= bIdx)
                {
                    auto* newLayer = new LayerBitmap(object->getUniqueLayerID());
                    const QRgb key = splitter.bucketKeyColor(bucketLayers.size());
                    const QString hex = QString::number(key & 0xFFFFFF, 16).rightJustified(6, QChar('0')).toUpper();
                    QString name = tr("拆分-#%1").arg(hex);
                    if (params.usePaletteNames)
                    {
                        const QString swatch = paletteNameFor(key);
                        if (!swatch.isEmpty() && !usedSwatchNames.contains(swatch))
                        {
                            name = tr("拆分-%1").arg(swatch);
                            usedSwatchNames.insert(swatch);
                        }
                    }
                    newLayer->setName(name);
                    object->insertLayer(nextInsertIndex++, newLayer);
                    bucketLayers.append(newLayer);
                }

                auto* newBitmap = new BitmapImage(frame.bitmap->topLeft(), piece);
                bucketLayers[bIdx]->addKeyFrame(frame.pos, newBitmap);
                newBitmap->setModified(true);
                mEditor->setModified(object->getIndex(bucketLayers[bIdx]), frame.pos);
            }
        }

        ++done;
        progress.setValue(done);
        QApplication::processEvents();
        if (progress.wasCanceled())
        {
            aborted = true;
            abortReason = tr("已取消。");
            break;
        }
    }

    if (bucketLayers.isEmpty())
    {
        QMessageBox::information(mParent, tipTitle, tr("没有可拆分的不透明像素。"));
        return Status::OK;
    }

    // 终排序：面积大者在上=最高索引。摘出后按面积升序回插（最小的先占低索引，最大的最后落在最上）
    if (params.sortLayers && !aborted)
    {
        const std::vector<int> order = splitter.sortedBucketOrder(); // 面积降序
        QList<LayerBitmap*> taken;
        taken.reserve(bucketLayers.size());
        for (LayerBitmap* created : bucketLayers)
        {
            object->takeLayer(created->id());
            taken.append(created);
        }
        bucketLayers.clear();
        int insertAt = sourceIndex + 1;
        for (auto it = order.rbegin(); it != order.rend(); ++it) // 升序回插
        {
            object->insertLayer(insertAt++, taken[*it]);
        }
        for (int bi : order) // 输出表保持面积降序（末位=最高索引）
        {
            bucketLayers.append(taken[bi]);
        }
    }

    // 新层整体入组「拆分」（连续组模型：成员恒相邻，插入序已满足）
    if (params.putInGroup && !bucketLayers.isEmpty())
    {
        const int groupId = object->createLayerGroup(tr("拆分"));
        for (LayerBitmap* created : bucketLayers)
        {
            created->setGroupId(groupId);
        }
    }

    if (params.hideOriginal)
    {
        bitmapLayer->setVisible(false);
    }

    mEditor->undoRedo()->pushUndoCommand(
        new SplitLayerCommand(mEditor, QList<Layer*>(bucketLayers.begin(), bucketLayers.end()),
                              bitmapLayer->id(), params.hideOriginal, undoGroups, tipTitle));

    // 选中最高处的新层并刷新（命令入栈的首次 redo 已被跳过，刷新由动作侧完成）
    Layer* topLayer = object->getLayer(sourceIndex + bucketLayers.size());
    if (topLayer != nullptr)
    {
        mEditor->layers()->setCurrentLayer(topLayer);
    }
    mEditor->scrubTo(mEditor->currentFrame());
    emit mEditor->updateTimeLine();
    mEditor->getScribbleArea()->onLayerChanged();

    if (aborted)
    {
        QMessageBox::information(mParent, tipTitle,
                                 tr("完成 %1/%2 帧。%3\n可 Ctrl+Z 撤销本次拆分。").arg(done).arg(frames.size()).arg(abortReason));
    }
    return Status::OK;
}

Status ActionCommands::propagateColorizeStrokes()
{
    const QString tipTitle = tr("跨帧传播填色");

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        return Status::FAIL;
    }
    if (layer->type() != Layer::COLORIZE)
    {
        QMessageBox::information(mParent, tipTitle,
            tr("跨帧传播只能在填色图层上使用：请选中填色图层，在当前帧涂好色点后再传播。"));
        return Status::CANCELED;
    }
    if (layer->locked())
    {
        QMessageBox::information(mParent, tipTitle, tr("图层“%1”已锁定，无法传播。").arg(layer->name()));
        return Status::CANCELED;
    }

    auto colorizeLayer = static_cast<LayerColorize*>(layer);

    // 线稿源层（锚点与目标帧的线稿都取自它）。当前帧不必已涂——
    // 多锚点模式下允许站在任意空帧上触发，锚点 = 全部已涂帧
    const int srcDisplay = colorizeLayer->displayFrameFor(mEditor->currentFrame());
    LayerBitmap* lineLayer = mEditor->object()->getColorizeSourceLayer(mEditor->currentLayerIndex(), srcDisplay);
    if (lineLayer == nullptr)
    {
        QMessageBox::information(mParent, tipTitle, tr("找不到线稿源图层：当前帧附近没有含画布内容的位图图层。"));
        return Status::CANCELED;
    }

    // 目标 = 线稿源层全部关键帧（全时间轴填隙：锚点前后的空帧都填，
    // 各帧从绝对距离最近的锚点取色；锚点/手涂帧由循环内保护逻辑跳过）
    QVector<int> targets;
    for (int p = 1; p <= lineLayer->getMaxKeyFramePosition(); ++p)
        if (lineLayer->keyExists(p))
            targets.append(p);
    if (targets.isEmpty())
    {
        QMessageBox::information(mParent, tipTitle, tr("线稿源图层没有帧块，无需传播。"));
        return Status::CANCELED;
    }

    // 搬运要求三图同尺寸：各帧图像按各自 topLeft 平铺到公共矩形（画布坐标系）
    auto flatten = [](BitmapImage& bmp, const QRect& canvas) {
        QImage img(canvas.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.drawImage(bmp.bounds().topLeft() - canvas.topLeft(),
                    bmp.image()->convertToFormat(QImage::Format_ARGB32_Premultiplied));
        p.end();
        return img;
    };
    auto nonEmptyBBox = [](const QImage& img) {
        int minX = img.width(), minY = img.height(), maxX = -1, maxY = -1;
        for (int y = 0; y < img.height(); ++y)
        {
            const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            for (int x = 0; x < img.width(); ++x)
                if (qAlpha(line[x]) > 0)
                {
                    minX = qMin(minX, x); maxX = qMax(maxX, x);
                    minY = qMin(minY, y); maxY = qMax(maxY, y);
                }
        }
        return maxX < 0 ? QRect() : QRect(QPoint(minX, minY), QPoint(maxX, maxY));
    };

    QProgressDialog progress(tr("正在跨帧传播色点…"), tr("取消"), 0, targets.size(), mParent);
    progress.setWindowTitle(tipTitle);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);

    // 工程色板注入（存量笔画无登记时认领）后源帧着色补算
    {
        QVector<QRgb> paletteColors;
        const int n = mEditor->object()->getColorCount();
        for (int i = 0; i < n; ++i)
            paletteColors.append(mEditor->object()->getColor(i).color.rgba());
        colorizeLayer->setPaletteColors(paletteColors);
    }

    // 区域映射以源帧着色结果为颜色事实源；未算过或已过期（涂后未刷新/
    // 图层结构变动）都同步补算——否则传播采样的是旧色场，新涂的颜色丢失
    const quint32 structureGen = mEditor->object()->layerStructureGeneration();
    const auto ensureSourceColoring = [&](ColorizeImage* frame) {
        // 无条件重算：着色缓存可能由旧版引擎算出（如清理地雷修复前的
        // 被掏空结果），needsUpdate/结构代数都识别不出引擎变更——传播
        // 的颜色事实源必须永远是当前引擎的产物（锚点数量少，成本可接受）
        colorizeLayer->updateColoringAtFrame(frame->pos(), lineLayer, structureGen);
        return !frame->coloringImage().isNull();
    };

    // 分割用滤波选项与填色面板同源（闭缝等参数对区域分割同样生效）
    Colorize::FilteringOptions filteringOptions;
    filteringOptions.useEdgeDetection = colorizeLayer->useEdgeDetection();
    filteringOptions.edgeDetectionSize = colorizeLayer->edgeDetectionSize();
    filteringOptions.fuzzyRadius = colorizeLayer->fuzzyRadius();
    filteringOptions.cleanUpAmount = colorizeLayer->cleanUpAmount();

    // --- 锚点集：填色层全部非空关键帧（开头/中间/结尾的手涂帧即锚点）---
    struct Anchor
    {
        int pos = 0;
        ColorizeImage* frame = nullptr;
        BitmapImage* lineFrame = nullptr;
    };
    QVector<Anchor> anchors;
    colorizeLayer->foreachKeyFrame([&](KeyFrame* key) {
        auto img = static_cast<ColorizeImage*>(key);
        if (img == nullptr || img->bounds().isEmpty() || img->isPropagated())
            return; // 传播帧是本功能旧产物，不是用户意图：不作锚点（可被刷新）
        auto line = static_cast<BitmapImage*>(
            lineLayer->getKeyFrameWhichCovers(lineLayer->displayFrameFor(img->pos())));
        if (line == nullptr || line->bounds().isEmpty())
            return; // 无线稿支撑：不能作源（仍受保护跳过）
        if (!ensureSourceColoring(img))
            return; // 着色失败：不能作源
        anchors.append(Anchor{ img->pos(), img, line });
    });
    std::sort(anchors.begin(), anchors.end(),
              [](const Anchor& a, const Anchor& b) { return a.pos < b.pos; });
    if (anchors.isEmpty())
    {
        QMessageBox::information(mParent, tipTitle,
            tr("没有已涂色点的锚点帧：请先在编辑模式下涂色点（开头/中间/结尾多帧都涂，传播更准），再传播。"));
        return Status::CANCELED;
    }

    // --- 锚点校验（纠错信号）：锚点对 A→B / B→A 各直推一遍，与用户实画
    // 对比。①区域级吻合度（正/反向）= 搬运精度的定量评分，驱动填隙的
    // 双向冲突裁决（L3 闭环纠错：精度悬殊时精度优先于距离）；②色集
    // 未预测到/多预测 = 两锚点间对应不稳的信号，补涂锚点即可改善
    QStringList validationNotes;
    QStringList agreementNotes;
    struct PairScore { qreal fwd = 1.0; qreal bwd = 1.0; };
    QHash<QPair<int, int>, PairScore> pairScores;
    const int pairCount = qMax(0, anchors.size() - 1);
    progress.setMaximum(targets.size() + pairCount);
    {
        const QRgb transp = colorizeLayer->transparentColor();
        const bool hasTransp = colorizeLayer->hasTransparentColor();
        // 锚点实画色集（面板同口径实心色，去透明标记色）
        const auto solidColorsAt = [&](int framePos) {
            QSet<QRgb> s;
            for (QRgb c : colorizeLayer->strokeColorsAtFrame(framePos))
                if (!(hasTransp && c == transp))
                    s.insert(c);
            return s;
        };
        for (int i = 0; i + 1 < anchors.size(); ++i)
        {
            progress.setValue(i);
            QCoreApplication::processEvents();
            const Anchor& A = anchors[i];
            const Anchor& B = anchors[i + 1];
            const QRect canvas = (A.lineFrame->bounds() | A.frame->coloringBounds()
                                  | B.lineFrame->bounds() | B.frame->coloringBounds())
                                     .adjusted(-16, -16, 16, 16);
            const QRect canvasRect(0, 0, canvas.width(), canvas.height());
            const QImage lineAFlat = flatten(*A.lineFrame, canvas);
            const QImage lineBFlat = flatten(*B.lineFrame, canvas);
            // 着色场平铺（两锚点各自）
            const auto flattenColoring = [&canvas](ColorizeImage* frame) {
                QImage flat(canvas.size(), QImage::Format_ARGB32_Premultiplied);
                flat.fill(Qt::transparent);
                QPainter p(&flat);
                p.drawImage(frame->coloringBounds().topLeft() - canvas.topLeft(), frame->coloringImage());
                p.end();
                return flat;
            };
            const QImage coloringAFlat = flattenColoring(A.frame);
            const QImage coloringBFlat = flattenColoring(B.frame);

            const QImage predicted = Colorize::transportStrokesByRegions(
                lineAFlat, coloringAFlat, lineBFlat, canvasRect, filteringOptions, transp, hasTransp);
            const QImage predictedBack = Colorize::transportStrokesByRegions(
                lineBFlat, coloringBFlat, lineAFlat, canvasRect, filteringOptions, transp, hasTransp);

            // 正/反向区域吻合度（预测标记 vs 用户实画笔画，锚点各自线稿分割）
            PairScore score;
            score.fwd = Colorize::measureRegionAgreement(lineBFlat, predicted, flatten(*B.frame, canvas),
                                                         canvasRect, filteringOptions, transp, hasTransp);
            score.bwd = Colorize::measureRegionAgreement(lineAFlat, predictedBack, flatten(*A.frame, canvas),
                                                         canvasRect, filteringOptions, transp, hasTransp);
            pairScores.insert(qMakePair(A.pos, B.pos), score);
            agreementNotes << tr("帧%1→帧%2 吻合 正%3%/反%4%")
                                  .arg(A.pos).arg(B.pos)
                                  .arg(qRound(score.fwd * 100)).arg(qRound(score.bwd * 100));

            QSet<QRgb> predictedColors;
            for (int y = 0; y < predicted.height(); ++y)
            {
                const QRgb* line = reinterpret_cast<const QRgb*>(predicted.constScanLine(y));
                for (int x = 0; x < predicted.width(); ++x)
                {
                    if (qAlpha(line[x]) == 0) continue;
                    const QRgb c = qUnpremultiply(line[x]);
                    if (hasTransp && c == transp) continue;
                    predictedColors.insert(c);
                }
            }
            QSet<QRgb> actualColors = solidColorsAt(B.pos);

            QStringList missed, extra;
            for (QRgb c : actualColors)
                if (!predictedColors.contains(c))
                    missed << QColor(c).name();
            for (QRgb c : predictedColors)
                if (!actualColors.contains(c))
                    extra << QColor(c).name();
            if (!missed.isEmpty() || !extra.isEmpty())
            {
                // 附两侧锚点实画色集：锚点间色集本就不同（如帧5没涂橙）
                // vs 搬运丢失，用户看色集即可自诊断
                const auto setNames = [](QSet<QRgb> colors) {
                    QStringList names;
                    QList<QRgb> ordered = colors.values();
                    std::sort(ordered.begin(), ordered.end());
                    for (QRgb c : ordered)
                        names << QColor(c).name();
                    return names.join(",");
                };
                validationNotes << tr("帧%1→帧%2：未预测到 %3；多预测 %4（帧%1 色集[%5] 帧%2 色集[%6]）")
                                      .arg(A.pos).arg(B.pos)
                                      .arg(missed.isEmpty() ? QStringLiteral("—") : missed.join(", "))
                                      .arg(extra.isEmpty() ? QStringLiteral("—") : extra.join(", "))
                                      .arg(setNames(solidColorsAt(A.pos)))
                                      .arg(setNames(actualColors));
            }
        }
    }

    // 区域邻近映射：锚帧各封闭区域颜色（着色结果）按质心最近继承到
    // 目标帧各区域，每区域一个标准标记点；已有手涂笔画的帧保护跳过
    int created = 0, written = 0, skippedPainted = 0, skippedNoLine = 0, canceled = 0;
    QVector<int> touched;
    QSet<int> touchedPos; // 本次运行写入的帧位（不算作保护对象）

    mEditor->beginLayerLayoutEdit(colorizeLayer);
    // 已有空块的像素修改走快照链；新建帧由布局事务托管，帧删除即整体撤销
    const SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    bool contentDirty = false;

    for (int i = 0; i < targets.size(); ++i)
    {
        progress.setValue(pairCount + i);
        QCoreApplication::processEvents();
        if (progress.wasCanceled())
        {
            canceled = targets.size() - i;
            break;
        }

        const int pos = targets[i];
        auto lineFrame = static_cast<BitmapImage*>(lineLayer->getKeyFrameAt(pos));
        if (lineFrame == nullptr || lineFrame->bounds().isEmpty())
        {
            ++skippedNoLine; // 空帧块：跳过继续
            continue;
        }

        // 保护判定只看「恰在该位的关键帧」：手涂键保护自身位置，锚点/
        // 用户修正帧不被覆盖；传播帧（isPropagated）是旧产物可刷新。
        // 不再看「覆盖键」（曝光跨度）——锚点工作流下稀疏锚点的曝光
        // 跨度就是全部空隙帧，覆盖式保护会把每个待填帧都判成手涂
        // （"新建0帧/跳过9帧"案：锚点1覆盖2-4、5覆盖6-8、9覆盖10-12）。
        // 空隙帧建键会切分锚点的曝光显示，这正是"根据线稿层建帧"的预期
        KeyFrame* existing = colorizeLayer->getKeyFrameAt(pos);
        if (existing != nullptr)
        {
            auto existingImg = static_cast<ColorizeImage*>(existing);
            if (!existingImg->bounds().isEmpty()
                && !existingImg->isPropagated()
                && !touchedPos.contains(pos))
            {
                ++skippedPainted; // 手涂帧（锚点/用户修正）：保护跳过
                continue;
            }
        }

        // 左右夹逼锚点：t 两侧最近的手涂锚（锚点集升序，O(n) 扫描）
        const Anchor* leftA = nullptr;   // 最大 pos < t
        const Anchor* rightB = nullptr;  // 最小 pos > t
        for (const Anchor& a : anchors)
        {
            if (a.pos < pos && (leftA == nullptr || a.pos > leftA->pos))
                leftA = &a;
            if (a.pos > pos && (rightB == nullptr || a.pos < rightB->pos))
                rightB = &a;
        }
        const Anchor* anchor = (leftA != nullptr) ? leftA : rightB;
        if (anchor == nullptr)
        {
            ++skippedNoLine; // 无可用锚点（理论不可达：当前帧必为锚点）
            continue;
        }

        // 着色场平铺与搬运的公共小工具（canvas 相对坐标，原点 0,0）
        const auto flattenColoringOf = [&colorizeLayer](ColorizeImage* frame, const QRect& canvas) {
            QImage flat(canvas.size(), QImage::Format_ARGB32_Premultiplied);
            flat.fill(Qt::transparent);
            QPainter p(&flat);
            p.drawImage(frame->coloringBounds().topLeft() - canvas.topLeft(), frame->coloringImage());
            p.end();
            return flat;
        };
        const auto transportFrom = [&](const Anchor& src, const QRect& canvas) {
            return Colorize::transportStrokesByRegions(
                flatten(*src.lineFrame, canvas), flattenColoringOf(src.frame, canvas),
                flatten(*lineFrame, canvas),
                QRect(0, 0, canvas.width(), canvas.height()), filteringOptions,
                colorizeLayer->transparentColor(), colorizeLayer->hasTransparentColor());
        };

        QImage transported;
        QRect canvas;
        if (leftA != nullptr && rightB != nullptr)
        {
            // L2 双向传播：两侧锚点各搬运到本帧，按区域融合。
            // 冲突裁决（L3 闭环纠错）：正/反向校验精度悬殊（差≥25%）时
            // 精度优先；否则近锚点方向优先
            canvas = (leftA->lineFrame->bounds() | leftA->frame->coloringBounds()
                      | rightB->lineFrame->bounds() | rightB->frame->coloringBounds()
                      | lineFrame->bounds()).adjusted(-16, -16, 16, 16);
            const QImage fwd = transportFrom(*leftA, canvas);
            const QImage bwd = transportFrom(*rightB, canvas);
            const PairScore score = pairScores.value(qMakePair(leftA->pos, rightB->pos), PairScore());
            const bool preferForward = qAbs(score.fwd - score.bwd) >= 0.25
                ? score.fwd > score.bwd
                : (pos - leftA->pos) <= (rightB->pos - pos);
            transported = Colorize::mergeBidirectionalMarkers(
                fwd, bwd, flatten(*lineFrame, canvas),
                QRect(0, 0, canvas.width(), canvas.height()), filteringOptions, preferForward);
        }
        else
        {
            // 锚点范围外（首锚点之前/末锚点之后）：最近锚点单侧搬运
            canvas = (anchor->lineFrame->bounds() | anchor->frame->coloringBounds() | lineFrame->bounds())
                         .adjusted(-16, -16, 16, 16);
            transported = transportFrom(*anchor, canvas);
        }

        // 已标记透明颜色：自动包裹背景——线稿外泛洪填透明保护色（Krita 手绘
        // 透明笔画保护背景的自动化），色点后画覆盖包裹
        if (colorizeLayer->hasTransparentColor())
        {
            const QImage wrap = Colorize::makeBackgroundWrap(flatten(*lineFrame, canvas),
                                                             QRect(0, 0, canvas.width(), canvas.height()),
                                                             colorizeLayer->transparentColor());
            QPainter wrapPainter(&transported);
            wrapPainter.drawImage(0, 0, wrap);
            wrapPainter.end();
        }
        const QRect box = nonEmptyBBox(transported);
        if (box.isEmpty())
        {
            ++skippedNoLine; // 搬运结果为空：跳过
            continue;
        }
        BitmapImage newStrokes(box.topLeft() + canvas.topLeft(), transported.copy(box));

        if (existing == nullptr)
        {
            auto newFrame = new ColorizeImage();
            newFrame->paste(&newStrokes);
            newFrame->setPropagated(true); // 传播产物：重传可刷新
            colorizeLayer->addKeyFrame(pos, newFrame);
            ++created;
        }
        else
        {
            auto existingImg = static_cast<ColorizeImage*>(existing);
            existingImg->paste(&newStrokes);
            existingImg->setModified(true);
            existingImg->setNeedsUpdate(true);
            existingImg->setPropagated(true);
            contentDirty = true;
            ++written;
        }
        touched.append(pos);
        touchedPos.insert(pos);
    }

    if (contentDirty)
        mEditor->undoRedo()->record(saveStateId, tr("跨帧传播填色", "Undo step text"));
    mEditor->endLayerLayoutEdit(tr("跨帧传播填色"));

    progress.setValue(targets.size() + pairCount);

    if (!touched.isEmpty())
    {
        for (int pos : touched)
            mEditor->colorizeUpdates()->requestUpdate(colorizeLayer, pos);
        emit mEditor->updateTimeLine();
        emit mEditor->framesModified();
    }

    QString summary = tr("传播完成：锚点 %5 个，新建 %1 帧、写入 %2 帧；跳过锚点/手涂 %3 帧、无线稿 %4 帧。")
                          .arg(created).arg(written).arg(skippedPainted).arg(skippedNoLine).arg(anchors.size());
    if (anchors.size() >= 2)
    {
        summary += tr("\n锚点校验：%1").arg(agreementNotes.join("；"));
        if (validationNotes.isEmpty())
            summary += tr("\n色集全部命中。");
        else
            summary += tr("\n对应不稳对（建议在两锚点之间补涂一个锚点后重传）：\n%1")
                           .arg(validationNotes.join("\n"));
    }
    if (canceled > 0)
        summary += tr("\n已取消：剩余 %1 帧未处理。").arg(canceled);
    QMessageBox::information(mParent, tipTitle, summary);
    return Status::OK;
}

Status ActionCommands::clearCurrentLayerCanvas()
{
    const QString tipTitle = tr("清除帧");

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        return Status::FAIL;
    }
    if (!layer->isBitmapKind())
    {
        QMessageBox::information(mParent, tipTitle, tr("清除帧只能在位图族图层（位图/填色）上使用。"));
        return Status::CANCELED;
    }
    if (layer->locked())
    {
        QMessageBox::information(mParent, tipTitle, tr("图层“%1”已锁定，无法清除。").arg(layer->name()));
        return Status::CANCELED;
    }

    // 与画布落笔同源：循环层清除的是显示帧背后的关键帧（所见即所编辑）
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* bitmap = static_cast<BitmapImage*>(
        bitmapLayer->getKeyFrameWhichCovers(bitmapLayer->displayFrameFor(mEditor->currentFrame())));
    if (bitmap == nullptr || bitmap->bounds().isEmpty())
    {
        QMessageBox::information(mParent, tipTitle, tr("当前帧没有可清除的画布内容。"));
        return Status::CANCELED;
    }

    // 撤销：显式双快照，针对实际修改的关键帧（循环层/任意帧安全，不经"当前帧"快照链）
    BitmapImage undoSnapshot = *bitmap;
    bitmap->clear();
    BitmapImage redoSnapshot = *bitmap;
    mEditor->undoRedo()->pushUndoCommand(
        new BitmapReplaceCommand(&undoSnapshot, &redoSnapshot, layer->id(),
                                 tr("清除帧", "Undo step text"), mEditor));
    // 数据失效：传实际关键帧 pos（Layer::setModified 按帧号精确找关键帧，
    // auto 块中部传 currentFrame 找不到 → dirty 不标记）
    mEditor->setModified(mEditor->layers()->currentLayerIndex(), bitmap->pos());
    // 显示缓存失效：按画布所见帧作废并重绘（endStroke 同款；auto 块中部 pos≠currentFrame，
    // 不作废显示帧缓存 → 画布贴旧像素 → "清不掉/内容错位"）
    mEditor->getScribbleArea()->onFrameModified(mEditor->currentFrame());
    return Status::OK;
}

void ActionCommands::setLayerVisibilityIndex(int index)
{
    mEditor->setLayerVisibility(static_cast<LayerVisibility>(index));
}

void ActionCommands::changeKeyframeLineColor()
{
    if (mEditor->layers()->currentLayer()->type() == Layer::BITMAP &&
            mEditor->layers()->currentLayer()->keyExists(mEditor->currentFrame()))
    {
        QRgb color = mEditor->color()->frontColor().rgb();
        LayerBitmap* layer = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer());
        layer->getBitmapImageAtFrame(mEditor->currentFrame())->fillNonAlphaPixels(color);
        mEditor->updateFrame();
    }
}

void ActionCommands::changeallKeyframeLineColor()
{
    if (mEditor->layers()->currentLayer()->type() == Layer::BITMAP)
    {
        QRgb color = mEditor->color()->frontColor().rgb();
        LayerBitmap* layer = static_cast<LayerBitmap*>(mEditor->layers()->currentLayer());
        for (int i = layer->firstKeyFramePosition(); i <= layer->getMaxKeyFramePosition(); i++)
        {
            if (layer->keyExists(i))
                layer->getBitmapImageAtFrame(i)->fillNonAlphaPixels(color);
        }
        mEditor->updateFrame();
    }
}


void ActionCommands::resetAllTools()
{
    mEditor->tools()->resetAllTools();
}

void ActionCommands::help()
{
    QString url = "http://www.pencil2d.org/doc/";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::quickGuide()
{
    QString sDocPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString sCopyDest = QDir(sDocPath).filePath("pencil2d_quick_guide.pdf");

    QFile quickGuideFile(":/app/pencil2d_quick_guide.pdf");
    quickGuideFile.copy(sCopyDest);

    QDesktopServices::openUrl(QUrl::fromLocalFile(sCopyDest));
}

void ActionCommands::website()
{
    QString url = "https://www.pencil2d.org/";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::forum()
{
    QString url = "https://discuss.pencil2d.org/";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::discord()
{
    QString url = "https://discord.gg/8FxdV2g";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::reportbug()
{
    QString url = "https://github.com/pencil2d/pencil/issues";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::checkForUpdates()
{
    CheckUpdatesDialog dialog;
    dialog.startChecking();
    dialog.exec();
}

// This action is a temporary measure until we have an automated recover mechanism in place
void ActionCommands::openTemporaryDirectory()
{
    int ret = QMessageBox::warning(mParent, tr("Warning"), tr("The temporary directory is meant to be used only by Pencil2D. Do not modify it unless you know what you are doing."), QMessageBox::Cancel, QMessageBox::Ok);
    if (ret == QMessageBox::Ok)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::temp().filePath("Pencil2D")));
    }
}

void ActionCommands::about()
{
    AboutDialog* aboutBox = new AboutDialog(mParent);
    aboutBox->setAttribute(Qt::WA_DeleteOnClose);
    aboutBox->init();
    aboutBox->exec();
}

void ActionCommands::showSoundClipWarningIfNeeded()
{
    int clipCount = mEditor->sound()->soundClipCount();
    if (clipCount >= MovieExporter::MAX_SOUND_FRAMES && !mSuppressSoundWarning) {
        QMessageBox::warning(mParent, tr("Warning"), tr("You currently have a total of %1 sound clips. Due to current limitations, you will be unable to export any animation exceeding %2 sound clips. We recommend splitting up larger projects into multiple smaller project to stay within this limit.").arg(clipCount).arg(MovieExporter::MAX_SOUND_FRAMES));
        mSuppressSoundWarning = true;
    } else {
        mSuppressSoundWarning = false;
    }
}
