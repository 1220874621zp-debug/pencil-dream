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
#include "colorizeoptionswidget.h"

#include <QCheckBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QGridLayout>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "editor.h"
#include "layer.h"
#include "layercolorize.h"
#include "colorizeimage.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "colorizeupdatemanager.h"

ColorizeOptionsWidget::ColorizeOptionsWidget(Editor* editor, QWidget* parent)
    : BaseWidget(parent), mEditor(editor)
{
    // 与兄弟选项部件一致：构造即建控件，updateUI 随时可能被图层切换触发
    initUI();
}

void ColorizeOptionsWidget::initUI()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    // --- 标题 + 更新（Krita: Update，纯手动刷新） ---
    auto* titleRow = new QHBoxLayout;
    auto* titleLabel = new QLabel(tr("Colorize Mask"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 0.5);
    titleLabel->setFont(titleFont);
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();

    mRefreshButton = new QPushButton(tr("Refresh"), this);
    mRefreshAllButton = new QPushButton(tr("Update All"), this);
    mRefreshButton->setToolTip(tr("Regenerate coloring for the current frame"));
    mRefreshAllButton->setToolTip(tr("Regenerate coloring for every frame of this layer"));
    titleRow->addWidget(mRefreshButton);
    titleRow->addWidget(mRefreshAllButton);
    rootLayout->addLayout(titleRow);

    // --- 显示/编辑模式（Krita: Edit key strokes / Show output） ---
    mEditKeyStrokesCheck = new QCheckBox(tr("Edit key strokes"), this);
    mShowColoringCheck = new QCheckBox(tr("Show output"), this);
    rootLayout->addWidget(mEditKeyStrokesCheck);
    rootLayout->addWidget(mShowColoringCheck);

    // --- 颜色列表（Krita: Key Strokes） ---
    mSourceLabel = new QLabel(this);
    mSourceLabel->setWordWrap(true);
    rootLayout->addWidget(mSourceLabel);

    auto* colorsLabel = new QLabel(tr("Key Strokes"), this);
    rootLayout->addWidget(colorsLabel);

    mColorsRow = new QHBoxLayout;
    mColorsRow->setSpacing(4);
    mColorsRow->addStretch();
    rootLayout->addLayout(mColorsRow);

    auto* colorButtonsRow = new QHBoxLayout;
    mTransparentButton = new QPushButton(tr("Transparent"), this);
    mRemoveButton = new QPushButton(tr("Remove"), this);
    mTransparentButton->setToolTip(tr("Mark the selected color as transparent: its stroke areas stay unfilled (use for background)"));
    mRemoveButton->setToolTip(tr("Erase all strokes of the selected color on this frame"));
    colorButtonsRow->addWidget(mTransparentButton);
    colorButtonsRow->addWidget(mRemoveButton);
    colorButtonsRow->addStretch();
    rootLayout->addLayout(colorButtonsRow);

    // --- 滤波参数（Krita 四参数） ---
    mEdgeDetectionCheck = new QCheckBox(tr("Edge detection (soft pencil lines)"), this);
    rootLayout->addWidget(mEdgeDetectionCheck);

    mEdgeSizeSpin = new QDoubleSpinBox(this);
    mEdgeSizeSpin->setRange(1.0, 50.0);
    mEdgeSizeSpin->setSingleStep(0.5);
    mEdgeSizeSpin->setDecimals(1);
    mEdgeSizeSpin->setSuffix(tr(" px"));

    mFuzzyRadiusSpin = new QDoubleSpinBox(this);
    mFuzzyRadiusSpin->setRange(0.0, 30.0);
    mFuzzyRadiusSpin->setSingleStep(0.5);
    mFuzzyRadiusSpin->setDecimals(1);
    mFuzzyRadiusSpin->setSuffix(tr(" px"));

    mCleanUpSpin = new QDoubleSpinBox(this);
    mCleanUpSpin->setRange(0.0, 100.0);
    mCleanUpSpin->setDecimals(0);
    mCleanUpSpin->setSingleStep(5);
    mCleanUpSpin->setSuffix(" %");

    // 参数行 = 标签 + 滑杆 + 输入框（输入框为数据源，滑杆双向同步）
    auto* form = new QVBoxLayout;
    form->setSpacing(4);
    auto addParamRow = [this, &form](const char* label, QSlider*& slider, QDoubleSpinBox* spin) {
        slider = new QSlider(Qt::Horizontal, this);
        // 双精度参数以 0.1 步长映射到整型滑杆
        slider->setRange(qRound(spin->minimum() * 10.0), qRound(spin->maximum() * 10.0));

        // 排版：标签独占一行（左对齐带冒号）；下一行滑杆+输入框；
        // 输入框跨两行高（右列定宽，三行统一）；滑杆列拉伸等宽
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(2);
        grid->setContentsMargins(0, 0, 0, 0);
        auto* labelWidget = new QLabel(tr(label) + QStringLiteral("："), this);
        labelWidget->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(labelWidget, 0, 0);
        grid->addWidget(slider, 1, 0);
        grid->addWidget(spin, 0, 1, 2, 1);
        grid->setColumnStretch(0, 1);
        spin->setFixedWidth(96); // 容纳「50.0 像素」+调节箭头
        spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        form->addLayout(grid);

        // 滑杆 → 输入框（输入框 valueChanged 统一驱动参数应用）
        connect(slider, &QSlider::valueChanged, this, [spin](int value) {
            if (qAbs(spin->value() - value / 10.0) >= 0.05) {
                QSignalBlocker blocker(spin);
                spin->setValue(value / 10.0);
            }
        });
        // 输入框 → 滑杆
        connect(spin, &QDoubleSpinBox::valueChanged, this, [slider](double value) {
            const int pos = qRound(value * 10.0);
            if (slider->value() != pos) {
                QSignalBlocker blocker(slider);
                slider->setValue(pos);
            }
        });
    };
    addParamRow(QT_TRANSLATE_NOOP("ColorizeOptionsWidget", "Edge size"), mEdgeSizeSlider, mEdgeSizeSpin);
    addParamRow(QT_TRANSLATE_NOOP("ColorizeOptionsWidget", "Gap closing radius"), mFuzzyRadiusSlider, mFuzzyRadiusSpin);
    addParamRow(QT_TRANSLATE_NOOP("ColorizeOptionsWidget", "Cleanup strength"), mCleanupSlider, mCleanUpSpin);
    rootLayout->addLayout(form);

    auto* hintLabel = new QLabel(tr("Paint color strokes with the brush; mark background color as transparent; press Refresh to fill."), this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: gray;");
    rootLayout->addWidget(hintLabel);

    // --- 连接 ---
    connect(mRefreshButton, &QPushButton::clicked, this, &ColorizeOptionsWidget::refreshCurrentFrame);
    connect(mRefreshAllButton, &QPushButton::clicked, this, &ColorizeOptionsWidget::refreshAllFrames);

    connect(mEditKeyStrokesCheck, &QCheckBox::toggled, this, [this](bool value) {
        LayerColorize* layer = currentColorizeLayer();
        if (layer == nullptr) { return; }
        layer->setEditKeyStrokes(value);
        repaintCanvas();
    });
    connect(mShowColoringCheck, &QCheckBox::toggled, this, [this](bool value) {
        LayerColorize* layer = currentColorizeLayer();
        if (layer == nullptr) { return; }
        layer->setShowColoring(value);
        repaintCanvas();
    });

    connect(mTransparentButton, &QPushButton::clicked, this, [this]() {
        LayerColorize* layer = currentColorizeLayer();
        if (layer == nullptr || mSelectedColor < 0) { return; }
        if (layer->hasTransparentColor() && layer->transparentColor() == static_cast<QRgb>(mSelectedColor))
            layer->clearTransparentColor();
        else
            layer->setTransparentColor(static_cast<QRgb>(mSelectedColor));
        invalidateAllFrames(layer);
        refreshColors();
        refreshCurrentFrame(); // 显式面板操作：立即重算当前帧给反馈
    });
    connect(mRemoveButton, &QPushButton::clicked, this, [this]() {
        LayerColorize* layer = currentColorizeLayer();
        if (layer == nullptr || mSelectedColor < 0) { return; }
        layer->removeStrokeColor(mEditor->currentFrame(), static_cast<QRgb>(mSelectedColor));
        mSelectedColor = -1;
        refreshColors();
        refreshCurrentFrame();
    });

    connect(mEdgeDetectionCheck, &QCheckBox::toggled, this, &ColorizeOptionsWidget::applyParams);
    connect(mEdgeSizeSpin, &QDoubleSpinBox::valueChanged, this, &ColorizeOptionsWidget::applyParams);
    connect(mFuzzyRadiusSpin, &QDoubleSpinBox::valueChanged, this, &ColorizeOptionsWidget::applyParams);
    connect(mCleanUpSpin, &QDoubleSpinBox::valueChanged, this, &ColorizeOptionsWidget::applyParams);

    // 笔画编辑后刷新颜色列表
    connect(mEditor, &Editor::frameModified, this, [this](int) {
        if (currentColorizeLayer() != nullptr)
            refreshColors();
    });

    updateUI();
}

void ColorizeOptionsWidget::updateUI()
{
    LayerColorize* layer = currentColorizeLayer();

    const QSignalBlocker b1(mEditKeyStrokesCheck);
    const QSignalBlocker b2(mShowColoringCheck);
    const QSignalBlocker b3(mEdgeDetectionCheck);
    const QSignalBlocker b4(mEdgeSizeSpin);
    const QSignalBlocker b5(mFuzzyRadiusSpin);
    const QSignalBlocker b6(mCleanUpSpin);

    if (layer == nullptr)
        return;

    // 线稿源提示（双向就近解析）
    {
        Object* obj = mEditor->object();
        const int index = obj->getIndex(layer);
        LayerBitmap* source = obj->getColorizeSourceLayer(index, mEditor->currentFrame());
        if (source != nullptr)
        {
            mSourceLabel->setText(tr("Line art source: %1").arg(source->name()));
            mSourceLabel->setStyleSheet("color: gray;");
        }
        else
        {
            mSourceLabel->setText(tr("No line art layer found! Add a bitmap layer with drawings."));
            mSourceLabel->setStyleSheet("color: #E85D5D;");
        }
    }

    mEditKeyStrokesCheck->setChecked(layer->editKeyStrokes());
    mShowColoringCheck->setChecked(layer->showColoring());
    mEdgeDetectionCheck->setChecked(layer->useEdgeDetection());
    mEdgeSizeSpin->setValue(layer->edgeDetectionSize());
    mFuzzyRadiusSpin->setValue(layer->fuzzyRadius());
    mCleanUpSpin->setValue(qRound(layer->cleanUpAmount() * 100.0));

    // 滑杆同步（updateUI 里 spin 被 blocker 屏蔽，信号链不触发，手动对齐）
    if (mEdgeSizeSlider) mEdgeSizeSlider->setValue(qRound(layer->edgeDetectionSize() * 10.0));
    if (mFuzzyRadiusSlider) mFuzzyRadiusSlider->setValue(qRound(layer->fuzzyRadius() * 10.0));
    if (mCleanupSlider) mCleanupSlider->setValue(qRound(layer->cleanUpAmount() * 100.0));

    refreshColors();
}

void ColorizeOptionsWidget::refreshColors()
{
    LayerColorize* layer = currentColorizeLayer();

    // 清空旧色块
    while (!mColorButtons.isEmpty())
    {
        QToolButton* button = mColorButtons.takeLast();
        mColorsRow->removeWidget(button);
        delete button;
    }

    if (layer == nullptr)
    {
        mSelectedColor = -1;
        return;
    }

    const QVector<QRgb> colors = layer->strokeColorsAtFrame(mEditor->currentFrame());
    qDebug() << "[填色] 颜色列表刷新 帧" << mEditor->currentFrame() << "颜色数" << colors.size();
    for (int i = 0; i < colors.size() && i < 16; ++i)
    {
        const QRgb color = colors[i];
        auto* button = new QToolButton(this);
        button->setFixedSize(QSize(22, 22));
        button->setAutoRaise(true);
        const bool isTransparent = layer->hasTransparentColor() && layer->transparentColor() == color;
        button->setToolTip(isTransparent ? tr("Transparent (stays unfilled)") : QColor(color).name());
        QString style = QString("QToolButton { background: %1; border: 1px solid #666; }").arg(QColor(color).name());
        if (isTransparent)
            style += "QToolButton { border: 2px dashed #F5A623; }";
        if (mSelectedColor < 0)
            mSelectedColor = color; // 默认选中第一个
        if (static_cast<QRgb>(mSelectedColor) == color)
            style += "QToolButton { border: 2px solid #fff; }";
        button->setStyleSheet(style);
        connect(button, &QToolButton::clicked, this, [this, color]() {
            mSelectedColor = color;
            refreshColors();
        });
        mColorsRow->insertWidget(mColorsRow->count() - 1, button); // 弹簧前插入
        mColorButtons.append(button);
    }
}

void ColorizeOptionsWidget::refreshCurrentFrame()
{
    LayerColorize* layer = currentColorizeLayer();
    if (layer == nullptr) { return; }

    qDebug() << "[填色] 点刷新 层" << (layer ? layer->name() : QString("?")) << "帧" << mEditor->currentFrame();
    if (mEditor->colorizeUpdates() != nullptr)
        mEditor->colorizeUpdates()->requestUpdate(layer, mEditor->currentFrame());
    repaintCanvas();
}

void ColorizeOptionsWidget::refreshAllFrames()
{
    LayerColorize* layer = currentColorizeLayer();
    if (layer == nullptr || mEditor->colorizeUpdates() == nullptr) { return; }

    layer->foreachKeyFrame([this, layer](KeyFrame* key) {
        mEditor->colorizeUpdates()->requestUpdate(layer, key->pos());
    });
}

void ColorizeOptionsWidget::applyParams()
{
    LayerColorize* layer = currentColorizeLayer();
    if (layer == nullptr)
        return;

    layer->setUseEdgeDetection(mEdgeDetectionCheck->isChecked());
    layer->setEdgeDetectionSize(mEdgeSizeSpin->value());
    layer->setFuzzyRadius(mFuzzyRadiusSpin->value());
    layer->setCleanUpAmount(mCleanUpSpin->value() / 100.0);

    // 检测尺寸保持常可用（参数无害，置灰曾被用户当作无法点击的故障）

    // 参数影响全部帧：标记待更新（手动刷新，不自动计算）
    invalidateAllFrames(layer);
}

void ColorizeOptionsWidget::invalidateAllFrames(LayerColorize* layer)
{
    if (layer == nullptr) { return; }
    layer->foreachKeyFrame([](KeyFrame* key) {
        if (auto* frame = static_cast<ColorizeImage*>(key))
            frame->setNeedsUpdate(true);
    });
    repaintCanvas();
}

void ColorizeOptionsWidget::repaintCanvas()
{
    if (mEditor->getScribbleArea() != nullptr)
    {
        mEditor->getScribbleArea()->invalidateCanvasCache();
    }
}

LayerColorize* ColorizeOptionsWidget::currentColorizeLayer() const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::COLORIZE)
        return nullptr;
    return static_cast<LayerColorize*>(layer);
}
