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
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QUndoCommand>

#include "pencildef.h"

#include <algorithm>

namespace
{
constexpr int GUTTER_W = 38; // 帧号栏宽（缩放基准）
constexpr int COL_W    = 66; // 图层列宽（缩放基准）
constexpr int ROW_H    = 16; // 一帧行高（缩放基准）
constexpr int HEADER_H = 46; // 列头高（固定，不随缩放）

constexpr qreal ZOOM_MIN = 0.6;
constexpr qreal ZOOM_MAX = 3.0;
constexpr qreal ZOOM_STEP = 1.15;
constexpr int    DRAG_THRESHOLD = 4; // 拖动启动阈值（像素）

// 帧号栏（标尺区域）与播放头高亮：两套主题下保持不变
const QColor GUTTER_BG    (0x19, 0x19, 0x19);
const QColor GUTTER_TEXT  (0xb9, 0xb9, 0xb9);
const QColor BAND_FILL    (255, 171, 64, 46);
const QColor BAND_LINE    (255, 171, 64, 200);
const QColor BAND_TEXT    (0xff, 0xab, 0x40);

/** 暗色配色（默认） */
SheetPalette darkSheetPalette()
{
    SheetPalette p;
    p.bodyBg      = QColor(0x1e, 0x1e, 0x1e);
    p.headerBg    = QColor(0x23, 0x23, 0x23);
    p.headerBgCur = QColor(0x30, 0x30, 0x30);
    p.lineFaint   = QColor(255, 255, 255, 18);
    p.lineMid     = QColor(255, 255, 255, 34);
    p.lineStrong  = QColor(255, 255, 255, 72);
    p.colSep      = QColor(255, 255, 255, 30);
    p.keyCircle   = QColor(0xf2, 0xf2, 0xf2);
    p.keyNumber   = QColor(255, 255, 255);
    p.dotFill     = QColor(0xc8, 0xc8, 0xc8);
    p.expoLine    = QColor(0x7a, 0x7a, 0x7a);
    p.expoOpen    = QColor(0x5e, 0x5e, 0x5e);
    p.cameraMark  = QColor(0x86, 0xb9, 0xe8);
    p.layerTint   = QColor(255, 255, 255, 10);
    p.nameText    = QColor(0xee, 0xee, 0xee);
    p.nameHidden  = QColor(0x9a, 0x9a, 0x9a);
    p.metaText    = QColor(0x9a, 0x9a, 0x9a);
    p.eyeOn       = QColor(0xe6, 0xe6, 0xe6);
    p.eyeOff      = QColor(0x78, 0x78, 0x78);
    return p;
}

/** 亮色配色（参考图）：底色 #DEDEDE、黑线、黑字 */
SheetPalette lightSheetPalette()
{
    SheetPalette p;
    p.bodyBg      = QColor(0xDE, 0xDE, 0xDE);
    p.headerBg    = QColor(0xDE, 0xDE, 0xDE);
    p.headerBgCur = QColor(0xC9, 0xC9, 0xC9);
    p.lineFaint   = QColor(0, 0, 0, 45);
    p.lineMid     = QColor(0, 0, 0, 90);
    p.lineStrong  = QColor(0, 0, 0, 160);
    p.colSep      = QColor(0, 0, 0, 60);
    p.keyCircle   = QColor(0x1a, 0x1a, 0x1a);
    p.keyNumber   = QColor(0x00, 0x00, 0x00);
    p.dotFill     = QColor(0x22, 0x22, 0x22);
    p.expoLine    = QColor(0x55, 0x55, 0x55);
    p.expoOpen    = QColor(0x77, 0x77, 0x77);
    p.cameraMark  = QColor(0x3a, 0x6e, 0xa5);
    p.layerTint   = QColor(0, 0, 0, 14);
    p.nameText    = QColor(0x1f, 0x1f, 0x1f);
    p.nameHidden  = QColor(0x8a, 0x8a, 0x8a);
    p.metaText    = QColor(0x5a, 0x5a, 0x5a);
    p.eyeOn       = QColor(0x33, 0x33, 0x33);
    p.eyeOff      = QColor(0x99, 0x99, 0x99);
    return p;
}

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

void drawEyeGlyph(QPainter& p, const QRectF& r, bool open, const QColor& onColor, const QColor& offColor)
{
    p.save();
    QPen pen(open ? onColor : offColor);
    pen.setWidthF(1.2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (open)
    {
        const QRectF eye(r.left(), r.center().y() - r.height() / 2, r.width(), r.height());
        p.drawEllipse(eye);
        p.setBrush(open ? onColor : offColor);
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
    mPalette = darkSheetPalette();
}

void ExposureSheetView::setLightMode(bool light)
{
    mLightMode = light;
    mPalette = light ? lightSheetPalette() : darkSheetPalette();
    viewport()->update();
}

int ExposureSheetView::gutterW() const { return qRound(GUTTER_W * mZoom); }
int ExposureSheetView::colW() const { return qRound(COL_W * mZoom); }
int ExposureSheetView::rowH() const { return qRound(ROW_H * mZoom); }
int ExposureSheetView::headerH() const { return qMax(30, qRound(HEADER_H * mZoom)); }

QRectF ExposureSheetView::eyeRectForColumn(int index) const
{
    const qreal w = qMax<qreal>(14.0, 18.0 * mZoom);
    const qreal h = qMax<qreal>(11.0, 14.0 * mZoom);
    return QRectF(gutterW() + index * colW() + qMax<qreal>(4.0, 6.0 * mZoom),
                  headerH() - h - qMax<qreal>(3.0, 4.0 * mZoom),
                  w, h);
}

int ExposureSheetView::frameRow(int frame) const
{
    return headerH() + (frame - 1) * rowH();
}

int ExposureSheetView::rowForY(int y) const
{
    return (y - headerH()) / rowH() + 1;
}

int ExposureSheetView::contentWidth() const
{
    return gutterW() + static_cast<int>(mColumns.size()) * colW();
}

int ExposureSheetView::contentHeight() const
{
    return HEADER_H + mFrameCount * rowH();
}

void ExposureSheetView::updateScrollRanges()
{
    const int w = qMax(contentWidth(), viewport()->width());
    const int h = qMax(contentHeight(), viewport()->height());
    horizontalScrollBar()->setRange(0, w - viewport()->width());
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setSingleStep(qMax(1, colW() / 2));
    verticalScrollBar()->setRange(0, h - viewport()->height());
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(qMax(1, rowH() * 3));
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

    mBitmapColCount = 0;
    for (const SheetColumn& c : mColumns)
    {
        if (!c.isCamera) { ++mBitmapColCount; }
        else { break; }
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
    const int yBot = yTop + rowH();
    if (yTop < vb->value() + headerH())
    {
        vb->setValue(qMax(0, yTop - headerH()));
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

void ExposureSheetView::addKeyAt(Layer* layer, int pos)
{
    if (layer == nullptr || !layer->isBitmapKind() || layer->keyExists(pos)) { return; }

    mEditor->beginLayerLayoutEdit(layer);
    const bool added = layer->addNewKeyFrameAt(pos);
    mEditor->endLayerLayoutEdit(tr("添加关键帧"));

    if (added)
    {
        mEditor->scrubTo(pos);
        mEditor->layers()->notifyLayerChanged(layer);
        mEditor->layers()->notifyAnimationLengthChanged();
        emit mEditor->framesModified();
    }
}

void ExposureSheetView::deleteKeyAt(Layer* layer, int pos)
{
    if (layer == nullptr || !layer->isBitmapKind() || !layer->keyExists(pos)) { return; }
    if (layer->keyFrameCount() <= 1) { return; } // 非声音层保底留最后一帧（与时间轴一致）

    mEditor->beginLayerLayoutEdit(layer);
    mEditor->takeLayerKeyFrame(layer, pos);
    layer->absorbGapsAt({ pos });
    mEditor->endLayerLayoutEdit(tr("删除关键帧"));

    mEditor->layers()->notifyLayerChanged(layer);
    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();
}

void ExposureSheetView::moveKeyBetween(Layer* srcLayer, int srcPos, Layer* dstLayer, int dstPos)
{
    if (srcLayer == nullptr || dstLayer == nullptr) { return; }
    if (!srcLayer->isBitmapKind() || !dstLayer->isBitmapKind()) { return; }
    if (srcLayer == dstLayer && srcPos == dstPos) { return; }
    if (!srcLayer->keyExists(srcPos)) { return; }
    if (dstLayer->keyExists(dstPos)) { return; } // 目标格已有关键帧：放弃移动

    mEditor->beginLayerLayoutEdit(srcLayer);
    if (dstLayer != srcLayer)
    {
        mEditor->addLayerToLayoutEdit(dstLayer);
    }
    KeyFrame* key = mEditor->takeLayerKeyFrame(srcLayer, srcPos);
    dstLayer->addKeyFrame(dstPos, key);
    srcLayer->absorbGapsAt({ srcPos });
    mEditor->endLayerLayoutEdit(tr("移动律表张数"));

    mEditor->layers()->notifyLayerChanged(srcLayer);
    if (dstLayer != srcLayer)
    {
        mEditor->layers()->notifyLayerChanged(dstLayer);
    }
    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();
}

void ExposureSheetView::commitDrag()
{
    if (!mDragging) { return; }
    if (mDragCurCol >= 0 && mDragCurPos >= 1 && mDragCurPos <= mFrameCount
        && (mDragCurCol != mDragPressCol || mDragCurPos != mDragSrcPos))
    {
        Layer* srcLayer = columnLayer(mDragPressCol);
        Layer* dstLayer = columnLayer(mDragCurCol);
        moveKeyBetween(srcLayer, mDragSrcPos, dstLayer, mDragCurPos);
    }
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
    const int gw = gutterW();
    const int cw = colW();
    const int rh = rowH();
    const int cW = qMax(contentWidth(), vpW);
    const int cH = contentHeight();

    // ---- 主体：帧网格 + 符号 + 播放头带 ----
    p.translate(-hOff, -vOff);
    p.fillRect(QRect(0, 0, cW, cH), mPalette.bodyBg);

    // 当前层列淡色底
    for (int i = 0; i < mColumns.size(); ++i)
    {
        if (mColumns[i].layerId == mCurrentLayerId)
        {
            p.fillRect(QRect(gw + i * cw, headerH(), cw, cH - headerH()), mPalette.layerTint);
        }
    }

    // 横向帧线（只画可视行）：每帧淡线、每3帧中线、每秒(fps帧)粗线
    const int fFirst = qBound(1, rowForY(vOff), mFrameCount);
    const int fLast = qBound(1, rowForY(vOff + vpH), mFrameCount);
    p.setBrush(Qt::NoBrush);
    for (int f = fFirst; f <= fLast; ++f)
    {
        const int y = frameRow(f) + rh;
        p.setPen(QPen(f % mFps == 0 ? mPalette.lineStrong : (f % 3 == 0 ? mPalette.lineMid : mPalette.lineFaint)));
        p.drawLine(gw, y, cW, y);
    }

    // 列分隔竖线
    p.setPen(QPen(mPalette.colSep));
    for (int i = 0; i <= mColumns.size(); ++i)
    {
        const int x = gw + i * cw;
        p.drawLine(x, headerH(), x, cH);
    }

    // 符号：原画=圆圈数字 / 中割=点 / 相机=菱形；曝光延续竖线（开放尾块=虚线）
    QFont numFont = font();
    numFont.setPixelSize(qMax(6, qRound(9 * mZoom)));
    numFont.setBold(true);
    const QFontMetrics numFm(numFont);
    const qreal dotR = qBound(2.0, 3.0 * mZoom, 6.0);
    const qreal diamondR = qBound(3.0, 5.0 * mZoom, 10.0);
    const int expoInset = qBound(4, qRound(6 * mZoom), 12);
    for (int i = 0; i < mColumns.size(); ++i)
    {
        const SheetColumn& col = mColumns[i];
        const int cx = gw + i * cw;
        for (const SheetKeyEntry& e : col.keys)
        {
            if (e.blockEnd > 0 && e.blockEnd < fFirst) { continue; } // 整块在可视区上方
            if (e.pos > fLast) { break; }                            // keys 升序，后面更远

            const int cy = frameRow(e.pos) + rh / 2;
            const int exposureEnd = (e.blockEnd > 0) ? e.blockEnd - 1 : mFrameCount;
            if (exposureEnd > e.pos)
            {
                QPen expoPen(e.blockEnd > 0 ? mPalette.expoLine : mPalette.expoOpen);
                if (e.blockEnd < 0) { expoPen.setStyle(Qt::DashLine); }
                p.setPen(expoPen);
                p.drawLine(cx + cw / 2, cy + expoInset, cx + cw / 2, frameRow(exposureEnd) + rh / 2 - expoInset);
            }

            if (col.isCamera)
            {
                p.setPen(QPen(mPalette.cameraMark));
                p.setBrush(mPalette.cameraMark);
                QPolygonF diamond;
                diamond << QPointF(cx + cw / 2, cy - diamondR)
                        << QPointF(cx + cw / 2 + diamondR, cy)
                        << QPointF(cx + cw / 2, cy + diamondR)
                        << QPointF(cx + cw / 2 - diamondR, cy);
                p.drawPolygon(diamond);
            }
            else if (e.keyDrawing)
            {
                const QString text = QString::number(e.number);
                const qreal w = qMax<qreal>(rh - 3, numFm.horizontalAdvance(text) + 6);
                p.setPen(QPen(mPalette.keyCircle));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(cx + cw / 2, cy + 0.5), w / 2, (rh - 3) / 2.0);
                p.setPen(mPalette.keyNumber);
                p.setFont(numFont);
                p.drawText(QRect(cx, cy - rh / 2, cw, rh), Qt::AlignCenter, text);
            }
            else
            {
                p.setPen(Qt::NoPen);
                p.setBrush(mPalette.dotFill);
                p.drawEllipse(QPointF(cx + cw / 2, cy + 0.5), dotR, dotR);
            }
        }
    }

    // 当前帧行高亮带
    if (mCurrentFrame >= 1 && mCurrentFrame <= mFrameCount)
    {
        const int bandY = frameRow(mCurrentFrame);
        p.fillRect(QRect(gw, bandY, cW - gw, rh), BAND_FILL);
        p.setPen(QPen(BAND_LINE));
        p.drawLine(gw, bandY, cW, bandY);
        p.drawLine(gw, bandY + rh, cW, bandY + rh);
    }

    // 拖动预览：目标格高亮 + 半透明符号
    if (mDragging && mDragCurCol >= 0 && mDragCurPos >= 1 && mDragCurPos <= mFrameCount)
    {
        const int gx = gw + mDragCurCol * cw;
        const int gy = frameRow(mDragCurPos);
        p.fillRect(QRect(gx, gy, cw, rh), QColor(255, 171, 64, 38));
        p.setPen(QPen(BAND_LINE));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRect(gx, gy, cw, rh).adjusted(0, 0, -1, -1));

        p.setOpacity(0.55);
        const int cy = gy + rh / 2;
        if (mDragWasKeyDrawing)
        {
            const QString text = QString::number(mDragNumber);
            const qreal w = qMax<qreal>(rh - 3, numFm.horizontalAdvance(text) + 6);
            p.setPen(QPen(mPalette.keyCircle));
            p.drawEllipse(QPointF(gx + cw / 2, cy + 0.5), w / 2, (rh - 3) / 2.0);
            p.setPen(mPalette.keyNumber);
            p.setFont(numFont);
            p.drawText(QRect(gx, cy - rh / 2, cw, rh), Qt::AlignCenter, text);
        }
        else
        {
            p.setPen(Qt::NoPen);
            p.setBrush(mPalette.dotFill);
            p.drawEllipse(QPointF(gx + cw / 2, cy + 0.5), dotR, dotR);
        }
        p.setOpacity(1.0);
    }
    p.setBrush(Qt::NoBrush);
    p.translate(hOff, vOff);

    // ---- 帧号栏：钉在视口左缘（不随横向滚动） ----
    p.fillRect(QRect(0, 0, gw, vpH), GUTTER_BG);
    p.translate(0, -vOff);
    QFont gutterFont = font();
    gutterFont.setPixelSize(qMax(6, qRound(9 * mZoom)));
    p.setFont(gutterFont);
    for (int f = fFirst; f <= fLast; ++f)
    {
        if (f != 1 && f % 3 != 0) { continue; }
        const int cy = frameRow(f) + rh / 2;
        p.setPen(f == mCurrentFrame ? BAND_TEXT : GUTTER_TEXT);
        p.drawText(QRect(0, cy - rh / 2, gw - 6, rh),
                   Qt::AlignVCenter | Qt::AlignRight, QString::number(f));
    }
    p.setPen(QPen(mPalette.colSep));
    p.drawLine(gw - 1, 0, gw - 1, cH);
    p.setBrush(Qt::NoBrush);
    p.translate(0, vOff);

    // ---- 列头：钉在视口上缘（不随纵向滚动），高度/字号/图标随缩放等比 ----
    const int hh = headerH();
    p.fillRect(QRect(0, 0, vpW, hh), mPalette.headerBg);
    p.translate(-hOff, 0);
    QFont nameFont = font();
    nameFont.setPixelSize(qBound(6, qRound(10 * mZoom), 30));
    nameFont.setBold(true);
    const QFontMetrics nameFm(nameFont);
    QFont metaFont = font();
    metaFont.setPixelSize(qBound(5, qRound(9 * mZoom), 26));
    const int namePad = qMax(3, qRound(4 * mZoom));
    const int bottomRowH = qMax(15, qRound(20 * mZoom));
    for (int i = 0; i < mColumns.size(); ++i)
    {
        const SheetColumn& col = mColumns[i];
        const int hx = gw + i * cw;
        if (col.layerId == mCurrentLayerId)
        {
            p.fillRect(QRect(hx + 1, 0, cw - 1, hh - 1), mPalette.headerBgCur);
        }

        // 层名：列头上半区（超宽省略）
        p.setFont(nameFont);
        p.setPen(col.visible ? mPalette.nameText : mPalette.nameHidden);
        const int nameRight = cw - 2 * namePad;
        p.drawText(QRect(hx + namePad, qRound(2 * mZoom), nameRight,
                         hh - bottomRowH - qRound(2 * mZoom)),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   nameFm.elidedText(col.name, Qt::ElideRight, nameRight));

        // 眼睛 + 透明度：列头下半区
        const QRectF eyeRect = eyeRectForColumn(i);
        drawEyeGlyph(p, eyeRect, col.visible, mPalette.eyeOn, mPalette.eyeOff);

        if (!col.isCamera)
        {
            p.setFont(metaFont);
            p.setPen(mPalette.metaText);
            const int metaLeft = qRound(eyeRect.right()) + qMax(4, qRound(6 * mZoom));
            p.drawText(QRect(metaLeft, hh - bottomRowH, cw - (metaLeft - hx) - namePad, bottomRowH),
                       Qt::AlignVCenter | Qt::AlignRight,
                       QString::number(qRound(col.opacity * 100)) + QStringLiteral("%"));
        }

        if (mHoverEyeColumn == i)
        {
            p.setPen(QPen(mPalette.eyeOn));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(eyeRect.adjusted(-3, -2, 3, 2), 3, 3);
        }
    }
    p.setPen(QPen(mPalette.colSep));
    for (int i = 0; i <= mColumns.size(); ++i)
    {
        const int x = gw + i * cw;
        p.drawLine(x, 0, x, hh);
    }
    p.setPen(QPen(mPalette.lineStrong));
    p.drawLine(gw, hh - 1, cW, hh - 1);
    p.setBrush(Qt::NoBrush);
    p.translate(hOff, 0);
    // 标尺拐角最后盖回：横向滚动后列内容（当前列高亮/文字/分隔线）会随平移
    // 越进视口左缘 [0..gw]，帧号栏顶格恒属标尺区域，须保持标尺底色
    p.fillRect(QRect(0, 0, gw, hh), GUTTER_BG);
}

void ExposureSheetView::mousePressEvent(QMouseEvent* event)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }

    // 中键按住拖动 = 平移律表（两轴自由滚动）
    if (event->button() == Qt::MiddleButton)
    {
        mPanning = true;
        mDragArmed = false;
        mDragging = false;
        mDragCurCol = -1;
        mDragCurPos = -1;
        mPanStartPos = event->pos();
        mPanStartH = horizontalScrollBar()->value();
        mPanStartV = verticalScrollBar()->value();
        viewport()->setCursor(Qt::ClosedHandCursor);
        viewport()->update();
        return;
    }

    const int x = event->pos().x();
    const int y = event->pos().y();
    const int hOff = horizontalScrollBar()->value();
    const int vOff = verticalScrollBar()->value();
    const int gw = gutterW();
    const int cw = colW();
    const int contentX = x + hOff;

    if (y < headerH())
    {
        if (contentX < gw) { return; }
        const int idx = (contentX - gw) / cw;
        Layer* layer = columnLayer(idx);
        if (layer == nullptr) { return; }

        QRectF eyeRect = eyeRectForColumn(idx);
        eyeRect.translate(-hOff, 0);
        if (eyeRect.contains(event->position()))
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

    const int idx = (contentX >= gw) ? (contentX - gw) / cw : -1;
    Layer* layer = columnLayer(idx);
    const bool bitmapCol = (layer && layer->isBitmapKind());

    if (event->button() == Qt::RightButton)
    {
        if (!bitmapCol) { return; }

        // 右键菜单：添加/删除关键帧 + 原画/中割切换
        QMenu menu(this);
        if (layer->keyExists(frame))
        {
            KeyFrame* key = layer->getKeyFrameAt(frame);
            QAction* toggleAct = menu.addAction(
                key->isKeyDrawing() ? tr("标记为中割") : tr("标记为原画"));
            menu.addSeparator();
            QAction* delAct = menu.addAction(tr("删除关键帧"));

            QAction* chosen = menu.exec(event->globalPosition().toPoint());
            if (chosen == toggleAct) { toggleKeyDrawingAt(layer, frame); }
            else if (chosen == delAct) { deleteKeyAt(layer, frame); }
        }
        else
        {
            QAction* addAct = menu.addAction(tr("添加关键帧"));
            if (menu.exec(event->globalPosition().toPoint()) == addAct)
            {
                addKeyAt(layer, frame);
            }
        }
        return;
    }

    if (event->button() != Qt::LeftButton) { return; }

    // 按在帧格上：先走点击语义（跳帧+切层），同时布防拖动
    if (bitmapCol && layer->keyExists(frame))
    {
        mDragArmed = true;
        mDragging = false;
        mDragPressPos = event->pos();
        mDragPressCol = idx;
        mDragSrcPos = frame;
        mDragCurCol = idx;
        mDragCurPos = frame;
        for (const SheetKeyEntry& e : mColumns[idx].keys)
        {
            if (e.pos == frame)
            {
                mDragWasKeyDrawing = e.keyDrawing;
                mDragNumber = e.number;
                break;
            }
        }
    }

    mEditor->scrubTo(frame);
    if (layer)
    {
        mEditor->layers()->setCurrentLayer(layer);
    }
}

void ExposureSheetView::mouseMoveEvent(QMouseEvent* event)
{
    if (mPanning && (event->buttons() & Qt::MiddleButton))
    {
        horizontalScrollBar()->setValue(mPanStartH - (event->pos().x() - mPanStartPos.x()));
        verticalScrollBar()->setValue(mPanStartV - (event->pos().y() - mPanStartPos.y()));
        return;
    }

    if (mDragArmed && (event->buttons() & Qt::LeftButton))
    {
        if (!mDragging)
        {
            if ((event->pos() - mDragPressPos).manhattanLength() > DRAG_THRESHOLD)
            {
                mDragging = true;
                mHoverEyeColumn = -1;
                viewport()->setCursor(Qt::ClosedHandCursor);
            }
        }
        if (mDragging)
        {
            const int vOff = verticalScrollBar()->value();
            const int hOff = horizontalScrollBar()->value();
            // 目标行/列：夹在位图列范围与表长内
            int frame = rowForY(event->pos().y() + vOff);
            frame = qBound(1, frame, mFrameCount);
            int col = (event->pos().x() + hOff >= gutterW())
                          ? (event->pos().x() + hOff - gutterW()) / colW() : 0;
            col = qBound(0, col, qMax(0, mBitmapColCount - 1));

            if (col != mDragCurCol || frame != mDragCurPos)
            {
                mDragCurCol = col;
                mDragCurPos = frame;
                viewport()->update();
            }
        }
        return;
    }

    const int x = event->pos().x();
    const int y = event->pos().y();
    const int hOff = horizontalScrollBar()->value();
    const int contentX = x + hOff;

    int hover = -1;
    if (y < headerH() && contentX >= gutterW())
    {
        const int idx = (contentX - gutterW()) / colW();
        if (idx >= 0 && idx < mColumns.size())
        {
            QRectF eyeRect = eyeRectForColumn(idx);
            eyeRect.translate(-hOff, 0);
            if (eyeRect.contains(event->position()))
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

void ExposureSheetView::mouseReleaseEvent(QMouseEvent* event)
{
    QAbstractScrollArea::mouseReleaseEvent(event);

    if (event->button() == Qt::MiddleButton && mPanning)
    {
        mPanning = false;
        viewport()->setCursor(Qt::ArrowCursor);
        return;
    }

    if (mDragArmed)
    {
        commitDrag();
        mDragArmed = false;
        mDragging = false;
        mDragCurCol = -1;
        mDragCurPos = -1;
        viewport()->setCursor(Qt::ArrowCursor);
        viewport()->update();
    }
}

void ExposureSheetView::wheelEvent(QWheelEvent* event)
{
    // 滚轮=缩放律表（光标下的帧/列锚定不动；锚定用缩放前后各自的真实度量）
    const qreal oldZoom = mZoom;
    const int oldHeaderH = headerH();
    const int oldRowH = rowH();
    const int oldGutterW = gutterW();
    const int oldColW = colW();

    const qreal factor = (event->angleDelta().y() > 0) ? ZOOM_STEP : (1.0 / ZOOM_STEP);
    mZoom = qBound<qreal>(ZOOM_MIN, mZoom * factor, ZOOM_MAX);
    if (qFuzzyCompare(mZoom, oldZoom))
    {
        event->accept();
        return;
    }

    const qreal px = event->position().x();
    const qreal py = event->position().y();
    const qreal rowF = (py + verticalScrollBar()->value() - oldHeaderH) / qMax<qreal>(1.0, oldRowH);
    const qreal colF = (px + horizontalScrollBar()->value() - oldGutterW) / qMax<qreal>(1.0, oldColW);

    updateScrollRanges();
    verticalScrollBar()->setValue(qRound(headerH() + rowF * rowH() - py));
    horizontalScrollBar()->setValue(qRound(gutterW() + colF * colW() - px));
    viewport()->update();
    event->accept();
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
    mToggleKeyButton->setToolTip(
        tr("切换当前张的原画/中割标记。\n右键律表格子：添加/删除关键帧、切换原画/中割。\n拖动帧格：移动张数（可跨层列）；滚轮：缩放；中键拖动：平移。"));
    topRow->addWidget(label);
    topRow->addWidget(mToggleKeyButton);
    topRow->addStretch();

    // 主题切换：亮色（参考图配色）/暗色（默认），选择持久化
    mThemeButton = new QPushButton(tr("暗色"), root);
    mThemeButton->setFixedHeight(24);
    mThemeButton->setMinimumWidth(72);
    mThemeButton->setToolTip(tr("切换摄影表亮色/暗色配色"));
    topRow->addWidget(mThemeButton);
    rootLay->addLayout(topRow);

    mView = new ExposureSheetView(root);
    rootLay->addWidget(mView, 1);

    setWidget(root);

    Editor* e = editor();
    Q_ASSERT(e != nullptr);
    mView->setEditor(e);

    // 恢复上次主题选择
    QSettings settings(PENCIL2D, PENCIL2D);
    const bool light = settings.value(QStringLiteral("ExposureSheet/LightMode"), false).toBool();
    mView->setLightMode(light);
    mThemeButton->setText(light ? tr("亮色") : tr("暗色"));
    connect(mThemeButton, &QPushButton::clicked, this, [this]
    {
        const bool light = !mView->isLightMode();
        mView->setLightMode(light);
        mThemeButton->setText(light ? tr("亮色") : tr("暗色"));
        QSettings s(PENCIL2D, PENCIL2D);
        s.setValue(QStringLiteral("ExposureSheet/LightMode"), light);
    });

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
