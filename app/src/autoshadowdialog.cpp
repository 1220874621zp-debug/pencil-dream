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

#include "bitmapimage.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

constexpr int PREVIEW_W = 360; // 预览框最大宽（像素）
constexpr int PREVIEW_H = 300; // 预览框最大高（像素）

/** 像素参数按预览缩放同比（角度/颜色/浓度不随缩放）；
    平行光语义保持（距离≥PARALLEL_DIST 不缩放，否则缩放后仍是点光的同比例光场） */
AutoShadowParams scaledForPreview(const AutoShadowParams& p, const double s)
{
    AutoShadowParams q = p;
    q.shadowRange = std::max(2, qRound(p.shadowRange * s));
    if (p.secondLevelRange > 0)
        q.secondLevelRange = std::max(q.shadowRange + 2, qRound(p.secondLevelRange * s));
    q.choke = qRound(p.choke * s);
    if (p.lightDistance < AutoShadow::PARALLEL_DIST)
        q.lightDistance = std::max(50, qRound(p.lightDistance * s));
    return q;
}

} // namespace

AutoShadowDialog::AutoShadowDialog(Editor* editor, QWidget* parent)
    : QDialog(parent)
    , mEditor(editor)
{
    setWindowTitle(tr("自动上阴影"));
    setModal(true);

    // 参数变动防抖：拖滑杆连续触发，只在停顿后重算一次预览
    mPreviewTimer = new QTimer(this);
    mPreviewTimer->setSingleShot(true);
    mPreviewTimer->setInterval(120);
    connect(mPreviewTimer, &QTimer::timeout, this, &AutoShadowDialog::renderPreview);

    auto* rootLayout = new QHBoxLayout(this);
    mParamColumn = new QVBoxLayout;
    auto* previewColumn = new QVBoxLayout;
    rootLayout->addLayout(mParamColumn, 1);
    rootLayout->addLayout(previewColumn, 0);

    mParamColumn->setSpacing(6);

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
    mParamColumn->addLayout(colorLayout);
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
    mParamColumn->addWidget(levelBox);

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
    connect(mTwoLevelRadio, &QRadioButton::toggled, this, [this] { schedulePreview(); });
    connect(mSecondSpin, &QDoubleSpinBox::valueChanged, this, [this](double) { schedulePreview(); });
    syncSecondEnabled();

    // 作用范围
    auto* scopeBox = new QGroupBox(tr("作用范围"), this);
    auto* scopeLayout = new QVBoxLayout(scopeBox);
    mCurrentFrameRadio = new QRadioButton(tr("仅当前帧"), scopeBox);
    mCurrentFrameRadio->setChecked(true);
    mAllKeyFramesRadio = new QRadioButton(tr("当前图层全部关键帧"), scopeBox);
    scopeLayout->addWidget(mCurrentFrameRadio);
    scopeLayout->addWidget(mAllKeyFramesRadio);
    mParamColumn->addWidget(scopeBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mParamColumn->addWidget(buttons);

    // 预览框（右栏）：当前帧实时预览，点击切原图对比
    auto* previewBox = new QGroupBox(tr("预览"), this);
    auto* previewLayout = new QVBoxLayout(previewBox);
    mPreviewLabel = new QLabel(previewBox);
    mPreviewLabel->setMinimumSize(PREVIEW_W, PREVIEW_H);
    mPreviewLabel->setAlignment(Qt::AlignCenter);
    mPreviewLabel->setCursor(Qt::PointingHandCursor);
    mPreviewLabel->setToolTip(tr("参数变化即时预览效果；点击切换显示原图对比。"));
    mPreviewLabel->installEventFilter(this);
    previewLayout->addWidget(mPreviewLabel);
    previewColumn->addWidget(previewBox);
    previewColumn->addStretch(1);

    grabPreviewSource();
    renderPreview();
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

bool AutoShadowDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mPreviewLabel && event->type() == QEvent::MouseButtonRelease)
    {
        mPreviewOriginal = !mPreviewOriginal;
        renderPreview();
        return true;
    }
    return QDialog::eventFilter(watched, event);
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
    mParamColumn->addLayout(grid);

    connect(slider, &QSlider::valueChanged, this, [spin](const int value) {
        if (qRound(spin->value()) != value)
            spin->setValue(value);
    });
    connect(spin, &QDoubleSpinBox::valueChanged, this, [slider](const double value) {
        const int pos = qRound(value);
        if (slider->value() != pos)
            slider->setValue(pos);
    });
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this](double) { schedulePreview(); });
    return spin;
}

void AutoShadowDialog::grabPreviewSource()
{
    mScaledSource = QImage();
    mPreviewScale = 1.0;
    if (mEditor == nullptr)
        return;

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || !layer->isBitmapKind())
        return;

    // 与画布落笔同源：循环层取显示帧背后的关键帧
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* bitmap = static_cast<BitmapImage*>(
        bitmapLayer->getKeyFrameWhichCovers(bitmapLayer->displayFrameFor(mEditor->currentFrame())));
    if (bitmap == nullptr || bitmap->image() == nullptr)
        return;

    QImage source = *bitmap->image(); // COW 浅拷贝，后续只读不动原图
    if (source.format() != QImage::Format_ARGB32_Premultiplied)
        source = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const double scale = std::min(1.0, std::min(static_cast<double>(PREVIEW_W) / source.width(),
                                                static_cast<double>(PREVIEW_H) / source.height()));
    if (scale < 1.0)
        mScaledSource = source.scaled(qMax(1, qRound(source.width() * scale)),
                                      qMax(1, qRound(source.height() * scale)),
                                      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    else
        mScaledSource = source;
    mPreviewScale = scale;
}

void AutoShadowDialog::schedulePreview()
{
    mPreviewTimer->start();
}

void AutoShadowDialog::renderPreview()
{
    if (mScaledSource.isNull())
    {
        mPreviewLabel->setPixmap(QPixmap());
        mPreviewLabel->setText(tr("当前帧没有可预览的位图内容。"));
        return;
    }

    if (mPreviewOriginal)
    {
        mPreviewLabel->setPixmap(QPixmap::fromImage(mScaledSource));
        return;
    }

    QImage preview = mScaledSource; // COW 拷贝，apply 就地改写
    AutoShadow::apply(preview, scaledForPreview(params(), mPreviewScale));
    mPreviewLabel->setPixmap(QPixmap::fromImage(preview));
}

void AutoShadowDialog::pickShadowColor()
{
    const QColor picked = QColorDialog::getColor(QColor(mShadowColor), this, tr("选择阴影颜色"));
    if (picked.isValid())
    {
        mShadowColor = picked.rgb();
        updateColorButton();
        schedulePreview();
    }
}

void AutoShadowDialog::updateColorButton()
{
    const QColor c(mShadowColor);
    mColorButton->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #888888;")
                                    .arg(c.name()));
    mColorButton->setText(c.name().toUpper());
}
