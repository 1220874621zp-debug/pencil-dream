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
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

namespace
{

constexpr int PREVIEW_W = 360; // 预览框最大宽（像素）
constexpr int PREVIEW_H = 300; // 预览框最大高（像素）

/** 像素参数按预览缩放同比（光源/阈值/强度/羽化是归一化或场值单位，不随缩放；
    体积高度是斜率放大（剖面同比缩、斜率不变）也不随缩放；
    部件半径/圆滑度/遮挡半径/排线间距随几何距离走，须同比——
    半椭球剖面形状由部件半径定义，R 与 σ 不同步缩放会让预览的丘形失真） */
AutoShadowParams scaledForPreview(const AutoShadowParams& p, const double s)
{
    AutoShadowParams q = p;
    q.formRadius = std::max(1, qRound(p.formRadius * s));
    q.formSmooth = std::max(0, qRound(p.formSmooth * s));
    q.occlusionStrength = std::max(0, qRound(p.occlusionStrength * s));
    q.hatchSpacing = std::max(1, qRound(p.hatchSpacing * s));
    q.chokeMatte = qRound(p.chokeMatte * s);
    return q;
}

/** 预设（CSP 预设语义）：一键整套光源列表 + 渐变强度 + 色带 + 阈值。
    id: 1=顺光（标准阴影默认） 2=逆光轮廓光（双光源） 3=夜晚 4=黄昏 5=风格化彩色 */
AutoShadowParams presetParams(const int id)
{
    AutoShadowParams p;
    if (id == 2)
    {
        AutoShadowLight a; a.x = 0.15; a.y = 0.30; a.height = 120;
        AutoShadowLight b; b.x = 0.85; b.y = 0.30; b.height = 120;
        p.lights = { a, b };
        p.gradientStrength = 20;
        p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
        p.levels[1] = { qRgb(255, 216, 172), AutoShadowBlendMode::Multiply };
        p.levels[2] = { qRgb(186, 142, 168), AutoShadowBlendMode::Multiply };
        p.levels[3] = { qRgb(92, 70, 108), AutoShadowBlendMode::Multiply };
    }
    else if (id == 3)
    {
        AutoShadowLight a; a.x = -0.10; a.y = -0.20; a.height = 200; a.intensity = 80;
        p.lights = { a };
        p.gradientStrength = 45;
        p.thresholds[0] = 18; p.thresholds[1] = 42; p.thresholds[2] = 72;
        p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
        p.levels[1] = { qRgb(168, 190, 255), AutoShadowBlendMode::Multiply };
        p.levels[2] = { qRgb(120, 140, 215), AutoShadowBlendMode::Multiply };
        p.levels[3] = { qRgb(58, 68, 130), AutoShadowBlendMode::Multiply };
    }
    else if (id == 4)
    {
        AutoShadowLight a; a.x = 1.10; a.y = 0.55; a.height = 70;
        p.lights = { a };
        p.gradientStrength = 50;
        p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
        p.levels[1] = { qRgb(255, 208, 150), AutoShadowBlendMode::Multiply };
        p.levels[2] = { qRgb(236, 148, 96), AutoShadowBlendMode::Multiply };
        p.levels[3] = { qRgb(150, 72, 62), AutoShadowBlendMode::Multiply };
    }
    else if (id == 5)
    {
        // 风格化彩色（旧默认色带，v4 照抄 CSP 截图的暖橙→洋红→蓝紫）
        p.gradientStrength = 30;
        p.levels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply };
        p.levels[1] = { qRgb(255, 191, 128), AutoShadowBlendMode::Multiply };
        p.levels[2] = { qRgb(255, 92, 158), AutoShadowBlendMode::LinearBurn };
        p.levels[3] = { qRgb(96, 76, 176), AutoShadowBlendMode::Multiply };
    }
    return p;
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

    // 默认色带：标准阴影——受光不动原图、暗部同色系深蓝灰逐级加深（CSP/AI 卡渲同款观感）
    mLevels[0] = { qRgb(255, 255, 255), AutoShadowBlendMode::Multiply, 100 };
    mLevels[1] = { qRgb(128, 136, 172), AutoShadowBlendMode::Multiply, 60 };
    mLevels[2] = { qRgb(100, 108, 146), AutoShadowBlendMode::Multiply, 80 };
    mLevels[3] = { qRgb(74, 82, 120), AutoShadowBlendMode::Multiply, 100 };

    // 光源列表：默认一盏主光（左上 45°）
    mLights = { AutoShadowLight{} };
    mCurrentLight = 0;

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

    // ── 预设（CSP 预设语义）：一键整套光源列表 + 渐变强度 + 色带 ──
    auto* presetRow = new QGridLayout;
    presetRow->setHorizontalSpacing(8);
    presetRow->setContentsMargins(0, 0, 0, 0);
    auto* presetLabel = new QLabel(tr("预设："), this);
    presetRow->addWidget(presetLabel, 0, 0);
    mPresetCombo = new QComboBox(this);
    mPresetCombo->addItem(tr("无"));
    mPresetCombo->addItem(tr("顺光"));
    mPresetCombo->addItem(tr("逆光轮廓光（双光源）"));
    mPresetCombo->addItem(tr("夜晚"));
    mPresetCombo->addItem(tr("黄昏"));
    mPresetCombo->addItem(tr("风格化彩色"));
    mPresetCombo->setToolTip(tr("一键应用整套光源与色带配置（CSP 预设语义）。应用后手动改任何参数，预设自动回到「无」。"));
    connect(mPresetCombo, &QComboBox::activated, this, [this](const int index) { applyPreset(index); });
    presetRow->addWidget(mPresetCombo, 0, 1);
    mParamColumn->addLayout(presetRow);

    // ── 光源列表（CSP 光源设置语义）：下拉选中编辑对象，可增删 ──
    auto* lightRow = new QHBoxLayout;
    lightRow->setContentsMargins(0, 0, 0, 0);
    mLightCombo = new QComboBox(this);
    mLightCombo->setToolTip(tr("当前编辑的光源。滑杆与预览框拖拽都作用于它；点击预览框里其他光源的编号标记可切换。"));
    connect(mLightCombo, &QComboBox::activated, this, [this](const int index) {
        mCurrentLight = index;
        syncLightControls();
        renderPreview(); // 立即刷新标记高亮
    });
    lightRow->addWidget(mLightCombo, 1);
    mAddLightButton = new QPushButton(tr("＋添加光源"), this);
    mAddLightButton->setAutoDefault(false);
    mAddLightButton->setToolTip(tr("再加一盏光源：多光源互补照明（照度=Σ 强度·max(0,N·L)），所有光都照不到的坡面才全暗——双光可做双侧轮廓光。"));
    connect(mAddLightButton, &QPushButton::clicked, this, [this] {
        AutoShadowLight l;
        l.x = 0.85;
        l.y = 0.10;
        mLights.append(l);
        mCurrentLight = mLights.size() - 1;
        refreshLightCombo();
        syncLightControls();
        schedulePreview();
    });
    lightRow->addWidget(mAddLightButton);
    mRemoveLightButton = new QPushButton(tr("－删除光源"), this);
    mRemoveLightButton->setAutoDefault(false);
    connect(mRemoveLightButton, &QPushButton::clicked, this, [this] {
        if (mLights.size() <= 1)
            return;
        mLights.removeAt(mCurrentLight);
        mCurrentLight = std::min(mCurrentLight, static_cast<int>(mLights.size()) - 1);
        refreshLightCombo();
        syncLightControls();
        schedulePreview();
    });
    lightRow->addWidget(mRemoveLightButton);
    mParamColumn->addLayout(lightRow);

    // ── 场生成段参数：选中光源（预览框点/拖定位）+ 黑透白不透掩膜 + 体积法线/渐变/遮挡 ──
    QDoubleSpinBox* lightXSpin = nullptr;
    QSlider* lightXSlider = nullptr;
    addSliderRow(tr("光源 X："), -100, 200, 15,
        tr("当前光源的水平位置（画面宽度的百分比，0=左缘 100=右缘，可拉出画面放远光）。也可直接在预览框里点击/拖拽定位。"),
        tr("%"), lightXSpin, lightXSlider);
    mLightXSpin = lightXSpin;
    mLightXSlider = lightXSlider;
    connect(mLightXSpin, &QDoubleSpinBox::valueChanged, this, [this](const double value) {
        if (mCurrentLight < mLights.size())
            mLights[mCurrentLight].x = value / 100.0;
    });

    QDoubleSpinBox* lightYSpin = nullptr;
    QSlider* lightYSlider = nullptr;
    addSliderRow(tr("光源 Y："), -100, 200, 5,
        tr("当前光源的垂直位置（画面高度的百分比，0=上缘 100=下缘；负值=画面上方光源）。"),
        tr("%"), lightYSpin, lightYSlider);
    mLightYSpin = lightYSpin;
    mLightYSlider = lightYSlider;
    connect(mLightYSpin, &QDoubleSpinBox::valueChanged, this, [this](const double value) {
        if (mCurrentLight < mLights.size())
            mLights[mCurrentLight].y = value / 100.0;
    });

    QDoubleSpinBox* lightHeightSpin = nullptr;
    QSlider* lightHeightSlider = nullptr;
    addSliderRow(tr("光源高度："), 0, 300, 150,
        tr("当前光源离画面的仰角高度（%）：100≈45° 斜射，越大越顶光（明暗交界线下移、受光面变大），越小越平射（阴影越多）。光源拉远时仰角不塌。"),
        tr("%"), lightHeightSpin, lightHeightSlider);
    mLightHeightSpin = lightHeightSpin;
    mLightHeightSlider = lightHeightSlider;
    connect(mLightHeightSpin, &QDoubleSpinBox::valueChanged, this, [this](const double value) {
        if (mCurrentLight < mLights.size())
            mLights[mCurrentLight].height = qRound(value);
    });

    QDoubleSpinBox* lightIntensitySpin = nullptr;
    QSlider* lightIntensitySlider = nullptr;
    addSliderRow(tr("光源强度："), 0, 100, 100,
        tr("当前光源的强度（%）：多光源按强度加权叠加照明；0=关闭该光源（只剩其余光源照明）。"),
        tr("%"), lightIntensitySpin, lightIntensitySlider);
    mLightIntensitySpin = lightIntensitySpin;
    mLightIntensitySlider = lightIntensitySlider;
    connect(mLightIntensitySpin, &QDoubleSpinBox::valueChanged, this, [this](const double value) {
        if (mCurrentLight < mLights.size())
            mLights[mCurrentLight].intensity = qRound(value);
    });

    QDoubleSpinBox* thresholdSpin = nullptr;
    QSlider* thresholdSlider = nullptr;
    addSliderRow(tr("去色阈值："), 1, 254, 128,
        tr("黑透白不透：图像去色后灰度≥该值为不透明白（受光填色面），低于为透明黑——线稿与深色区成为掩膜上的山谷，形体阴影沿山谷两侧生长。"),
        QString(), thresholdSpin, thresholdSlider);
    mThresholdSpin = thresholdSpin;

    QDoubleSpinBox* chokeSpin = nullptr;
    QSlider* chokeSlider = nullptr;
    addSliderRow(tr("阻塞遮罩："), -20, 20, 0,
        tr("简单阻塞（AE 语义）：以小增量收缩或扩展掩膜边缘，得到更整洁的掩膜。正值阻塞（收缩白区，吃掉白边与细丝），负值扩展（并掉小黑洞）。配合遮罩视图调最直观。"),
        tr(" px"), chokeSpin, chokeSlider);
    mChokeSpin = chokeSpin;

    QDoubleSpinBox* normalSpin = nullptr;
    QSlider* normalSlider = nullptr;
    addSliderRow(tr("体积强度："), 0, 100, 100,
        tr("SDF 伪法线 N·L 形体阴影（0..100，主阴影场）：掩膜距离变换当伪高度场——每个色块是一座圆润小丘、线稿是山谷，表面朝向决定明暗——脸颊出弧形交界线、发缕各自分块、贴线阴影自动成立。"),
        QString(), normalSpin, normalSlider);
    mNormalSpin = normalSpin;

    QDoubleSpinBox* formHeightSpin = nullptr;
    QSlider* formHeightSlider = nullptr;
    addSliderRow(tr("体积高度："), 1, 40, 6,
        tr("伪高度场的鼓起程度：越大形体越鼓（法线越陡，明暗交界线贴近边缘、阴影带窄），越小越扁平（交界线圆润、过渡带宽）。"),
        QString(), formHeightSpin, formHeightSlider);
    mFormHeightSpin = formHeightSpin;

    QDoubleSpinBox* formRadiusSpin = nullptr;
    QSlider* formRadiusSlider = nullptr;
    addSliderRow(tr("部件半径："), 1, 200, 30,
        tr("半椭球丘的鼓起半径（px）：每个色块鼓成球冠，法线在部件内部连续放射——明暗交界线横切形体中部（脸颊弧线、脖子横切宽面）。越大交界线越往部件中心移，越小越贴线稿边缘。"),
        tr(" px"), formRadiusSpin, formRadiusSlider);
    mFormRadiusSpin = formRadiusSpin;

    QDoubleSpinBox* formSmoothSpin = nullptr;
    QSlider* formSmoothSlider = nullptr;
    addSliderRow(tr("形体圆滑度："), 0, 40, 6,
        tr("伪高度场的高斯模糊半径（px）：越大丘顶越圆、交界线越弧；过小会出棱角感。"),
        tr(" px"), formSmoothSpin, formSmoothSlider);
    mFormSmoothSpin = formSmoothSpin;

    QDoubleSpinBox* gradientSpin = nullptr;
    QSlider* gradientSlider = nullptr;
    addSliderRow(tr("渐变强度："), 0, 100, 30,
        tr("圆形渐变底场（0..100）：离光源越远整体越暗——叠加在形体阴影上的全局衰减，CSP 同款底感。"),
        QString(), gradientSpin, gradientSlider);
    mGradientSpin = gradientSpin;

    QDoubleSpinBox* occlusionSpin = nullptr;
    QSlider* occlusionSlider = nullptr;
    addSliderRow(tr("遮挡强度："), 0, 100, 0,
        tr("径向遮挡（像素半径）：沿射向光源采样掩膜，线稿洞/前层挡在光路上时其背光侧投出遮挡阴影——洞在体积场里是山谷，这里再补「投影」式的洞后暗带。"),
        tr(" px"), occlusionSpin, occlusionSlider);
    mOcclusionSpin = occlusionSpin;

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

    // 排线输出（漫画网点）：色阶改为固定角度斜线图案，线隙透出原图
    auto* hatchBox = new QGroupBox(tr("排线输出（漫画网点）"), this);
    auto* hatchLayout = new QVBoxLayout(hatchBox);
    mHatchCheck = new QCheckBox(tr("启用排线（黑白漫画质感）"), hatchBox);
    mHatchCheck->setToolTip(tr("色阶覆盖度改为固定角度斜线图案：线上按该阶颜色混合，线隙透出原图——画面呈排线网点阴影。关闭=常规色块阴影。"));
    connect(mHatchCheck, &QCheckBox::toggled, this, [this] { syncHatchEnabled(); schedulePreview(); });
    hatchLayout->addWidget(mHatchCheck);

    QDoubleSpinBox* hatchAngleSpin = nullptr;
    QSlider* hatchAngleSlider = nullptr;
    addSliderRowTo(hatchLayout, tr("排线角度："), 0, 180, 135,
        tr("斜线方向（度）：135=左下到右上（漫画常用），0=水平，90=垂直。"),
        tr("°"), hatchAngleSpin, hatchAngleSlider);
    mHatchAngleSpin = hatchAngleSpin;
    mHatchAngleSlider = hatchAngleSlider;

    QDoubleSpinBox* hatchSpacingSpin = nullptr;
    QSlider* hatchSpacingSlider = nullptr;
    addSliderRowTo(hatchLayout, tr("排线间距："), 1, 24, 6,
        tr("相邻斜线的间距（px）：越小排线越密（阴影越重），线宽自动取间距的约三分之一。"),
        tr(" px"), hatchSpacingSpin, hatchSpacingSlider);
    mHatchSpacingSpin = hatchSpacingSpin;
    mHatchSpacingSlider = hatchSpacingSlider;
    mParamColumn->addWidget(hatchBox);
    syncHatchEnabled(); // 未启用时排线参数灰显常驻（防排版跳动）

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
    const char* modeNames[11] = { "正常", "正片叠底", "线性加深", "变暗", "颜色加深",
                                  "变亮", "滤色", "叠加", "柔光", "强光", "线性减淡" };
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
        mLevelCombos[i]->setToolTip(tr("该色阶与原图的混合模式（PS/AE 语义）：正片叠底/线性加深=压暗保细节（阴影常用），滤色/线性减淡=提亮（受光面可用），正常=直接换色。"));
        connect(mLevelCombos[i], &QComboBox::currentIndexChanged, this, [this, levelIndex](const int index) {
            mLevels[levelIndex].mode = static_cast<AutoShadowBlendMode>(index);
            schedulePreview();
        });
        levelRows->addWidget(mLevelCombos[i], i, 2);

        mLevelOpacitySpins[i] = new QDoubleSpinBox(levelBox);
        mLevelOpacitySpins[i]->setDecimals(0);
        mLevelOpacitySpins[i]->setRange(0, 100);
        mLevelOpacitySpins[i]->setValue(mLevels[i].opacity);
        mLevelOpacitySpins[i]->setSuffix(tr("%"));
        mLevelOpacitySpins[i]->setFixedWidth(64);
        mLevelOpacitySpins[i]->setToolTip(tr("该色阶的不透明度：混合结果按此比例回混原色，100=全强度。"));
        connect(mLevelOpacitySpins[i], &QDoubleSpinBox::valueChanged, this, [this, levelIndex](const double value) {
            mLevels[levelIndex].opacity = qRound(value);
            schedulePreview();
        });
        levelRows->addWidget(mLevelOpacitySpins[i], i, 3);
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

    // ── 作用范围（横排紧凑） ──
    auto* scopeRow = new QHBoxLayout;
    scopeRow->setContentsMargins(0, 0, 0, 0);
    mCurrentFrameRadio = new QRadioButton(tr("仅当前帧"), this);
    mCurrentFrameRadio->setChecked(true);
    mAllKeyFramesRadio = new QRadioButton(tr("当前图层全部关键帧"), this);
    scopeRow->addWidget(mCurrentFrameRadio);
    scopeRow->addWidget(mAllKeyFramesRadio);
    scopeRow->addStretch(1);
    mParamColumn->addLayout(scopeRow);

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
    refreshLightCombo();
    syncLightControls();
    renderPreview();
}

AutoShadowParams AutoShadowDialog::params() const
{
    AutoShadowParams p;
    p.lights = mLights;
    p.maskThreshold = qRound(mThresholdSpin->value());
    p.chokeMatte = qRound(mChokeSpin->value());
    p.gradientStrength = qRound(mGradientSpin->value());
    p.normalStrength = qRound(mNormalSpin->value());
    p.formHeight = qRound(mFormHeightSpin->value());
    p.formRadius = qRound(mFormRadiusSpin->value());
    p.formSmooth = qRound(mFormSmoothSpin->value());
    p.occlusionStrength = qRound(mOcclusionSpin->value());
    for (int i = 0; i < 3; ++i)
        p.thresholds[i] = mLevelsBar->thresholds(i);
    p.edgeFeather = qRound(mFeatherSpin->value());
    p.smooth = mTypeCombo->currentIndex() == 1;
    p.invertLevels = mInvertCheck->isChecked();
    p.hatch = mHatchCheck->isChecked();
    p.hatchAngle = qRound(mHatchAngleSpin->value());
    p.hatchSpacing = qRound(mHatchSpacingSpin->value());
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
    // 点中其他光源的编号标记（±12px）→ 切换为当前编辑光源
    for (int i = 0; i < mLights.size(); ++i)
    {
        const double mx = mLights[i].x * iw + (lw - iw) / 2.0;
        const double my = mLights[i].y * ih + (lh - ih) / 2.0;
        const double ddx = pos.x() - mx;
        const double ddy = pos.y() - my;
        if (ddx * ddx + ddy * ddy <= 12.0 * 12.0)
        {
            if (i != mCurrentLight)
            {
                mCurrentLight = i;
                syncLightControls();
                renderPreview();
            }
            return;
        }
    }
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
    addSliderRowTo(mParamColumn, labelText, minV, maxV, defV, tip, suffix, spinOut, sliderOut);
}

void AutoShadowDialog::addSliderRowTo(QLayout* layout, const QString& labelText, const int minV, const int maxV,
                                      const int defV, const QString& tip, const QString& suffix,
                                      QDoubleSpinBox*& spinOut, QSlider*& sliderOut)
{    // 单行紧凑排版：标签 | 滑杆(拉伸) | 数值框——13 行参数也放得下常规屏幕
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(labelText, this);
    label->setMinimumWidth(84);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(label, 0, 0);

    auto* slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(minV, maxV);
    slider->setValue(defV);
    slider->setToolTip(tip);
    grid->addWidget(slider, 0, 1);

    auto* spin = new QDoubleSpinBox(this);
    spin->setDecimals(0);
    spin->setRange(minV, maxV);
    spin->setValue(defV);
    spin->setSuffix(suffix);
    spin->setFixedWidth(96);
    spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    spin->setToolTip(tip);

    grid->addWidget(spin, 0, 2);
    grid->setColumnStretch(1, 1);
    layout->addItem(grid);

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

void AutoShadowDialog::syncHatchEnabled()
{
    const bool on = mHatchCheck != nullptr && mHatchCheck->isChecked();
    // 输入框与其配对滑杆一起启停（滑杆在构造时接入了同步信号）
    if (mHatchAngleSpin != nullptr)
    {
        mHatchAngleSpin->setEnabled(on);
        mHatchAngleSlider->setEnabled(on);
    }
    if (mHatchSpacingSpin != nullptr)
    {
        mHatchSpacingSpin->setEnabled(on);
        mHatchSpacingSlider->setEnabled(on);
    }
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
    // 手动改参后预设不再是当前状态：回到「无」（应用预设期间除外）
    if (!mApplyingPreset && mPresetCombo != nullptr && mPresetCombo->currentIndex() != 0)
    {
        QSignalBlocker blocker(mPresetCombo);
        mPresetCombo->setCurrentIndex(0);
    }
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

    // 画所有光源标记（编号圈，各自配色；选中的更大+白描边），只在画面范围内显示
    static const QColor kMarkerColors[5] = {
        QColor(255, 220, 60), QColor(60, 200, 255), QColor(255, 90, 220),
        QColor(90, 230, 130), QColor(255, 150, 60),
    };
    for (int i = 0; i < p.lights.size(); ++i)
    {
        const int mx = qRound(p.lights[i].x * preview.width());
        const int my = qRound(p.lights[i].y * preview.height());
        if (mx < -8 || mx > preview.width() + 8 || my < -8 || my > preview.height() + 8)
            continue;
        const bool selected = (i == mCurrentLight);
        const int r = selected ? 9 : 7;
        QPainter painter(&preview);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(selected ? QColor(255, 255, 255) : QColor(30, 30, 30), selected ? 2 : 1));
        painter.setBrush(kMarkerColors[i % 5]);
        painter.drawEllipse(QPoint(mx, my), r, r);
        painter.setPen(QPen(QColor(30, 30, 30)));
        QFont markerFont = font();
        markerFont.setPointSizeF(std::max(7.0, font().pointSizeF() - 3.0));
        markerFont.setBold(true);
        painter.setFont(markerFont);
        painter.drawText(QRect(mx - r, my - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(i + 1));
    }
    mPreviewLabel->setPixmap(QPixmap::fromImage(preview));
}

void AutoShadowDialog::refreshLightCombo()
{
    QSignalBlocker blocker(mLightCombo);
    mLightCombo->clear();
    for (int i = 0; i < mLights.size(); ++i)
        mLightCombo->addItem(tr("光源 %1").arg(i + 1));
    mCurrentLight = std::min(mCurrentLight, std::max(0, static_cast<int>(mLights.size()) - 1));
    mLightCombo->setCurrentIndex(mCurrentLight);
    if (mRemoveLightButton != nullptr)
        mRemoveLightButton->setEnabled(mLights.size() > 1);
}

void AutoShadowDialog::syncLightControls()
{
    mCurrentLight = std::min(mCurrentLight, std::max(0, static_cast<int>(mLights.size()) - 1));
    const AutoShadowLight& l = mLights[std::max(0, mCurrentLight)];
    // 屏蔽信号写滑杆/数值框：避免触发挂钩把值写回错误的光源
    QSignalBlocker bx(mLightXSpin);
    QSignalBlocker by(mLightYSpin);
    QSignalBlocker bh(mLightHeightSpin);
    QSignalBlocker bi(mLightIntensitySpin);
    QSignalBlocker sx(mLightXSlider);
    QSignalBlocker sy(mLightYSlider);
    QSignalBlocker sh(mLightHeightSlider);
    QSignalBlocker si(mLightIntensitySlider);
    mLightXSpin->setValue(l.x * 100.0);
    mLightYSpin->setValue(l.y * 100.0);
    mLightHeightSpin->setValue(l.height);
    mLightIntensitySpin->setValue(l.intensity);
    mLightXSlider->setValue(qRound(l.x * 100.0));
    mLightYSlider->setValue(qRound(l.y * 100.0));
    mLightHeightSlider->setValue(l.height);
    mLightIntensitySlider->setValue(l.intensity);
    QSignalBlocker bc(mLightCombo);
    mLightCombo->setCurrentIndex(mCurrentLight);
}

void AutoShadowDialog::syncLevelRow(const int levelIndex)
{
    updateLevelButton(levelIndex);
    QSignalBlocker bc(mLevelCombos[levelIndex]);
    mLevelCombos[levelIndex]->setCurrentIndex(static_cast<int>(mLevels[levelIndex].mode));
    QSignalBlocker bo(mLevelOpacitySpins[levelIndex]);
    mLevelOpacitySpins[levelIndex]->setValue(mLevels[levelIndex].opacity);
}

void AutoShadowDialog::applyPreset(const int presetIndex)
{
    if (presetIndex <= 0)
        return; // 无：不动作
    mApplyingPreset = true;
    const AutoShadowParams p = presetParams(presetIndex);
    mLights = p.lights;
    mCurrentLight = 0;
    mGradientSpin->setValue(p.gradientStrength);
    for (int i = 0; i < 4; ++i)
    {
        mLevels[i] = p.levels[i];
        syncLevelRow(i);
    }
    mLevelsBar->setThresholds(p.thresholds);
    mTypeCombo->setCurrentIndex(0);      // 色调分离（赛璐璐）
    mInvertCheck->setChecked(false);
    mHatchCheck->setChecked(false);       // 预设不碰排线：留给漫画流程单独开
    syncHatchEnabled();
    refreshLightCombo();
    syncLightControls();
    syncLevelsBar();
    mApplyingPreset = false;
    schedulePreview();
}
