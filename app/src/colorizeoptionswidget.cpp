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
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "editor.h"
#include "layer.h"
#include "layercolorize.h"
#include "colorizeimage.h"
#include "layermanager.h"
#include "scribblearea.h"

ColorizeOptionsWidget::ColorizeOptionsWidget(Editor* editor, QWidget* parent)
    : BaseWidget(parent), mEditor(editor)
{
}

void ColorizeOptionsWidget::initUI()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    auto* titleLabel = new QLabel(tr("Colorize Mask"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 0.5);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

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

    mCleanUpSpin = new QSpinBox(this);
    mCleanUpSpin->setRange(0, 100);
    mCleanUpSpin->setSingleStep(5);
    mCleanUpSpin->setSuffix(" %");

    auto* form = new QVBoxLayout;
    form->setSpacing(4);
    auto addRow = [this, &form](QWidget* field, const char* label, int indent = 0) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        if (indent > 0)
            row->setContentsMargins(indent, 0, 0, 0);
        row->addWidget(new QLabel(tr(label), this));
        row->addStretch();
        row->addWidget(field);
        form->addLayout(row);
    };
    addRow(mEdgeSizeSpin, QT_TRANSLATE_NOOP("ColorizeOptionsWidget", "Edge size"), 16);
    addRow(mFuzzyRadiusSpin, QT_TRANSLATE_NOOP("ColorizeOptionsWidget", "Gap closing radius"));
    addRow(mCleanUpSpin, QT_TRANSLATE_NOOP("ColorizeOptionsWidget", "Cleanup strength"));
    rootLayout->addLayout(form);

    auto* hintLabel = new QLabel(tr("Paint color strokes with the brush; erase strokes to keep areas empty."), this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: gray;");
    rootLayout->addWidget(hintLabel);

    connect(mEdgeDetectionCheck, &QCheckBox::toggled, this, &ColorizeOptionsWidget::applyAndRefresh);
    connect(mEdgeSizeSpin, &QDoubleSpinBox::valueChanged, this, &ColorizeOptionsWidget::applyAndRefresh);
    connect(mFuzzyRadiusSpin, &QDoubleSpinBox::valueChanged, this, &ColorizeOptionsWidget::applyAndRefresh);
    connect(mCleanUpSpin, &QSpinBox::valueChanged, this, &ColorizeOptionsWidget::applyAndRefresh);

    updateUI();
}

void ColorizeOptionsWidget::updateUI()
{
    LayerColorize* layer = currentColorizeLayer();

    const QSignalBlocker b1(mEdgeDetectionCheck);
    const QSignalBlocker b2(mEdgeSizeSpin);
    const QSignalBlocker b3(mFuzzyRadiusSpin);
    const QSignalBlocker b4(mCleanUpSpin);

    if (layer == nullptr)
        return;

    mEdgeDetectionCheck->setChecked(layer->useEdgeDetection());
    mEdgeSizeSpin->setValue(layer->edgeDetectionSize());
    mEdgeSizeSpin->setEnabled(layer->useEdgeDetection());
    mFuzzyRadiusSpin->setValue(layer->fuzzyRadius());
    mCleanUpSpin->setValue(qRound(layer->cleanUpAmount() * 100.0));
}

LayerColorize* ColorizeOptionsWidget::currentColorizeLayer() const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || layer->type() != Layer::COLORIZE)
        return nullptr;
    return static_cast<LayerColorize*>(layer);
}

void ColorizeOptionsWidget::applyAndRefresh()
{
    LayerColorize* layer = currentColorizeLayer();
    if (layer == nullptr)
        return;

    layer->setUseEdgeDetection(mEdgeDetectionCheck->isChecked());
    layer->setEdgeDetectionSize(mEdgeSizeSpin->value());
    layer->setFuzzyRadius(mFuzzyRadiusSpin->value());
    layer->setCleanUpAmount(mCleanUpSpin->value() / 100.0);

    mEdgeSizeSpin->setEnabled(layer->useEdgeDetection());

    // 参数影响全部帧：整层失效，渲染懒触发会重算当前帧
    layer->foreachKeyFrame([](KeyFrame* key)
    {
        if (auto* frame = static_cast<ColorizeImage*>(key))
            frame->setNeedsUpdate(true);
    });

    if (mEditor->getScribbleArea() != nullptr)
    {
        mEditor->getScribbleArea()->updateFrame();
    }
}
