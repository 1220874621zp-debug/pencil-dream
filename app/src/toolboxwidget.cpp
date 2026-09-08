/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2024-2099 Oliver S. Larsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "toolboxwidget.h"
#include "ui_toolboxwidget.h"

#include <QScrollBar>
#include <QResizeEvent>
#include <QDebug>
#include <QButtonGroup>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QSet>
#include <QStyleHints>
#include <QTimer>

#include "layermanager.h"
#include "toolmanager.h"
#include "editor.h"
#include "pencilsettings.h"
#include "theme.h"

// ----------------------------------------------------------------------------------
QString GetToolTips(QString strCommandName)
{
    strCommandName = QString("shortcuts/") + strCommandName;
    QKeySequence keySequence(pencilSettings().value(strCommandName).toString());
    return QString("<b>%1</b>").arg(keySequence.toString()); // don't tr() this string.
}

// 工具组按钮图标（原样，无角标——角标由 CornerBadge 覆盖控件绘制）
static QIcon variantIcon(ToolType toolType)
{
    switch (toolType)
    {
    case LASSO:  return QIcon(":/icons/themes/playful/tools/tool-lasso.svg");
    case SELECT: return QIcon(":/icons/themes/playful/tools/tool-select.svg");
    case DEFORM: return QIcon(":/icons/themes/playful/tools/tool-deform.svg");
    case MOVE:   return QIcon(":/icons/themes/playful/tools/tool-move.svg");
    default:     return QIcon();
    }
}

// Krita/Qt 原生范式：变体角标画在「按钮」右下角（不是烙进图标），小三角、
// 无描边、跟随主题调色板，双层绘制明暗主题均可见（与选区蚂蚁线同款手法）
class CornerBadge : public QWidget
{
public:
    explicit CornerBadge(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(parent->width() - 14, parent->height() - 14, 14, 14);
        raise();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        const qreal m = 3.0;                    // 与按钮角的留白
        const qreal leg = width() - m * 2.0;    // 三角直角边长
        QPolygonF triangle { QPointF(m, height() - m),
                             QPointF(m + leg, height() - m),
                             QPointF(m + leg, height() - m - leg) };

        QPolygonF shadow = triangle.translated(0.7, 0.7);
        painter.setBrush(QColor(0, 0, 0, 130));
        painter.drawPolygon(shadow);

        // 红粉角标：与主题强调色一致（选中/播放头同款）
        painter.setBrush(Theme::Accent);
        painter.drawPolygon(triangle);
    }
};

ToolBoxWidget::ToolBoxWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::ToolBoxWidget)
{
    ui->setupUi(this);
}

ToolBoxWidget::~ToolBoxWidget()
{
    delete ui;
}

void ToolBoxWidget::initUI()
{

#ifdef __APPLE__
    // Only Mac needs this. ToolButton is naturally borderless on Win/Linux.
    QString sStyle =
        "QToolButton { border: 0px; }"
        "QToolButton:pressed { border: 1px solid #ADADAD; border-radius: 2px; background-color: #D5D5D5; }"
        "QToolButton:checked { border: 1px solid #ADADAD; border-radius: 2px; background-color: #D5D5D5; }";
    ui->pencilButton->setStyleSheet(sStyle);
    ui->lassoButton->setStyleSheet(sStyle);
    ui->deformButton->setStyleSheet(sStyle);
    ui->onionAlignButton->setStyleSheet(sStyle);
    ui->handButton->setStyleSheet(sStyle);
    ui->penButton->setStyleSheet(sStyle);
    ui->eraserButton->setStyleSheet(sStyle);
    ui->polylineButton->setStyleSheet(sStyle);
    ui->bucketButton->setStyleSheet(sStyle);
    ui->brushButton->setStyleSheet(sStyle);
    ui->eyedropperButton->setStyleSheet(sStyle);
    ui->smudgeButton->setStyleSheet(sStyle);
#endif

    ui->pencilButton->setToolTip( tr( "Pencil Tool (%1): Sketch with pencil" )
        .arg( GetToolTips( CMD_TOOL_PENCIL ) ) );
    ui->onionAlignButton->setToolTip( tr( "洋葱皮对位工具 (%1)：拖动红/蓝幽灵像对位中割；双击=中心对齐；Alt+点击=归零" )
        .arg( GetToolTips( CMD_TOOL_ONION_ALIGN ) ) );
    ui->handButton->setToolTip( tr( "Hand Tool (%1): Move the canvas" )
        .arg( GetToolTips( CMD_TOOL_HAND ) ) );
    ui->penButton->setToolTip( tr( "Pen Tool (%1): Sketch with pen" )
        .arg( GetToolTips( CMD_TOOL_PEN ) ) );
    ui->eraserButton->setToolTip( tr( "Eraser Tool (%1): Erase" )
        .arg( GetToolTips( CMD_TOOL_ERASER ) ) );
    ui->polylineButton->setToolTip( tr( "Polyline Tool (%1): Create line/curves" )
        .arg( GetToolTips( CMD_TOOL_POLYLINE ) ) );
    ui->bucketButton->setToolTip( tr( "Paint Bucket Tool (%1): Fill selected area with a color" )
        .arg( GetToolTips( CMD_TOOL_BUCKET ) ) );
    ui->brushButton->setToolTip( tr( "Brush Tool (%1): Paint smooth stroke with a brush" )
        .arg( GetToolTips( CMD_TOOL_BRUSH ) ) );
    ui->eyedropperButton->setToolTip( tr( "Eyedropper Tool (%1): "
            "Set color from the stage<br>[ALT] for instant access" )
        .arg( GetToolTips( CMD_TOOL_EYEDROPPER ) ) );
    ui->smudgeButton->setToolTip( tr( "Smudge Tool (%1):<br>Edit polyline/curves<br>"
            "Liquify bitmap pixels<br> (%1)+[Alt]: Smooth" )
        .arg( GetToolTips( CMD_TOOL_SMUDGE ) ) );

    ui->pencilButton->setWhatsThis( tr( "Pencil Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_PENCIL ) ) );
    ui->lassoButton->setWhatsThis( tr( "Select a free-form (lasso) or rectangular area; press and hold the button to switch variants" ) );
    ui->deformButton->setWhatsThis( tr( "Deform or move; press and hold the button to switch variants" ) );
    ui->onionAlignButton->setWhatsThis( tr( "洋葱皮对位工具 (%1)" )
        .arg( GetToolTips( CMD_TOOL_ONION_ALIGN ) ) );
    ui->handButton->setWhatsThis( tr( "Hand Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_HAND ) ) );
    ui->penButton->setWhatsThis( tr( "Pen Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_PEN ) ) );
    ui->eraserButton->setWhatsThis( tr( "Eraser Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_ERASER ) ) );
    ui->polylineButton->setWhatsThis( tr( "Polyline Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_POLYLINE ) ) );
    ui->bucketButton->setWhatsThis( tr( "Paint Bucket Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_BUCKET ) ) );
    ui->brushButton->setWhatsThis( tr( "Brush Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_BRUSH ) ) );
    ui->eyedropperButton->setWhatsThis( tr( "Eyedropper Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_EYEDROPPER ) ) );
    ui->smudgeButton->setWhatsThis( tr( "Smudge Tool (%1)" )
        .arg( GetToolTips( CMD_TOOL_SMUDGE ) ) );

    connect(ui->pencilButton, &QToolButton::clicked, this, &ToolBoxWidget::pencilOn);
    connect(ui->eraserButton, &QToolButton::clicked, this, &ToolBoxWidget::eraserOn);
    // 工具组按钮：短按=激活当前变体；长按=弹变体菜单（eventFilter 计时）
    connect(ui->lassoButton, &QToolButton::clicked, this, [this]() { variantOn(mSelectionGroup); });
    connect(ui->deformButton, &QToolButton::clicked, this, [this]() { variantOn(mTransformGroup); });
    connect(ui->onionAlignButton, &QToolButton::clicked, this, &ToolBoxWidget::onionAlignOn);
    connect(ui->penButton, &QToolButton::clicked, this, &ToolBoxWidget::penOn);
    connect(ui->handButton, &QToolButton::clicked, this, &ToolBoxWidget::handOn);
    connect(ui->polylineButton, &QToolButton::clicked, this, &ToolBoxWidget::polylineOn);
    connect(ui->bucketButton, &QToolButton::clicked, this, &ToolBoxWidget::bucketOn);
    connect(ui->eyedropperButton, &QToolButton::clicked, this, &ToolBoxWidget::eyedropperOn);
    connect(ui->brushButton, &QToolButton::clicked, this, &ToolBoxWidget::brushOn);
    connect(ui->smudgeButton, &QToolButton::clicked, this, &ToolBoxWidget::smudgeOn);

    mFlowlayout = new ToolBoxLayout(nullptr, 3,3,3);

    mFlowlayout->addWidget(ui->pencilButton);
    mFlowlayout->addWidget(ui->eraserButton);
    mFlowlayout->addWidget(ui->lassoButton);
    mFlowlayout->addWidget(ui->deformButton);
    mFlowlayout->addWidget(ui->onionAlignButton);
    mFlowlayout->addWidget(ui->penButton);
    mFlowlayout->addWidget(ui->handButton);
    mFlowlayout->addWidget(ui->polylineButton);
    mFlowlayout->addWidget(ui->bucketButton);
    mFlowlayout->addWidget(ui->eyedropperButton);
    mFlowlayout->addWidget(ui->brushButton);
    mFlowlayout->addWidget(ui->smudgeButton);

    delete ui->scrollAreaWidgetContents_2->layout();
    ui->scrollAreaWidgetContents_2->setLayout(mFlowlayout);

    // 防御：未被流式布局接管的按钮会以任意几何叠画在工具栏上（.ui 与接线
    // 漂移、或并行编译竞态残留旧生成头时发生过）——把流浪按钮隐藏掉
    QSet<QWidget*> managed;
    for (int i = 0; i < mFlowlayout->count(); ++i)
    {
        managed.insert(mFlowlayout->itemAt(i)->widget());
    }
    for (QToolButton* stray : ui->scrollAreaWidgetContents_2->findChildren<QToolButton*>())
    {
        if (!managed.contains(stray)) { stray->hide(); }
    }

    // Important to set the proper minimumSize;
    ui->scrollArea->setMinimumSize(QSize(1,1));
    setMinimumSize(mFlowlayout->minimumSize());

    QButtonGroup* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->pencilButton);
    buttonGroup->addButton(ui->eraserButton);
    buttonGroup->addButton(ui->lassoButton);
    buttonGroup->addButton(ui->deformButton);
    buttonGroup->addButton(ui->onionAlignButton);
    buttonGroup->addButton(ui->penButton);
    buttonGroup->addButton(ui->handButton);
    buttonGroup->addButton(ui->polylineButton);
    buttonGroup->addButton(ui->bucketButton);
    buttonGroup->addButton(ui->eyedropperButton);
    buttonGroup->addButton(ui->brushButton);
    buttonGroup->addButton(ui->smudgeButton);

    // 工具组（PS 式）：套索按钮 = 套索/矩形选择；变形按钮 = 变形/移动
    setupVariantGroup(mSelectionGroup, ui->lassoButton, LASSO, { LASSO, SELECT });
    setupVariantGroup(mTransformGroup, ui->deformButton, DEFORM, { DEFORM, MOVE });
}

int ToolBoxWidget::getMinHeightForWidth(int width) const
{
    return mFlowlayout->heightForWidth(width);
}

QSize ToolBoxWidget::sizeHint() const
{
    return minimumSizeHint();
}

QSize ToolBoxWidget::minimumSizeHint() const
{
    int minWidth = mFlowlayout->minimumSize().width();
    return QSize(minWidth, getMinHeightForWidth(width()));
}

void ToolBoxWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    updateLayoutAlignment();
}

void ToolBoxWidget::updateLayoutAlignment()
{
    mFlowlayout->invalidate();
    if (mFlowlayout->rows() > 1) {
        mFlowlayout->setAlignment(Qt::AlignJustify);
    } else {
        mFlowlayout->setAlignment(Qt::AlignHCenter);
    }

    mFlowlayout->activate();
}

void ToolBoxWidget::updateUI()
{
}

void ToolBoxWidget::setActiveTool(ToolType toolType)
{
    switch (toolType) {
    case ToolType::BRUSH:
        brushOn();
        break;
    case ToolType::PEN:
        penOn();
        break;
    case ToolType::PENCIL:
        pencilOn();
        break;
    case ToolType::SELECT:
        selectOn();
        break;
    case ToolType::LASSO:
        lassoOn();
        break;
    case ToolType::DEFORM:
        deformOn();
        break;
    case ToolType::HAND:
        handOn();
        break;
    case ToolType::MOVE:
    case ToolType::CAMERA:
        moveOn();
        break;
    case ToolType::ONION_ALIGN:
        onionAlignOn();
        break;
    case ToolType::ERASER:
        eraserOn();
        break;
    case ToolType::POLYLINE:
        polylineOn();
        break;
    case ToolType::SMUDGE:
        smudgeOn();
        break;
    case ToolType::BUCKET:
        bucketOn();
        break;
    case ToolType::EYEDROPPER:
        eyedropperOn();
        break;
    default:
        break;
    }
}

void ToolBoxWidget::pencilOn()
{
    toolOn(PENCIL, ui->pencilButton);
}

void ToolBoxWidget::eraserOn()
{
    toolOn(ERASER, ui->eraserButton);
}

void ToolBoxWidget::selectOn()
{
    setVariant(mSelectionGroup, SELECT);
    toolOn(SELECT, ui->lassoButton);
}

void ToolBoxWidget::lassoOn()
{
    setVariant(mSelectionGroup, LASSO);
    toolOn(LASSO, ui->lassoButton);
}

void ToolBoxWidget::deformOn()
{
    setVariant(mTransformGroup, DEFORM);
    toolOn(DEFORM, ui->deformButton);
}

void ToolBoxWidget::moveOn()
{
    setVariant(mTransformGroup, MOVE);
    if (mEditor->layers()->currentLayer()->type() == Layer::CAMERA) {
        toolOn(CAMERA, ui->deformButton);
    } else {
        toolOn(MOVE, ui->deformButton);
    }
}

void ToolBoxWidget::onionAlignOn()
{
    toolOn(ONION_ALIGN, ui->onionAlignButton);
}

void ToolBoxWidget::penOn()
{
    toolOn(PEN, ui->penButton);
}

void ToolBoxWidget::handOn()
{
    toolOn(HAND, ui->handButton);
}

void ToolBoxWidget::polylineOn()
{
    toolOn(POLYLINE, ui->polylineButton);
}

void ToolBoxWidget::bucketOn()
{
    toolOn(BUCKET, ui->bucketButton);
}

void ToolBoxWidget::eyedropperOn()
{
    toolOn(EYEDROPPER, ui->eyedropperButton);
}

void ToolBoxWidget::brushOn()
{
    toolOn(BRUSH, ui->brushButton);
}

void ToolBoxWidget::smudgeOn()
{
    toolOn(SMUDGE, ui->smudgeButton);
}

void ToolBoxWidget::deselectAllTools()
{
    ui->pencilButton->setChecked(false);
    ui->eraserButton->setChecked(false);
    ui->lassoButton->setChecked(false);
    ui->deformButton->setChecked(false);
    ui->onionAlignButton->setChecked(false);
    ui->handButton->setChecked(false);
    ui->penButton->setChecked(false);
    ui->polylineButton->setChecked(false);
    ui->bucketButton->setChecked(false);
    ui->eyedropperButton->setChecked(false);
    ui->brushButton->setChecked(false);
    ui->smudgeButton->setChecked(false);
}

void ToolBoxWidget::toolOn(ToolType toolType, QToolButton* toolButton)
{
    if (mEditor->tools()->currentTool()->type() == toolType) {
        // Prevent un-checking the current tool and do nothing
        toolButton->setChecked(true);
        return;
    }
    if (!mEditor->tools()->leavingThisTool())
    {
        toolButton->setChecked(false);
        return;
    }
    mEditor->tools()->setCurrentTool(toolType);
}

void ToolBoxWidget::setupVariantGroup(VariantGroup& group, QToolButton* button,
                                      ToolType defaultVariant, const QList<ToolType>& variants)
{
    group.button = button;

    group.menu = new QMenu(this);
    for (ToolType toolType : variants)
    {
        QAction* action = group.menu->addAction(
            variantIcon(toolType),
            tr("%1（%2）：%3").arg(variantName(toolType), GetToolTips(variantCommand(toolType)), variantDesc(toolType)));
        connect(action, &QAction::triggered, this, [this, &group, toolType]() {
            setVariant(group, toolType);
            variantOn(group);
        });
    }

    group.holdTimer = new QTimer(this);
    group.holdTimer->setSingleShot(true);
    // 系统默认 mousePressAndHoldInterval（Windows≈500ms）偏钝，缩短到 250ms：
    // 仍显著长于普通点击（<150ms），不会误触弹菜单
    group.holdTimer->setInterval(250);
    connect(group.holdTimer, &QTimer::timeout, this, [this, &group]() {
        showVariantMenu(group);
    });
    button->installEventFilter(this);

    // 变体角标：按钮右下角小三角（Krita/Qt 原生范式）
    new CornerBadge(button);

    setVariant(group, defaultVariant);
}

void ToolBoxWidget::setVariant(VariantGroup& group, ToolType toolType)
{
    group.current = toolType;
    group.button->setIcon(variantIcon(toolType));
    group.button->setToolTip(
        tr("%1（%2）：%3；长按此按钮可切换同类工具")
            .arg(variantName(toolType), GetToolTips(variantCommand(toolType)), variantDesc(toolType)));
}

void ToolBoxWidget::variantOn(VariantGroup& group)
{
    if (&group == &mTransformGroup && group.current == MOVE)
    {
        // 移动变体保留相机层特化：相机层上激活的是 CAMERA 工具
        moveOn();
    }
    else
    {
        toolOn(group.current, group.button);
    }
}

void ToolBoxWidget::showVariantMenu(VariantGroup& group)
{
    // 菜单弹在按钮右侧，不遮挡工具栏
    group.menu->exec(group.button->mapToGlobal(QPoint(group.button->width() + 4, 0)));
}

QString ToolBoxWidget::variantName(ToolType toolType)
{
    switch (toolType)
    {
    case SELECT: return tr("矩形选择工具");
    case LASSO:  return tr("套索工具");
    case DEFORM: return tr("变形工具");
    case MOVE:   return tr("移动工具");
    default:     return tr("工具");
    }
}

QString ToolBoxWidget::variantDesc(ToolType toolType)
{
    switch (toolType)
    {
    case SELECT: return tr("拖拽框选区域");
    case LASSO:  return tr("圈选任意形状区域");
    case DEFORM: return tr("自由/液化/弯曲/笼罩/透视（见工具选项）");
    case MOVE:   return tr("移动对象，相机层上为移动相机");
    default:     return QString();
    }
}

QString ToolBoxWidget::variantCommand(ToolType toolType)
{
    switch (toolType)
    {
    case SELECT: return CMD_TOOL_SELECT;
    case LASSO:  return CMD_TOOL_LASSO;
    case DEFORM: return CMD_TOOL_DEFORM;
    case MOVE:   return CMD_TOOL_MOVE;
    default:     return QString();
    }
}

bool ToolBoxWidget::eventFilter(QObject* watched, QEvent* event)
{
    for (VariantGroup* group : { &mSelectionGroup, &mTransformGroup })
    {
        if (watched != group->button) { continue; }

        if (event->type() == QEvent::MouseButtonPress)
        {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                group->holdTimer->start();
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            // 短按：停表，放行 clicked() 激活当前变体
            group->holdTimer->stop();
        }
    }
    return QWidget::eventFilter(watched, event);
}
