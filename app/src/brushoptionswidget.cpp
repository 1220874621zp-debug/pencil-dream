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
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
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
    tipLayout->addWidget(mSizeSlider);
    tipLayout->addWidget(mOpacitySlider);
    tipLayout->addWidget(mFlowSlider);
    tipLayout->addWidget(mHardnessSlider);
    tipLayout->addWidget(mRatioSlider);
    tipLayout->addWidget(mAngleSlider);
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

    layout->addStretch();
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    QVBoxLayout* top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(scroll);

    // ---- 信号：任何改动 → 整套参数回写工具 ----
    const auto sliders = { mSizeSlider, mOpacitySlider, mFlowSlider, mHardnessSlider,
                           mRatioSlider, mAngleSlider, mSpacingCoeffSlider, mSpacingSlider,
                           mScatterSlider, mAirbrushRateSlider };
    for (SpinSlider* slider : sliders) {
        connect(slider, &SpinSlider::valueChanged, this, &BrushOptionsWidget::applyFromWidgets);
    }
    const auto boxes = { mAutoSpacingBox, mAirbrushBox, mMirrorXBox, mMirrorYBox };
    for (QCheckBox* box : boxes) {
        connect(box, &QCheckBox::clicked, this, [this]() { applyFromWidgets(); });
    }
    connect(mPaintingModeCombo, &QComboBox::activated, this, [this]() { applyFromWidgets(); });
    connect(mBlendModeCombo, &QComboBox::activated, this, [this]() { applyFromWidgets(); });
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
    mBlendModeGroup->setVisible(brushTool != nullptr);
    mPaintingModeRow->setVisible(!isSmudge);
    mScatterSlider->setVisible(!isSmudge);
    mMirrorRow->setVisible(!isSmudge);
    mFlowSlider->setLabel(isSmudge ? tr("混合速率 %") : tr("流量 %"));
    mMirrorXBox->setChecked(s.mirrorX);
    mMirrorYBox->setChecked(s.mirrorY);
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
