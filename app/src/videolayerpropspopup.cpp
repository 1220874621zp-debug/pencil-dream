#include "videolayerpropspopup.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>

#include "editor.h"
#include "scribblearea.h"
#include "layervideo.h"

// ---------------------------------------------------------------- DragNumber --

DragNumberField::DragNumberField(double min, double max, double step, int decimals,
                                 const QString& suffix, QWidget* parent)
    : QWidget(parent)
    , mMin(min), mMax(max), mStep(step), mDecimals(decimals), mSuffix(suffix)
{
    setCursor(Qt::SizeHorCursor); // 可拖交互必配光标预览
    setFixedWidth(96);
    setFixedHeight(22);
    setMouseTracking(true);
}

void DragNumberField::setValue(double v)
{
    v = qBound(mMin, v, mMax);
    mValue = v;
    update();
    emit valueEdited(mValue);
}

void DragNumberField::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor frame(0x33, 0x33, 0x3A);
    QColor fill(0x1C, 0x1C, 0x21);
    if (mHovered || mDragging) { fill = QColor(0x28, 0x28, 0x30); }
    p.setBrush(fill);
    p.setPen(QPen(frame, 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    // 数值(AE 惯例:可拖数字用淡蓝)
    p.setPen(QColor(0x8F, 0xC5, 0xFF));
    p.drawText(rect().adjusted(6, 0, -18, 0),
               Qt::AlignVCenter | Qt::AlignLeft,
               QString::number(mValue, 'f', mDecimals) + mSuffix);

    // 左右拖动指示箭头
    p.setPen(QColor(0x6A, 0x6A, 0x74));
    QPainterPath arrows;
    arrows.moveTo(width() - 9, height() / 2.0 - 3);
    arrows.lineTo(width() - 13, height() / 2.0);
    arrows.lineTo(width() - 9, height() / 2.0 + 3);
    arrows.moveTo(width() - 5, height() / 2.0 - 3);
    arrows.lineTo(width() - 1, height() / 2.0);
    arrows.lineTo(width() - 5, height() / 2.0 + 3);
    p.drawPath(arrows);
}

void DragNumberField::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        mPressPos = e->pos();
        mPressValue = mValue;
        mDragging = false; // 移动超阈值才算拖动(与双击共存)
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void DragNumberField::mouseMoveEvent(QMouseEvent* e)
{
    if ((e->buttons() & Qt::LeftButton) && !mPressPos.isNull())
    {
        const int dx = e->pos().x() - mPressPos.x();
        if (!mDragging && qAbs(dx) > 2) { mDragging = true; }
        if (mDragging)
        {
            // Shift 加速(×10):大范围数值不必拖穿桌面
            const double speed = (e->modifiers() & Qt::ShiftModifier) ? mStep * 10.0 : mStep;
            setValue(mPressValue + dx * speed);
        }
        e->accept();
        return;
    }
    mHovered = true;
    update();
    QWidget::mouseMoveEvent(e);
}

void DragNumberField::mouseReleaseEvent(QMouseEvent* e)
{
    Q_UNUSED(e)
    mDragging = false;
    mPressPos = QPoint();
}

void DragNumberField::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        startEditing();
        e->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(e);
}

void DragNumberField::leaveEvent(QEvent* e)
{
    mHovered = false;
    update();
    QWidget::leaveEvent(e);
}

void DragNumberField::startEditing()
{
    if (mEditor) { return; }
    mEditor = new QLineEdit(QString::number(mValue, 'f', mDecimals), this);
    mEditor->setGeometry(rect().adjusted(1, 1, -1, -1));
    mEditor->setFrame(false);
    mEditor->setAlignment(Qt::AlignRight);
    mEditor->setStyleSheet("background:#101014; color:#E8E8EA;");
    mEditor->show();
    mEditor->setFocus();
    mEditor->selectAll();
    connect(mEditor, &QLineEdit::editingFinished, this, [this]()
    {
        const double v = mEditor->text().toDouble();
        finishEditing();
        setValue(v); // 非法文本 toDouble=0,走钳制
    });
}

void DragNumberField::finishEditing()
{
    if (mEditor == nullptr) { return; }
    mEditor->hide();
    mEditor->deleteLater();
    mEditor = nullptr;
}

// ------------------------------------------------------------------- Popup --

VideoLayerPropsPopup::VideoLayerPropsPopup(LayerVideo* layer, Editor* editor, QWidget* parent)
    : QWidget(parent, Qt::Popup)
    , mLayer(layer)
    , mEditor(editor)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("VideoLayerPropsPopup { background:#1C1C21; border:1px solid #33333A; }"
                  "QLabel { color:#C8C8CE; background:transparent; }"
                  "QSlider::groove:horizontal { height:4px; background:#33333A; }"
                  "QSlider::handle:horizontal { width:10px; height:14px; margin:-5px 0;"
                  " background:#E8E8EA; border-radius:2px; }");

    mScaleSlider = new QSlider(Qt::Horizontal, this);
    mScaleSlider->setRange(5, 800);
    mScaleSlider->setFixedWidth(110);

    mScaleField = new DragNumberField(5.0, 800.0, 5.0, 1, " %", this);
    mOffsetXField = new DragNumberField(-99999.0, 99999.0, 1.0, 1, QString(), this);
    mOffsetYField = new DragNumberField(-99999.0, 99999.0, 1.0, 1, QString(), this);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 8, 10, 8);
    lay->setSpacing(6);
    lay->addWidget(new QLabel(tr("缩放"), this));
    lay->addWidget(mScaleSlider);
    lay->addWidget(mScaleField);
    lay->addSpacing(4);
    lay->addWidget(new QLabel(tr("位移 X"), this));
    lay->addWidget(mOffsetXField);
    lay->addSpacing(4);
    lay->addWidget(new QLabel(tr("位移 Y"), this));
    lay->addWidget(mOffsetYField);

    // 回填当前值:同步期守卫挡住 valueEdited→applyToLayer 回环
    mSyncing = true;
    mScaleSlider->setValue(qRound(mLayer->videoScale() * 100.0));
    mScaleField->setValue(mLayer->videoScale() * 100.0);
    mOffsetXField->setValue(mLayer->videoOffset().x());
    mOffsetYField->setValue(mLayer->videoOffset().y());
    mSyncing = false;

    connect(mScaleSlider, &QSlider::valueChanged, this, &VideoLayerPropsPopup::scaleSliderMoved);
    connect(mScaleField, &DragNumberField::valueEdited, this, &VideoLayerPropsPopup::scaleFieldEdited);
    connect(mOffsetXField, &DragNumberField::valueEdited, this, [this](double v) { offsetFieldEdited(0, v); });
    connect(mOffsetYField, &DragNumberField::valueEdited, this, [this](double v) { offsetFieldEdited(1, v); });
}

void VideoLayerPropsPopup::showPopup(LayerVideo* layer, Editor* editor, const QPoint& globalPos, QWidget* parent)
{
    auto* popup = new VideoLayerPropsPopup(layer, editor, parent);
    popup->adjustSize();
    popup->move(globalPos - QPoint(8, 8)); // 让点击点落在面板内侧,免得立即被视为"点外部"
    popup->show();
}

void VideoLayerPropsPopup::hideEvent(QHideEvent* e)
{
    // Qt::Popup 点外部即 hide:趁机自毁,层指针不再持有
    deleteLater();
    QWidget::hideEvent(e);
}

void VideoLayerPropsPopup::scaleSliderMoved(int v)
{
    if (mSyncing) { return; }
    mSyncing = true;
    mScaleField->setValue(static_cast<double>(v));
    mSyncing = false;
    applyToLayer();
}

void VideoLayerPropsPopup::scaleFieldEdited(double v)
{
    if (mSyncing) { return; }
    mSyncing = true;
    mScaleSlider->setValue(qRound(v));
    mSyncing = false;
    applyToLayer();
}

void VideoLayerPropsPopup::offsetFieldEdited(int axis, double v)
{
    QPointF off = mLayer->videoOffset();
    if (axis == 0) { off.setX(v); } else { off.setY(v); }
    mLayer->setVideoOffset(off);
    mEditor->getScribbleArea()->update();
}

void VideoLayerPropsPopup::applyToLayer()
{
    mLayer->setVideoScale(mScaleField->value() / 100.0);
    mEditor->getScribbleArea()->update();
}
