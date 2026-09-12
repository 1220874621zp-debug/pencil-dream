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
#include "tvptoolsdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>

#include "bitmapimage.h"
#include "colorpalettewidget.h"
#include "colorref.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "scribblearea.h"

namespace
{
    QImage contentThumb(BitmapImage* image, const QSize& size)
    {
        if (image == nullptr || image->image() == nullptr || image->image()->isNull())
        {
            return QImage();
        }
        const QImage src = image->image()->copy(image->bounds());
        if (src.isNull()) { return QImage(); }
        return src.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QList<QPair<QString, Layer*>> findPhonemeLayers(Editor* editor, const QStringList& phonemes)
    {
        QList<QPair<QString, Layer*>> found;
        Object* obj = editor->object();
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            Layer* layer = obj->getLayer(i);
            if (layer == nullptr || layer->type() != Layer::BITMAP) { continue; }
            const QString name = layer->name().trimmed().toUpper();
            for (const QString& phoneme : phonemes)
            {
                if (name == phoneme)
                {
                    found.append(qMakePair(phoneme, layer));
                    break;
                }
            }
        }
        return found;
    }
}

// ---------------------------------------------------------------- 口型同步 ---

LipsyncDialog::LipsyncDialog(Editor* editor, QWidget* parent)
    : QDialog(parent)
    , mEditor(editor)
{
    setWindowTitle(tr("口型同步切换器"));
    setModal(false);
    setMinimumWidth(340);

    auto* layout = new QVBoxLayout(this);
    QLabel* hint = new QLabel(tr("口型图层命名 A/E/I/O/U/N/MBP/FV/L/WQ，点击即在“口型”图层的当前帧插入该口型。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    mPhonemeGrid = new QWidget();
    layout->addWidget(mPhonemeGrid);

    auto* buttonRow = new QHBoxLayout();
    QPushButton* refreshButton = new QPushButton(tr("刷新口型"));
    QPushButton* clearButton = new QPushButton(tr("清空此帧"));
    buttonRow->addWidget(refreshButton);
    buttonRow->addWidget(clearButton);
    layout->addLayout(buttonRow);

    connect(refreshButton, &QPushButton::clicked, this, &LipsyncDialog::refreshPhonemes);
    connect(clearButton, &QPushButton::clicked, this, &LipsyncDialog::clearCurrentFrame);

    refreshPhonemes();
}

Layer* LipsyncDialog::targetLayer()
{
    Layer* target = mEditor->layers()->findLayerByName(tr("口型"), Layer::BITMAP);
    if (target == nullptr)
    {
        target = mEditor->layers()->createBitmapLayer(tr("口型"));
    }
    return target;
}

void LipsyncDialog::refreshPhonemes()
{
    delete mPhonemeGrid->layout();
    while (!mPhonemeGrid->children().isEmpty())
    {
        delete mPhonemeGrid->children().first();
    }

    const QStringList phonemes = { "A", "E", "I", "O", "U", "N", "MBP", "FV", "L", "WQ" };
    const auto found = findPhonemeLayers(mEditor, phonemes);

    auto* grid = new QGridLayout(mPhonemeGrid);
    grid->setSpacing(6);
    int row = 0;
    int col = 0;
    for (const auto& pair : found)
    {
        LayerBitmap* source = static_cast<LayerBitmap*>(pair.second);
        BitmapImage* image = source->getBitmapImageAtFrame(1);
        if (image == nullptr)
        {
            image = source->getLastBitmapImageAtFrame(1);
        }

        QPushButton* button = new QPushButton(mPhonemeGrid);
        button->setFixedSize(84, 84);
        button->setToolTip(tr("在当前帧插入口型 %1").arg(pair.first));

        const QImage thumb = contentThumb(image, QSize(72, 60));
        if (!thumb.isNull())
        {
            QPixmap pixmap(80, 76);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.drawImage(QPoint((80 - thumb.width()) / 2, 2), thumb);
            painter.setPen(QPen(palette().text().color()));
            painter.drawText(QRect(0, 60, 80, 16), Qt::AlignCenter, pair.first);
            painter.end();
            button->setIconSize(QSize(80, 76));
            button->setIcon(QIcon(pixmap));
        }
        else
        {
            button->setText(pair.first);
        }
        grid->addWidget(button, row, col);
        connect(button, &QPushButton::clicked, this, [this, pair]() { insertPhoneme(pair.first); });

        col++;
        if (col >= 3)
        {
            col = 0;
            row++;
        }
    }
    if (found.isEmpty())
    {
        grid->addWidget(new QLabel(tr("未找到口型图层（图层名为 A/E/I/O/U 等）")), 0, 0, 1, 3);
    }
}

void LipsyncDialog::insertPhoneme(const QString& phoneme)
{
    const auto found = findPhonemeLayers(mEditor, { phoneme });
    if (found.isEmpty()) { return; }

    LayerBitmap* source = static_cast<LayerBitmap*>(found.first().second);
    BitmapImage* srcImage = source->getBitmapImageAtFrame(1);
    if (srcImage == nullptr)
    {
        srcImage = source->getLastBitmapImageAtFrame(1);
    }
    if (srcImage == nullptr) { return; }

    Layer* target = targetLayer();
    const int frame = mEditor->currentFrame();

    mEditor->beginLayerLayoutEdit(target);
    if (target->keyExists(frame))
    {
        KeyFrame* old = mEditor->takeLayerKeyFrame(target, frame);
        delete old;
    }
    target->addKeyFrame(frame, srcImage->clone());
    mEditor->endLayerLayoutEdit(tr("插入口型 %1").arg(phoneme));

    emit mEditor->framesModified();
    mEditor->layers()->notifyAnimationLengthChanged();
    mEditor->getScribbleArea()->update();
}

void LipsyncDialog::clearCurrentFrame()
{
    Layer* target = targetLayer();
    const int frame = mEditor->currentFrame();

    mEditor->beginLayerLayoutEdit(target);
    if (target->keyExists(frame))
    {
        KeyFrame* old = mEditor->takeLayerKeyFrame(target, frame);
        delete old;
    }
    // a blank 1x1 transparent key stops the previous mouth from showing
    QImage blank(1, 1, QImage::Format_ARGB32_Premultiplied);
    blank.fill(Qt::transparent);
    target->addKeyFrame(frame, new BitmapImage(QPoint(0, 0), blank));
    mEditor->endLayerLayoutEdit(tr("清空口型帧"));

    emit mEditor->framesModified();
    mEditor->getScribbleArea()->update();
}

// ---------------------------------------------------------------- 调色板 ---+

PaletteExtractDialog::PaletteExtractDialog(Editor* editor, QWidget* parent)
    : QDialog(parent)
    , mEditor(editor)
{
    setWindowTitle(tr("调色板提取"));
    setModal(false);
    setMinimumWidth(360);

    auto* layout = new QVBoxLayout(this);
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("颜色数量：")));
    mCountSpin = new QSpinBox();
    mCountSpin->setRange(1, 20);
    mCountSpin->setValue(8);
    row->addWidget(mCountSpin);
    row->addStretch();
    QPushButton* pickButton = new QPushButton(tr("选择图片并提取"));
    row->addWidget(pickButton);
    layout->addLayout(row);

    mPreviewLabel = new QLabel(tr("提取后将生成色块图层，并将色值复制到剪贴板。"));
    mPreviewLabel->setWordWrap(true);
    layout->addWidget(mPreviewLabel);

    connect(pickButton, &QPushButton::clicked, this, &PaletteExtractDialog::pickImageAndExtract);
}

// TVP-style quantization: 5-bit bins, top-N bins by population,
// the most common original color inside each bin wins
QList<QRgb> PaletteExtractDialog::extractColors(const QImage& image, int wanted)
{
    QList<QRgb> result;
    const QImage src = image.convertToFormat(QImage::Format_ARGB32);

    QHash<int, int> binTotal;
    QHash<int, QHash<QRgb, int>> binColors;
    const int step = qMax(1, static_cast<int>(qLn(src.width() * src.height()) - 6.0));
    for (int y = 0; y < src.height(); y += step)
    {
        for (int x = 0; x < src.width(); x += step)
        {
            const QRgb rgb = src.pixel(x, y);
            if (qAlpha(rgb) < 16) { continue; }
            const int bin = ((qRed(rgb) >> 3) << 10) | ((qGreen(rgb) >> 3) << 5) | (qBlue(rgb) >> 3);
            binTotal[bin] += 1;
            binColors[bin][rgb] += 1;
        }
    }

    QList<int> bins = binTotal.keys();
    std::sort(bins.begin(), bins.end(), [&binTotal](int a, int b) { return binTotal[a] > binTotal[b]; });
    bins = bins.mid(0, wanted);

    for (int bin : bins)
    {
        const QHash<QRgb, int>& colors = binColors[bin];
        QRgb best = 0;
        int bestCount = -1;
        for (auto it = colors.cbegin(); it != colors.cend(); ++it)
        {
            if (it.value() > bestCount)
            {
                bestCount = it.value();
                best = it.key();
            }
        }
        result.append(best);
    }
    return result;
}

void PaletteExtractDialog::pickImageAndExtract()
{
    QSettings settings("Pencil", "Pencil");
    const QString lastDir = settings.value("paletteExtractLastDir").toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("选择图片"), lastDir);
    if (path.isEmpty()) { return; }
    settings.setValue("paletteExtractLastDir", QFileInfo(path).absolutePath());

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
    {
        QMessageBox::warning(this, tr("调色板提取"), tr("无法读取图片：%1").arg(reader.errorString()));
        return;
    }
    mLastColors = extractColors(image, mCountSpin->value());
    if (mLastColors.isEmpty()) { return; }

    QStringList hexList;
    for (QRgb best : mLastColors)
    {
        hexList << QString("#%1%2%3")
                       .arg(qRed(best), 2, 16, QChar('0'))
                       .arg(qGreen(best), 2, 16, QChar('0'))
                       .arg(qBlue(best), 2, 16, QChar('0')).toUpper();
    }

    QApplication::clipboard()->setText(hexList.join(" "));

    // TVP parity: the extracted swatches also go into the color palette panel
    Object* paletteObject = mEditor->object();
    for (QRgb rgb : mLastColors)
    {
        paletteObject->addColorAtIndex(paletteObject->getColorCount(), ColorRef(QColor::fromRgb(rgb)));
    }
    if (QWidget* host = (parentWidget() != nullptr) ? parentWidget()->window() : nullptr)
    {
        if (ColorPaletteWidget* paletteWidget = host->findChild<ColorPaletteWidget*>())
        {
            paletteWidget->refreshColorList();
        }
    }

    // build one big swatch grid image, centered on the canvas origin
    const int cols = static_cast<int>(qCeil(qSqrt(static_cast<qreal>(mLastColors.count()))));
    const int rows = (mLastColors.count() + cols - 1) / cols;
    const int cell = 100;
    const int gap = 10;
    const int totalW = cols * cell + (cols - 1) * gap;
    const int totalH = rows * cell + (rows - 1) * gap;
    QImage grid(totalW, totalH, QImage::Format_ARGB32_Premultiplied);
    grid.fill(Qt::transparent);
    {
        QPainter painter(&grid);
        for (int i = 0; i < mLastColors.count(); ++i)
        {
            const int cx = (i % cols) * (cell + gap);
            const int cy = (i / cols) * (cell + gap);
            painter.fillRect(QRect(cx, cy, cell, cell), mLastColors[i]);
            painter.setPen(QPen(QColor(0, 0, 0, 90), 2));
            painter.drawRect(cx, cy, cell, cell);
        }
    }

    const int frame = mEditor->currentFrame();
    LayerBitmap* layer = mEditor->layers()->createBitmapLayer(
        QFileInfo(path).completeBaseName() + tr("_调色板"));

    mEditor->beginLayerLayoutEdit(layer);
    if (layer->keyExists(frame))
    {
        KeyFrame* old = mEditor->takeLayerKeyFrame(layer, frame);
        delete old;
    }
    layer->addKeyFrame(frame, new BitmapImage(QPoint(-totalW / 2, -totalH / 2), grid));
    mEditor->endLayerLayoutEdit(tr("生成调色板"));

    emit mEditor->framesModified();
    mEditor->layers()->notifyAnimationLengthChanged();
    mEditor->getScribbleArea()->update();

    // small preview row
    QPixmap preview(mLastColors.count() * 26 + 4, 30);
    preview.fill(Qt::transparent);
    {
        QPainter painter(&preview);
        for (int i = 0; i < mLastColors.count(); ++i)
        {
            painter.fillRect(2 + i * 26, 2, 24, 26, mLastColors[i]);
        }
    }
    mPreviewLabel->setPixmap(preview);
}

// ------------------------------------------------------------ 视频抽帧中割 ---

VideoExtractDialog::VideoExtractDialog(Editor* editor, QWidget* parent)
    : QDialog(parent)
    , mEditor(editor)
{
    setWindowTitle(tr("视频抽帧中割"));
    setModal(false);
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout(this);

    auto* ffmpegRow = new QHBoxLayout();
    ffmpegRow->addWidget(new QLabel(tr("ffmpeg：")));
    mFfmpegEdit = new QLineEdit();
    QSettings settings("Pencil", "Pencil");
    mFfmpegEdit->setText(settings.value("videoInbetween/ffmpeg", "ffmpeg").toString());
    ffmpegRow->addWidget(mFfmpegEdit);
    layout->addLayout(ffmpegRow);

    auto* videoRow = new QHBoxLayout();
    videoRow->addWidget(new QLabel(tr("视频：")));
    mVideoEdit = new QLineEdit();
    videoRow->addWidget(mVideoEdit);
    QPushButton* browseButton = new QPushButton(tr("浏览"));
    videoRow->addWidget(browseButton);
    QPushButton* probeButton = new QPushButton(tr("探测"));
    videoRow->addWidget(probeButton);
    layout->addLayout(videoRow);

    mSlider = new QSlider(Qt::Horizontal);
    mSlider->setRange(0, 0);
    mSlider->setEnabled(false);
    layout->addWidget(mSlider);

    auto* playRow = new QHBoxLayout();
    mPlayButton = new QPushButton(tr("播放"));
    mPlayButton->setEnabled(false);
    mFrameLabel = new QLabel(tr("帧 0 / 0"));
    playRow->addWidget(mPlayButton);
    playRow->addWidget(mFrameLabel);
    playRow->addStretch();
    layout->addLayout(playRow);

    mPreview = new QLabel();
    mPreview->setMinimumSize(320, 180);
    mPreview->setAlignment(Qt::AlignCenter);
    mPreview->setStyleSheet("background:#101014;");
    layout->addWidget(mPreview, 1);

    auto* importRow = new QHBoxLayout();
    importRow->addWidget(new QLabel(tr("起始帧：")));
    mStartSpin = new QSpinBox();
    importRow->addWidget(mStartSpin);
    importRow->addWidget(new QLabel(tr("结束帧：")));
    mEndSpin = new QSpinBox();
    importRow->addWidget(mEndSpin);
    mImportButton = new QPushButton(tr("导入到时间轴"));
    mImportButton->setEnabled(false);
    importRow->addWidget(mImportButton);
    layout->addLayout(importRow);

    mProgress = new QProgressBar();
    mProgress->setVisible(false);
    layout->addWidget(mProgress);

    mPlayTimer = new QTimer(this);
    mPlayTimer->setInterval(80);
    mProcess = new QProcess(this);
    mTempDir = new QTemporaryDir();

    connect(browseButton, &QPushButton::clicked, this, &VideoExtractDialog::browseVideo);
    connect(probeButton, &QPushButton::clicked, this, &VideoExtractDialog::probeVideo);
    connect(mSlider, &QSlider::valueChanged, this, &VideoExtractDialog::sliderMoved);
    connect(mPlayButton, &QPushButton::clicked, this, &VideoExtractDialog::togglePlay);
    connect(mPlayTimer, &QTimer::timeout, this, [this]()
    {
        const int next = (mCurrentFrame + 1) % qMax(1, mFrameCount);
        mSlider->setValue(next);
    });
    connect(mProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoExtractDialog::fetchFinished);
    connect(mImportButton, &QPushButton::clicked, this, &VideoExtractDialog::importFrames);
}

VideoExtractDialog::~VideoExtractDialog()
{
    delete mTempDir;
}

void VideoExtractDialog::browseVideo()
{
    QSettings settings("Pencil", "Pencil");
    const QString path = QFileDialog::getOpenFileName(this, tr("选择视频"),
                                                      settings.value("videoInbetweenLastDir").toString(),
                                                      tr("视频文件 (*.mp4 *.avi *.mov *.mkv *.webm *.gif)"));
    if (path.isEmpty()) { return; }
    settings.setValue("videoInbetweenLastDir", QFileInfo(path).absolutePath());
    mVideoEdit->setText(path);
    probeVideo();
}

void VideoExtractDialog::probeVideo()
{
    QSettings settings("Pencil", "Pencil");
    settings.setValue("videoInbetween/ffmpeg", mFfmpegEdit->text());
    const QString ffmpeg = mFfmpegEdit->text().trimmed();

    QProcess probe(this);
    probe.start(ffmpeg, { "-v", "error", "-select_streams", "v:0",
                          "-show_entries", "stream=r_frame_rate,duration",
                          "-show_entries", "format=duration",
                          "-of", "default=noprint_wrappers=1", mVideoEdit->text() });
    if (!probe.waitForStarted(3000) || !probe.waitForFinished(10000))
    {
        QMessageBox::warning(this, tr("视频抽帧中割"),
                             tr("无法运行 ffmpeg/ffprobe，请在上方填写正确的 ffmpeg 路径。"));
        return;
    }
    const QString out = probe.readAllStandardOutput();

    // duration: prefer the stream value, fall back to the container value
    double duration = 0.0;
    QRegularExpression durRx("duration=([0-9.]+)");
    auto it = durRx.globalMatch(out);
    while (it.hasNext())
    {
        const double v = it.next().captured(1).toDouble();
        if (v > 0.0) { duration = (duration <= 0.0 || v < duration) ? v : duration; }
    }

    // fps: a fraction like "30000/1001"
    double fps = 0.0;
    QRegularExpression fpsRx("r_frame_rate=(\\d+)/(\\d+)");
    const auto fpsMatch = fpsRx.match(out);
    if (fpsMatch.hasMatch() && fpsMatch.captured(2).toInt() > 0)
    {
        fps = fpsMatch.captured(1).toDouble() / fpsMatch.captured(2).toDouble();
    }

    if (duration <= 0.0 || fps <= 0.0)
    {
        QMessageBox::warning(this, tr("视频抽帧中割"), tr("无法解析视频时长或帧率。"));
        return;
    }

    mDuration = duration;
    mFps = fps;
    mFrameCount = qMax(1, qRound(duration * fps));
    mCurrentFrame = 0;

    mSlider->setEnabled(true);
    mSlider->setRange(0, mFrameCount - 1);
    mSlider->setValue(0);
    mPlayButton->setEnabled(true);
    mImportButton->setEnabled(true);
    mStartSpin->setRange(0, mFrameCount - 1);
    mEndSpin->setRange(0, mFrameCount - 1);
    mEndSpin->setValue(qMin(mFrameCount - 1, 100));
    mFrameCache.clear();
    mCacheOrder.clear();

    showFrame(0);
}

void VideoExtractDialog::sliderMoved(int frame)
{
    mCurrentFrame = frame;
    mFrameLabel->setText(tr("帧 %1 / %2").arg(frame).arg(qMax(0, mFrameCount - 1)));
    showFrame(frame);
}

void VideoExtractDialog::showFrame(int frame)
{
    const auto it = mFrameCache.constFind(frame);
    if (it != mFrameCache.constEnd())
    {
        mPreview->setPixmap(QPixmap::fromImage(it.value()).scaled(
            mPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        // prefetch the next frames while playing
        if (mPlayTimer->isActive())
        {
            for (int ahead = 1; ahead <= 3; ++ahead)
            {
                if (!mFrameCache.contains((frame + ahead) % mFrameCount))
                {
                    requestFrame((frame + ahead) % mFrameCount);
                    break;
                }
            }
        }
        return;
    }
    requestFrame(frame);
}

void VideoExtractDialog::requestFrame(int frame)
{
    if (mProcess->state() != QProcess::NotRunning) { return; }
    if (!mTempDir->isValid()) { return; }

    mPendingFrame = frame;
    const double t = frame / mFps;
    const QString outPath = mTempDir->filePath(QString("f_%1.png").arg(frame));
    mProcess->start(mFfmpegEdit->text().trimmed(),
                    { "-y", "-ss", QString::number(t, 'f', 3),
                      "-i", mVideoEdit->text(),
                      "-frames:v", "1", outPath });
}

void VideoExtractDialog::fetchFinished()
{
    if (mPendingFrame < 0) { return; }
    const QString outPath = mTempDir->filePath(QString("f_%1.png").arg(mPendingFrame));
    QImage img(outPath);
    if (!img.isNull())
    {
        // simple LRU: drop the oldest entry beyond 120 cached frames
        mFrameCache.insert(mPendingFrame, img);
        mCacheOrder.removeAll(mPendingFrame);
        mCacheOrder.append(mPendingFrame);
        while (mCacheOrder.count() > 120)
        {
            mFrameCache.remove(mCacheOrder.takeFirst());
        }
        if (mPendingFrame == mCurrentFrame)
        {
            mPreview->setPixmap(QPixmap::fromImage(img).scaled(
                mPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    mPendingFrame = -1;

    // keep filling the current request if it is still missing
    if (!mFrameCache.contains(mCurrentFrame))
    {
        requestFrame(mCurrentFrame);
    }
}

void VideoExtractDialog::togglePlay()
{
    if (mPlayTimer->isActive())
    {
        mPlayTimer->stop();
        mPlayButton->setText(tr("播放"));
    }
    else
    {
        mPlayTimer->start(qMax(30, static_cast<int>(1000.0 / mFps)));
        mPlayButton->setText(tr("暂停"));
    }
}

QImage VideoExtractDialog::renderImportImage(int frame, const QSize& targetSize)
{
    // blocking extraction: importing runs a visible progress anyway
    const QString outPath = mTempDir->filePath(QString("imp_%1.png").arg(frame));
    QProcess ffmpeg(this);
    ffmpeg.start(mFfmpegEdit->text().trimmed(),
                 { "-y", "-ss", QString::number(frame / mFps, 'f', 3),
                   "-i", mVideoEdit->text(),
                   "-frames:v", "1", outPath });
    if (!ffmpeg.waitForStarted(3000) || !ffmpeg.waitForFinished(30000))
    {
        return QImage();
    }
    QImage img = QImage(outPath);
    if (img.isNull()) { return QImage(); }
    return img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void VideoExtractDialog::importFrames()
{
    const int start = mStartSpin->value();
    const int end = mEndSpin->value();
    if (end < start) { return; }

    // destination size: the camera view keeps the imported frames inside frame
    QSize target(800, 600);
    LayerCamera* camera = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
    if (camera != nullptr)
    {
        target = camera->getViewRect().size();
    }

    LayerBitmap* layer = mEditor->layers()->createBitmapLayer(tr("AI中割"));
    const int frame0 = mEditor->currentFrame();
    const int total = end - start + 1;

    mProgress->setVisible(true);
    mProgress->setRange(0, total);
    mImportButton->setEnabled(false);

    mEditor->beginLayerLayoutEdit(layer);
    int imported = 0;
    for (int i = 0; i < total; ++i)
    {
        qApp->processEvents();
        const QImage img = renderImportImage(start + i, target);
        mProgress->setValue(i + 1);
        if (img.isNull()) { continue; }

        const QPoint topLeft(-img.width() / 2, -img.height() / 2);
        layer->addKeyFrame(frame0 + i, new BitmapImage(topLeft, img));
        ++imported;
    }
    mEditor->endLayerLayoutEdit(tr("导入视频帧 %1 张").arg(imported));

    mProgress->setVisible(false);
    mImportButton->setEnabled(true);

    emit mEditor->framesModified();
    mEditor->layers()->notifyAnimationLengthChanged();
    mEditor->getScribbleArea()->update();
}
