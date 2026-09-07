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
#include "transformoptionswidget.h"
#include "ui_transformoptionswidget.h"

#include "editor.h"
#include "toolmanager.h"
#include "transformtool.h"
#include "deformtool.h"
#include "lassotool.h"

TransformOptionsWidget::TransformOptionsWidget(Editor* editor, QWidget *parent) :
    BaseWidget(parent),
    ui(new Ui::TransformOptionsWidget), mEditor(editor)
{
    ui->setupUi(this);
    initUI();
}

TransformOptionsWidget::~TransformOptionsWidget()
{
    delete ui;
}

void TransformOptionsWidget::initUI()
{
    makeConnectionsFromUIToModel();

    // mode switch drives the parameter stack
    connect(ui->deformModeComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        ui->modeStackedWidget->setCurrentIndex(index);
        DeformTool* deformTool = dynamic_cast<DeformTool*>(mTransformTool);
        if (deformTool != nullptr && deformTool->type() == DEFORM) {
            deformTool->setDeformMode(index);
        }
    });
}

void TransformOptionsWidget::updateUI()
{
    TransformTool* currentTool = mEditor->tools()->currentTransformTool();
    if (currentTool == nullptr) { return; }

    updatePropertyVisibility();
    updateToolConnections(currentTool);
    const TransformToolProperties selectP = currentTool->transformSettings();

    if (currentTool->isPropertyEnabled(TransformToolProperties::SHOWSELECTIONINFO_ENABLED)) {
        setShowSelectionInfo(selectP.showSelectionInfoEnabled());
    }

    if (currentTool->isPropertyEnabled(TransformToolProperties::ANTI_ALIASING_ENABLED)) {
        setAntiAliasingEnabled(selectP.antiAliasingEnabled());
    }
}

void TransformOptionsWidget::updatePropertyVisibility()
{
    TransformTool* currentTool = mEditor->tools()->currentTransformTool();
    if (mEditor->tools()->currentTransformTool() == nullptr) { return; }

    ui->antiAliasingCheckBox->setVisible(currentTool->isPropertyEnabled(TransformToolProperties::ANTI_ALIASING_ENABLED));

    const bool isDeform = currentTool->type() == DEFORM;
    ui->deformModeLabel->setVisible(isDeform);
    ui->deformModeComboBox->setVisible(isDeform);
    ui->modeStackedWidget->setVisible(isDeform);

    const bool isLasso = currentTool->type() == LASSO;
    ui->selectionActionLabel->setVisible(isLasso);
    ui->selectionActionComboBox->setVisible(isLasso);
    ui->selectionGrowLabel->setVisible(isLasso);
    ui->selectionGrowSpinBox->setVisible(isLasso);
}

void TransformOptionsWidget::updateToolConnections(BaseTool* tool)
{
    if (mTransformTool) {
        disconnect(mTransformTool, nullptr, this, nullptr);
    }

    mTransformTool = static_cast<TransformTool*>(tool);

    makeConnectionFromModelToUI(mTransformTool);
}

void TransformOptionsWidget::makeConnectionsFromUIToModel()
{
    connect(ui->showSelectionInfoCheckBox, &QCheckBox::clicked, this, [=](bool enabled) {
       mTransformTool->setShowSelectionInfo(enabled);
    });

    connect(ui->antiAliasingCheckBox, &QCheckBox::clicked, this, [=](bool enabled) {
       mTransformTool->setAntiAliasingEnabled(enabled);
    });

    auto deformTool = [] (TransformTool* tool) -> DeformTool* {
        DeformTool* deformTool = dynamic_cast<DeformTool*>(tool);
        return (deformTool != nullptr && deformTool->type() == DEFORM) ? deformTool : nullptr;
    };

    connect(ui->gridSizeSpinBox, &QSpinBox::valueChanged, this, [=](int value) {
        if (DeformTool* t = deformTool(mTransformTool)) { t->setGridSize(value); }
    });

    connect(ui->liquifyOpComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        if (DeformTool* t = deformTool(mTransformTool)) { t->setLiquifyOp(index); }
    });

    connect(ui->liquifySizeSpinBox, &QSpinBox::valueChanged, this, [=](int value) {
        if (DeformTool* t = deformTool(mTransformTool)) { t->setLiquifySize(value); }
    });

    connect(ui->liquifyAmountSpinBox, &QDoubleSpinBox::valueChanged, this, [=](double value) {
        if (DeformTool* t = deformTool(mTransformTool)) { t->setLiquifyAmount(value); }
    });

    connect(ui->liquifyReverseCheckBox, &QCheckBox::clicked, this, [=](bool checked) {
        if (DeformTool* t = deformTool(mTransformTool)) { t->setLiquifyReverse(checked); }
    });

    connect(ui->warpAlphaSpinBox, &QDoubleSpinBox::valueChanged, this, [=](double value) {
        if (DeformTool* t = deformTool(mTransformTool)) { t->setWarpAlpha(value); }
    });

    connect(ui->warpTypeComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        // the combo lists affine/similitude/rigid in Krita's enum order
        if (DeformTool* t = deformTool(mTransformTool)) { t->setWarpType(index); }
    });

    connect(ui->selectionActionComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        LassoTool* lassoTool = dynamic_cast<LassoTool*>(mTransformTool);
        if (lassoTool != nullptr && lassoTool->type() == LASSO) {
            lassoTool->setSelectionAction(index);
        }
    });

    connect(ui->selectionGrowSpinBox, &QSpinBox::valueChanged, this, [=](int value) {
        LassoTool* lassoTool = dynamic_cast<LassoTool*>(mTransformTool);
        if (lassoTool != nullptr && lassoTool->type() == LASSO) {
            lassoTool->setGrowValue(value);
        }
    });
}

void TransformOptionsWidget::makeConnectionFromModelToUI(TransformTool* transformTool)
{
    connect(transformTool, &TransformTool::showSelectionInfoChanged, this, &TransformOptionsWidget::setShowSelectionInfo);
    connect(transformTool, &TransformTool::antiAliasingChanged, this, &TransformOptionsWidget::setAntiAliasingEnabled);

    DeformTool* deformTool = dynamic_cast<DeformTool*>(transformTool);
    if (deformTool != nullptr && deformTool->type() == DEFORM) {
        connect(deformTool, &DeformTool::gridSizeChanged, this, &TransformOptionsWidget::setGridSize);
        connect(deformTool, &DeformTool::deformModeChanged, this, &TransformOptionsWidget::setDeformMode);
        connect(deformTool, &DeformTool::liquifyOpChanged, this, &TransformOptionsWidget::setLiquifyOp);
        connect(deformTool, &DeformTool::liquifySizeChanged, this, &TransformOptionsWidget::setLiquifySize);
        connect(deformTool, &DeformTool::liquifyAmountChanged, this, &TransformOptionsWidget::setLiquifyAmount);
        connect(deformTool, &DeformTool::liquifyReverseChanged, this, &TransformOptionsWidget::setLiquifyReverse);
        connect(deformTool, &DeformTool::warpAlphaChanged, this, &TransformOptionsWidget::setWarpAlpha);
        connect(deformTool, &DeformTool::warpTypeChanged, this, &TransformOptionsWidget::setWarpType);

        setGridSize(deformTool->gridSize());
        setDeformMode(deformTool->deformMode());
        setLiquifyOp(deformTool->liquifyOp());
        setLiquifySize(deformTool->liquifySize());
        setLiquifyAmount(deformTool->liquifyAmount());
        setLiquifyReverse(deformTool->liquifyReverse());
        setWarpAlpha(deformTool->warpAlpha());
        setWarpType(deformTool->warpType());
    }

    LassoTool* lassoTool = dynamic_cast<LassoTool*>(transformTool);
    if (lassoTool != nullptr && lassoTool->type() == LASSO) {
        connect(lassoTool, &LassoTool::selectionActionChanged, this, &TransformOptionsWidget::setSelectionAction);
        connect(lassoTool, &LassoTool::growValueChanged, this, &TransformOptionsWidget::setSelectionGrow);
        setSelectionAction(lassoTool->selectionAction());
        setSelectionGrow(lassoTool->growValue());
    }
}

void TransformOptionsWidget::setShowSelectionInfo(bool enabled)
{
    QSignalBlocker b(ui->showSelectionInfoCheckBox);
    ui->showSelectionInfoCheckBox->setChecked(enabled);
}

void TransformOptionsWidget::setAntiAliasingEnabled(bool enabled)
{
    QSignalBlocker b(ui->antiAliasingCheckBox);
    ui->antiAliasingCheckBox->setChecked(enabled);
}

void TransformOptionsWidget::setGridSize(int size)
{
    QSignalBlocker b(ui->gridSizeSpinBox);
    ui->gridSizeSpinBox->setValue(size);
}

void TransformOptionsWidget::setDeformMode(int mode)
{
    QSignalBlocker b(ui->deformModeComboBox);
    ui->deformModeComboBox->setCurrentIndex(mode);
    ui->modeStackedWidget->setCurrentIndex(mode);
}

void TransformOptionsWidget::setLiquifyOp(int op)
{
    QSignalBlocker b(ui->liquifyOpComboBox);
    ui->liquifyOpComboBox->setCurrentIndex(op);
}

void TransformOptionsWidget::setLiquifySize(int size)
{
    QSignalBlocker b(ui->liquifySizeSpinBox);
    ui->liquifySizeSpinBox->setValue(size);
}

void TransformOptionsWidget::setLiquifyAmount(qreal amount)
{
    QSignalBlocker b(ui->liquifyAmountSpinBox);
    ui->liquifyAmountSpinBox->setValue(amount);
}

void TransformOptionsWidget::setLiquifyReverse(bool reverse)
{
    QSignalBlocker b(ui->liquifyReverseCheckBox);
    ui->liquifyReverseCheckBox->setChecked(reverse);
}

void TransformOptionsWidget::setWarpAlpha(qreal alpha)
{
    QSignalBlocker b(ui->warpAlphaSpinBox);
    ui->warpAlphaSpinBox->setValue(alpha);
}

void TransformOptionsWidget::setWarpType(int type)
{
    QSignalBlocker b(ui->warpTypeComboBox);
    ui->warpTypeComboBox->setCurrentIndex(type);
}

void TransformOptionsWidget::setSelectionAction(int action)
{
    QSignalBlocker b(ui->selectionActionComboBox);
    ui->selectionActionComboBox->setCurrentIndex(action);
}

void TransformOptionsWidget::setSelectionGrow(int grow)
{
    QSignalBlocker b(ui->selectionGrowSpinBox);
    ui->selectionGrowSpinBox->setValue(grow);
}
