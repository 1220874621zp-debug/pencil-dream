/*

    GapFill gap-fill assistant for Pencil2D bitmap layers.

    Workflow: snapshot the current coloring layer and the selected
    line-art layer into one shared RGBA canvas, run the ported
    GapFill core (detection + prediction) on it, then write accepted
    fill colors back through an undoable pixel command.

    Core algorithm: Copyright (c) 2026 marc2825 (GapFill, MIT).

*/

#include "gapfilldialog.h"
#include "ui_gapfilldialog.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QPainter>
#include <QProgressDialog>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "bitmapimage.h"
#include "object.h"
#include "undoredomanager.h"

#include "gapfill/core/smart_gap_propagation.hpp"
#include "gapfill/predictors/rule_based_predictor.hpp"

using namespace gap_assist;

namespace {

// Paints one bitmap keyframe into a transparent RGBA8888 canvas whose
// (0,0) corresponds to canvasRect.topLeft() in document coordinates.
QImage keyframeToCanvas(BitmapImage* keyframe, const QRect& canvasRect)
{
    QImage canvas(canvasRect.size(), QImage::Format_RGBA8888);
    canvas.fill(Qt::transparent);
    if (keyframe == nullptr || keyframe->image() == nullptr) { return canvas; }
    const QRect bounds = keyframe->bounds();
    QPainter painter(&canvas);
    painter.drawImage(bounds.topLeft() - canvasRect.topLeft(),
                      *keyframe->image());
    painter.end();
    return canvas;
}

// gap_assist::Image stores tightly packed row-major RGBA8, the same
// byte order as QImage::Format_RGBA8888.
Image canvasToGapImage(const QImage& canvas)
{
    Image image(canvas.width(), canvas.height());
    const int bytesPerRow = canvas.width() * 4;
    for (int y = 0; y < canvas.height(); ++y) {
        std::memcpy(&image.pixels()[static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(canvas.width())],
                    canvas.scanLine(y), bytesPerRow);
    }
    return image;
}

} // namespace

GapFillDialog::GapFillDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::GapFillDialog)
{
    ui->setupUi(this);

    connect(ui->cbGapSize, &QComboBox::currentIndexChanged,
            this, &GapFillDialog::gapSizeChanged);
    connect(ui->btnDetect, &QPushButton::clicked, this, &GapFillDialog::detectGaps);
    connect(ui->btnHighConfidence, &QPushButton::clicked,
            this, &GapFillDialog::checkHighConfidence);
    connect(ui->btnClearChecks, &QPushButton::clicked,
            this, &GapFillDialog::clearChecks);
    connect(ui->btnApply, &QPushButton::clicked, this, &GapFillDialog::applySelected);
    connect(ui->btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->gapsTable, &QTableWidget::cellChanged,
            this, &GapFillDialog::cellChanged);

    ui->cbGapSize->setCurrentIndex(1);      // 中（≤10 像素）
    ui->cbConfidence->setCurrentIndex(1);   // 平衡
    ui->gapsTable->horizontalHeader()->setStretchLastSection(true);
    ui->gapsTable->setColumnWidth(0, 60);
}

GapFillDialog::~GapFillDialog()
{
    delete ui;
}

void GapFillDialog::setCore(Editor* editor)
{
    mEditor = editor;
}

void GapFillDialog::initUI()
{
    const Layer* current = mEditor->layers()->currentLayer();
    const bool bitmapLayer = current != nullptr && current->type() == Layer::BITMAP;
    ui->btnDetect->setEnabled(bitmapLayer);
    if (!bitmapLayer) {
        setStatus(tr("当前图层不是位图图层。请选择着色用的位图图层后再检测。"));
    }
    refreshLineLayerCombo();
    connect(mEditor->layers(), &LayerManager::currentLayerChanged,
            this, [this](int) {
        const Layer* layer = mEditor->layers()->currentLayer();
        const bool bitmap = layer != nullptr && layer->type() == Layer::BITMAP;
        ui->btnDetect->setEnabled(bitmap);
        refreshLineLayerCombo();
        if (!bitmap) {
            setStatus(tr("当前图层不是位图图层。请选择着色用的位图图层后再检测。"));
        }
    });
}

void GapFillDialog::refreshLineLayerCombo()
{
    ui->cbLineLayer->blockSignals(true);
    ui->cbLineLayer->clear();
    ui->cbLineLayer->addItem(tr("不使用（规则预测）"), -1);

    Layer* current = mEditor->layers()->currentLayer();
    if (current == nullptr) {
        ui->cbLineLayer->blockSignals(false);
        return;
    }
    Object* object = mEditor->object();
    const int count = object->getLayerCount();
    int currentIndex = -1;
    for (int index = 0; index < count; ++index) {
        if (object->getLayer(index) == current) {
            currentIndex = index;
            break;
        }
    }

    // Prefer the nearest neighbouring bitmap layer as line art: the
    // learned model needs a separate line-art source.
    int preferredId = -1;
    for (int distance = 1; distance < count && preferredId == -1; ++distance) {
        for (const int index : {currentIndex - distance, currentIndex + distance}) {
            if (index < 0 || index >= count) { continue; }
            Layer* layer = object->getLayer(index);
            if (layer != nullptr && layer->type() == Layer::BITMAP &&
                layer != current) {
                preferredId = layer->id();
                break;
            }
        }
    }
    int preferredRow = 0;
    for (int index = 0; index < count; ++index) {
        Layer* layer = object->getLayer(index);
        if (layer == nullptr || layer->type() != Layer::BITMAP ||
            layer == current) {
            continue;
        }
        ui->cbLineLayer->addItem(layer->name(), layer->id());
        if (layer->id() == preferredId) {
            preferredRow = ui->cbLineLayer->count() - 1;
        }
    }
    ui->cbLineLayer->setCurrentIndex(preferredRow);
    ui->cbLineLayer->blockSignals(false);
}

void GapFillDialog::gapSizeChanged(int index)
{
    ui->sbCustomSize->setEnabled(index == 3);
}

void GapFillDialog::setStatus(const QString& text)
{
    ui->labStatus->setText(text);
}

void GapFillDialog::detectGaps()
{
    LayerManager* layers = mEditor->layers();
    Layer* current = layers->currentLayer();
    if (current == nullptr || current->type() != Layer::BITMAP) { return; }
    auto* coloringLayer = static_cast<LayerBitmap*>(current);

    const int frame = mEditor->currentFrame();
    BitmapImage* coloringKeyframe = coloringLayer->getLastBitmapImageAtFrame(frame);
    if (coloringKeyframe == nullptr || coloringKeyframe->image() == nullptr) {
        setStatus(tr("当前帧没有可用的关键帧，请先在着色图层绘制。"));
        qDebug() << "[ui] gapfill detect: no keyframe at frame" << frame;
        return;
    }
    const QRect coloringBounds = coloringKeyframe->bounds();

    BitmapImage* lineKeyframe = nullptr;
    const int lineLayerId = ui->cbLineLayer->currentData().toInt();
    if (lineLayerId > 0) {
        Layer* lineLayer = layers->findLayerById(lineLayerId);
        if (lineLayer != nullptr && lineLayer->type() == Layer::BITMAP) {
            lineKeyframe = static_cast<LayerBitmap*>(lineLayer)
                    ->getLastBitmapImageAtFrame(frame);
        }
    }

    mCanvasRect = coloringBounds;
    if (lineKeyframe != nullptr && lineKeyframe->image() != nullptr) {
        mCanvasRect = mCanvasRect.united(lineKeyframe->bounds());
    }
    if (mCanvasRect.width() <= 0 || mCanvasRect.height() <= 0) {
        setStatus(tr("着色图层为空，没有可检测的内容。"));
        qDebug() << "[ui] gapfill detect: empty layer bounds";
        return;
    }
    qDebug() << "[ui] gapfill detect: frame" << frame
             << "layer" << current->name() << "lineLayerId" << lineLayerId
             << "canvas" << mCanvasRect;

    const Image coloringImage = canvasToGapImage(
                keyframeToCanvas(coloringKeyframe, mCanvasRect));
    Image lineImage;
    if (lineKeyframe != nullptr && lineKeyframe->image() != nullptr) {
        lineImage = canvasToGapImage(keyframeToCanvas(lineKeyframe, mCanvasRect));
    }

    // ---- predictor: learned model when a line layer exists and the
    // ONNX runtime + model are available, rule-based otherwise.
    std::unique_ptr<GapColorPredictor> predictor;
    QString backendError;
    if (!lineImage.empty()) {
        mBackend = std::make_unique<OrtOnnxBackend>(defaultModelPath(),
                                                    &backendError);
        if (mBackend->isValid()) {
            predictor = std::make_unique<LearnedGapPredictor>(*mBackend);
        }
    }
    const bool learned = predictor != nullptr;
    if (!learned) {
        predictor = std::make_unique<RuleBasedPredictor>();
    }
    qDebug() << "[ui] gapfill detect: learned" << learned
             << "backendError" << backendError;

    Settings settings;
    settings.scope = Scope::WholeLayer;
    settings.connectivity = ui->cbConnectivity->currentIndex() == 0
            ? Connectivity::Four : Connectivity::Eight;
    settings.confidencePreset =
            static_cast<ConfidencePreset>(ui->cbConfidence->currentIndex());
    switch (ui->cbGapSize->currentIndex()) {
        case 0: settings.gapSizePreset = GapSizePreset::Small; break;
        case 2: settings.gapSizePreset = GapSizePreset::Large; break;
        case 3:
            settings.gapSizePreset = GapSizePreset::Custom;
            settings.customGapThreshold = static_cast<std::size_t>(
                        ui->sbCustomSize->value());
            break;
        default: settings.gapSizePreset = GapSizePreset::Medium; break;
    }

    auto geometry = lineImage.empty()
            ? normalizeCanonicalColoringGeometry(coloringImage)
            : normalizeLegacyRgbaGeometry(coloringImage, &lineImage, nullptr);

    QProgressDialog progress(tr("正在检测间隙…"), tr("取消"), 0, 1000, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    std::atomic_bool cancelled{false};
    const auto detectionProgress = [&](std::size_t done, std::size_t total) {
        if (total == 0) { return; }
        progress.setValue(static_cast<int>(600LL * static_cast<long long>(done)
                                          / static_cast<long long>(total)));
        QCoreApplication::processEvents();
        if (progress.wasCanceled()) { cancelled = true; }
    };
    int pollCount = 0;
    const auto cancellationPoll = [&]() {
        progress.setValue(600 + std::min(399, pollCount * 4));
        ++pollCount;
        QCoreApplication::processEvents();
        if (progress.wasCanceled()) { cancelled = true; }
    };

    AnalysisResult analysisResult;
    try {
        SmartGapPropagation propagation;
        analysisResult = propagation.analyze(coloringImage, geometry, settings,
                                     *predictor, nullptr, &cancelled,
                                     detectionProgress, cancellationPoll,
                                     lineImage.empty() ? nullptr : &lineImage,
                                     nullptr);
    } catch (const std::exception& error) {
        progress.cancel();
        if (cancelled.load()) {
            setStatus(tr("检测已取消。"));
        } else {
            setStatus(tr("检测失败：%1").arg(QString::fromUtf8(error.what())));
        }
        return;
    }
    progress.setValue(1000);
    progress.cancel();

    mGaps = std::move(analysisResult.gaps);
    mColoringLayerId = coloringLayer->id();
    mColoringKeyPos = coloringKeyframe->pos();
    mApplied.assign(mGaps.size(), false);

    mSuppressCellChanged = true;
    ui->gapsTable->setRowCount(static_cast<int>(mGaps.size()));
    int highCount = 0;
    for (int row = 0; row < static_cast<int>(mGaps.size()); ++row) {
        const GapCandidate& gap = mGaps[static_cast<std::size_t>(row)];

        auto* applyItem = new QTableWidgetItem;
        applyItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        // The frozen model's absolute confidence rarely exceeds the
        // official 0.55/0.85 bands (0.3~0.5 is typical even when the
        // color is right), so pre-check every row that has a
        // suggestion; review happens by unchecking in the list.
        applyItem->setCheckState(gap.suggestedColor.has_value()
                                     ? Qt::Checked : Qt::Unchecked);
        ui->gapsTable->setItem(row, 0, applyItem);

        ui->gapsTable->setItem(row, 1,
                new QTableWidgetItem(QString::number(gap.id + 1)));
        ui->gapsTable->setItem(row, 2,
                new QTableWidgetItem(QString::number(gap.area)));

        if (gap.suggestedColor.has_value()) {
            const Rgba& color = *gap.suggestedColor;
            auto* colorItem = new QTableWidgetItem(
                        QStringLiteral("#%1%2%3")
                        .arg(color.r, 2, 16, QChar('0'))
                        .arg(color.g, 2, 16, QChar('0'))
                        .arg(color.b, 2, 16, QChar('0')));
            colorItem->setBackground(QColor(color.r, color.g, color.b));
            ui->gapsTable->setItem(row, 3, colorItem);
        } else {
            ui->gapsTable->setItem(row, 3, new QTableWidgetItem(tr("无建议")));
        }

        const bool isLearned = gap.predictionProvenance ==
                PredictionProvenance::Learned;
        QString confidenceText = QStringLiteral("—");
        if (isLearned && gap.learnedConfidence.has_value()) {
            confidenceText = tr("%1%")
                    .arg(std::lround(*gap.learnedConfidence * 100.0));
            if (gap.confidenceBand == ConfidenceBand::High) {
                confidenceText += tr("（高）");
                ++highCount;
            } else if (gap.confidenceBand == ConfidenceBand::Medium) {
                confidenceText += tr("（中）");
            } else {
                confidenceText += tr("（低）");
            }
        } else if (gap.heuristicScore.has_value()) {
            confidenceText = tr("规则分 %1")
                    .arg(std::lround(*gap.heuristicScore * 100.0));
        }
        ui->gapsTable->setItem(row, 4, new QTableWidgetItem(confidenceText));
        ui->gapsTable->setItem(row, 5, new QTableWidgetItem(
                isLearned ? tr("学习模型")
                          : (gap.suggestedColor.has_value()
                                 ? tr("规则") : QStringLiteral("—"))));
    }
    mSuppressCellChanged = false;

    QString status = tr("共检测到 %1 个间隙，高置信 %2 个。")
            .arg(mGaps.size()).arg(highCount);
    status += learned ? tr(" 使用学习模型预测。")
                      : tr(" 使用规则预测"
                           "（学习模型需要线稿图层、onnxruntime.dll 和 unet32.onnx）。%1")
                            .arg(backendError.isEmpty() ? QString()
                                                        : backendError + QStringLiteral(" "));
    setStatus(status);
    int preChecked = 0;
    for (int row = 0; row < ui->gapsTable->rowCount(); ++row) {
        const QTableWidgetItem* item = ui->gapsTable->item(row, 0);
        if (item != nullptr && item->checkState() == Qt::Checked) { ++preChecked; }
    }
    qDebug() << "[ui] gapfill detect done: gaps" << mGaps.size()
             << "high" << highCount << "learned" << learned
             << "preChecked" << preChecked;
}

void GapFillDialog::checkHighConfidence()
{
    mSuppressCellChanged = true;
    for (int row = 0; row < ui->gapsTable->rowCount(); ++row) {
        QTableWidgetItem* item = ui->gapsTable->item(row, 0);
        if (item == nullptr) { continue; }
        const bool high = row < static_cast<int>(mGaps.size()) &&
                mGaps[static_cast<std::size_t>(row)].confidenceBand ==
                ConfidenceBand::High &&
                mGaps[static_cast<std::size_t>(row)]
                    .predictionProvenance == PredictionProvenance::Learned;
        item->setCheckState(high && !mApplied[static_cast<std::size_t>(row)]
                                ? Qt::Checked : Qt::Unchecked);
    }
    mSuppressCellChanged = false;
}

void GapFillDialog::clearChecks()
{
    mSuppressCellChanged = true;
    for (int row = 0; row < ui->gapsTable->rowCount(); ++row) {
        if (QTableWidgetItem* item = ui->gapsTable->item(row, 0)) {
            item->setCheckState(Qt::Unchecked);
        }
    }
    mSuppressCellChanged = false;
}

void GapFillDialog::cellChanged(int, int column)
{
    if (mSuppressCellChanged || column != 0) { return; }
}

void GapFillDialog::applySelected()
{
    if (mColoringLayerId < 0 || mGaps.empty()) {
        setStatus(tr("请先检测间隙。"));
        return;
    }
    Object* object = mEditor->object();
    int layerIndex = -1;
    Layer* layer = nullptr;
    for (int index = 0; index < object->getLayerCount(); ++index) {
        Layer* candidate = object->getLayer(index);
        if (candidate != nullptr && candidate->id() == mColoringLayerId) {
            layerIndex = index;
            layer = candidate;
            break;
        }
    }
    if (layer == nullptr || layer->type() != Layer::BITMAP) {
        setStatus(tr("着色图层已不存在，请重新检测。"));
        return;
    }
    auto* coloringLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* keyframe = coloringLayer->getBitmapImageAtFrame(mColoringKeyPos);
    if (keyframe == nullptr || keyframe->image() == nullptr) {
        setStatus(tr("关键帧已不存在，请重新检测。"));
        return;
    }
    QImage* image = keyframe->image();

    int checkedRows = 0;
    for (int row = 0; row < static_cast<int>(mGaps.size()); ++row) {
        const QTableWidgetItem* item = ui->gapsTable->item(row, 0);
        if (item != nullptr && item->checkState() == Qt::Checked &&
            !mApplied[static_cast<std::size_t>(row)]) {
            ++checkedRows;
        }
    }
    if (checkedRows == 0) {
        setStatus(tr("没有勾选任何间隙：请先在列表第一列勾选要应用的行。"));
        qDebug() << "[ui] gapfill apply: no rows checked";
        return;
    }
    qDebug() << "[ui] gapfill apply: checkedRows" << checkedRows
             << "layerId" << mColoringLayerId << "keyPos" << mColoringKeyPos;

    // The new undo system snapshots the CURRENT layer + frame inside
    // createState(), so make the coloring keyframe current before
    // snapshotting, then record() after the pixels are written.
    mEditor->layers()->setCurrentLayer(layer);
    mEditor->scrubTo(mColoringKeyPos);
    const SAVESTATE_ID undoState = mEditor->undoRedo()->createState(
                UndoRedoRecordType::KEYFRAME_MODIFY);

    // Gap pixel indices are canvas coordinates; convert to the
    // keyframe image via its current top-left offset inside the canvas.
    const QPoint canvasOffset = keyframe->bounds().topLeft() - mCanvasRect.topLeft();
    const int canvasWidth = mCanvasRect.width();

    int appliedGaps = 0;
    int totalWritten = 0;
    for (int row = 0; row < static_cast<int>(mGaps.size()); ++row) {
        const auto gapIndex = static_cast<std::size_t>(row);
        if (mApplied[gapIndex]) { continue; }
        const QTableWidgetItem* item = ui->gapsTable->item(row, 0);
        if (item == nullptr || item->checkState() != Qt::Checked) { continue; }
        const GapCandidate& gap = mGaps[gapIndex];
        if (!gap.suggestedColor.has_value()) { continue; }
        const auto& pixels = candidateApplicationPixels(gap);
        if (pixels.empty()) { continue; }

        int written = 0;
        for (const std::uint32_t rawIndex : pixels) {
            const int cx = static_cast<int>(
                        rawIndex % static_cast<std::uint32_t>(canvasWidth));
            const int cy = static_cast<int>(
                        rawIndex / static_cast<std::uint32_t>(canvasWidth));
            const QPoint pos = QPoint(cx, cy) - canvasOffset;
            if (!image->rect().contains(pos)) { continue; }
            image->setPixel(pos, qRgba(gap.suggestedColor->r,
                                       gap.suggestedColor->g,
                                       gap.suggestedColor->b, 255));
            ++written;
        }
        if (written == 0) { continue; }
        mApplied[gapIndex] = true;
        ++appliedGaps;
        totalWritten += written;
    }

    if (appliedGaps == 0) {
        setStatus(tr("选中的间隙没有写入任何像素（图层内容可能已变化），请重新检测。"));
        qDebug() << "[ui] gapfill apply: zero pixels written, canvas"
                 << mCanvasRect << "offset" << canvasOffset;
        return;
    }
    keyframe->modification();
    mEditor->undoRedo()->record(undoState, tr("间隙填充"));
    mEditor->updateFrame();

    mSuppressCellChanged = true;
    for (int row = 0; row < static_cast<int>(mGaps.size()); ++row) {
        if (!mApplied[static_cast<std::size_t>(row)]) { continue; }
        if (QTableWidgetItem* item = ui->gapsTable->item(row, 0)) {
            item->setCheckState(Qt::Unchecked);
            item->setText(tr("已应用"));
            item->setFlags(Qt::ItemIsEnabled);
        }
    }
    mSuppressCellChanged = false;
    setStatus(tr("已填充 %1 个间隙（共 %2 像素，可通过撤销还原）。")
                  .arg(appliedGaps).arg(totalWritten));
    qDebug() << "[ui] gapfill apply done: gaps" << appliedGaps
             << "pixels" << totalWritten;
}
