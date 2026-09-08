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

#include "onionskinwidget.h"
#include "ui_onionskin.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>

#include "preferencemanager.h"
#include "editor.h"
#include "util.h"

OnionSkinWidget::OnionSkinWidget(QWidget* parent) :
    BaseDockWidget(parent),
    ui(new Ui::OnionSkin)
{
    ui->setupUi(this);
}

OnionSkinWidget::~OnionSkinWidget()
{
    delete ui;
}

void OnionSkinWidget::initUI()
{
    buildParamRows();
    updateUI();
    makeConnections();

    clearFocusOnFinished(mPrevFramesSpin);
    clearFocusOnFinished(mNextFramesSpin);
    clearFocusOnFinished(mMaxOpacitySpin);
    clearFocusOnFinished(mMinOpacitySpin);
}

void OnionSkinWidget::buildParamRows()
{
    QVBoxLayout* rows = ui->paramRowsLayout;

    // --- 灯泡总开关：同时开启前后帧 ---
    mOnionToggleButton = new QToolButton(this);
    mOnionToggleButton->setIcon(QIcon(":/icons/themes/playful/onion/onionskin-enable.svg"));
    mOnionToggleButton->setIconSize(QSize(22, 22));
    mOnionToggleButton->setCheckable(true);
    mOnionToggleButton->setAutoRaise(true);
    mOnionToggleButton->setToolTip(tr("Toggle onion skin (previous & next frames together)"));

    // 红蓝着色按钮与灯泡开关同行
    mOnionRedButton = new QToolButton(this);
    mOnionRedButton->setIcon(QIcon(":/icons/themes/playful/onion/onionskin-red.svg"));
    mOnionRedButton->setIconSize(QSize(22, 22));
    mOnionRedButton->setCheckable(true);
    mOnionRedButton->setAutoRaise(true);
    mOnionRedButton->setToolTip(tr("Onion skin color: red"));

    mOnionBlueButton = new QToolButton(this);
    mOnionBlueButton->setIcon(QIcon(":/icons/themes/playful/onion/onionskin-blue.svg"));
    mOnionBlueButton->setIconSize(QSize(22, 22));
    mOnionBlueButton->setCheckable(true);
    mOnionBlueButton->setAutoRaise(true);
    mOnionBlueButton->setToolTip(tr("Onion skin color: blue"));

    auto* toggleRow = new QHBoxLayout;
    toggleRow->setSpacing(6);
    toggleRow->addWidget(mOnionToggleButton);
    auto* toggleLabel = new QLabel(tr("Onion Skin On/Off："), this);
    toggleRow->addWidget(toggleLabel, 1);
    toggleRow->addWidget(mOnionRedButton);
    toggleRow->addWidget(mOnionBlueButton);
    rows->addLayout(toggleRow);

    // --- 滑杆+输入框参数行（见知识库 slider-spinbox-param-row.md 规范） ---
    auto addParamRow = [this, rows](const char* label, QSlider*& slider, QDoubleSpinBox*& spin,
                                    qreal min, qreal max, const QString& suffix) {
        spin = new QDoubleSpinBox(this);
        spin->setRange(min, max);
        spin->setDecimals(0);
        spin->setSuffix(suffix);
        spin->setFixedWidth(96);  // 四行统一宽
        spin->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(qRound(min), qRound(max));
        slider->setMaximumWidth(110); // 四行统一宽（窄面板下不过分拉伸）

        // 标签独占一行；滑杆+输入框一行，输入框常规高度靠右
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(2);
        grid->setContentsMargins(0, 0, 0, 0);
        auto* labelWidget = new QLabel(tr(label) + QStringLiteral("："), this);
        labelWidget->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(labelWidget, 0, 0, 1, 2);
        grid->addWidget(slider, 1, 0);
        grid->addWidget(spin, 1, 1, Qt::AlignRight);
        grid->setColumnStretch(0, 1);
        rows->addLayout(grid);

        // 滑杆 → 输入框
        connect(slider, &QSlider::valueChanged, this, [spin](int value) {
            if (qAbs(spin->value() - value) >= 1) {
                QSignalBlocker blocker(spin);
                spin->setValue(value);
            }
        });
        // 输入框 → 滑杆（整型参数直通）
        connect(spin, &QDoubleSpinBox::valueChanged, this, [slider](double value) {
            const int pos = qRound(value);
            if (slider->value() != pos) {
                QSignalBlocker blocker(slider);
                slider->setValue(pos);
            }
        });
    };

    addParamRow(QT_TRANSLATE_NOOP("OnionSkinWidget", "Previous frames"), mPrevFramesSlider, mPrevFramesSpin, 0, 60, QString()); // 0=关闭该方向
    addParamRow(QT_TRANSLATE_NOOP("OnionSkinWidget", "Next frames"), mNextFramesSlider, mNextFramesSpin, 0, 60, QString()); // 0=关闭该方向
    addParamRow(QT_TRANSLATE_NOOP("OnionSkinWidget", "Max opacity"), mMaxOpacitySlider, mMaxOpacitySpin, 0, 100, tr(" %"));
    addParamRow(QT_TRANSLATE_NOOP("OnionSkinWidget", "Min opacity"), mMinOpacitySlider, mMinOpacitySpin, 0, 100, tr(" %"));
}

void OnionSkinWidget::makeConnections()
{
    // QDoubleSpinBox::valueChanged(double) 直连 int 槽会在运行时连接失败，必须 lambda 取整
    connect(mPrevFramesSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) { onionPrevFramesNumChange(qRound(v)); });
    connect(mNextFramesSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) { onionNextFramesNumChange(qRound(v)); });
    connect(mMaxOpacitySpin, &QDoubleSpinBox::valueChanged, this, [this](double v) { onionMaxOpacityChange(qRound(v)); });
    connect(mMinOpacitySpin, &QDoubleSpinBox::valueChanged, this, [this](double v) { onionMinOpacityChange(qRound(v)); });

    connect(mOnionToggleButton, &QToolButton::clicked, this, &OnionSkinWidget::onionToggleClicked);
    connect(mOnionBlueButton, &QToolButton::clicked, this, &OnionSkinWidget::onionBlueButtonClicked);
    connect(mOnionRedButton, &QToolButton::clicked, this, &OnionSkinWidget::onionRedButtonClicked);

    connect(ui->onionSkinMode, &QCheckBox::stateChanged, this, &OnionSkinWidget::onionSkinModeChange);
    connect(ui->onionWhilePlayback, &QCheckBox::stateChanged, this, &OnionSkinWidget::playbackStateChanged);
    connect(ui->onionSkinMultiLayer, &QCheckBox::stateChanged, this, &OnionSkinWidget::onionSkinMultipleLayersEnabled);

    PreferenceManager* prefs = editor()->preference();
    connect(prefs, &PreferenceManager::optionChanged, this, &OnionSkinWidget::updateUI);
}

void OnionSkinWidget::updateUI()
{
    PreferenceManager* prefs = editor()->preference();

    QSignalBlocker b1(mOnionToggleButton);
    mOnionToggleButton->setChecked(prefs->isOn(SETTING::PREV_ONION) && prefs->isOn(SETTING::NEXT_ONION));

    QSignalBlocker b2(mOnionRedButton);
    mOnionRedButton->setChecked(prefs->isOn(SETTING::ONION_RED));

    QSignalBlocker b3(mOnionBlueButton);
    mOnionBlueButton->setChecked(prefs->isOn(SETTING::ONION_BLUE));

    mPrevFramesSpin->setValue(prefs->getInt(SETTING::ONION_PREV_FRAMES_NUM));
    mNextFramesSpin->setValue(prefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM));
    mMaxOpacitySpin->setValue(prefs->getInt(SETTING::ONION_MAX_OPACITY));
    mMinOpacitySpin->setValue(prefs->getInt(SETTING::ONION_MIN_OPACITY));
    // 滑杆手动对齐（updateUI 的 setValue 不经信号链时）
    mPrevFramesSlider->setValue(qRound(mPrevFramesSpin->value()));
    mNextFramesSlider->setValue(qRound(mNextFramesSpin->value()));
    mMaxOpacitySlider->setValue(qRound(mMaxOpacitySpin->value()));
    mMinOpacitySlider->setValue(qRound(mMinOpacitySpin->value()));

    QSignalBlocker b5(ui->onionSkinMode);
    ui->onionSkinMode->setChecked(prefs->getString(SETTING::ONION_TYPE) == "absolute");

    QSignalBlocker b6(ui->onionWhilePlayback);
    ui->onionWhilePlayback->setChecked(prefs->isOn(SETTING::ONION_WHILE_PLAYBACK));

    QSignalBlocker b7(ui->onionSkinMultiLayer);
    ui->onionSkinMultiLayer->setChecked(prefs->isOn(SETTING::ONION_MUTLIPLE_LAYERS));
}

void OnionSkinWidget::onionToggleClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::PREV_ONION, isOn);
    prefs->set(SETTING::NEXT_ONION, isOn);
}

void OnionSkinWidget::onionRedButtonClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_RED, isOn);
}

void OnionSkinWidget::onionBlueButtonClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_BLUE, isOn);
}

void OnionSkinWidget::onionMaxOpacityChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_MAX_OPACITY, value);
}

void OnionSkinWidget::onionMinOpacityChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_MIN_OPACITY, value);
}

void OnionSkinWidget::onionPrevFramesNumChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_PREV_FRAMES_NUM, value);
}

void OnionSkinWidget::onionNextFramesNumChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_NEXT_FRAMES_NUM, value);
}

void OnionSkinWidget::onionSkinModeChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    if (value == Qt::Checked)
    {
        prefs->set(SETTING::ONION_TYPE, QString("absolute"));
    }
    else
    {
        prefs->set(SETTING::ONION_TYPE, QString("relative"));
    }
}

void OnionSkinWidget::playbackStateChanged(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_WHILE_PLAYBACK, value);
}

void OnionSkinWidget::onionSkinMultipleLayersEnabled(bool value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set(SETTING::ONION_MUTLIPLE_LAYERS, value);
}
