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
#include "brushoptionswidget.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "brushtool.h"
#include "erasertool.h"
#include "smudgetool.h"
#include "editor.h"
#include "managers/toolmanager.h"
#include "pencildef.h"
#include "spinslider.h"
#include "stroketool.h"

using BrushSettingsPM = BrushSettings::PaintingMode;
using BrushSettingsBM = BrushSettings::BlendMode;

BrushOptionsWidget::BrushOptionsWidget(Editor* editor, QWidget* parent)
    : BaseWidget(parent),
      mEditor(editor)
{
    // 控件必须在构造期建好：点工具切换会立刻触发 updateUI()
    initUI();
}

BrushOptionsWidget::~BrushOptionsWidget()
{
}

void BrushOptionsWidget::initUI()
{
    QWidget* content = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    const auto makeSlider = [this](const QString& label, qreal min, qreal max,
                                   SpinSlider::GROWTH_TYPE growth = SpinSlider::LINEAR) {
        SpinSlider* slider = new SpinSlider(this);
        slider->init(label, growth, min, max);
        return slider;
    };

    // ---- 笔尖 ----
    QGroupBox* tipGroup = new QGroupBox(tr("笔尖"), this);
    QVBoxLayout* tipLayout = new QVBoxLayout(tipGroup);
    tipLayout->setSpacing(2);
    mSizeSlider = makeSlider(tr("大小"), 1.0, 200.0, SpinSlider::EXPONENT);
    mOpacitySlider = makeSlider(tr("不透明度 %"), 1.0, 100.0);
    mFlowSlider = makeSlider(tr("流量 %"), 1.0, 100.0);
    mHardnessSlider = makeSlider(tr("硬度 %"), 1.0, 100.0);
    mRatioSlider = makeSlider(tr("纵横比 %"), 5.0, 100.0);
    mAngleSlider = makeSlider(tr("角度 °"), 0.0, 360.0);
    mTipImageRow = new QWidget(this);
    QHBoxLayout* tipImageRow = new QHBoxLayout(mTipImageRow);
    tipImageRow->setContentsMargins(0, 0, 0, 0);
    mTipImageButton = new QPushButton(tr("导入笔尖图…"), this);
    mTipImageClearButton = new QPushButton(tr("清除"), this);
    tipImageRow->addWidget(mTipImageButton, 1);
    tipImageRow->addWidget(mTipImageClearButton);
    tipLayout->addWidget(mSizeSlider);
    tipLayout->addWidget(mOpacitySlider);
    tipLayout->addWidget(mFlowSlider);
    tipLayout->addWidget(mHardnessSlider);
    tipLayout->addWidget(mRatioSlider);
    tipLayout->addWidget(mAngleSlider);
    tipLayout->addWidget(mTipImageRow);
    layout->addWidget(tipGroup);

    // ---- 描边 ----
    QGroupBox* strokeGroup = new QGroupBox(tr("描边"), this);
    QVBoxLayout* strokeLayout = new QVBoxLayout(strokeGroup);
    strokeLayout->setSpacing(2);
    mAutoSpacingBox = new QCheckBox(tr("自动间距"), this);
    mSpacingCoeffSlider = makeSlider(tr("自动间距系数"), 0.25, 5.0);
    mSpacingSlider = makeSlider(tr("固定间距 %（直径比例）"), 2.0, 500.0);
    strokeLayout->addWidget(mAutoSpacingBox);
    strokeLayout->addWidget(mSpacingCoeffSlider);
    strokeLayout->addWidget(mSpacingSlider);
    layout->addWidget(strokeGroup);

    // ---- 动态 ----
    QGroupBox* dynGroup = new QGroupBox(tr("动态"), this);
    QVBoxLayout* dynLayout = new QVBoxLayout(dynGroup);
    dynLayout->setSpacing(2);
    mPressureBox = new QCheckBox(tr("压感"), this);
    QHBoxLayout* stabilRow = new QHBoxLayout();
    stabilRow->addWidget(new QLabel(tr("防抖"), this));
    mStabilizerCombo = new QComboBox(this);
    mStabilizerCombo->addItem(tr("关"));
    mStabilizerCombo->addItem(tr("弱"));
    mStabilizerCombo->addItem(tr("中"));
    mStabilizerCombo->addItem(tr("强"));
    stabilRow->addWidget(mStabilizerCombo, 1);
    dynLayout->addWidget(mPressureBox);
    dynLayout->addLayout(stabilRow);
    layout->addWidget(dynGroup);

    // ---- 高级 ----
    QGroupBox* advGroup = new QGroupBox(tr("高级"), this);
    QVBoxLayout* advLayout = new QVBoxLayout(advGroup);
    advLayout->setSpacing(2);
    mScatterSlider = makeSlider(tr("散布 %（直径比例）"), 0.0, 500.0);
    mAirbrushBox = new QCheckBox(tr("喷枪"), this);
    mAirbrushRateSlider = makeSlider(tr("喷枪速率 /秒"), 1.0, 100.0);
    mPaintingModeRow = new QWidget(this);
    QHBoxLayout* paintModeRow = new QHBoxLayout(mPaintingModeRow);
    paintModeRow->setContentsMargins(0, 0, 0, 0);
    paintModeRow->addWidget(new QLabel(tr("涂料模式"), this));
    mPaintingModeCombo = new QComboBox(this);
    mPaintingModeCombo->addItem(tr("涂抹（同笔不变深）"));
    mPaintingModeCombo->addItem(tr("叠加（越描越深）"));
    paintModeRow->addWidget(mPaintingModeCombo, 1);
    mBlendModeGroup = new QGroupBox(tr("笔尖混合"), this);
    QHBoxLayout* blendRow = new QHBoxLayout(mBlendModeGroup);
    mBlendModeCombo = new QComboBox(this);
    mBlendModeCombo->addItem(tr("正常"));
    mBlendModeCombo->addItem(tr("正片叠底"));
    mBlendModeCombo->addItem(tr("滤色"));
    blendRow->addWidget(mBlendModeCombo);
    mMirrorRow = new QWidget(this);
    QHBoxLayout* mirrorRow = new QHBoxLayout(mMirrorRow);
    mirrorRow->setContentsMargins(0, 0, 0, 0);
    QLabel* mirrorLabel = new QLabel(tr("镜像"), this);
    mMirrorXBox = new QCheckBox(tr("水平"), this);
    mMirrorYBox = new QCheckBox(tr("垂直"), this);
    mirrorRow->addWidget(mirrorLabel);
    mirrorRow->addWidget(mMirrorXBox);
    mirrorRow->addWidget(mMirrorYBox);
    mirrorRow->addStretch();
    advLayout->addWidget(mScatterSlider);
    advLayout->addWidget(mAirbrushBox);
    advLayout->addWidget(mAirbrushRateSlider);
    advLayout->addWidget(mPaintingModeRow);
    advLayout->addWidget(mBlendModeGroup);
    advLayout->addWidget(mMirrorRow);
    layout->addWidget(advGroup);

    // ---- 双笔尖（Krita MaskingBrush：副笔尖沿同轨迹做 alpha 复合）----
    mMaskGroup = new QGroupBox(tr("双笔尖"), this);
    QVBoxLayout* maskLayout = new QVBoxLayout(mMaskGroup);
    maskLayout->setSpacing(2);
    mMaskBox = new QCheckBox(tr("启用（副笔尖做浓淡/枯笔）"), this);
    QWidget* maskModeRow = new QWidget(this);
    QHBoxLayout* maskModeLayout = new QHBoxLayout(maskModeRow);
    maskModeLayout->setContentsMargins(0, 0, 0, 0);
    maskModeLayout->addWidget(new QLabel(tr("复合模式"), this));
    mMaskModeCombo = new QComboBox(this);
    mMaskModeCombo->addItem(tr("加深（burn）"));
    mMaskModeCombo->addItem(tr("正片叠底"));
    mMaskModeCombo->addItem(tr("变暗"));
    mMaskModeCombo->addItem(tr("叠加"));
    mMaskModeCombo->addItem(tr("颜色减淡"));
    mMaskModeCombo->addItem(tr("线性减淡"));
    mMaskModeCombo->addItem(tr("线性加深"));
    mMaskModeCombo->addItem(tr("硬混合"));
    mMaskModeCombo->addItem(tr("硬混合(柔和)"));
    mMaskModeCombo->addItem(tr("减去"));
    maskModeLayout->addWidget(mMaskModeCombo, 1);
    mMaskCoeffSlider = makeSlider(tr("副笔尖尺寸系数"), 5.0, 300.0);
    QWidget* maskTipRow = new QWidget(this);
    QHBoxLayout* maskTipLayout = new QHBoxLayout(maskTipRow);
    maskTipLayout->setContentsMargins(0, 0, 0, 0);
    mMaskTipButton = new QPushButton(tr("导入副笔尖图…"), this);
    mMaskTipClearButton = new QPushButton(tr("清除"), this);
    maskTipLayout->addWidget(mMaskTipButton, 1);
    maskTipLayout->addWidget(mMaskTipClearButton);
    maskLayout->addWidget(mMaskBox);
    maskLayout->addWidget(maskModeRow);
    maskLayout->addWidget(mMaskCoeffSlider);
    maskLayout->addWidget(maskTipRow);
    layout->addWidget(mMaskGroup);

    // ---- 纹理（Krita KisTextureOption：图案按画布位置调制浓淡）----
    mTextureGroup = new QGroupBox(tr("纹理"), this);
    QVBoxLayout* textureLayout = new QVBoxLayout(mTextureGroup);
    textureLayout->setSpacing(2);
    mTextureBox = new QCheckBox(tr("启用（纸纹/杂点）"), this);
    QWidget* textureModeRow = new QWidget(this);
    QHBoxLayout* textureModeLayout = new QHBoxLayout(textureModeRow);
    textureModeLayout->setContentsMargins(0, 0, 0, 0);
    textureModeLayout->addWidget(new QLabel(tr("模式"), this));
    mTextureModeCombo = new QComboBox(this);
    mTextureModeCombo->addItem(tr("正片叠底"));
    mTextureModeCombo->addItem(tr("减去"));
    mTextureModeCombo->addItem(tr("变暗"));
    mTextureModeCombo->addItem(tr("叠加"));
    mTextureModeCombo->addItem(tr("颜色减淡"));
    mTextureModeCombo->addItem(tr("颜色加深"));
    mTextureModeCombo->addItem(tr("线性减淡"));
    mTextureModeCombo->addItem(tr("线性加深"));
    mTextureModeCombo->addItem(tr("硬混合"));
    mTextureModeCombo->addItem(tr("硬混合(柔和)"));
    mTextureModeCombo->addItem(tr("高度"));
    mTextureModeCombo->addItem(tr("线性高度"));
    mTextureModeCombo->addItem(tr("高度(PS)"));
    mTextureModeCombo->addItem(tr("线性高度(PS)"));
    textureModeLayout->addWidget(mTextureModeCombo, 1);
    mTextureStrengthSlider = makeSlider(tr("强度 %"), 1.0, 100.0);
    mTextureButton = new QPushButton(tr("导入纹理图…"), this);
    mPatternColorBox = new QCheckBox(tr("用图案上色（颜色=纹理图）"), this);
    textureLayout->addWidget(mTextureBox);
    textureLayout->addWidget(textureModeRow);
    textureLayout->addWidget(mTextureStrengthSlider);
    textureLayout->addWidget(mTextureButton);
    textureLayout->addWidget(mPatternColorBox);
    layout->addWidget(mTextureGroup);

    layout->addStretch();
    // 无内层滚动区：宿主 ToolOptionWidget 的外层滚动区（widgetResizable）
    // 已负责滚动。内层 QScrollArea 的 sizeHint 是有界的（702px 内容只报
    // 288），外层布局按 288 结算后把面板剩余高度留给尾部弹簧，形成
    // "内层出滚动条 + 下方死空间"的假被限制状态
    QVBoxLayout* top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(content);

    // ---- 信号：任何改动 → 整套参数回写工具 ----
    const auto sliders = { mSizeSlider, mOpacitySlider, mFlowSlider, mHardnessSlider,
                           mRatioSlider, mAngleSlider, mSpacingCoeffSlider, mSpacingSlider,
                           mScatterSlider, mAirbrushRateSlider, mMaskCoeffSlider,
                           mTextureStrengthSlider };
    for (SpinSlider* slider : sliders) {
        connect(slider, &SpinSlider::valueChanged, this, &BrushOptionsWidget::applyFromWidgets);
    }
    const auto boxes = { mAutoSpacingBox, mAirbrushBox, mMirrorXBox, mMirrorYBox,
                         mMaskBox, mTextureBox, mPatternColorBox };
    for (QCheckBox* box : boxes) {
        connect(box, &QCheckBox::clicked, this, [this]() { applyFromWidgets(); });
    }
    connect(mPaintingModeCombo, &QComboBox::activated, this, [this]() { applyFromWidgets(); });
    connect(mBlendModeCombo, &QComboBox::activated, this, [this]() { applyFromWidgets(); });
    connect(mMaskModeCombo, &QComboBox::activated, this, [this]() { applyFromWidgets(); });
    connect(mTextureModeCombo, &QComboBox::activated, this, [this]() { applyFromWidgets(); });
    connect(mTipImageButton, &QPushButton::clicked, this, [this]() { importTipImage(false); });
    connect(mTipImageClearButton, &QPushButton::clicked, this, [this]() { clearTipImage(false); });
    connect(mMaskTipButton, &QPushButton::clicked, this, [this]() { importTipImage(true); });
    connect(mMaskTipClearButton, &QPushButton::clicked, this, [this]() { clearTipImage(true); });
    connect(mTextureButton, &QPushButton::clicked, this, [this]() { importTexture(); });
    connect(mPressureBox, &QCheckBox::clicked, this, [this](bool on) {
        if (mApplying) return;
        if (StrokeTool* tool = dynamic_cast<StrokeTool*>(currentPresetCapableTool())) {
            tool->setPressureEnabled(on);
        }
    });
    connect(mStabilizerCombo, &QComboBox::activated, this, [this](int index) {
        if (mApplying) return;
        if (StrokeTool* tool = dynamic_cast<StrokeTool*>(currentPresetCapableTool())) {
            tool->setStablizationLevel(index);
        }
    });
}

void BrushOptionsWidget::updateUI()
{
    if (!mSizeSlider) {
        return; // 构造早期信号（initUI 未跑完）
    }
    BaseTool* baseTool = currentPresetCapableTool();
    if (!baseTool) {
        return;
    }
    BrushTool* brushTool = dynamic_cast<BrushTool*>(baseTool);
    EraserTool* eraserTool = dynamic_cast<EraserTool*>(baseTool);
    SmudgeTool* smudgeTool = dynamic_cast<SmudgeTool*>(baseTool);
    if (!brushTool && !eraserTool && !smudgeTool) {
        return;
    }
    StrokeTool* strokeTool = static_cast<StrokeTool*>(baseTool);
    const BrushSettings s = brushTool ? brushTool->currentBrushSettings()
                          : eraserTool ? eraserTool->currentBrushSettings()
                          : smudgeTool->currentBrushSettings();

    mApplying = true;
    mSizeSlider->setValue(s.diameter);
    mOpacitySlider->setValue(s.opacity * 100.0);
    mFlowSlider->setValue(s.flow * 100.0);
    mHardnessSlider->setValue(s.hardness * 100.0);
    mRatioSlider->setValue(s.ratio * 100.0);
    mAngleSlider->setValue(s.angle);

    mAutoSpacingBox->setChecked(s.spacingMode == BrushSettings::SpacingMode::Auto);
    mSpacingCoeffSlider->setValue(s.autoSpacingCoeff);
    mSpacingSlider->setValue(s.spacing * 100.0);
    mSpacingCoeffSlider->setEnabled(s.spacingMode == BrushSettings::SpacingMode::Auto);
    mSpacingSlider->setEnabled(s.spacingMode != BrushSettings::SpacingMode::Auto);

    mPressureBox->setChecked(strokeTool->strokeToolProperties().pressureEnabled());
    mStabilizerCombo->setCurrentIndex(strokeTool->strokeToolProperties().stabilizerLevel());

    mScatterSlider->setValue(s.scatter * 100.0);
    mAirbrushBox->setChecked(s.airbrushEnabled);
    mAirbrushRateSlider->setValue(s.airbrushRate);
    mAirbrushRateSlider->setEnabled(s.airbrushEnabled);
    mPaintingModeCombo->setCurrentIndex(s.paintingMode == BrushSettingsPM::Buildup ? 1 : 0);
    mBlendModeCombo->setCurrentIndex(static_cast<int>(s.blendMode));
    // 擦除/混合不吃颜色混合；混合笔刷另隐藏涂料模式/散布/镜像（走采样公式）
    const bool isSmudge = smudgeTool != nullptr;
    const bool isBrush = brushTool != nullptr;
    mBlendModeGroup->setVisible(isBrush);
    mPaintingModeRow->setVisible(!isSmudge);
    mScatterSlider->setVisible(!isSmudge);
    mMirrorRow->setVisible(!isSmudge);
    mFlowSlider->setLabel(isSmudge ? tr("混合速率 %") : tr("流量 %"));
    mMirrorXBox->setChecked(s.mirrorX);
    mMirrorYBox->setChecked(s.mirrorY);

    // 图像笔尖 / 双笔尖 / 纹理（仅画笔/橡皮；双笔尖与纹理只挂画笔）
    mTipImageRow->setVisible(!isSmudge);
    mHardnessSlider->setEnabled(s.tipShape != BrushSettings::TipShape::Image);
    mMaskGroup->setVisible(isBrush);
    mTextureGroup->setVisible(isBrush);
    mMaskBox->setChecked(s.mask.enabled);
    mMaskModeCombo->setCurrentIndex(maskModeToCombo(s.mask.mode));
    mMaskCoeffSlider->setValue(qBound(5.0, s.mask.sizeCoeff * 100.0, 300.0));
    const bool maskOn = s.mask.enabled && s.mask.sub != nullptr;
    mMaskModeCombo->setEnabled(maskOn);
    mMaskCoeffSlider->setEnabled(maskOn);
    mMaskTipButton->setEnabled(maskOn);
    mMaskTipClearButton->setEnabled(maskOn && s.mask.sub
                                    && s.mask.sub->tipShape == BrushSettings::TipShape::Image);
    mTextureBox->setChecked(s.texture.enabled);
    mTextureModeCombo->setCurrentIndex(textureModeToCombo(s.texture.mode));
    mTextureStrengthSlider->setValue(s.texture.strength * 100.0);
    const bool texOn = s.texture.enabled && !s.texture.pattern.isNull();
    mTextureModeCombo->setEnabled(texOn);
    mTextureStrengthSlider->setEnabled(texOn);
    mTextureButton->setEnabled(s.texture.enabled);
    mPatternColorBox->setEnabled(texOn);
    mPatternColorBox->setChecked(s.colorSource == BrushSettings::ColorSource::Pattern);
    mApplying = false;
}

BaseTool* BrushOptionsWidget::currentPresetCapableTool() const
{
    if (!mEditor) {
        return nullptr;
    }
    BaseTool* tool = mEditor->tools()->currentTool();
    if (tool && (tool->type() == BRUSH || tool->type() == ERASER || tool->type() == SMUDGE)) {
        return tool;
    }
    return mEditor->tools()->getTool(BRUSH);
}

void BrushOptionsWidget::applyFromWidgets()
{
    if (mApplying) {
        return;
    }
    BaseTool* baseTool = currentPresetCapableTool();
    if (!baseTool) {
        return;
    }
    BrushSettings s;
    if (BrushTool* tool = dynamic_cast<BrushTool*>(baseTool)) {
        s = tool->currentBrushSettings();
    } else if (EraserTool* tool = dynamic_cast<EraserTool*>(baseTool)) {
        s = tool->currentBrushSettings();
    } else if (SmudgeTool* tool = dynamic_cast<SmudgeTool*>(baseTool)) {
        s = tool->currentBrushSettings();
    } else {
        return;
    }

    s.diameter = qBound(1.0, mSizeSlider->value(), 200.0);
    s.opacity = qBound(0.01, mOpacitySlider->value() / 100.0, 1.0);
    s.flow = qBound(0.01, mFlowSlider->value() / 100.0, 1.0);
    s.hardness = qBound(0.01, mHardnessSlider->value() / 100.0, 1.0);
    s.ratio = qBound(0.05, mRatioSlider->value() / 100.0, 1.0);
    s.angle = qBound(0.0, mAngleSlider->value(), 360.0);

    s.spacingMode = mAutoSpacingBox->isChecked()
                    ? BrushSettings::SpacingMode::Auto
                    : BrushSettings::SpacingMode::Fixed;
    s.autoSpacingCoeff = qBound(0.25, mSpacingCoeffSlider->value(), 5.0);
    s.spacing = qBound(0.02, mSpacingSlider->value() / 100.0, 5.0);

    s.scatter = qBound(0.0, mScatterSlider->value() / 100.0, 5.0);
    s.airbrushEnabled = mAirbrushBox->isChecked();
    s.airbrushRate = qBound(1, qRound(mAirbrushRateSlider->value()), 100);
    s.paintingMode = mPaintingModeCombo->currentIndex() == 1
                     ? BrushSettingsPM::Buildup : BrushSettingsPM::Wash;
    s.blendMode = static_cast<BrushSettingsBM>(mBlendModeCombo->currentIndex());
    s.mirrorX = mMirrorXBox->isChecked();
    s.mirrorY = mMirrorYBox->isChecked();

    // 双笔尖 / 纹理 / 图案颜色源（画笔专属）
    if (dynamic_cast<BrushTool*>(baseTool) != nullptr) {
        s.mask.enabled = mMaskBox->isChecked() && s.mask.sub != nullptr;
        s.mask.mode = comboToMaskMode(mMaskModeCombo->currentIndex());
        s.mask.sizeCoeff = qBound(0.05, mMaskCoeffSlider->value() / 100.0, 3.0);
        s.texture.enabled = mTextureBox->isChecked() && !s.texture.pattern.isNull();
        s.texture.mode = comboToTextureMode(mTextureModeCombo->currentIndex());
        s.texture.strength = qBound(0.01, mTextureStrengthSlider->value() / 100.0, 1.0);
        s.colorSource = (mPatternColorBox->isChecked() && !s.texture.pattern.isNull())
                        ? BrushSettings::ColorSource::Pattern
                        : BrushSettings::ColorSource::Plain;
    }

    if (BrushTool* tool = dynamic_cast<BrushTool*>(baseTool)) {
        tool->applyBrushOptions(s);
    } else if (EraserTool* tool = dynamic_cast<EraserTool*>(baseTool)) {
        tool->applyBrushOptions(s);
    } else if (SmudgeTool* tool = dynamic_cast<SmudgeTool*>(baseTool)) {
        tool->applyBrushOptions(s);
    }
    mAirbrushRateSlider->setEnabled(s.airbrushEnabled);
    mSpacingCoeffSlider->setEnabled(s.spacingMode == BrushSettings::SpacingMode::Auto);
    mSpacingSlider->setEnabled(s.spacingMode != BrushSettings::SpacingMode::Auto);
}

int BrushOptionsWidget::maskModeToCombo(BrushMaskSettings::Mode mode)
{
    switch (mode) {
    case BrushMaskSettings::Mode::Mult: return 1;
    case BrushMaskSettings::Mode::Darken: return 2;
    case BrushMaskSettings::Mode::Overlay: return 3;
    case BrushMaskSettings::Mode::Dodge: return 4;
    case BrushMaskSettings::Mode::LinearDodge: return 5;
    case BrushMaskSettings::Mode::LinearBurn: return 6;
    case BrushMaskSettings::Mode::HardMix: return 7;
    case BrushMaskSettings::Mode::HardMixSofter: return 8;
    case BrushMaskSettings::Mode::Subtract: return 9;
    case BrushMaskSettings::Mode::Burn:
    default: return 0;
    }
}

BrushMaskSettings::Mode BrushOptionsWidget::comboToMaskMode(int index)
{
    switch (index) {
    case 1: return BrushMaskSettings::Mode::Mult;
    case 2: return BrushMaskSettings::Mode::Darken;
    case 3: return BrushMaskSettings::Mode::Overlay;
    case 4: return BrushMaskSettings::Mode::Dodge;
    case 5: return BrushMaskSettings::Mode::LinearDodge;
    case 6: return BrushMaskSettings::Mode::LinearBurn;
    case 7: return BrushMaskSettings::Mode::HardMix;
    case 8: return BrushMaskSettings::Mode::HardMixSofter;
    case 9: return BrushMaskSettings::Mode::Subtract;
    default: return BrushMaskSettings::Mode::Burn;
    }
}

int BrushOptionsWidget::textureModeToCombo(int kritaMode)
{
    // combo 顺序 ↔ Krita TexturingMode 编号
    static const int order[] = { 0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    for (int i = 0; i < int(sizeof(order) / sizeof(order[0])); ++i) {
        if (order[i] == kritaMode) {
            return i;
        }
    }
    return 9; // 默认硬混合(柔和) = 11 号
}

int BrushOptionsWidget::comboToTextureMode(int index)
{
    static const int order[] = { 0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    if (index < 0 || index >= int(sizeof(order) / sizeof(order[0]))) {
        return 11;
    }
    return order[index];
}

void BrushOptionsWidget::importTipImage(bool subTip)
{
    const QString file = QFileDialog::getOpenFileName(
        this, subTip ? tr("选择副笔尖图") : tr("选择笔尖图"),
        QString(), tr("图像 (*.png *.jpg *.jpeg *.bmp);;%1").arg(tr("所有文件 (*.*)")));
    if (file.isEmpty()) {
        return;
    }
    QImage image(file);
    if (image.isNull()) {
        return;
    }
    BaseTool* baseTool = currentPresetCapableTool();
    if (!baseTool) {
        return;
    }
    BrushSettings s;
    if (BrushTool* tool = dynamic_cast<BrushTool*>(baseTool)) {
        s = tool->currentBrushSettings();
    } else if (EraserTool* tool = dynamic_cast<EraserTool*>(baseTool)) {
        s = tool->currentBrushSettings();
    } else {
        return;
    }
    if (subTip) {
        if (!s.mask.sub) {
            s.mask.sub = std::make_unique<BrushSettings>();
            s.mask.sub->name = s.name + QStringLiteral("-副");
        }
        s.mask.sub->tipImage = image;
        s.mask.sub->tipShape = BrushSettings::TipShape::Image;
        s.mask.sub->bakeTipMask();
    } else {
        s.tipImage = image;
        s.tipShape = BrushSettings::TipShape::Image;
        s.bakeTipMask();
    }
    if (BrushTool* tool = dynamic_cast<BrushTool*>(baseTool)) {
        tool->applyBrushOptions(s);
    } else if (EraserTool* tool = dynamic_cast<EraserTool*>(baseTool)) {
        tool->applyBrushOptions(s);
    }
    updateUI();
}

void BrushOptionsWidget::clearTipImage(bool subTip)
{
    BaseTool* baseTool = currentPresetCapableTool();
    if (!baseTool) {
        return;
    }
    BrushSettings s;
    if (BrushTool* tool = dynamic_cast<BrushTool*>(baseTool)) {
        s = tool->currentBrushSettings();
    } else if (EraserTool* tool = dynamic_cast<EraserTool*>(baseTool)) {
        s = tool->currentBrushSettings();
    } else {
        return;
    }
    if (subTip) {
        if (!s.mask.sub) {
            return;
        }
        s.mask.sub->tipImage = QImage();
        s.mask.sub->tipMask = QImage();
        s.mask.sub->tipShape = BrushSettings::TipShape::Circle;
    } else {
        s.tipImage = QImage();
        s.tipMask = QImage();
        s.tipShape = BrushSettings::TipShape::Circle;
    }
    if (BrushTool* tool = dynamic_cast<BrushTool*>(baseTool)) {
        tool->applyBrushOptions(s);
    } else if (EraserTool* tool = dynamic_cast<EraserTool*>(baseTool)) {
        tool->applyBrushOptions(s);
    }
    updateUI();
}

void BrushOptionsWidget::importTexture()
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("选择纹理图"), QString(),
        tr("图像 (*.png *.jpg *.jpeg *.bmp);;%1").arg(tr("所有文件 (*.*)")));
    if (file.isEmpty()) {
        return;
    }
    QImage image(file);
    if (image.isNull()) {
        return;
    }
    BaseTool* baseTool = currentPresetCapableTool();
    if (!baseTool) {
        return;
    }
    if (BrushTool* tool = dynamic_cast<BrushTool*>(baseTool)) {
        BrushSettings s = tool->currentBrushSettings();
        s.texture.pattern = image;
        s.texture.bake();
        s.texture.enabled = !s.texture.bakedMask.isNull();
        tool->applyBrushOptions(s);
        updateUI();
    }
}
