/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License;
version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "exposuresheetpanel.h"

#include "editor.h"
#include "layer.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "scribblearea.h"
#include "undoredomanager.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollBar>
#include <QUndoCommand>

#include <algorithm>

namespace
{
constexpr int GUTTER_W = 38; // 帧号栏宽
constexpr int COL_W    = 66; // 图层列宽
constexpr int ROW_H    = 16; // 一帧行高
constexpr int HEADER_H = 46; // 列头高

const QColor BODY_BG      (0x1e, 0x1e, 0x1e);
const QColor GUTTER_BG    (0x19, 0x19, 0x19);
const QColor HEADER_BG    (0x23, 0x23, 0x23);
const QColor HEADER_BG_CUR(0x30, 0x30, 0x30);
const QColor LINE_FAINT   (255, 255, 255, 18);
const QColor LINE_MID     (255, 255, 255, 34);
const QColor LINE_STRONG  (255, 255, 255, 72);
const QColor COL_SEP      (255, 255, 255, 30);
const QColor KEY_CIRCLE   (0xf2, 0xf2, 0xf2);
const QColor KEY_NUMBER   (255, 255, 255);
const QColor DOT_FILL     (0xc8, 0xc8, 0xc8);
const QColor EXPO_LINE    (0x7a, 0x7a, 0x7a);
const QColor EXPO_OPEN    (0x5e, 0x5e, 0x5e);
const QColor CAMERA_MARK  (0x86, 0xb9, 0xe8);
const QColor BAND_FILL    (255, 171, 64, 46);
const QColor BAND_LINE    (255, 171, 64, 200);
const QColor LAYER_TINT   (255, 255, 255, 10);
const QColor EYE_ON       (0xe6, 0xe6, 0xe6);
const QColor EYE_OFF      (0x78, 0x78, 0x78);
const QColor TEXT_DIM     (0x9a, 0x9a, 0x9a);
const QColor GUTTER_TEXT  (0xb9, 0xb9, 0xb9);
const QColor BAND_TEXT    (0xff, 0xab, 0x40);

/** 原画/中割标记切换（撤销命令）：按 layerId+pos 寻址，图层/关键帧已删则安全空转 */
class ToggleKeyDrawingCommand : public QUndoCommand
{
public:
    ToggleKeyDrawingCommand(Editor* editor, int layerId, int pos, bool newValue) :
        mEditor(editor), mLayerId(layerId), mPos(pos), mNewValue(newValue)
    {
        setText(QCoreApplication::translate("ExposureSheetPanel", "切换原画/中割"));
    }

    void redo() override { apply(mNewValue); }
    void undo() override { apply(!mNewValue); }

private:
    void apply(bool v)
    {
        Layer* layer = mEditor->object()->findLayerById(mLayerId);
        KeyFrame* key = layer ? layer->getKeyFrameAt(mPos) : nullptr;
        if (key == nullptr) { return; }

        key->setKeyDrawing(v);
        emit mEditor->framesModified(); // 律表重绘
        emit mEditor->needSave();
    }

    Editor* mEditor = nullptr;
    int mLayerId = -1;
    int mPos = 0;
    bool mNewValue = true;
};

void drawEyeGlyph(QPainter& p, const QRectF& r, bool open)
{
    p.save();
    QPen pen(open ? EYE_ON : EYE_OFF);
    pen.setWidthF(1.2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (open)
    {
        const QRectF eye(r.left(), r.center().y() - r.height() / 2, r.width(), r.height());
        p.drawEllipse(eye);
        p.setBrush(open ? EYE_ON : EYE_OFF);
        p.drawEllipse(eye.center(), 1.8, 1.8);
    }
    else
    {
        QPainterPath lid;
        lid.moveTo(r.left(), r.center().y());
        lid.quadTo(r.center().x(), r.bottom() + 2.0, r.right(), r.center().y());
        p.drawPath(lid);
    }
    p.restore();
}
} // namespace

// ---------------------------------------------------------------------------
// ExposureSheetView
// ---------------------------------------------------------------------------

ExposureSheetView::ExposureSheetView(QWidget* parent) : QAbstractScrollArea(parent)
{
    setFrameStyle(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    horizontalScrollBar()->setSingleStep(COL_W / 2);
    verticalScrollBar()->setSingleStep(ROW_H * 3);
}

int ExposureSheetView::frameRow(int frame) const
{
    return HEADER_H + (frame - 1) * ROW_H;
}

int ExposureSheetView::rowForY(int y) const
{
    return (y - HEADER_H) / ROW_H + 1;
}

int ExposureSheetView::contentWidth() const
{
    return GUTTER_W + static_cast<int>(mColumns.size()) * COL_W;
}

int ExposureSheetView::contentHeight() const
{
    return HEADER_H + mFrameCount * ROW_H;
}

void ExposureSheetView::updateScrollRanges()
{
    const int w = qMax(contentWidth(), viewport()->width());
    const int h = qMax(contentHeight(), viewport()->height());
    horizontalScrollBar()->setRange(0, w - viewport()->width());
    horizontalScrollBar()->setPageStep(viewport()->width());
    verticalScrollBar()->setRange(0, h - viewport()->height());
    verticalScrollBar()->setPageStep(viewport()->height());
}

void ExposureSheetView::rebuildColumns()
{
    mColumns.clear();
    int lastKeyPos = 1;

    Object* obj = mEditor ? mEditor->object() : nullptr;
    if (obj)
    {
        mFps = qMax(1, mEditor->playback()->fps());
        mCurrentFrame = qMax(1, mEditor->currentFrame());
        if (Layer* cur = mEditor->layers()->currentLayer())
        {
            mCurrentLayerId = cur->id();
        }

        // 位图族列：图层 index 0 = 视觉栈底（背景）→ 律表最左列
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            Layer* l = obj->getLayer(i);
            if (l == nullptr || !l->isBitmapKind()) { continue; }

            SheetColumn col;
            col.layerId = l->id();
            col.name = l->name();
            col.visible = l->visible();
            col.opacity = l->opacity();

            QList<int> positions;
            l->foreachKeyFrame([&positions](KeyFrame* k) { positions << k->pos(); });
            std::sort(positions.begin(), positions.end());

            int number = 1;
            for (int pos : positions)
            {
                KeyFrame* key = l->getKeyFrameAt(pos);
                if (key == nullptr) { continue; }

                SheetKeyEntry e;
                e.pos = pos;
                e.number = number++;
                e.keyDrawing = key->isKeyDrawing();
                e.blockEnd = l->getBlockEnd(key);
                col.keys.append(e);
                lastKeyPos = qMax(lastKeyPos, pos);
            }
            mColumns.append(col);
        }

        // 相机列：钉在最右（传统律表 カメラ 在末尾），只画关键帧菱形
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            Layer* l = obj->getLayer(i);
            if (l == nullptr || l->type() != Layer::CAMERA) { continue; }

            SheetColumn col;
            col.layerId = l->id();
            col.isCamera = true;
            col.name = l->name();
            col.visible = l->visible();

            QList<int> positions;
            l->foreachKeyFrame([&positions](KeyFrame* k) { positions << k->pos(); });
            std::sort(positions.begin(), positions.end());

            int number = 1;
            for (int pos : positions)
            {
                SheetKeyEntry e;
                e.pos = pos;
                e.number = number++;
                col.keys.append(e);
                lastKeyPos = qMax(lastKeyPos, pos);
            }
            mColumns.append(col);
        }
    }

    mFrameCount = qMax(48, lastKeyPos + mFps);
    updateScrollRanges();
    viewport()->update();
    updateToggleTarget();
}

void ExposureSheetView::updateCurrentFrame(int frame)
{
    mCurrentFrame = frame;
    ensureFrameVisible(frame);
    viewport()->update();
    updateToggleTarget();
}

void ExposureSheetView::refreshHighlight()
{
    if (mEditor && mEditor->object())
    {
        mCurrentFrame = qMax(1, mEditor->currentFrame());
        if (Layer* cur = mEditor->layers()->currentLayer())
        {
            mCurrentLayerId = cur->id();
        }
    }
    viewport()->update();
    updateToggleTarget();
}

void ExposureSheetView::ensureFrameVisible(int frame)
{
    QScrollBar* vb = verticalScrollBar();
    const int yTop = frameRow(frame);
    const int yBot = yTop + ROW_H;
    if (yTop < vb->value() + HEADER_H)
    {
        vb->setValue(qMax(0, yTop - HEADER_H));
    }
    else if (yBot > vb->value() + viewport()->height())
    {
        vb->setValue(yBot - viewport()->height());
    }
}

void ExposureSheetView::updateToggleTarget()
{
    bool hasKey = false;
    bool isKey = true;
    if (mEditor && mEditor->object())
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer && layer->isBitmapKind())
        {
            KeyFrame* key = layer->getKeyFrameAt(mEditor->currentFrame());
            if (key)
            {
                hasKey = true;
                isKey = key->isKeyDrawing();
            }
        }
    }
    emit toggleTargetChanged(hasKey, isKey);
}

void ExposureSheetView::toggleKeyDrawingAt(Layer* layer, int pos)
{
    KeyFrame* key = layer ? layer->getKeyFrameAt(pos) : nullptr;
    if (key == nullptr) { return; }

    mEditor->undoRedo()->pushUndoCommand(
        new ToggleKeyDrawingCommand(mEditor, layer->id(), pos, !key->isKeyDrawing()));
}

void ExposureSheetView::toggleCurrentKeyDrawing()
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || !layer->isBitmapKind()) { return; }

    toggleKeyDrawingAt(layer, mEditor->currentFrame());
}

Layer* ExposureSheetView::columnLayer(int index) const
{
    if (index < 0 || index >= mColumns.size()) { return nullptr; }
    if (mEditor == nullptr || mEditor->object() == nullptr) { return nullptr; }
    return mEditor->object()->findLayerById(mColumns[index].layerId);
}

void ExposureSheetView::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    const int hOff = horizontalScrollBar()->value();
    const int vOff = verticalScrollBar()->value();
    const int vpW = viewport()->width();
    const int vpH = viewport()->height();
    const int cW = qMax(contentWidth(), vpW);
    const int cH = contentHeight();

    // ---- 主体：帧网格 + 符号 + 播放头带 ----
    p.translate(-hOff, -vOff);
    p.fillRect(QRect(0, 0, cW, cH), BODY_BG);

    // 当前层列淡色底
    for (int i = 0; i < mColumns.size(); ++i)
    {
        if (mColumns[i].layerId == mCurrentLayerId)
        {
            p.fillRect(QRect(GUTTER_W + i * COL_W, HEADER_H, COL_W, cH - HEADER_H), LAYER_TINT);
        }
    }

    // 横向帧线（只画可视行）：每帧淡线、每3帧中线、每秒(fps帧)粗线
    const int fFirst = qBound(1, rowForY(vOff), mFrameCount);
    const int fLast = qBound(1, rowForY(vOff + vpH), mFrameCount);
    p.setBrush(Qt::NoBrush);
    for (int f = fFirst; f <= fLast; ++f)
    {
        const int y = frameRow(f) + ROW_H;
        p.setPen(QPen(f % mFps == 0 ? LINE_STRONG : (f % 3 == 0 ? LINE_MID : LINE_FAINT)));
        p.drawLine(GUTTER_W, y, cW, y);
    }

    // 列分隔竖线
    p.setPen(QPen(COL_SEP));
    for (int i = 0; i <= mColumns.size(); ++i)
    {
        const int x = GUTTER_W + i * COL_W;
        p.drawLine(x, HEADER_H, x, cH);
    }

    // 符号：原画=圆圈数字 / 中割=点 / 相机=菱形；曝光延续竖线（开放尾块=虚线）
    QFont numFont = font();
    numFont.setPixelSize(9);
    numFont.setBold(true);
    const QFontMetrics numFm(numFont);
    for (int i = 0; i < mColumns.size(); ++i)
    {
        const SheetColumn& col = mColumns[i];
        const int cx = GUTTER_W + i * COL_W;
        for (const SheetKeyEntry& e : col.keys)
        {
            if (e.blockEnd > 0 && e.blockEnd < fFirst) { continue; } // 整块在可视区上方
            if (e.pos > fLast) { break; }                            // keys 升序，后面更远

            const int cy = frameRow(e.pos) + ROW_H / 2;
            const int exposureEnd = (e.blockEnd > 0) ? e.blockEnd - 1 : mFrameCount;
            if (exposureEnd > e.pos)
            {
                QPen expoPen(e.blockEnd > 0 ? EXPO_LINE : EXPO_OPEN);
                if (e.blockEnd < 0) { expoPen.setStyle(Qt::DashLine); }
                p.setPen(expoPen);
                p.drawLine(cx + COL_W / 2, cy + 6, cx + COL_W / 2, frameRow(exposureEnd) + ROW_H / 2 - 6);
            }

            if (col.isCamera)
            {
                p.setPen(QPen(CAMERA_MARK));
                p.setBrush(CAMERA_MARK);
                const qreal r = 5.0;
                QPolygonF diamond;
                diamond << QPointF(cx + COL_W / 2, cy - r)
                        << QPointF(cx + COL_W / 2 + r, cy)
                        << QPointF(cx + COL_W / 2, cy + r)
                        << QPointF(cx + COL_W / 2 - r, cy);
                p.drawPolygon(diamond);
            }
            else if (e.keyDrawing)
            {
                const QString text = QString::number(e.number);
                const qreal w = qMax<qreal>(ROW_H - 3, numFm.horizontalAdvance(text) + 6);
                p.setPen(QPen(KEY_CIRCLE));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(cx + COL_W / 2, cy + 0.5), w / 2, (ROW_H - 3) / 2.0);
                p.setPen(KEY_NUMBER);
                p.setFont(numFont);
                p.drawText(QRect(cx, cy - ROW_H / 2, COL_W, ROW_H), Qt::AlignCenter, text);
            }
            else
            {
                p.setPen(Qt::NoPen);
                p.setBrush(DOT_FILL);
                p.drawEllipse(QPointF(cx + COL_W / 2, cy + 0.5), 3.0, 3.0);
            }
        }
    }

    // 当前帧行高亮带
    if (mCurrentFrame >= 1 && mCurrentFrame <= mFrameCount)
    {
        const int bandY = frameRow(mCurrentFrame);
        p.fillRect(QRect(GUTTER_W, bandY, cW - GUTTER_W, ROW_H), BAND_FILL);
        p.setPen(QPen(BAND_LINE));
        p.drawLine(GUTTER_W, bandY, cW, bandY);
        p.drawLine(GUTTER_W, bandY + ROW_H, cW, bandY + ROW_H);
    }
    p.setBrush(Qt::NoBrush);
    p.translate(hOff, vOff);

    // ---- 帧号栏：钉在视口左缘（不随横向滚动） ----
    p.fillRect(QRect(0, 0, GUTTER_W, vpH), GUTTER_BG);
    p.translate(0, -vOff);
    QFont gutterFont = font();
    gutterFont.setPixelSize(9);
    p.setFont(gutterFont);
    for (int f = fFirst; f <= fLast; ++f)
    {
        if (f != 1 && f % 3 != 0) { continue; }
        const int cy = frameRow(f) + ROW_H / 2;
        p.setPen(f == mCurrentFrame ? BAND_TEXT : GUTTER_TEXT);
        p.drawText(QRect(0, cy - ROW_H / 2, GUTTER_W - 6, ROW_H),
                   Qt::AlignVCenter | Qt::AlignRight, QString::number(f));
    }
    p.setPen(QPen(COL_SEP));
    p.drawLine(GUTTER_W - 1, 0, GUTTER_W - 1, cH);
    p.setBrush(Qt::NoBrush);
    p.translate(0, vOff);

    // ---- 列头：钉在视口上缘（不随纵向滚动） ----
    p.fillRect(QRect(0, 0, vpW, HEADER_H), HEADER_BG);
    p.translate(-hOff, 0);
    QFont nameFont = font();
    nameFont.setPixelSize(10);
    nameFont.setBold(true);
    const QFontMetrics nameFm(nameFont);
    QFont metaFont = font();
    metaFont.setPixelSize(9);
    for (int i = 0; i < mColumns.size(); ++i)
    {
        const SheetColumn& col = mColumns[i];
        const int hx = GUTTER_W + i * COL_W;
        if (col.layerId == mCurrentLayerId)
        {
            p.fillRect(QRect(hx + 1, 0, COL_W - 1, HEADER_H - 1), HEADER_BG_CUR);
        }

        p.setFont(nameFont);
        p.setPen(col.visible ? QColor(0xee, 0xee, 0xee) : TEXT_DIM);
        p.drawText(QRect(hx + 4, 3, COL_W - 8, 18), Qt::AlignVCenter | Qt::AlignLeft,
                   nameFm.elidedText(col.name, Qt::ElideRight, COL_W - 8));

        const QRectF eyeRect(hx + 6, HEADER_H - 20, 18, 14);
        drawEyeGlyph(p, eyeRect, col.visible);

        if (!col.isCamera)
        {
            p.setFont(metaFont);
            p.setPen(TEXT_DIM);
            p.drawText(QRect(hx, HEADER_H - 21, COL_W - 8, 16), Qt::AlignVCenter | Qt::AlignRight,
                       QString::number(qRound(col.opacity * 100)) + QStringLiteral("%"));
        }

        if (mHoverEyeColumn == i)
        {
            p.setPen(QPen(EYE_ON));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(eyeRect.adjusted(-3, -2, 3, 2), 3, 3);
        }
    }
    p.setPen(QPen(COL_SEP));
    for (int i = 0; i <= mColumns.size(); ++i)
    {
        const int x = GUTTER_W + i * COL_W;
        p.drawLine(x, 0, x, HEADER_H);
    }
    p.setPen(QPen(LINE_STRONG));
    p.drawLine(0, HEADER_H - 1, cW, HEADER_H - 1);
    p.setBrush(Qt::NoBrush);
    p.translate(hOff, 0);
}

void ExposureSheetView::mousePressEvent(QMouseEvent* event)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }

    const int x = event->pos().x();
    const int y = event->pos().y();
    const int hOff = horizontalScrollBar()->value();
    const int vOff = verticalScrollBar()->value();
    const int contentX = x + hOff;

    if (y < HEADER_H)
    {
        if (contentX < GUTTER_W) { return; }
        const int idx = (contentX - GUTTER_W) / COL_W;
        Layer* layer = columnLayer(idx);
        if (layer == nullptr) { return; }

        const QRect eyeRect(GUTTER_W + idx * COL_W + 6 - hOff, HEADER_H - 20, 18, 14);
        if (eyeRect.contains(event->pos()))
        {
            layer->setVisible(!layer->visible());
            emit mEditor->updateTimeLine();
            mEditor->getScribbleArea()->update();
            viewport()->update();
            return;
        }

        mEditor->layers()->setCurrentLayer(layer);
        return;
    }

    const int frame = rowForY(y + vOff);
    if (frame < 1 || frame > mFrameCount) { return; }

    const int idx = (contentX >= GUTTER_W) ? (contentX - GUTTER_W) / COL_W : -1;

    if (event->button() == Qt::RightButton)
    {
        Layer* layer = columnLayer(idx);
        if (layer && layer->isBitmapKind())
        {
            toggleKeyDrawingAt(layer, frame);
        }
        return;
    }

    mEditor->scrubTo(frame);
    if (Layer* layer = columnLayer(idx))
    {
        mEditor->layers()->setCurrentLayer(layer);
    }
}

void ExposureSheetView::mouseMoveEvent(QMouseEvent* event)
{
    const int x = event->pos().x();
    const int y = event->pos().y();
    const int hOff = horizontalScrollBar()->value();
    const int contentX = x + hOff;

    int hover = -1;
    if (y < HEADER_H && contentX >= GUTTER_W)
    {
        const int idx = (contentX - GUTTER_W) / COL_W;
        if (idx >= 0 && idx < mColumns.size())
        {
            const QRect eyeRect(GUTTER_W + idx * COL_W + 6 - hOff, HEADER_H - 20, 18, 14);
            if (eyeRect.contains(event->pos()))
            {
                hover = idx;
            }
        }
    }
    viewport()->setCursor(hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (hover != mHoverEyeColumn)
    {
        mHoverEyeColumn = hover;
        viewport()->update();
    }
}

void ExposureSheetView::leaveEvent(QEvent* event)
{
    QAbstractScrollArea::leaveEvent(event);
    mHoverEyeColumn = -1;
    viewport()->setCursor(Qt::ArrowCursor);
    viewport()->update();
}

void ExposureSheetView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void ExposureSheetView::showEvent(QShowEvent* event)
{
    QAbstractScrollArea::showEvent(event);
    ensureFrameVisible(mCurrentFrame);
    viewport()->update();
}

// ---------------------------------------------------------------------------
// ExposureSheetPanel
// ---------------------------------------------------------------------------

ExposureSheetPanel::ExposureSheetPanel(QWidget* parent) : BaseDockWidget(parent)
{
}

void ExposureSheetPanel::initUI()
{
    setWindowTitle(tr("摄影表"));
    setTitle(tr("摄影表"));

    QWidget* root = new QWidget(this);
    QVBoxLayout* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(4, 4, 4, 4);
    rootLay->setSpacing(4);

    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    QLabel* label = new QLabel(tr("当前张标记："), root);
    mToggleKeyButton = new QPushButton(tr("原画"), root);
    mToggleKeyButton->setCheckable(true);
    mToggleKeyButton->setChecked(true);
    mToggleKeyButton->setEnabled(false);
    mToggleKeyButton->setFixedHeight(24);
    mToggleKeyButton->setMinimumWidth(72);
    mToggleKeyButton->setToolTip(tr("切换当前张的原画/中割标记（也可右键律表格子切换）"));
    topRow->addWidget(label);
    topRow->addWidget(mToggleKeyButton);
    topRow->addStretch();
    rootLay->addLayout(topRow);

    mView = new ExposureSheetView(root);
    rootLay->addWidget(mView, 1);

    setWidget(root);

    Editor* e = editor();
    Q_ASSERT(e != nullptr);
    mView->setEditor(e);

    // 按钮与律表状态双向同步（mSyncingButton 防回环）
    connect(mToggleKeyButton, &QPushButton::toggled, this, [this](bool)
    {
        if (!mSyncingButton)
        {
            mView->toggleCurrentKeyDrawing();
        }
    });
    connect(mView, &ExposureSheetView::toggleTargetChanged,
            this, [this](bool hasKey, bool isKeyDrawing)
    {
        syncToggleButton(hasKey, isKeyDrawing);
    });

    // 与编辑器同步
    connect(e, &Editor::scrubbed, this, [this](int frame) { mView->updateCurrentFrame(frame); });
    connect(e, &Editor::framesModified, this, [this] { mView->rebuildColumns(); });
    connect(e, &Editor::updateTimeLine, this, [this] { mView->rebuildColumns(); });
    connect(e, &Editor::updateLayerCount, this, [this] { mView->rebuildColumns(); });
    connect(e, &Editor::fpsChanged, this, [this](int) { mView->rebuildColumns(); });
    connect(e, &Editor::objectLoaded, this, [this, e]
    {
        mView->verticalScrollBar()->setValue(0);
        mView->rebuildColumns();
        mView->updateCurrentFrame(e->currentFrame());
    });
    connect(e->layers(), &LayerManager::currentLayerChanged, this, [this](int) { mView->refreshHighlight(); });
    connect(e->layers(), &LayerManager::layerDeleted, this, [this](int) { mView->rebuildColumns(); });
    connect(e->layers(), &LayerManager::layerCountChanged, this, [this](int) { mView->rebuildColumns(); });

    // 列头眼睛开关后律表重绘（updateTimeLine 已覆盖，这里兜底首建）
    mView->rebuildColumns();
    mView->updateCurrentFrame(e->currentFrame());
}

void ExposureSheetPanel::updateUI()
{
    mView->rebuildColumns();
    mView->updateCurrentFrame(editor()->currentFrame());
}

void ExposureSheetPanel::syncToggleButton(bool hasKey, bool isKeyDrawing)
{
    mSyncingButton = true;
    mToggleKeyButton->setEnabled(hasKey);
    mToggleKeyButton->setChecked(hasKey && isKeyDrawing);
    mToggleKeyButton->setText(hasKey ? (isKeyDrawing ? tr("原画") : tr("中割")) : tr("无关键帧"));
    mSyncingButton = false;
}
