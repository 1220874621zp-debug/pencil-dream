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

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
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
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

constexpr int PREVIEW_W = 360; // 预览框最大宽（像素）
constexpr int PREVIEW_H = 300; // 预览框最大高（像素）

/** 像素参数按预览缩放同比（角度/颜色/阈值/羽化是场值单位，不随缩放） */
AutoShadowParams scaledForPreview(const AutoShadowParams& p, const double s)
{
    AutoShadowParams q = p;
    q.displaceStrength = qRound(p.displaceStrength * s);
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

    // 默认色带：受光暖黄→橙→洋红→背光蓝紫（CSP 截图同款暖到冷序列）
    mLevels[0] = { qRgb(255, 244, 186), AutoShadowBlendMode::Multiply };
    mLevels[1] = { qRgb(255, 191, 128), AutoShadowBlendMode::Multiply };
    mLevels[2] = { qRgb(255, 92, 158), AutoShadowBlendMode::LinearBurn };
    mLevels[3] = { qRgb(96, 76, 176), AutoShadowBlendMode::Multiply };

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

    // ── 场生成段参数 ──
    QDoubleSpinBox* angleSpin = nullptr;
    QSlider* angleSlider = nullptr;
    addSliderRow(tr("光源角度："), 0, 359, 135,
        tr("光源方向（度）：0=右 90=上 135=左上 180=左 270=下。默认左上光。"), angleSpin, angleSlider);
    mAngleSpin = angleSpin;

    QDoubleSpinBox* distanceSpin = nullptr;
    QSlider* distanceSlider = nullptr;
    addSliderRow(tr("光源距离："), 100, 5000, 600,
        tr("光源到画面中心的距离（像素）：近=色阶边界弯成圆弧，远=接近平直。"), distanceSpin, distanceSlider);
    mDistanceSpin = distanceSpin;

    QDoubleSpinBox* displaceSpin = nullptr;
    QSlider* displaceSlider = nullptr;
    addSliderRow(tr("置换强度："), 0, 30, 8,
        tr("按原图亮度置换场采样点的强度（像素）：暗线处边界推移，色阶边界贴合线稿起伏而非完美圆弧。"),
        displaceSpin, displaceSlider);
    mDisplaceSpin = displaceSpin;

    QSlider* featherSlider = nullptr;
    addSliderRow(tr("边缘羽化："), 0, 50, 0,
        tr("色调分离模式下色阶边界的过渡带宽（场值单位）：0=硬边赛璐璐，越大越软。平滑阴影模式下无效。"),
        mFeatherSpin, featherSlider);
    mFeatherSlider = featherSlider;

    // ── 映射段参数（CSP 色调设置）──
    auto* typeRow = new QGridLayout;
    typeRow->setHorizontalSpacing(8);
    typeRow->setContentsMargins(0, 0, 0, 0);
    auto* typeLabel = new QLabel(tr("阴影类型："), this);
    typeRow->addWidget(typeLabel, 0, 0);
    mTypeCombo = new QComboBox(this);
    mTypeCombo->addItem(tr("色调分离阴影"));
    mTypeCombo->addItem(tr("平滑阴影"));
    mTypeCombo->setToolTip(tr("色调分离=按阈值切分硬边色阶（赛璐璐）；平滑=色带连续渐变映射。"));
    typeRow->addWidget(mTypeCombo, 0, 1);
    mParamColumn->addLayout(typeRow);

    mInvertCheck = new QCheckBox(tr("反转应用色阶的顺序"), this);
    mInvertCheck->setToolTip(tr("色带 1↔4 镜像：光源换到另一侧时无需重调四组颜色。"));
    connect(mInvertCheck, &QCheckBox::toggled, this, [this] { schedulePreview(); });
    mParamColumn->addWidget(mInvertCheck);

    // 色阶阈值：三游标切四阶（归一化场值 0..100）
    auto* thresholdRow = new QGridLayout;
    thresholdRow->setHorizontalSpacing(8);
    thresholdRow->setContentsMargins(0, 0, 0, 0);
    auto* thresholdLabel = new QLabel(tr("色阶阈值："), this);
    thresholdLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    thresholdRow->addWidget(thresholdLabel, 0, 0);
    const int thresholdDefaults[3] = { 20, 45, 80 };
    for (int i = 0; i < 3; ++i)
    {
        mThresholdSpins[i] = new QSpinBox(this);
        mThresholdSpins[i]->setRange(1, 100);
        mThresholdSpins[i]->setValue(thresholdDefaults[i]);
        mThresholdSpins[i]->setFixedWidth(64);
        mThresholdSpins[i]->setToolTip(tr("光场值（0=离光源最近，100=最远）超过该阈值进入下一色阶。三个阈值须递增，乱序会就近收敛。"));
        thresholdRow->addWidget(mThresholdSpins[i], 0, i + 1);
        connect(mThresholdSpins[i], &QSpinBox::valueChanged, this, [this](int) { schedulePreview(); });
    }
    thresholdRow->setColumnStretch(4, 1);
    mParamColumn->addLayout(thresholdRow);

    // 色阶颜色：4 行，每行 独立色 + 独立混合模式
    auto* levelBox = new QGroupBox(tr("色阶颜色（近光→背光）"), this);
    auto* levelLayout = new QGridLayout(levelBox);
    levelLayout->setHorizontalSpacing(8);
    levelLayout->setVerticalSpacing(2);
    const char* modeNames[3] = { "正常", "正片叠底", "线性加深" };
    for (int i = 0; i < 4; ++i)
    {
        auto* levelLabel = new QLabel(tr("色阶 %1：").arg(i + 1), levelBox);
        levelLayout->addWidget(levelLabel, i, 0);

        mLevelButtons[i] = new QPushButton(levelBox);
        mLevelButtons[i]->setAutoDefault(false);
        mLevelButtons[i]->setToolTip(tr("该色阶的颜色。乘性混合下选越浅的颜色该阶越淡。"));
        const int levelIndex = i;
        connect(mLevelButtons[i], &QPushButton::clicked, this, [this, levelIndex] {
            pickLevelColor(levelIndex);
        });
        levelLayout->addWidget(mLevelButtons[i], i, 1);
        updateLevelButton(i);

        mLevelCombos[i] = new QComboBox(levelBox);
        for (const char* modeName : modeNames)
            mLevelCombos[i]->addItem(tr(modeName));
        mLevelCombos[i]->setCurrentIndex(static_cast<int>(mLevels[i].mode));
        mLevelCombos[i]->setToolTip(tr("该色阶与原图的混合模式：正片叠底/线性加深=压暗保细节（CSP 常用），正常=直接换色。"));
        connect(mLevelCombos[i], &QComboBox::currentIndexChanged, this, [this, levelIndex](const int index) {
            mLevels[levelIndex].mode = static_cast<AutoShadowBlendMode>(index);
            schedulePreview();
        });
        levelLayout->addWidget(mLevelCombos[i], i, 2);
    }
    levelLayout->setColumnStretch(1, 1);
    mParamColumn->addWidget(levelBox);

    // 羽化只在色调分离模式下有意义
    const auto syncFeatherEnabled = [this] {
        const bool posterized = mTypeCombo->currentIndex() == 0;
        mFeatherSpin->setEnabled(posterized);
        mFeatherSlider->setEnabled(posterized);
    };
    connect(mTypeCombo, &QComboBox::currentIndexChanged, this, [this, syncFeatherEnabled] {
        syncFeatherEnabled();
        schedulePreview();
    });
    syncFeatherEnabled();

    // ── 作用范围 ──
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
    p.displaceStrength = qRound(mDisplaceSpin->value());
    for (int i = 0; i < 3; ++i)
        p.thresholds[i] = mThresholdSpins[i]->value();
    p.edgeFeather = qRound(mFeatherSpin->value());
    p.smooth = mTypeCombo->currentIndex() == 1;
    p.invertLevels = mInvertCheck->isChecked();
    for (int i = 0; i < 4; ++i)
        p.levels[i] = mLevels[i];
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

void AutoShadowDialog::addSliderRow(const QString& labelText, const int minV, const int maxV,
                                    const int defV, const QString& tip,
                                    QDoubleSpinBox*& spinOut, QSlider*& sliderOut)
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
    spinOut = spin;
    sliderOut = slider;
}

void AutoShadowDialog::pickLevelColor(const int levelIndex)
{
    const QColor picked = QColorDialog::getColor(QColor(mLevels[levelIndex].color), this, tr("选择色阶颜色"));
    if (picked.isValid())
    {
        mLevels[levelIndex].color = picked.rgb();
        updateLevelButton(levelIndex);
        schedulePreview();
    }
}

void AutoShadowDialog::updateLevelButton(const int levelIndex)
{
    const QColor c(mLevels[levelIndex].color);
    mLevelButtons[levelIndex]->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #888888;")
                                                 .arg(c.name()));
    mLevelButtons[levelIndex]->setText(c.name().toUpper());
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
