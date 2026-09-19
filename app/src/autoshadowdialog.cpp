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
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace
{

constexpr int PREVIEW_W = 360; // 预览框最大宽（像素）
constexpr int PREVIEW_H = 300; // 预览框最大高（像素）

/** 像素参数按预览缩放同比（光源/阈值/羽化是归一化或场值单位，不随缩放） */
AutoShadowParams scaledForPreview(const AutoShadowParams& p, const double s)
{
    AutoShadowParams q = p;
    q.shadowDistance = std::max(1, qRound(p.shadowDistance * s));
    q.shadowSize = qRound(p.shadowSize * s);
    q.chokeMatte = qRound(p.chokeMatte * s);
    return q;
}

} // namespace

/** PS 色阶语义的阈值条：色带渐变 + 三个可拖动滑块（三角游标+数值）。
    点色段=改该色阶颜色；拖游标=改阈值。回调式，不依赖 moc。 */
class LevelsBar : public QWidget
{
public:
    LevelsBar(QWidget* parent, std::function<void()> onChanged, std::function<void(int)> onSegmentClick)
        : QWidget(parent)
        , mOnChanged(std::move(onChanged))
        , mOnSegmentClick(std::move(onSegmentClick))
    {
        setMouseTracking(true);
        setMinimumHeight(52);
        setCursor(Qt::PointingHandCursor);
    }

    void setThresholds(const int t[3])
    {
        for (int i = 0; i < 3; ++i)
            mThresholds[i] = t[i];
        update();
    }

    void setLevels(const AutoShadowLevel levels[4], const bool smooth)
    {
        for (int i = 0; i < 4; ++i)
            mLevels[i] = levels[i];
        mSmooth = smooth;
        update();
    }

    int thresholds(int i) const { return mThresholds[i]; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const int m = 10;                      // 左右留白
        const int barW = width() - 2 * m;
        const int barY = 6;
        const int barH = 16;

        // 色带：色调分离=分阶色段；平滑=连续渐变
        if (mSmooth)
        {
            QLinearGradient grad(m, 0, m + barW, 0);
            grad.setColorAt(0.0, QColor(mLevels[0].color));
            grad.setColorAt(mThresholds[0] / 100.0, QColor(mLevels[1].color));
            grad.setColorAt(mThresholds[1] / 100.0, QColor(mLevels[2].color));
            grad.setColorAt(mThresholds[2] / 100.0, QColor(mLevels[3].color));
            grad.setColorAt(1.0, QColor(mLevels[3].color));
            p.fillRect(m, barY, barW, barH, grad);
        }
        else
        {
            const int edges[5] = { 0, mThresholds[0], mThresholds[1], mThresholds[2], 100 };
            for (int i = 0; i < 4; ++i)
            {
                const int x0 = m + edges[i] * barW / 100;
                const int x1 = m + edges[i + 1] * barW / 100;
                p.fillRect(x0, barY, x1 - x0, barH, QColor(mLevels[i].color));
            }
        }
        p.setPen(QPen(QColor(120, 120, 120), 1));
        p.drawRect(m, barY, barW - 1, barH - 1);

        // 游标：三角+数值（PS 语义）
        QFont smallFont = font();
        smallFont.setPointSizeF(std::max(7.0, font().pointSizeF() - 2.0));
        p.setFont(smallFont);
        for (int i = 0; i < 3; ++i)
        {
            const int x = markerX(i);
            p.setPen(Qt::NoPen);
            p.setBrush(i == mDragIndex ? QColor(35, 131, 212) : QColor(85, 85, 85));
            p.drawPolygon(QPolygon() << QPoint(x - 5, barY + barH + 2)
                                      << QPoint(x + 5, barY + barH + 2)
                                      << QPoint(x, barY + barH - 4));
            p.setPen(QColor(160, 160, 160));
            p.drawText(QRect(x - 16, barY + barH + 6, 32, 14), Qt::AlignHCenter | Qt::AlignTop,
                       QString::number(mThresholds[i]));
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        const int hit = hitMarker(event->pos().x());
        if (hit >= 0)
        {
            mDragIndex = hit;
            update();
            return;
        }
        // 点在色段上：改该段色阶的颜色
        const int t = posToValue(event->pos().x());
        int level = 3;
        if (t < mThresholds[0]) level = 0;
        else if (t < mThresholds[1]) level = 1;
        else if (t < mThresholds[2]) level = 2;
        if (mOnSegmentClick)
            mOnSegmentClick(level);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (mDragIndex < 0)
        {
            setCursor(hitMarker(event->pos().x()) >= 0 ? Qt::SizeAllCursor : Qt::PointingHandCursor);
            return;
        }
        const int lo = mDragIndex == 0 ? 1 : mThresholds[mDragIndex - 1] + 1;
        const int hi = mDragIndex == 2 ? 100 : mThresholds[mDragIndex + 1] - 1;
        const int t = std::min(hi, std::max(lo, posToValue(event->pos().x())));
        if (t != mThresholds[mDragIndex])
        {
            mThresholds[mDragIndex] = t;
            update();
            if (mOnChanged)
                mOnChanged();
        }
    }

    void mouseReleaseEvent(QMouseEvent*) override
    {
        if (mDragIndex >= 0)
        {
            mDragIndex = -1;
            update();
        }
    }

private:
    int barLeft() const { return 10; }
    int barWidth() const { return width() - 2 * barLeft(); }
    int markerX(const int i) const { return barLeft() + mThresholds[i] * barWidth() / 100; }
    int posToValue(const int x) const
    {
        return std::min(100, std::max(0, (x - barLeft()) * 100 / std::max(1, barWidth())));
    }
    int hitMarker(const int x) const
    {
        for (int i = 0; i < 3; ++i)
        {
            if (std::abs(x - markerX(i)) <= 7)
                return i;
        }
        return -1;
    }

    int mThresholds[3] = { 20, 45, 80 };
    AutoShadowLevel mLevels[4];
    bool mSmooth = false;
    int mDragIndex = -1;
    std::function<void()> mOnChanged;
    std::function<void(int)> mOnSegmentClick;
};

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
    mPreviewTimer->setInterval(90);
    connect(mPreviewTimer, &QTimer::timeout, this, &AutoShadowDialog::renderPreview);

    auto* rootLayout = new QHBoxLayout(this);
    mParamColumn = new QVBoxLayout;
    auto* previewColumn = new QVBoxLayout;
    rootLayout->addLayout(mParamColumn, 1);
    rootLayout->addLayout(previewColumn, 0);

    mParamColumn->setSpacing(6);

    // ── 场生成段参数：光源=预览框点/拖定位 + 黑透白不透掩膜 + 内阴影距离/大小 ──
    QDoubleSpinBox* lightXSpin = nullptr;
    QSlider* lightXSlider = nullptr;
    addSliderRow(tr("光源 X："), -100, 200, 15,
        tr("光源水平位置（画面宽度的百分比，0=左缘 100=右缘，可拉出画面放远光）。也可直接在预览框里点击/拖拽定位。"),
        tr("%"), lightXSpin, lightXSlider);
    mLightXSpin = lightXSpin;

    QDoubleSpinBox* lightYSpin = nullptr;
    QSlider* lightYSlider = nullptr;
    addSliderRow(tr("光源 Y："), -100, 200, 5,
        tr("光源垂直位置（画面高度的百分比，0=上缘 100=下缘；负值=画面上方光源）。"),
        tr("%"), lightYSpin, lightYSlider);
    mLightYSpin = lightYSpin;

    QDoubleSpinBox* thresholdSpin = nullptr;
    QSlider* thresholdSlider = nullptr;
    addSliderRow(tr("去色阈值："), 1, 254, 128,
        tr("黑透白不透：图像去色后灰度≥该值为不透明白（受光填色面），低于为透明黑——线稿与深色区成为掩膜上的洞，内阴影沿这些边界生长。"),
        QString(), thresholdSpin, thresholdSlider);
    mThresholdSpin = thresholdSpin;

    QDoubleSpinBox* chokeSpin = nullptr;
    QSlider* chokeSlider = nullptr;
    addSliderRow(tr("阻塞遮罩："), -20, 20, 0,
        tr("简单阻塞（AE 语义）：以小增量收缩或扩展掩膜边缘，得到更整洁的掩膜。正值阻塞（收缩白区，吃掉白边与细丝），负值扩展（并掉小黑洞）。配合遮罩视图调最直观。"),
        tr(" px"), chokeSpin, chokeSlider);
    mChokeSpin = chokeSpin;

    QDoubleSpinBox* distanceSpin = nullptr;
    QSlider* distanceSlider = nullptr;
    addSliderRow(tr("内阴影距离："), 1, 60, 16,
        tr("阴影带深入形体的宽度（像素）：外轮廓远光侧月牙、线槽贴线阴影都由它决定，类似 PS 内阴影的距离。"),
        tr(" px"), distanceSpin, distanceSlider);
    mDistanceSpin = distanceSpin;

    QDoubleSpinBox* sizeSpin = nullptr;
    QSlider* sizeSlider = nullptr;
    addSliderRow(tr("内阴影大小："), 0, 40, 8,
        tr("阴影边界的模糊半径（像素）：0=硬边，越大越软；色阶阈值会在渐变上切出多层断层。"),
        tr(" px"), sizeSpin, sizeSlider);
    mSizeSpin = sizeSpin;

    QSlider* featherSlider = nullptr;
    addSliderRow(tr("边缘羽化："), 0, 50, 0,
        tr("色调分离模式下色阶边界的过渡带宽（场值单位）：0=硬边赛璐璐，越大越软。平滑阴影模式下无效。"),
        QString(), mFeatherSpin, featherSlider);
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

    // 色阶阈值：PS 语义渐变条拖块（点色段=改该阶颜色）
    auto* levelBox = new QGroupBox(tr("色阶（近光→背光）"), this);
    auto* levelLayout = new QVBoxLayout(levelBox);
    mLevelsBar = new LevelsBar(levelBox,
                               [this] { schedulePreview(); },
                               [this](const int levelIndex) { pickLevelColor(levelIndex); });
    mLevelsBar->setToolTip(tr("拖动三角游标调整色阶阈值（场值 0-100）；点击色段直接修改该色阶颜色。"));
    levelLayout->addWidget(mLevelsBar);

    auto* levelRows = new QGridLayout;
    levelRows->setHorizontalSpacing(8);
    levelRows->setVerticalSpacing(2);
    const char* modeNames[3] = { "正常", "正片叠底", "线性加深" };
    for (int i = 0; i < 4; ++i)
    {
        auto* levelLabel = new QLabel(tr("色阶 %1：").arg(i + 1), levelBox);
        levelRows->addWidget(levelLabel, i, 0);

        mLevelButtons[i] = new QPushButton(levelBox);
        mLevelButtons[i]->setAutoDefault(false);
        mLevelButtons[i]->setToolTip(tr("该色阶的颜色。乘性混合下选越浅的颜色该阶越淡。"));
        const int levelIndex = i;
        connect(mLevelButtons[i], &QPushButton::clicked, this, [this, levelIndex] {
            pickLevelColor(levelIndex);
        });
        levelRows->addWidget(mLevelButtons[i], i, 1);
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
        levelRows->addWidget(mLevelCombos[i], i, 2);
    }
    levelRows->setColumnStretch(1, 1);
    levelLayout->addLayout(levelRows);
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

    // 预览框（右栏）：点击/拖拽定位光源，按钮切换对比原图
    auto* previewBox = new QGroupBox(tr("预览"), this);
    auto* previewLayout = new QVBoxLayout(previewBox);
    mPreviewLabel = new QLabel(previewBox);
    mPreviewLabel->setMinimumSize(PREVIEW_W, PREVIEW_H);
    mPreviewLabel->setAlignment(Qt::AlignCenter);
    mPreviewLabel->setCursor(Qt::CrossCursor);
    mPreviewLabel->setToolTip(tr("点击或拖拽定位光源（黄色标记），阴影实时跟随；对比请用下方按钮。"));
    mPreviewLabel->installEventFilter(this);
    previewLayout->addWidget(mPreviewLabel);
    auto* viewRow = new QHBoxLayout;
    auto* viewLabel = new QLabel(tr("视图："), previewBox);
    viewRow->addWidget(viewLabel);
    mViewCombo = new QComboBox(previewBox);
    mViewCombo->addItem(tr("最终输出"));
    mViewCombo->addItem(tr("遮罩视图"));
    mViewCombo->setToolTip(tr("遮罩视图=黑白图：白=不透明（受光面），黑=透明（线稿槽/洞）。调去色阈值与阻塞时切过来看最直观。"));
    connect(mViewCombo, &QComboBox::currentIndexChanged, this, [this](int) { renderPreview(); });
    viewRow->addWidget(mViewCombo, 1);
    mCompareButton = new QPushButton(tr("按住对比原图"), previewBox);
    mCompareButton->setCheckable(true);
    mCompareButton->setAutoDefault(false);
    connect(mCompareButton, &QPushButton::toggled, this, [this] { renderPreview(); });
    viewRow->addWidget(mCompareButton);
    previewLayout->addLayout(viewRow);
    previewColumn->addWidget(previewBox);
    previewColumn->addStretch(1);

    grabPreviewSource();
    renderPreview();
}

AutoShadowParams AutoShadowDialog::params() const
{
    AutoShadowParams p;
    p.lightX = mLightXSpin->value() / 100.0;
    p.lightY = mLightYSpin->value() / 100.0;
    p.maskThreshold = qRound(mThresholdSpin->value());
    p.chokeMatte = qRound(mChokeSpin->value());
    p.shadowDistance = qRound(mDistanceSpin->value());
    p.shadowSize = qRound(mSizeSpin->value());
    for (int i = 0; i < 3; ++i)
        p.thresholds[i] = mLevelsBar->thresholds(i);
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
    if (watched == mPreviewLabel)
    {
        const QMouseEvent* mouseEvent = dynamic_cast<const QMouseEvent*>(event);
        if (mouseEvent != nullptr
            && event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton)
        {
            mDraggingLight = true;
            setLightFromPreview(mouseEvent->position().toPoint());
            return true;
        }
        if (mouseEvent != nullptr && event->type() == QEvent::MouseMove && mDraggingLight)
        {
            setLightFromPreview(mouseEvent->position().toPoint());
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease)
        {
            mDraggingLight = false;
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void AutoShadowDialog::setLightFromPreview(const QPoint& pos)
{
    if (mScaledSource.isNull() || mLightXSpin == nullptr)
        return;
    // label 内图像按 AlignCenter 居中，先换算到图像坐标再归一化
    const int lw = mPreviewLabel->width();
    const int lh = mPreviewLabel->height();
    const int iw = mScaledSource.width();
    const int ih = mScaledSource.height();
    const double u = std::min(1.0, std::max(0.0, (pos.x() - (lw - iw) / 2.0) / iw));
    const double v = std::min(1.0, std::max(0.0, (pos.y() - (lh - ih) / 2.0) / ih));
    mLightXSpin->setValue(qRound(u * 100.0));
    mLightYSpin->setValue(qRound(v * 100.0));
    renderPreview(); // 拖拽即时回显（光源标记），阴影防抖由 valueChanged→schedulePreview 处理
}

void AutoShadowDialog::addSliderRow(const QString& labelText, const int minV, const int maxV,
                                    const int defV, const QString& tip, const QString& suffix,
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
    spin->setSuffix(suffix);
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
        syncLevelsBar();
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

void AutoShadowDialog::syncLevelsBar()
{
    mLevelsBar->setLevels(mLevels, mTypeCombo->currentIndex() == 1);
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
    syncLevelsBar();

    if (mScaledSource.isNull())
    {
        mPreviewLabel->setPixmap(QPixmap());
        mPreviewLabel->setText(tr("当前帧没有可预览的位图内容。"));
        return;
    }

    if (mCompareButton != nullptr && mCompareButton->isChecked())
    {
        mPreviewLabel->setPixmap(QPixmap::fromImage(mScaledSource));
        return;
    }

    const AutoShadowParams p = scaledForPreview(params(), mPreviewScale);

    // 遮罩视图：黑透白不透 + 简单阻塞后的黑白掩膜
    if (mViewCombo != nullptr && mViewCombo->currentIndex() == 1)
    {
        mPreviewLabel->setPixmap(QPixmap::fromImage(AutoShadow::renderMattePreview(mScaledSource, p)));
        return;
    }

    QImage preview = mScaledSource; // COW 拷贝，apply 就地改写
    AutoShadow::apply(preview, p);

    // 画光源标记（黄圈+十字），只在画面范围内显示
    const int mx = qRound(p.lightX * preview.width());
    const int my = qRound(p.lightY * preview.height());
    if (mx >= -8 && mx <= preview.width() + 8 && my >= -8 && my <= preview.height() + 8)
    {
        QPainter painter(&preview);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(255, 220, 60), 2));
        painter.setBrush(QColor(255, 220, 60, 170));
        painter.drawEllipse(QPoint(mx, my), 7, 7);
        painter.setPen(QPen(QColor(255, 250, 200), 1));
        painter.drawLine(mx - 11, my, mx - 4, my);
        painter.drawLine(mx + 4, my, mx + 11, my);
        painter.drawLine(mx, my - 11, mx, my - 4);
        painter.drawLine(mx, my + 4, mx, my + 11);
    }
    mPreviewLabel->setPixmap(QPixmap::fromImage(preview));
}
