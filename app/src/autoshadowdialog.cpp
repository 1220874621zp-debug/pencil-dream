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
#include "autoshadowdialog.h"

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

AutoShadowDialog::AutoShadowDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("自动上阴影"));
    setModal(true);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(6);

    // 滑杆+输入框参数行（数值守卫防环，输入框是唯一数据源——同 ColorToAlphaDialog 范式）
    mAngleSpin = addSliderRow(tr("光源角度："), 0, 359, 135,
        tr("光源方向（度）：0=右 90=上 135=左上 180=左 270=下。默认左上光。"));
    mDistanceSpin = addSliderRow(tr("光源距离："), 100, 5000, 600,
        tr("光源到画面中心的距离（像素）：越远光线越平行，拉到最大（5000）即为平行光。"));
    mRangeSpin = addSliderRow(tr("阴影范围："), 4, 120, 40,
        tr("背光多深才出现阴影（像素）：沿光线方向累计的材料厚度达到该值的像素进入阴影。"
           "值越大阴影越收敛到深凹处。"));
    mOpacitySpin = addSliderRow(tr("阴影浓度："), 10, 100, 45,
        tr("阴影色以正片叠底方式叠加的强度（百分比）。"));
    mChokeSpin = addSliderRow(tr("阻塞："), 0, 8, 2,
        tr("阴影边界向外膨胀的像素数：让阴影咬进线条、不留亮缝，也不会溢出内容边界。"));

    // 阴影颜色
    auto* colorLayout = new QGridLayout;
    colorLayout->setHorizontalSpacing(8);
    colorLayout->setVerticalSpacing(2);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    auto* colorLabel = new QLabel(tr("阴影颜色："), this);
    colorLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    colorLayout->addWidget(colorLabel, 0, 0);
    mColorButton = new QPushButton(this);
    mColorButton->setToolTip(tr("点击选择阴影色（正片叠底，建议选偏紫/偏蓝的暗色）"));
    mColorButton->setAutoDefault(false);
    connect(mColorButton, &QPushButton::clicked, this, &AutoShadowDialog::pickShadowColor);
    colorLayout->addWidget(mColorButton, 1, 0);
    rootLayout->addLayout(colorLayout);
    updateColorButton();

    // 阴影层级：单层/双层（双层=更深的凹陷压得更暗）
    auto* levelBox = new QGroupBox(tr("阴影层级"), this);
    auto* levelLayout = new QVBoxLayout(levelBox);
    mSingleLevelRadio = new QRadioButton(tr("单层阴影"), levelBox);
    mSingleLevelRadio->setChecked(true);
    mTwoLevelRadio = new QRadioButton(tr("双层阴影（深凹处更暗）"), levelBox);
    levelLayout->addWidget(mSingleLevelRadio);
    levelLayout->addWidget(mTwoLevelRadio);

    mSecondLabel = new QLabel(tr("第二层阈值："), levelBox);
    mSecondLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    levelLayout->addWidget(mSecondLabel);
    mSecondSlider = new QSlider(Qt::Horizontal, levelBox);
    mSecondSlider->setRange(6, 200);
    mSecondSlider->setValue(64);
    mSecondSpin = new QDoubleSpinBox(levelBox);
    mSecondSpin->setDecimals(0);
    mSecondSpin->setRange(6, 200);
    mSecondSpin->setValue(64);
    mSecondSpin->setFixedWidth(96);
    mSecondSpin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    mSecondSpin->setToolTip(tr("材料厚度达到该值的像素压到第二档（更暗），须大于阴影范围。"));
    auto* secondLayout = new QGridLayout;
    secondLayout->setHorizontalSpacing(8);
    secondLayout->setContentsMargins(0, 0, 0, 0);
    secondLayout->addWidget(mSecondSlider, 0, 0);
    secondLayout->addWidget(mSecondSpin, 0, 1);
    levelLayout->addLayout(secondLayout);
    rootLayout->addWidget(levelBox);

    connect(mSecondSlider, &QSlider::valueChanged, this, [this](const int value) {
        if (qRound(mSecondSpin->value()) != value)
            mSecondSpin->setValue(value);
    });
    connect(mSecondSpin, &QDoubleSpinBox::valueChanged, this, [this](const double value) {
        const int pos = qRound(value);
        if (mSecondSlider->value() != pos)
            mSecondSlider->setValue(pos);
    });
    const auto syncSecondEnabled = [this] {
        const bool twoLevel = mTwoLevelRadio->isChecked();
        mSecondLabel->setEnabled(twoLevel);
        mSecondSlider->setEnabled(twoLevel);
        mSecondSpin->setEnabled(twoLevel);
    };
    connect(mSingleLevelRadio, &QRadioButton::toggled, this, syncSecondEnabled);
    connect(mTwoLevelRadio, &QRadioButton::toggled, this, syncSecondEnabled);
    syncSecondEnabled();

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

AutoShadowParams AutoShadowDialog::params() const
{
    AutoShadowParams p;
    p.lightAngle = qRound(mAngleSpin->value());
    p.lightDistance = qRound(mDistanceSpin->value());
    p.shadowRange = qRound(mRangeSpin->value());
    p.secondLevelRange = mTwoLevelRadio->isChecked() ? qRound(mSecondSpin->value()) : 0;
    p.shadowColor = mShadowColor;
    p.shadowOpacity = qRound(mOpacitySpin->value());
    p.choke = qRound(mChokeSpin->value());
    return p;
}

bool AutoShadowDialog::applyToAllKeyFrames() const
{
    return mAllKeyFramesRadio->isChecked();
}

QDoubleSpinBox* AutoShadowDialog::addSliderRow(const QString& labelText, const int minV, const int maxV,
                                               const int defV, const QString& tip)
{
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(labelText, this);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(label, 0, 0);

    auto* slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(minV, maxV);
    slider->setValue(defV);
    auto* spin = new QDoubleSpinBox(this);
    spin->setDecimals(0);
    spin->setRange(minV, maxV);
    spin->setValue(defV);
    spin->setFixedWidth(96);
    spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    spin->setToolTip(tip);

    grid->addWidget(slider, 1, 0);
    grid->addWidget(spin, 0, 1, 2, 1);
    grid->setColumnStretch(0, 1);
    static_cast<QVBoxLayout*>(layout())->addLayout(grid);

    connect(slider, &QSlider::valueChanged, this, [spin](const int value) {
        if (qRound(spin->value()) != value)
            spin->setValue(value);
    });
    connect(spin, &QDoubleSpinBox::valueChanged, this, [slider](const double value) {
        const int pos = qRound(value);
        if (slider->value() != pos)
            slider->setValue(pos);
    });
    return spin;
}

void AutoShadowDialog::pickShadowColor()
{
    const QColor picked = QColorDialog::getColor(QColor(mShadowColor), this, tr("选择阴影颜色"));
    if (picked.isValid())
    {
        mShadowColor = picked.rgb();
        updateColorButton();
    }
}

void AutoShadowDialog::updateColorButton()
{
    const QColor c(mShadowColor);
    mColorButton->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #888888;")
                                    .arg(c.name()));
    mColorButton->setText(c.name().toUpper());
}
