/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2026 Pencil Dream contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "pantooptionswidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "editor.h"
#include "pantotool.h"
#include "spinslider.h"
#include "toolmanager.h"

PantoOptionsWidget::PantoOptionsWidget(Editor* editor, QWidget* parent)
    : BaseWidget(parent)
    , mEditor(editor)
{
    // 控件必须在构造期建好：点工具切换会立刻触发 updateUI()
    initUI();
}

void PantoOptionsWidget::initUI()
{
    mTool = dynamic_cast<PantoTool*>(mEditor->tools()->getTool(PANTO));
    Q_ASSERT(mTool != nullptr);

    QWidget* content = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // ---- 取样源 ----
    QWidget* sourceRow = new QWidget(this);
    QHBoxLayout* sourceLayout = new QHBoxLayout(sourceRow);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->addWidget(new QLabel(tr("取样源"), this));
    mSourceCombo = new QComboBox(this);
    mSourceCombo->addItem(tr("上一帧（灯桌同源）"));
    mSourceCombo->addItem(tr("下一帧（灯桌同源）"));
    mSourceCombo->addItem(tr("当前帧（自克隆）"));
    mSourceCombo->addItem(tr("剪贴板（Spare）"));
    mSourceCombo->setToolTip(tr("上一/下一帧与洋葱皮灯桌同源：绝对模式取上/下一个关键帧，逐帧模式取相邻帧的覆盖画面；循环层自动回绕"));
    sourceLayout->addWidget(mSourceCombo, 1);
    layout->addWidget(sourceRow);

    // ---- 偏移模式 ----
    QWidget* offsetModeRow = new QWidget(this);
    QHBoxLayout* offsetModeLayout = new QHBoxLayout(offsetModeRow);
    offsetModeLayout->setContentsMargins(0, 0, 0, 0);
    offsetModeLayout->addWidget(new QLabel(tr("偏移模式"), this));
    mOffsetCombo = new QComboBox(this);
    mOffsetCombo->addItem(tr("原位（原地拓印）"));
    mOffsetCombo->addItem(tr("固定偏移"));
    mOffsetCombo->addItem(tr("跟随（Alt+点击定源点）"));
    mOffsetCombo->setToolTip(tr("原位=把源帧同位置像素印过来；固定偏移=Alt+拖动定义的向量持续生效；跟随=PS 仿制图章式，每笔以落点对齐源点"));
    offsetModeLayout->addWidget(mOffsetCombo, 1);
    layout->addWidget(offsetModeRow);

    // ---- 偏移量 ----
    mOffsetXSlider = new SpinSlider(this);
    mOffsetXSlider->init(tr("偏移 X"), SpinSlider::LINEAR, -2000.0, 2000.0);
    mOffsetYSlider = new SpinSlider(this);
    mOffsetYSlider->init(tr("偏移 Y"), SpinSlider::LINEAR, -2000.0, 2000.0);
    layout->addWidget(mOffsetXSlider);
    layout->addWidget(mOffsetYSlider);

    // ---- 复位 + 提示 ----
    QPushButton* resetButton = new QPushButton(tr("复位偏移"), this);
    resetButton->setToolTip(tr("偏移归零并回到原位模式，清除跟随源点"));
    layout->addWidget(resetButton);

    QLabel* hintLabel = new QLabel(tr("画布上 Alt+拖动=定义偏移向量（自动切固定偏移），Alt+点击=设定跟随源点；空白帧下笔自动新建关键帧"), this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #9a9a9a;");
    layout->addWidget(hintLabel);

    layout->addStretch();
    QVBoxLayout* top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(content);

    connect(mSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PantoOptionsWidget::applyFromWidgets);
    connect(mOffsetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PantoOptionsWidget::applyFromWidgets);
    connect(mOffsetXSlider, &SpinSlider::valueChanged, this, &PantoOptionsWidget::applyFromWidgets);
    connect(mOffsetYSlider, &SpinSlider::valueChanged, this, &PantoOptionsWidget::applyFromWidgets);
    connect(resetButton, &QPushButton::clicked, mTool, &PantoTool::resetOffset);
    connect(mTool, &PantoTool::pantoOptionsChanged, this, &PantoOptionsWidget::updateUI);

    updateUI();
}

void PantoOptionsWidget::updateUI()
{
    if (mTool == nullptr) { return; }
    mSyncing = true;
    mSourceCombo->setCurrentIndex(static_cast<int>(mTool->sourceMode()));
    mOffsetCombo->setCurrentIndex(static_cast<int>(mTool->offsetMode()));
    const QPointF offset = mTool->cloneOffset();
    mOffsetXSlider->setValue(offset.x());
    mOffsetYSlider->setValue(offset.y());
    mSyncing = false;
}

void PantoOptionsWidget::applyFromWidgets()
{
    if (mSyncing || mTool == nullptr) { return; }
    mTool->setSourceMode(static_cast<PantoTool::SourceMode>(mSourceCombo->currentIndex()));
    mTool->setOffsetMode(static_cast<PantoTool::OffsetMode>(mOffsetCombo->currentIndex()));
    mTool->setCloneOffset(QPointF(mOffsetXSlider->value(), mOffsetYSlider->value()));
}
