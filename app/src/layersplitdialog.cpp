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
#include "layersplitdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>

LayerSplitDialog::LayerSplitDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("拆分图层颜色"));
    setModal(true);

    auto* rootLayout = new QVBoxLayout(this);

    // 模糊度（滑杆+输入框参数行）
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    auto* fuzzLabel = new QLabel(tr("颜色模糊度："), this);
    fuzzLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(fuzzLabel, 0, 0);

    mFuzzinessSlider = new QSlider(Qt::Horizontal, this);
    mFuzzinessSlider->setRange(0, 200);
    mFuzzinessSlider->setValue(20);

    mFuzzinessSpin = new QDoubleSpinBox(this);
    mFuzzinessSpin->setDecimals(0);
    mFuzzinessSpin->setRange(0, 200);
    mFuzzinessSpin->setValue(20);
    mFuzzinessSpin->setFixedWidth(96);
    mFuzzinessSpin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    mFuzzinessSpin->setToolTip(tr("感知色差（ΔE）容差：差值小于模糊度的颜色归入同一层。"
                                  "0=完全相同的颜色才同层；过大时不同色会被并成一层。"));

    grid->addWidget(mFuzzinessSlider, 1, 0);
    grid->addWidget(mFuzzinessSpin, 0, 1, 2, 1);
    grid->setColumnStretch(0, 1);
    rootLayout->addLayout(grid);

    // 滑杆 ↔ 输入框双向同步（数值守卫防环，输入框是唯一数据源）
    connect(mFuzzinessSlider, &QSlider::valueChanged, this, [this](const int value) {
        if (qRound(mFuzzinessSpin->value()) != value)
            mFuzzinessSpin->setValue(value);
    });
    connect(mFuzzinessSpin, &QDoubleSpinBox::valueChanged, this, [this](const double value) {
        const int pos = qRound(value);
        if (mFuzzinessSlider->value() != pos)
            mFuzzinessSlider->setValue(pos);
    });

    // 选项
    mDisregardOpacityCheck = new QCheckBox(tr("匹配时忽略透明度（半透明边按颜色归层）"), this);
    mDisregardOpacityCheck->setChecked(true);
    mSortLayersCheck = new QCheckBox(tr("按面积排序新图层（面积大者在上）"), this);
    mSortLayersCheck->setChecked(true);
    mHideOriginalCheck = new QCheckBox(tr("隐藏原始图层"), this);
    mHideOriginalCheck->setChecked(false);
    rootLayout->addWidget(mDisregardOpacityCheck);
    rootLayout->addWidget(mSortLayersCheck);
    rootLayout->addWidget(mHideOriginalCheck);

    // 作用范围
    auto* scopeBox = new QGroupBox(tr("作用范围"), this);
    auto* scopeLayout = new QVBoxLayout(scopeBox);
    mCurrentFrameRadio = new QRadioButton(tr("仅当前帧"), scopeBox);
    mCurrentFrameRadio->setChecked(true);
    mAllKeyFramesRadio = new QRadioButton(tr("当前图层全部关键帧（同色跨帧归同一层）"), scopeBox);
    scopeLayout->addWidget(mCurrentFrameRadio);
    scopeLayout->addWidget(mAllKeyFramesRadio);
    rootLayout->addWidget(scopeBox);

    auto* hint = new QLabel(tr("提示：适合平涂上色稿。照片/渐变类图像会拆出大量图层（上限 256 层）。"), this);
    hint->setWordWrap(true);
    rootLayout->addWidget(hint);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

LayerSplitParams LayerSplitDialog::params() const
{
    LayerSplitParams p;
    p.fuzziness = qRound(mFuzzinessSpin->value());
    p.disregardOpacity = mDisregardOpacityCheck->isChecked();
    p.sortLayers = mSortLayersCheck->isChecked();
    p.hideOriginal = mHideOriginalCheck->isChecked();
    return p;
}

bool LayerSplitDialog::applyToAllKeyFrames() const
{
    return mAllKeyFramesRadio->isChecked();
}
