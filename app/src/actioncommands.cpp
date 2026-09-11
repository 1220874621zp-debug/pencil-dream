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

#include "pencildef.h"
#include "editor.h"
#include "object.h"
#include "viewmanager.h"
#include "layermanager.h"
#include "undoredomanager.h"
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
    QProcess probe(this);
    probe.start(ffprobeLocation(), { "-v", "error", "-select_streams", "v:0",
                                     "-show_entries", "stream=r_frame_rate,duration",
                                     "-show_entries", "format=duration",
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
        QRegularExpression fpsRx("r_frame_rate=(\d+)/(\d+)");
        const auto fpsMatch = fpsRx.match(out);
        if (fpsMatch.hasMatch() && fpsMatch.captured(2).toInt() > 0)
        {
            videoFps = fpsMatch.captured(1).toDouble() / fpsMatch.captured(2).toDouble();
        }
    }
    if (duration <= 0.0)
    {
        QMessageBox::warning(mParent, tr("导入参考视频"), tr("无法解析视频时长,请确认文件完好。"));
        return Status::FAIL;
    }
    if (videoFps <= 0.0) { videoFps = mEditor->playback()->fps(); }

    const int frames = qMax(1, qRound(duration * videoFps));
    const QFileInfo info(filePath);
    LayerVideo* layer = mEditor->layers()->createVideoLayer(info.completeBaseName());
    layer->setVideoSource(info.absoluteFilePath(), videoFps, frames, mEditor->currentFrame());

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
    bool ok;
    QString text = QInputDialog::getText(nullptr, tr("Layer Properties"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         mEditor->layers()->nameSuggestLayer(tr("Colorize Layer")), &ok);
    if (ok && !text.isEmpty())
    {
        mEditor->layers()->createColorizeLayer(text);
    }
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
