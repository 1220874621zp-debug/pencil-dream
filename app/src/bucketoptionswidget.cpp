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
#include "bucketoptionswidget.h"
#include "ui_bucketoptionswidget.h"

#include <QDebug>
#include <QColorDialog>

#include "spinslider.h"

#include "layer.h"
#include "editor.h"
#include "layermanager.h"
#include "toolmanager.h"
#include "util.h"

#include "stroketool.h"
#include "buckettool.h"

BucketOptionsWidget::BucketOptionsWidget(Editor* editor, QWidget* parent) :
    BaseWidget(parent),
    ui(new Ui::BucketOptionsWidget),
    mEditor(editor)
{
    ui->setupUi(this);

    initUI();
}

BucketOptionsWidget::~BucketOptionsWidget()
{
    delete ui;
}

void BucketOptionsWidget::initUI()
{
    mBucketTool = static_cast<BucketTool*>(mEditor->tools()->getTool(BUCKET));

    BucketToolProperties properties = mBucketTool->settings();

    auto toleranceInfo = properties.getInfo(BucketToolProperties::COLORTOLERANCE_VALUE);
    auto expandInfo = properties.getInfo(BucketToolProperties::FILLEXPAND_VALUE);
    auto closeGapInfo = properties.getInfo(BucketToolProperties::CLOSEGAP_VALUE);
    auto featherInfo = properties.getInfo(BucketToolProperties::FEATHER_VALUE);

    ui->colorToleranceSlider->init(tr("Color tolerance"), SpinSlider::GROWTH_TYPE::LINEAR, toleranceInfo.minInt(), toleranceInfo.maxInt());
    ui->expandSlider->init(tr("扩展/收缩"), SpinSlider::GROWTH_TYPE::LINEAR, expandInfo.minInt(), expandInfo.maxInt());
    ui->closeGapSlider->init(tr("封闭间隙"), SpinSlider::GROWTH_TYPE::LINEAR, closeGapInfo.minInt(), closeGapInfo.maxInt());
    ui->featherSlider->init(tr("羽化"), SpinSlider::GROWTH_TYPE::LINEAR, featherInfo.minInt(), featherInfo.maxInt());

    ui->expandSpinBox->setMinimum(expandInfo.minInt());
    ui->expandSpinBox->setMaximum(expandInfo.maxInt());
    ui->colorToleranceSpinbox->setMaximum(toleranceInfo.maxInt());
    ui->closeGapSpinbox->setMinimum(closeGapInfo.minInt());
    ui->closeGapSpinbox->setMaximum(closeGapInfo.maxInt());
    ui->featherSpinbox->setMinimum(featherInfo.minInt());
    ui->featherSpinbox->setMaximum(featherInfo.maxInt());

    ui->referenceLayerComboBox->addItem(tr("Current layer", "Reference Layer Options"), 0);
    ui->referenceLayerComboBox->addItem(tr("All layers", "Reference Layer Options"), 1);
    ui->referenceLayerComboBox->setToolTip(tr("Refers to the layer that used to flood fill from"));

    ui->blendModeComboBox->addItem(tr("Overlay", "Blend Mode dropdown option"), 0);
    ui->blendModeComboBox->addItem(tr("Replace", "Blend Mode dropdown option"), 1);
    ui->blendModeComboBox->addItem(tr("Behind",  "Blend Mode dropdown option"), 2);
    ui->blendModeComboBox->setToolTip(tr("Defines how the fill will behave when the new color is not opaque"));

    ui->regionModeComboBox->addItem(tr("连续区域"), 0);
    ui->regionModeComboBox->addItem(tr("相似颜色"), 1);
    ui->regionModeComboBox->addItem(tr("到边界色"), 2);
    ui->regionModeComboBox->addItem(tr("选区填充"), 3);

    ui->dragModeComboBox->addItem(tr("仅相似区域"), 0);
    ui->dragModeComboBox->addItem(tr("任意区域"), 1);
    ui->dragModeComboBox->addItem(tr("不用"), 2);

    setBoundaryColor(QColor::fromRgb(static_cast<QRgb>(properties.boundaryColor())));

    makeConnectionsFromUIToModel();
    makeConnectionsFromModelToUI();

    clearFocusOnFinished(ui->colorToleranceSpinbox);
    clearFocusOnFinished(ui->expandSpinBox);
    clearFocusOnFinished(ui->closeGapSpinbox);
    clearFocusOnFinished(ui->featherSpinbox);

    updatePropertyVisibility();
}

void BucketOptionsWidget::updateUI()
{
    updatePropertyVisibility();

    BucketToolProperties properties = mBucketTool->settings();

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::FILLEXPAND_ENABLED)) {
        mBucketTool->setFillExpandEnabled(properties.fillExpandEnabled());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::FILLEXPAND_VALUE)) {
        mBucketTool->setFillExpand(properties.fillExpandAmount());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE)) {
        mBucketTool->setFillReferenceMode(properties.fillReferenceMode());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::FILLMODE_VALUE)) {
        mBucketTool->setFillMode(properties.fillMode());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::COLORTOLERANCE_VALUE)) {
        mBucketTool->setColorTolerance(properties.tolerance());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::COLORTOLERANCE_ENABLED)) {
        mBucketTool->setColorToleranceEnabled(properties.colorToleranceEnabled());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::CLOSEGAP_VALUE)) {
        mBucketTool->setCloseGap(properties.closeGapPx());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::FEATHER_VALUE)) {
        mBucketTool->setFeather(properties.featherPx());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::ANTIALIASING_ENABLED)) {
        mBucketTool->setAntiAliasingEnabled(properties.antiAliasingEnabled());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::GROWSTOPDARKEST_ENABLED)) {
        mBucketTool->setGrowStopDarkestEnabled(properties.growStopDarkestEnabled());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::REGIONMODE_VALUE)) {
        mBucketTool->setRegionMode(properties.regionFillMode());
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::BOUNDARYCOLOR_VALUE)) {
        mBucketTool->setBoundaryColor(QColor::fromRgb(static_cast<QRgb>(properties.boundaryColor())));
    }

    if (mBucketTool->isPropertyEnabled(BucketToolProperties::DRAGMODE_VALUE)) {
        mBucketTool->setDragMode(properties.dragFillMode());
    }
}

void BucketOptionsWidget::makeConnectionsFromModelToUI()
{
    connect(mBucketTool, &BucketTool::toleranceChanged, this, [=](int value) {
       setColorTolerance(value);
    });

    connect(mBucketTool, &BucketTool::toleranceEnabledChanged, this, [=](bool enabled) {
        setColorToleranceEnabled(enabled);
    });

    connect(mBucketTool, &BucketTool::fillExpandChanged, this, [=](int value) {
        setFillExpand(value);
    });

    connect(mBucketTool, &BucketTool::fillExpandEnabledChanged, this, [=](bool enabled) {
        setFillExpandEnabled(enabled);
    });

    connect(mBucketTool, &BucketTool::fillReferenceModeChanged, this, [=](int value) {
        setFillReferenceMode(value);
    });

    connect(mBucketTool, &BucketTool::fillModeChanged, this, [=](int value) {
        setFillMode(value);
    });

    connect(mBucketTool, &BucketTool::closeGapChanged, this, [=](int value) {
        setCloseGap(value);
    });

    connect(mBucketTool, &BucketTool::featherChanged, this, [=](int value) {
        setFeather(value);
    });

    connect(mBucketTool, &BucketTool::antiAliasingEnabledChanged, this, [=](bool enabled) {
        setAntiAliasingEnabled(enabled);
    });

    connect(mBucketTool, &BucketTool::growStopDarkestEnabledChanged, this, [=](bool enabled) {
        setGrowStopDarkestEnabled(enabled);
    });

    connect(mBucketTool, &BucketTool::regionModeChanged, this, [=](int value) {
        setRegionMode(value);
    });

    connect(mBucketTool, &BucketTool::boundaryColorChanged, this, [=](const QColor& color) {
        setBoundaryColor(color);
    });

    connect(mBucketTool, &BucketTool::dragModeChanged, this, [=](int value) {
        setDragMode(value);
    });
}

void BucketOptionsWidget::makeConnectionsFromUIToModel()
{
    connect(ui->colorToleranceSlider, &SpinSlider::valueChanged, [=](int value) {
        mBucketTool->setColorTolerance(value);
    });
    connect(ui->colorToleranceSpinbox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        mBucketTool->setColorTolerance(value);
    });

    connect(ui->colorToleranceCheckbox, &QCheckBox::toggled, [=](bool enabled) {
        mBucketTool->setColorToleranceEnabled(enabled);
    });

    connect(ui->expandSlider, &SpinSlider::valueChanged, [=](int value) {
        mBucketTool->setFillExpand(value);
    });
    connect(ui->expandSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        mBucketTool->setFillExpand(value);
    });

    connect(ui->expandCheckbox, &QCheckBox::toggled, [=](bool enabled) {
        mBucketTool->setFillExpandEnabled(enabled);
    });

    connect(ui->closeGapSlider, &SpinSlider::valueChanged, [=](int value) {
        mBucketTool->setCloseGap(value);
    });
    connect(ui->closeGapSpinbox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        mBucketTool->setCloseGap(value);
    });

    connect(ui->featherSlider, &SpinSlider::valueChanged, [=](int value) {
        mBucketTool->setFeather(value);
    });
    connect(ui->featherSpinbox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        mBucketTool->setFeather(value);
    });

    connect(ui->antialiasCheckbox, &QCheckBox::toggled, [=](bool enabled) {
        mBucketTool->setAntiAliasingEnabled(enabled);
    });

    connect(ui->growStopDarkestCheckbox, &QCheckBox::toggled, [=](bool enabled) {
        mBucketTool->setGrowStopDarkestEnabled(enabled);
    });

    connect(ui->referenceLayerComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int value) {
        mBucketTool->setFillReferenceMode(value);
    });

    connect(ui->blendModeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int value) {
        mBucketTool->setFillMode(value);
    });

    connect(ui->regionModeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int value) {
        mBucketTool->setRegionMode(value);
        updateBoundaryColorEnabled();
    });

    connect(ui->dragModeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int value) {
        mBucketTool->setDragMode(value);
    });

    connect(ui->boundaryColorButton, &QPushButton::clicked, [=]() {
        const QColor current = QColor::fromRgb(static_cast<QRgb>(mBucketTool->settings().boundaryColor()));
        const QColor picked = QColorDialog::getColor(current, this, tr("选择边界色"));
        if (picked.isValid()) {
            mBucketTool->setBoundaryColor(picked);
        }
    });
}

void BucketOptionsWidget::updatePropertyVisibility()
{
    ui->colorToleranceCheckbox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::COLORTOLERANCE_ENABLED));
    ui->colorToleranceSlider->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::COLORTOLERANCE_VALUE));
    ui->colorToleranceSpinbox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::COLORTOLERANCE_VALUE));
    ui->expandCheckbox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FILLEXPAND_ENABLED));
    ui->expandSlider->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FILLEXPAND_VALUE));
    ui->expandSpinBox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FILLEXPAND_VALUE));
    ui->referenceLayerComboBox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE));
    ui->referenceLayerDescLabel->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE));
    ui->blendModeComboBox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FILLMODE_VALUE));
    ui->blendModeLabel->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FILLMODE_VALUE));
    ui->closeGapSlider->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::CLOSEGAP_VALUE));
    ui->closeGapSpinbox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::CLOSEGAP_VALUE));
    ui->featherSlider->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FEATHER_VALUE));
    ui->featherSpinbox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::FEATHER_VALUE));
    ui->antialiasCheckbox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::ANTIALIASING_ENABLED));
    ui->growStopDarkestCheckbox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::GROWSTOPDARKEST_ENABLED));
    ui->regionModeComboBox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::REGIONMODE_VALUE));
    ui->regionModeLabel->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::REGIONMODE_VALUE));
    ui->boundaryColorButton->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::BOUNDARYCOLOR_VALUE));
    ui->boundaryColorLabel->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::BOUNDARYCOLOR_VALUE));
    ui->dragModeComboBox->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::DRAGMODE_VALUE));
    ui->dragModeLabel->setVisible(mBucketTool->isPropertyEnabled(BucketToolProperties::DRAGMODE_VALUE));

    updateBoundaryColorEnabled();
}

void BucketOptionsWidget::updateBoundaryColorEnabled()
{
    // 常驻不隐藏，避免排版跳动：仅在到边界色模式下可用
    const bool untilColor = ui->regionModeComboBox->currentIndex() == 2;
    ui->boundaryColorLabel->setEnabled(untilColor);
    ui->boundaryColorButton->setEnabled(untilColor);
}

void BucketOptionsWidget::onLayerChanged(int)
{
    updatePropertyVisibility();
}

void BucketOptionsWidget::setColorTolerance(int tolerance)
{
    QSignalBlocker b(ui->colorToleranceSlider);
    ui->colorToleranceSlider->setValue(tolerance);

    QSignalBlocker b2(ui->colorToleranceSpinbox);
    ui->colorToleranceSpinbox->setValue(tolerance);
}

void BucketOptionsWidget::setColorToleranceEnabled(bool enabled)
{
    QSignalBlocker b(ui->colorToleranceCheckbox);
    ui->colorToleranceCheckbox->setChecked(enabled);
}

void BucketOptionsWidget::setFillMode(int mode)
{
    QSignalBlocker b(ui->blendModeComboBox);
    ui->blendModeComboBox->setCurrentIndex(mode);
}

void BucketOptionsWidget::setFillExpandEnabled(bool enabled)
{
    QSignalBlocker b(ui->expandCheckbox);
    ui->expandCheckbox->setChecked(enabled);
}

void BucketOptionsWidget::setFillExpand(int value)
{
    QSignalBlocker b(ui->expandSlider);
    ui->expandSlider->setValue(value);

    QSignalBlocker b2(ui->expandSpinBox);
    ui->expandSpinBox->setValue(value);
}

void BucketOptionsWidget::setFillReferenceMode(int referenceMode)
{
    QSignalBlocker b(ui->referenceLayerComboBox);
    ui->referenceLayerComboBox->setCurrentIndex(referenceMode);
}

void BucketOptionsWidget::setCloseGap(int closeGap)
{
    QSignalBlocker b(ui->closeGapSlider);
    ui->closeGapSlider->setValue(closeGap);

    QSignalBlocker b2(ui->closeGapSpinbox);
    ui->closeGapSpinbox->setValue(closeGap);
}

void BucketOptionsWidget::setFeather(int feather)
{
    QSignalBlocker b(ui->featherSlider);
    ui->featherSlider->setValue(feather);

    QSignalBlocker b2(ui->featherSpinbox);
    ui->featherSpinbox->setValue(feather);
}

void BucketOptionsWidget::setAntiAliasingEnabled(bool enabled)
{
    QSignalBlocker b(ui->antialiasCheckbox);
    ui->antialiasCheckbox->setChecked(enabled);
}

void BucketOptionsWidget::setGrowStopDarkestEnabled(bool enabled)
{
    QSignalBlocker b(ui->growStopDarkestCheckbox);
    ui->growStopDarkestCheckbox->setChecked(enabled);
}

void BucketOptionsWidget::setRegionMode(int mode)
{
    QSignalBlocker b(ui->regionModeComboBox);
    ui->regionModeComboBox->setCurrentIndex(mode);
    updateBoundaryColorEnabled();
}

void BucketOptionsWidget::setBoundaryColor(const QColor& color)
{
    ui->boundaryColorButton->setStyleSheet(
        QString("background-color: %1; border: 1px solid #808080; border-radius: 2px;")
            .arg(color.name()));
}

void BucketOptionsWidget::setDragMode(int mode)
{
    QSignalBlocker b(ui->dragModeComboBox);
    ui->dragModeComboBox->setCurrentIndex(mode);
}
