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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
#include "colortoalphadialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>

ColorToAlphaDialog::ColorToAlphaDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("颜色转为透明度"));
    setModal(true);

    auto* rootLayout = new QVBoxLayout(this);

    // 目标颜色
    auto* colorLayout = new QGridLayout;
    colorLayout->setHorizontalSpacing(8);
    colorLayout->setVerticalSpacing(2);
    colorLayout->setContentsMargins(0, 0, 0, 0);

    auto* colorLabel = new QLabel(tr("目标颜色："), this);
    colorLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    colorLayout->addWidget(colorLabel, 0, 0);

    mColorButton = new QPushButton(this);
    mColorButton->setToolTip(tr("点击选择要转为透明的颜色（默认白色：白底扫描件去底提线）"));
    mColorButton->setAutoDefault(false);
    connect(mColorButton, &QPushButton::clicked, this, &ColorToAlphaDialog::pickTargetColor);
    colorLayout->addWidget(mColorButton, 1, 0);

    rootLayout->addLayout(colorLayout);
    updateColorButton();

    // 阈值（滑杆+输入框参数行）
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    auto* thresholdLabel = new QLabel(tr("阈值："), this);
    thresholdLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(thresholdLabel, 0, 0);

    mThresholdSlider = new QSlider(Qt::Horizontal, this);
    mThresholdSlider->setRange(1, 255);
    mThresholdSlider->setValue(100);

    mThresholdSpin = new QDoubleSpinBox(this);
    mThresholdSpin->setDecimals(0);
    mThresholdSpin->setRange(1, 255);
    mThresholdSpin->setValue(100);
    mThresholdSpin->setFixedWidth(96);
    mThresholdSpin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    mThresholdSpin->setToolTip(tr("颜色容差（感知色差 ΔE）：与目标色差值达到阈值的像素全保留，"
                                  "差值越小越透明。阈值过大会连浅色线条一起淡化。"));

    grid->addWidget(mThresholdSlider, 1, 0);
    grid->addWidget(mThresholdSpin, 0, 1, 2, 1);
    grid->setColumnStretch(0, 1);
    rootLayout->addLayout(grid);

    // 滑杆 ↔ 输入框双向同步（数值守卫防环，输入框是唯一数据源）
    connect(mThresholdSlider, &QSlider::valueChanged, this, [this](const int value) {
        if (qRound(mThresholdSpin->value()) != value)
            mThresholdSpin->setValue(value);
    });
    connect(mThresholdSpin, &QDoubleSpinBox::valueChanged, this, [this](const double value) {
        const int pos = qRound(value);
        if (mThresholdSlider->value() != pos)
            mThresholdSlider->setValue(pos);
    });

    // 作用范围
    auto* scopeBox = new QGroupBox(tr("作用范围"), this);
    auto* scopeLayout = new QVBoxLayout(scopeBox);
    mCurrentFrameRadio = new QRadioButton(tr("仅当前帧"), scopeBox);
    mCurrentFrameRadio->setChecked(true);
    mAllKeyFramesRadio = new QRadioButton(tr("当前图层全部关键帧"), scopeBox);
    scopeLayout->addWidget(mCurrentFrameRadio);
    scopeLayout->addWidget(mAllKeyFramesRadio);
    rootLayout->addWidget(scopeBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

ColorToAlphaParams ColorToAlphaDialog::params() const
{
    ColorToAlphaParams p;
    p.targetColor = mTargetColor;
    p.threshold = qRound(mThresholdSpin->value());
    return p;
}

bool ColorToAlphaDialog::applyToAllKeyFrames() const
{
    return mAllKeyFramesRadio->isChecked();
}

void ColorToAlphaDialog::pickTargetColor()
{
    const QColor picked = QColorDialog::getColor(QColor(mTargetColor), this, tr("选择目标颜色"));
    if (picked.isValid())
    {
        mTargetColor = picked.rgb();
        updateColorButton();
    }
}

void ColorToAlphaDialog::updateColorButton()
{
    const QColor c(mTargetColor);
    mColorButton->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #888888;")
                                    .arg(c.name()));
    mColorButton->setText(c.name().toUpper());
}
