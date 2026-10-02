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
#include "shadowoptionswidget.h"

#include "editor.h"
#include "layershadow.h"
#include "shadowimage.h"
#include "object.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "colormanager.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCoreApplication>
#include <QProgressDialog>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPainter>

namespace
{

QIcon makeSwatchIcon(QRgb color, bool transparentMarked)
{
    QPixmap pm(20, 20);
    if (transparentMarked)
    {
        // 透明标记：灰底 + 红斜杠
        pm.fill(QColor(0xD0, 0xD0, 0xD0));
        QPainter p(&pm);
        p.setPen(QPen(QColor(0xCC, 0x22, 0x22), 3));
        p.drawLine(2, 17, 17, 2);
        p.end();
        // 原色小角标
        QPainter p2(&pm);
        p2.setPen(Qt::NoPen);
        p2.setBrush(QColor(color));
        p2.drawRect(0, 14, 6, 6);
        p2.end();
        QPainter p3(&pm);
        p3.setPen(QPen(QColor(0x60, 0x60, 0x60), 1));
        p3.drawRect(0, 0, 19, 19);
        p3.end();
    }
    else
    {
        pm.fill(QColor(color));
        // 1px 对比描边：黑/白按明度自动选，深色面板上黑色块也可辨
        QPainter p(&pm);
        p.setPen(QPen(qGray(color) < 128 ? QColor(0xE8, 0xE8, 0xE8) : QColor(0x20, 0x20, 0x20), 1));
        p.drawRect(0, 0, 19, 19);
        p.end();
    }
    return QIcon(pm);
}

} // namespace

ShadowOptionsWidget::ShadowOptionsWidget(Editor* editor, QWidget* parent)
    : BaseWidget(parent),
      mEditor(editor)
{
    initUI();
}

void ShadowOptionsWidget::initUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // 线稿源提示行
    mSourceLabel = new QLabel(this);
    mSourceLabel->setWordWrap(true);
    mainLayout->addWidget(mSourceLabel);

    // 方向四选一：阴影在 左/右/上/下
    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(new QLabel(tr("阴影在："), this));
    const QStringList dirNames = { tr("左"), tr("右"), tr("上"), tr("下") };
    for (int i = 0; i < dirNames.size(); ++i)
    {
        auto* btn = new QToolButton(this);
        btn->setText(dirNames.at(i));
        btn->setCheckable(true);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        connect(btn, &QToolButton::clicked, this, [this, i]() { onDirectionClicked(i); });
        mDirectionButtons.append(btn);
        dirRow->addWidget(btn);
    }
    dirRow->addStretch(1);
    mainLayout->addLayout(dirRow);

    // 阴影填充色
    auto* fillRow = new QHBoxLayout();
    fillRow->addWidget(new QLabel(tr("阴影填充色："), this));
    mFillColorButton = new QToolButton(this);
    mFillColorButton->setIconSize(QSize(20, 20));
    mFillColorButton->setToolTip(tr("点击更换阴影填充颜色"));
    connect(mFillColorButton, &QToolButton::clicked, this, &ShadowOptionsWidget::onFillColorClicked);
    fillRow->addWidget(mFillColorButton);
    fillRow->addStretch(1);
    mainLayout->addLayout(fillRow);

    // 标记色三槽
    auto* markerRow = new QHBoxLayout();
    markerRow->addWidget(new QLabel(tr("分割线标记色："), this));
    for (int i = 0; i < LayerShadow::kMarkerSlotCount; ++i)
    {
        auto* btn = new QToolButton(this);
        btn->setIconSize(QSize(20, 20));
        btn->setCheckable(true);
        btn->setToolTip(tr("选中后用画笔以该颜色画分割线"));
        connect(btn, &QToolButton::clicked, this, [this, i]() { onMarkerColorClicked(i); });
        mMarkerButtons.append(btn);
        markerRow->addWidget(btn);
    }
    markerRow->addStretch(1);
    mainLayout->addLayout(markerRow);

    auto* markerOpRow = new QHBoxLayout();
    mChangeMarkerColorButton = new QPushButton(tr("更换标记颜色"), this);
    connect(mChangeMarkerColorButton, &QPushButton::clicked, this, &ShadowOptionsWidget::onChangeMarkerColor);
    markerOpRow->addWidget(mChangeMarkerColorButton);
    mTransparentButton = new QPushButton(tr("标记透明（禁用该线）"), this);
    mTransparentButton->setCheckable(true);
    mTransparentButton->setToolTip(tr("被标记为透明的标记色，其分割线不参与阴影判定"));
    connect(mTransparentButton, &QPushButton::toggled, this, &ShadowOptionsWidget::onToggleMarkerTransparent);
    markerOpRow->addWidget(mTransparentButton);
    markerOpRow->addStretch(1);
    mainLayout->addLayout(markerOpRow);

    // 闭缝半径
    auto* gapRow = new QHBoxLayout();
    gapRow->addWidget(new QLabel(tr("闭缝半径："), this));
    mGapRadiusSpin = new QDoubleSpinBox(this);
    mGapRadiusSpin->setRange(0.0, 32.0);
    mGapRadiusSpin->setDecimals(0);
    mGapRadiusSpin->setSuffix(tr(" px"));
    connect(mGapRadiusSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &ShadowOptionsWidget::onGapRadiusChanged);
    gapRow->addWidget(mGapRadiusSpin);
    gapRow->addStretch(1);
    mainLayout->addLayout(gapRow);

    // 编辑/显示开关
    mEditLinesCheck = new QCheckBox(tr("编辑分割线"), this);
    connect(mEditLinesCheck, &QCheckBox::toggled, this, &ShadowOptionsWidget::onEditLinesChanged);
    mShowFillCheck = new QCheckBox(tr("显示阴影"), this);
    connect(mShowFillCheck, &QCheckBox::toggled, this, &ShadowOptionsWidget::onShowFillChanged);
    auto* toggleRow = new QHBoxLayout();
    toggleRow->addWidget(mEditLinesCheck);
    toggleRow->addWidget(mShowFillCheck);
    toggleRow->addStretch(1);
    mainLayout->addLayout(toggleRow);

    // 生成按钮
    auto* genRow = new QHBoxLayout();
    mGenerateButton = new QPushButton(tr("生成当前帧阴影"), this);
    connect(mGenerateButton, &QPushButton::clicked, this, &ShadowOptionsWidget::generateCurrentFrame);
    mGenerateAllButton = new QPushButton(tr("全帧生成（跨帧传播）"), this);
    connect(mGenerateAllButton, &QPushButton::clicked, this, &ShadowOptionsWidget::generateAllFrames);
    genRow->addWidget(mGenerateButton);
    genRow->addWidget(mGenerateAllButton);
    mainLayout->addLayout(genRow);

    // 警告行（未切开分割线/无法判定的图形）——静默无反应是禁忌
    mWarningLabel = new QLabel(this);
    mWarningLabel->setWordWrap(true);
    mWarningLabel->setStyleSheet("color: #B8860B;");
    mWarningLabel->hide();
    mainLayout->addWidget(mWarningLabel);

    mainLayout->addStretch(1);

    // 帧切换/帧编辑时刷新警告行与线稿源提示
    connect(mEditor, &Editor::scrubbed, this, [this](int) {
        refreshSourceLabel();
        refreshWarnings();
    });
    connect(mEditor, &Editor::frameModified, this, [this](int) {
        refreshSourceLabel();
        refreshWarnings();
    });
}

void ShadowOptionsWidget::updateUI()
{
    if (mUpdatingUI)
        return;
    LayerShadow* layer = currentShadowLayer();
    setEnabled(layer != nullptr);
    if (layer == nullptr)
        return;

    mUpdatingUI = true;

    for (int i = 0; i < mDirectionButtons.size(); ++i)
        mDirectionButtons.at(i)->setChecked(layer->direction() == static_cast<ShadowFill::Direction>(i));

    mFillColorButton->setIcon(makeSwatchIcon(layer->fillColor(), false));

    refreshMarkerSwatches();
    mSelectedMarkerSlot = qBound(0, mSelectedMarkerSlot, mMarkerButtons.size() - 1);
    mTransparentButton->setChecked(layer->markerTransparent(mSelectedMarkerSlot));

    mGapRadiusSpin->setValue(layer->gapRadius());
    mEditLinesCheck->setChecked(layer->editLines());
    mShowFillCheck->setChecked(layer->showFill());

    refreshSourceLabel();
    refreshWarnings();

    mUpdatingUI = false;
}

LayerShadow* ShadowOptionsWidget::currentShadowLayer() const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::SHADOW)
        return nullptr;
    return static_cast<LayerShadow*>(layer);
}

void ShadowOptionsWidget::refreshSourceLabel()
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;
    LayerBitmap* source = mEditor->object()->getColorizeSourceLayer(mEditor->layers()->currentLayerIndex(),
                                                                    mEditor->currentFrame());
    if (source != nullptr)
        mSourceLabel->setText(tr("线稿源：%1").arg(source->name()));
    else
        mSourceLabel->setText(tr("⚠ 未找到线稿源图层（需要就近的非空位图图层）"));
}

void ShadowOptionsWidget::refreshWarnings()
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;
    ShadowImage* frame = layer->getShadowImageAtFrame(mEditor->currentFrame());
    if (frame == nullptr)
    {
        mWarningLabel->hide();
        return;
    }

    const QVector<ShadowFill::Warning> warnings = frame->warnings();
    if (warnings.isEmpty())
    {
        mWarningLabel->hide();
        return;
    }

    int unclosed = 0, unresolved = 0;
    for (const ShadowFill::Warning& wv : warnings)
    {
        if (wv.kind == ShadowFill::Warning::UnclosedDivider)
            ++unclosed;
        else
            ++unresolved;
    }
    QStringList parts;
    if (unclosed > 0)
        parts << tr("%1 个图形的分割线未完全切开（线尾需搭到线稿，或调大闭缝半径）").arg(unclosed);
    if (unresolved > 0)
        parts << tr("%1 个图形按所选方向无法判定（已取默认侧填充，建议核对）").arg(unresolved);
    mWarningLabel->setText(parts.join("；"));
    mWarningLabel->show();
}

void ShadowOptionsWidget::refreshMarkerSwatches()
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;
    for (int i = 0; i < mMarkerButtons.size(); ++i)
    {
        mMarkerButtons.at(i)->setIcon(makeSwatchIcon(layer->markerColor(i), layer->markerTransparent(i)));
        mMarkerButtons.at(i)->setChecked(i == mSelectedMarkerSlot);
    }
}

void ShadowOptionsWidget::onDirectionClicked(int id)
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr || mUpdatingUI)
        return;
    layer->setDirection(static_cast<ShadowFill::Direction>(id));
    for (int i = 0; i < mDirectionButtons.size(); ++i)
        mDirectionButtons.at(i)->setChecked(i == id);
    invalidateAllFrames(layer);
    repaintCanvas();
}

void ShadowOptionsWidget::onFillColorClicked()
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;
    const QColor initial = QColor::fromRgb(layer->fillColor());
    const QColor picked = QColorDialog::getColor(initial, this, tr("阴影填充色"));
    if (!picked.isValid())
        return;
    layer->setFillColor(picked.rgba());
    mFillColorButton->setIcon(makeSwatchIcon(layer->fillColor(), false));
    invalidateAllFrames(layer);
    repaintCanvas();
}

void ShadowOptionsWidget::onMarkerColorClicked(int slot)
{
    mSelectedMarkerSlot = slot;
    refreshMarkerSwatches();
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;
    mTransparentButton->setChecked(layer->markerTransparent(slot));
    // 选中标记色 = 当前笔刷色（Krita 语义：点色块换笔）
    mEditor->color()->setFrontColor(QColor::fromRgb(layer->markerColor(slot)));
}

void ShadowOptionsWidget::onChangeMarkerColor()
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;
    const QColor initial = QColor::fromRgb(layer->markerColor(mSelectedMarkerSlot));
    const QColor picked = QColorDialog::getColor(initial, this, tr("分割线标记色"));
    if (!picked.isValid())
        return;
    layer->setMarkerColor(mSelectedMarkerSlot, picked.rgba());
    refreshMarkerSwatches();
    mEditor->color()->setFrontColor(picked);
    invalidateAllFrames(layer);
    repaintCanvas();
}

void ShadowOptionsWidget::onToggleMarkerTransparent(bool checked)
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr || mUpdatingUI)
        return;
    layer->setMarkerTransparent(mSelectedMarkerSlot, checked);
    refreshMarkerSwatches();
    invalidateAllFrames(layer);
    repaintCanvas();
}

void ShadowOptionsWidget::onGapRadiusChanged(double value)
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr || mUpdatingUI)
        return;
    layer->setGapRadius(value);
    invalidateAllFrames(layer);
    repaintCanvas();
}

void ShadowOptionsWidget::onEditLinesChanged(bool checked)
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr || mUpdatingUI)
        return;
    layer->setEditLines(checked);
    repaintCanvas();
}

void ShadowOptionsWidget::onShowFillChanged(bool checked)
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr || mUpdatingUI)
        return;
    layer->setShowFill(checked);
    repaintCanvas();
}

void ShadowOptionsWidget::generateCurrentFrame()
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;
    const int frame = mEditor->currentFrame();
    const quint32 gen = mEditor->object()->layerStructureGeneration();
    layer->updateShadowAtFrame(frame,
                               mEditor->object()->getColorizeSourceLayer(mEditor->layers()->currentLayerIndex(), frame),
                               gen);
    refreshWarnings();
    repaintCanvas();
    emit mEditor->updateTimeLine();
}

void ShadowOptionsWidget::generateAllFrames()
{
    LayerShadow* layer = currentShadowLayer();
    if (layer == nullptr)
        return;

    QVector<int> positions;
    layer->foreachKeyFrame([&](KeyFrame* key) { positions.append(key->pos()); });
    if (positions.isEmpty())
        return;

    QProgressDialog progress(tr("正在逐帧计算阴影…"), tr("取消"), 0, positions.size(), this);
    progress.setWindowTitle(tr("全帧生成阴影"));
    progress.setWindowModality(Qt::WindowModal);
    const quint32 gen = mEditor->object()->layerStructureGeneration();
    for (int i = 0; i < positions.size(); ++i)
    {
        progress.setValue(i);
        QCoreApplication::processEvents(); // 同步计算循环中保持进度框响应
        if (progress.wasCanceled())
            break;
        const int frame = positions.at(i);
        layer->updateShadowAtFrame(frame,
                                   mEditor->object()->getColorizeSourceLayer(mEditor->layers()->currentLayerIndex(), frame),
                                   gen);
    }
    progress.setValue(positions.size());

    refreshWarnings();
    repaintCanvas();
    emit mEditor->updateTimeLine();
}

void ShadowOptionsWidget::invalidateAllFrames(LayerShadow* layer)
{
    layer->foreachKeyFrame([](KeyFrame* key)
    {
        if (auto* frame = static_cast<ShadowImage*>(key))
            frame->setNeedsUpdate(true);
    });
}

void ShadowOptionsWidget::repaintCanvas()
{
    mEditor->getScribbleArea()->invalidateCanvasCache();
}
