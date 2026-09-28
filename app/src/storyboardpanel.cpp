/*
    Pencil Dream（Pencil2D TVP化分支）— 分镜面板

    分镜 = 图层结构的投影视图（TVP 哲学：分镜不是独立数据结构）：
    - 镜头单元 = 连续图层组（多层镜头）或未分组的位图层（单层镜头）
    - 镜头顺序 = 时间轴行序（索引大 = 屏幕上方 = 第一镜）
    - Action/Dialog/Notes 注释存于锚点层（组镜头挂首成员）
    - 拖动重排/并组走 LayerManager 事务（LayerOrderCommand 单步撤销）
    - 缩略图 = 镜头内容层在代表帧的多层合成（异步队列，同 TimeLineCells 模式）
*/
#include "storyboardpanel.h"

#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

#include <limits>

#include "bitmapimage.h"
#include "editor.h"
#include "keyframe.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "pencildef.h"

namespace
{
constexpr int SB_MARGIN = 10;
constexpr int SB_GAP = 10;
constexpr int SB_THUMB_W = 320; // 缩略图烘焙分辨率（卡内缩放绘制）
constexpr int SB_THUMB_H = 180;
constexpr int SB_DRAG_THRESHOLD = 6;

/** 分镜场次颜色表（colorIndex 0-7；-1 = 无色） */
QColor sbLabelColor(int index)
{
    static const QColor table[] =
    {
        QColor(0xE5, 0x48, 0x4D), QColor(0xF7, 0x6B, 0x15), QColor(0xFF, 0xC5, 0x3D), QColor(0x46, 0xA7, 0x58),
        QColor(0x00, 0xA2, 0xC7), QColor(0x00, 0x90, 0xFF), QColor(0x8E, 0x4E, 0xC6), QColor(0xD6, 0x40, 0x9F),
    };
    if (index < 0 || index >= 8) { return QColor(0x80, 0x80, 0x88); }
    return table[index];
}

qint64 sbThumbKey(int layerId, int framePos)
{
    return (static_cast<qint64>(layerId) << 32) | static_cast<quint32>(framePos);
}
} // namespace

// ---------------------------------------------------------------------------
// StoryboardView
// ---------------------------------------------------------------------------

StoryboardView::StoryboardView(QWidget* parent) : QAbstractScrollArea(parent)
{
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameStyle(QFrame::NoFrame);

    mThumbTimer = new QTimer(this);
    mThumbTimer->setSingleShot(true);
    connect(mThumbTimer, &QTimer::timeout, this, &StoryboardView::processThumbQueue);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { viewport()->update(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { viewport()->update(); });
}

int StoryboardView::columns() const
{
    const int usable = viewport()->width() - 2 * SB_MARGIN + SB_GAP;
    return qMax(1, usable / (mCardW + SB_GAP));
}

int StoryboardView::totalFrames() const
{
    int total = 0;
    for (const StoryboardShot& shot : mShots)
    {
        if (shot.lastFrame >= shot.firstFrame)
        {
            total += shot.lastFrame - shot.firstFrame + 1;
        }
    }
    return total;
}

QRect StoryboardView::cardRect(int shotIndex) const
{
    const int cols = columns();
    const int col = shotIndex % cols;
    const int row = shotIndex / cols;
    return QRect(SB_MARGIN + col * (mCardW + SB_GAP),
                 SB_MARGIN + row * (cardH() + SB_GAP),
                 mCardW, cardH());
}

void StoryboardView::eyeRectFor(int shotIndex, QRect* outEye) const
{
    const QRect card = cardRect(shotIndex);
    *outEye = QRect(card.right() - 22, card.top() + 8, 18, 14);
}

int StoryboardView::shotIndexAt(const QPoint& contentPos) const
{
    const int cols = columns();
    const int col = (contentPos.x() - SB_MARGIN) / (mCardW + SB_GAP);
    const int row = (contentPos.y() - SB_MARGIN) / (cardH() + SB_GAP);
    if (col < 0 || col >= cols || row < 0) { return -1; }
    const int index = row * cols + col;
    if (index >= mShots.size()) { return -1; }
    if (!cardRect(index).contains(contentPos)) { return -1; }
    return index;
}

int StoryboardView::gapIndexAt(const QPoint& contentPos) const
{
    const int cols = columns();
    const int n = mShots.size();
    if (n == 0) { return 0; }
    const int rows = (n + cols - 1) / cols;
    int row = (contentPos.y() - SB_MARGIN) / (cardH() + SB_GAP);
    row = qBound(0, row, rows - 1);
    const qreal step = mCardW + SB_GAP;
    int col = qRound((contentPos.x() - SB_MARGIN) / step);
    col = qBound(0, col, cols);
    return qBound(0, row * cols + col, n);
}

void StoryboardView::updateScrollRanges()
{
    const int cols = columns();
    const int rows = (mShots.size() + cols - 1) / cols;
    const int contentW = mShots.isEmpty() ? 0 : 2 * SB_MARGIN + cols * mCardW + (cols - 1) * SB_GAP;
    const int contentH = mShots.isEmpty() ? 0 : 2 * SB_MARGIN + rows * cardH() + (rows - 1) * SB_GAP;
    horizontalScrollBar()->setRange(0, qMax(0, contentW - viewport()->width()));
    verticalScrollBar()->setRange(0, qMax(0, contentH - viewport()->height()));
    horizontalScrollBar()->setPageStep(qMax(1, viewport()->width()));
    verticalScrollBar()->setPageStep(qMax(1, viewport()->height()));
}

void StoryboardView::rebuildShots()
{
    mShots.clear();
    mThumbQueue.clear();
    mThumbQueued.clear();

    if (mEditor == nullptr || mEditor->object() == nullptr)
    {
        updateScrollRanges();
        viewport()->update();
        return;
    }
    Object* obj = mEditor->object();
    const int n = obj->getLayerCount();

    LayerCamera* cameraLayer = dynamic_cast<LayerCamera*>(mEditor->layers()->getLastCameraLayer());
    mCameraLayerId = (cameraLayer != nullptr) ? cameraLayer->id() : -1;

    QSet<int> consumedGroups;
    // 显示序：索引大 = 屏幕上方 = 第一镜 → 从栈顶（大索引）往下走
    for (int i = n - 1; i >= 0; --i)
    {
        Layer* layer = obj->getLayer(i);
        if (layer == nullptr || !layer->isGroupable()) { continue; } // 声音/相机/参考视频不构成镜头

        StoryboardShot shot;
        if (layer->groupId() >= 0)
        {
            const int gid = layer->groupId();
            if (consumedGroups.contains(gid)) { continue; } // 该组镜头已收集
            consumedGroups.insert(gid);

            const QList<int> members = obj->layerGroupMemberIndices(gid); // 栈序升序连续段
            if (members.isEmpty()) { continue; }
            shot.groupId = gid;
            shot.firstLayerIndex = members.first();
            shot.layerCount = members.size();
            shot.anchorLayerId = obj->getLayer(members.first())->id();
            shot.topLayerId = obj->getLayer(members.last())->id();
            const LayerGroupInfo* info = obj->layerGroupInfo(gid);
            shot.name = (info != nullptr) ? info->name : tr("镜头组");
            shot.visible = obj->isLayerGroupVisible(gid);
            shot.colorIndex = obj->getLayer(members.first())->colorIndex();
        }
        else
        {
            shot.groupId = -1;
            shot.firstLayerIndex = i;
            shot.layerCount = 1;
            shot.anchorLayerId = layer->id();
            shot.topLayerId = layer->id();
            shot.name = layer->name();
            shot.visible = layer->visible();
            shot.colorIndex = layer->colorIndex();
        }

        // 帧范围：镜头内容层的曝光覆盖域（首key..末key+长度-1）
        int first = std::numeric_limits<int>::max();
        int last = -1;
        for (int m = shot.firstLayerIndex; m < shot.firstLayerIndex + shot.layerCount; ++m)
        {
            Layer* member = obj->getLayer(m);
            if (member == nullptr) { continue; }
            member->foreachKeyFrame([&first, &last](KeyFrame* key)
            {
                first = qMin(first, key->pos());
                last = qMax(last, key->pos() + key->length() - 1);
            });
        }
        if (last >= first)
        {
            shot.firstFrame = first;
            shot.lastFrame = last;
        }

        // 注释三字段（挂锚点层）
        Layer* anchor = obj->findLayerById(shot.anchorLayerId);
        if (anchor != nullptr)
        {
            shot.hasAction = !anchor->storyboardAction().isEmpty();
            shot.hasDialog = !anchor->storyboardDialog().isEmpty();
            shot.hasNotes = !anchor->storyboardNotes().isEmpty();
        }

        // 运镜角标：镜头帧范围内相机关键帧 > 1
        if (cameraLayer != nullptr && shot.lastFrame >= shot.firstFrame)
        {
            const int rangeStart = shot.firstFrame;
            const int rangeEnd = shot.lastFrame;
            int cameraKeys = 0;
            cameraLayer->foreachKeyFrame([&cameraKeys, rangeStart, rangeEnd](KeyFrame* key)
            {
                if (key->pos() >= rangeStart && key->pos() <= rangeEnd) { ++cameraKeys; }
            });
            shot.cameraMoves = cameraKeys > 1;
        }

        mShots.append(shot);
    }

    // 修剪失效缩略图（镜头已删/代表帧变化的残留条目）
    if (!mThumbCache.isEmpty())
    {
        QSet<qint64> validKeys;
        for (const StoryboardShot& shot : mShots)
        {
            validKeys.insert(sbThumbKey(shot.anchorLayerId, shot.firstFrame));
        }
        const QList<qint64> stale = [&validKeys, this]
        {
            QList<qint64> result;
            for (auto it = mThumbCache.keyBegin(); it != mThumbCache.keyEnd(); ++it)
            {
                if (!validKeys.contains(*it)) { result.append(*it); }
            }
            return result;
        }();
        for (qint64 key : stale) { mThumbCache.remove(key); }
    }

    updateScrollRanges();
    refreshHighlight();
    viewport()->update();
}

void StoryboardView::updateCurrentFrame(int frame)
{
    mCurrentFrame = frame;
    viewport()->update();
}

void StoryboardView::refreshHighlight()
{
    mCurrentShotByLayer = -1;
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }
    Layer* current = mEditor->layers()->currentLayer();
    if (current == nullptr) { return; }
    const int index = mEditor->object()->getIndex(current);
    for (int i = 0; i < mShots.size(); ++i)
    {
        const StoryboardShot& shot = mShots[i];
        if (index >= shot.firstLayerIndex && index < shot.firstLayerIndex + shot.layerCount)
        {
            mCurrentShotByLayer = i;
            break;
        }
    }
    viewport()->update();
}

void StoryboardView::setCardWidth(int width)
{
    mCardW = qBound(80, width, 240);
    updateScrollRanges();
    viewport()->update();
}

// ---------------------------------------------------------------------------
// 缩略图异步引擎
// ---------------------------------------------------------------------------

const StoryboardThumb* StoryboardView::thumbnailFor(int shotIndex)
{
    const StoryboardShot& shot = mShots[shotIndex];
    const qint64 key = sbThumbKey(shot.anchorLayerId, shot.firstFrame);
    const auto it = mThumbCache.constFind(key);
    if (it != mThumbCache.constEnd())
    {
        return &it.value();
    }
    if (!mThumbQueued.contains(key))
    {
        mThumbQueued.insert(key);
        mThumbQueue.enqueue({ shot.anchorLayerId, shot.firstFrame });
        if (!mThumbTimer->isActive())
        {
            mThumbTimer->start(30);
        }
    }
    return nullptr;
}

void StoryboardView::processThumbQueue()
{
    if (mEditor == nullptr || mEditor->object() == nullptr)
    {
        mThumbQueue.clear();
        mThumbQueued.clear();
        return;
    }
    Object* obj = mEditor->object();

    int generated = 0;
    while (!mThumbQueue.isEmpty() && generated < 4)
    {
        const QPair<int, int> request = mThumbQueue.dequeue();
        const qint64 key = sbThumbKey(request.first, request.second);
        mThumbQueued.remove(key);
        if (mThumbCache.contains(key)) { continue; }

        Layer* anchor = obj->findLayerById(request.first);
        if (anchor == nullptr) { continue; }
        const int anchorIndex = obj->getIndex(anchor);
        if (anchorIndex < 0) { continue; }

        // 展开镜头成员：锚点起同组连续段（散层 = 单层）
        int firstIndex = anchorIndex;
        int lastIndex = anchorIndex;
        const int gid = anchor->groupId();
        if (gid >= 0)
        {
            while (firstIndex - 1 >= 0 && obj->getLayer(firstIndex - 1)->groupId() == gid) { --firstIndex; }
            while (lastIndex + 1 < obj->getLayerCount() && obj->getLayer(lastIndex + 1)->groupId() == gid) { ++lastIndex; }
        }

        // 收集可见内容层在代表帧的图像 + 内容包围盒（位图世界坐标）
        const int frame = request.second;
        QRect world;
        QList<QPair<Layer*, BitmapImage*>> parts;
        for (int i = firstIndex; i <= lastIndex; ++i)
        {
            Layer* layer = obj->getLayer(i);
            if (layer == nullptr || !layer->isBitmapKind() || !obj->isLayerRenderable(layer)) { continue; }
            LayerBitmap* bitmapLayer = dynamic_cast<LayerBitmap*>(layer);
            if (bitmapLayer == nullptr) { continue; }
            // 循环层按显示帧取内容（paintImage 同式）
            BitmapImage* image = dynamic_cast<BitmapImage*>(
                bitmapLayer->getKeyFrameWhichCovers(bitmapLayer->displayFrameFor(frame)));
            if (image == nullptr || image->image() == nullptr || image->image()->isNull()) { continue; }
            const QRect bounds = image->bounds();
            if (bounds.isEmpty()) { continue; }
            world = world.isNull() ? bounds : world.united(bounds);
            parts.append({ layer, image });
        }

        StoryboardThumb thumb;
        if (!world.isNull() && !world.isEmpty() && !parts.isEmpty())
        {
            QImage canvas(world.size(), QImage::Format_ARGB32_Premultiplied);
            canvas.fill(Qt::transparent);
            QPainter canvasPainter(&canvas);
            canvasPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            canvasPainter.translate(-world.topLeft());
            for (const auto& part : parts)
            {
                canvasPainter.setOpacity(qBound(0.0, part.second->getOpacity() - (1.0 - part.first->opacity()), 1.0));
                part.second->paintImage(canvasPainter);
            }
            canvasPainter.end();

            // letterbox 进固定 16:9 烘焙卡
            QImage card(SB_THUMB_W, SB_THUMB_H, QImage::Format_ARGB32_Premultiplied);
            card.fill(Qt::transparent);
            const QSize scaled = world.size().scaled(SB_THUMB_W, SB_THUMB_H, Qt::KeepAspectRatio);
            const int dx = (SB_THUMB_W - scaled.width()) / 2;
            const int dy = (SB_THUMB_H - scaled.height()) / 2;
            QPainter cardPainter(&card);
            cardPainter.drawImage(QRect(dx, dy, scaled.width(), scaled.height()), canvas);
            cardPainter.end();

            thumb.pixmap = QPixmap::fromImage(card);
            thumb.worldRect = world;
            thumb.valid = true;
        }
        mThumbCache.insert(key, thumb);
        ++generated;
    }

    if (!mThumbQueue.isEmpty())
    {
        mThumbTimer->start(30);
    }
    else
    {
        viewport()->update();
    }
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

QString StoryboardView::durationText(const StoryboardShot& shot) const
{
    if (shot.lastFrame < shot.firstFrame) { return tr("空"); }
    const int frames = shot.lastFrame - shot.firstFrame + 1;
    if (mShowFrames || mFps <= 0) { return tr("%1帧").arg(frames); }
    return tr("%1秒").arg(QString::number(frames / static_cast<double>(mFps), 'f', 1));
}

void StoryboardView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), QColor(0x17, 0x17, 0x1B));

    if (mShots.isEmpty())
    {
        painter.setPen(QColor(0x8A, 0x8A, 0x94));
        painter.drawText(viewport()->rect(), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("暂无镜头\n位图图层（或图层组）即分镜镜头"));
        return;
    }

    painter.translate(-horizontalScrollBar()->value(), -verticalScrollBar()->value());

    for (int i = 0; i < mShots.size(); ++i)
    {
        drawCard(painter, i, cardRect(i));
    }

    // 拖动覆盖层：源卡压暗 + 幽灵卡 + 插入线/并组高亮
    if (mDragging && mDragSourceShot >= 0 && mDragSourceShot < mShots.size())
    {
        const QRect source = cardRect(mDragSourceShot);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 110));
        painter.drawRect(source);

        if (mDragMergeShot >= 0 && mDragMergeShot != mDragSourceShot)
        {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(0x46, 0xA7, 0x58), 3));
            painter.drawRoundedRect(cardRect(mDragMergeShot).adjusted(-2, -2, 2, 2), 7, 7);
        }
        else if (mDragGap >= 0)
        {
            const int n = mShots.size();
            const int gap = qBound(0, mDragGap, n);
            const int lineShot = (gap < n) ? gap : n - 1;
            const QRect lineCard = cardRect(lineShot);
            const int x = (gap < n) ? lineCard.left() - SB_GAP / 2
                                    : lineCard.right() + SB_GAP / 2;
            painter.setPen(QPen(QColor(0xFF, 0x5C, 0x8A), 3));
            painter.drawLine(x, lineCard.top(), x, lineCard.bottom());
        }

        // 幽灵卡（跟随鼠标的缩微卡）
        const StoryboardShot& shot = mShots[mDragSourceShot];
        QRect ghost(mDragPos.x() - mCardW / 5, mDragPos.y() - 16, mCardW / 2.5, cardH() / 2.5);
        painter.setPen(QPen(QColor(0xFF, 0x5C, 0x8A), 1));
        painter.setBrush(QColor(0x2C, 0x2C, 0x33, 220));
        painter.drawRoundedRect(ghost, 5, 5);
        painter.setPen(QColor(0xD9, 0xD9, 0xE0));
        painter.drawText(ghost.adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft,
                         fontMetrics().elidedText(shot.name, Qt::ElideRight, ghost.width() - 12));
    }
}

void StoryboardView::drawCard(QPainter& painter, int shotIndex, const QRect& rect)
{
    const StoryboardShot& shot = mShots[shotIndex];
    const bool isSelected = (shotIndex == mCurrentShotByLayer);

    // 卡底（自绘纪律：显式置刷）
    QColor base(0x23, 0x23, 0x29);
    if (mHoverShot == shotIndex) { base = QColor(0x2A, 0x2A, 0x31); }
    painter.setBrush(base);
    painter.setPen(QPen(QColor(0x38, 0x38, 0x41), 1));
    painter.drawRoundedRect(rect, 6, 6);

    // 场次颜色条（左缘）
    if (shot.colorIndex >= 0)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(sbLabelColor(shot.colorIndex));
        painter.drawRect(QRect(rect.left(), rect.top() + 2, 4, rect.height() - 4));
    }

    // 缩略图区
    const QRect thumbRect(rect.left() + 6, rect.top() + 6, rect.width() - 12, thumbH() - 8);
    const StoryboardThumb* thumb = thumbnailFor(shotIndex);
    if (thumb != nullptr && thumb->valid)
    {
        // letterbox：worldRect 等比缩放居中进卡（与烘焙同一几何）
        const QSize scaled = thumb->worldRect.size().scaled(thumbRect.width(), thumbRect.height(), Qt::KeepAspectRatio);
        const int dx = thumbRect.left() + (thumbRect.width() - scaled.width()) / 2;
        const int dy = thumbRect.top() + (thumbRect.height() - scaled.height()) / 2;
        const QRect dest(dx, dy, scaled.width(), scaled.height());
        painter.setBrush(QColor(0x10, 0x10, 0x13));
        painter.setPen(QPen(QColor(0x30, 0x30, 0x38), 1));
        painter.drawRect(thumbRect);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::NoPen);
        painter.drawPixmap(dest, thumb->pixmap);

        if (mShowCamera)
        {
            drawCameraOverlay(painter, thumbRect, thumb->worldRect, shot.firstFrame);
        }
    }
    else
    {
        painter.setBrush(QColor(0x14, 0x14, 0x18));
        painter.setPen(QPen(QColor(0x2C, 0x2C, 0x33), 1));
        painter.drawRect(thumbRect);
        painter.setPen(QColor(0x55, 0x55, 0x60));
        painter.drawText(thumbRect, Qt::AlignCenter, QStringLiteral("…"));
    }

    // 字号：角标用小字
    QFont smallFont = font();
    smallFont.setPointSizeF(qMax<qreal>(7.5, font().pointSizeF() - 2.0));

    // 时长角标（缩略图左下）
    const QString duration = durationText(shot);
    painter.setFont(smallFont);
    const int durationW = QFontMetrics(smallFont).horizontalAdvance(duration) + 10;
    QRect durationChip(thumbRect.left(), thumbRect.bottom() - 16, durationW, 15);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 165));
    painter.drawRoundedRect(durationChip, 3, 3);
    painter.setPen(QColor(0xE8, 0xE8, 0xEE));
    painter.drawText(durationChip, Qt::AlignCenter, duration);

    // 运镜角标（缩略图右下）
    if (shot.cameraMoves)
    {
        const QString cameraText = tr("运镜");
        const int cameraW = QFontMetrics(smallFont).horizontalAdvance(cameraText) + 10;
        QRect cameraChip(thumbRect.right() - cameraW, thumbRect.bottom() - 16, cameraW, 15);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x00, 0x90, 0xFF, 200));
        painter.drawRoundedRect(cameraChip, 3, 3);
        painter.setPen(Qt::white);
        painter.drawText(cameraChip, Qt::AlignCenter, cameraText);
    }

    // 层数角标（缩略图左上，仅组镜头）
    if (shot.layerCount > 1)
    {
        const QString layersText = tr("%1层").arg(shot.layerCount);
        const int layersW = QFontMetrics(smallFont).horizontalAdvance(layersText) + 10;
        QRect layersChip(thumbRect.left() + 1, thumbRect.top() + 1, layersW, 15);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 165));
        painter.drawRoundedRect(layersChip, 3, 3);
        painter.setPen(QColor(0xE8, 0xE8, 0xEE));
        painter.drawText(layersChip, Qt::AlignCenter, layersText);
    }

    // 眼睛开关（缩略图右上）
    QRect eyeRect;
    eyeRectFor(shotIndex, &eyeRect);
    painter.setPen(Qt::NoPen);
    painter.setBrush(mHoverEye && mHoverShot == shotIndex ? QColor(0x3C, 0x3C, 0x46)
                                                          : QColor(0, 0, 0, 150));
    painter.drawRoundedRect(eyeRect, 3, 3);
    const QColor eyeColor = shot.visible ? QColor(0xE8, 0xE8, 0xEE) : QColor(0x6A, 0x6A, 0x74);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(eyeColor, 1.2));
    if (shot.visible)
    {
        painter.drawEllipse(eyeRect.center() + QPoint(0, 1), 5, 3);
        painter.setBrush(eyeColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(eyeRect.center() + QPoint(0, 1), 1, 1);
    }
    else
    {
        painter.drawLine(eyeRect.left() + 3, eyeRect.center().y() + 1, eyeRect.right() - 3, eyeRect.center().y() + 1);
    }

    // 信息区：镜头名
    QFont nameFont = font();
    nameFont.setBold(true);
    painter.setFont(nameFont);
    const QRect nameRect(rect.left() + 10, rect.top() + thumbH() - 2, rect.width() - 20, 20);
    painter.setPen(QColor(0xEC, 0xEC, 0xF2));
    painter.drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetrics(nameFont).elidedText(shot.name, Qt::ElideRight, nameRect.width()));

    // 信息区第二行：注释标记（动/对/备，仅非空字段）
    painter.setFont(smallFont);
    const struct { bool on; QString label; QColor color; } chips[] =
    {
        { shot.hasAction, tr("动"), QColor(0x46, 0xA7, 0x58) },
        { shot.hasDialog, tr("对"), QColor(0x00, 0x90, 0xFF) },
        { shot.hasNotes,  tr("备"), QColor(0xF7, 0x6B, 0x15) },
    };
    int chipX = rect.left() + 10;
    const int chipY = nameRect.bottom() + 3;
    for (const auto& chip : chips)
    {
        if (!chip.on) { continue; }
        const int w = QFontMetrics(smallFont).horizontalAdvance(chip.label) + 8;
        painter.setPen(Qt::NoPen);
        painter.setBrush(chip.color);
        painter.drawRoundedRect(QRect(chipX, chipY, w, 14), 3, 3);
        painter.setPen(Qt::white);
        painter.drawText(QRect(chipX, chipY, w, 14), Qt::AlignCenter, chip.label);
        chipX += w + 4;
    }

    // 播放头所在镜头：蓝色描边；当前层所属镜头：红粉描边（时间轴选中规范）
    painter.setBrush(Qt::NoBrush);
    if (mCurrentFrame >= shot.firstFrame && mCurrentFrame <= shot.lastFrame)
    {
        painter.setPen(QPen(QColor(0x4A, 0x9E, 0xE8), 1));
        painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);
    }
    if (isSelected)
    {
        painter.setPen(QPen(QColor(0xFF, 0x5C, 0x8A), 2));
        painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);
    }
}

void StoryboardView::drawCameraOverlay(QPainter& painter, const QRect& thumbRect,
                                       const QRect& worldRect, int frame)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }
    LayerCamera* cameraLayer = dynamic_cast<LayerCamera*>(mEditor->object()->findLayerById(mCameraLayerId));
    if (cameraLayer == nullptr) { return; }

    // 相机在位图世界坐标看到的区域 = 逆(视图变换) ∘ 取景矩形
    const QTransform viewTransform = cameraLayer->getViewAtFrame(frame);
    if (!viewTransform.isInvertible()) { return; }
    const QPolygonF worldPolygon = viewTransform.inverted().map(QPolygonF(QRectF(cameraLayer->getViewRect())));

    // 与缩略图同一 letterbox 映射
    const QSize scaled = worldRect.size().scaled(thumbRect.width(), thumbRect.height(), Qt::KeepAspectRatio);
    const qreal dx = thumbRect.left() + (thumbRect.width() - scaled.width()) / 2.0;
    const qreal dy = thumbRect.top() + (thumbRect.height() - scaled.height()) / 2.0;
    QTransform map;
    map.translate(dx, dy);
    map.scale(scaled.width() / static_cast<qreal>(worldRect.width()),
              scaled.height() / static_cast<qreal>(worldRect.height()));
    map.translate(-worldRect.x(), -worldRect.y());
    const QPolygonF poly = map.map(worldPolygon);

    // 白垫底 + 黑虚线（缩略图底色不可控，双描保证可见）
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 170), 3.4));
    painter.drawPolygon(poly);
    painter.setPen(QPen(Qt::black, 1.3, Qt::DashLine));
    painter.drawPolygon(poly);
}

// ---------------------------------------------------------------------------
// 交互
// ---------------------------------------------------------------------------

void StoryboardView::selectShot(int shotIndex)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }
    const StoryboardShot& shot = mShots[shotIndex];
    Layer* top = mEditor->object()->findLayerById(shot.topLayerId);
    if (top != nullptr)
    {
        mEditor->layers()->setCurrentLayer(mEditor->object()->getIndex(top));
    }
    if (shot.lastFrame >= shot.firstFrame)
    {
        mEditor->scrubTo(shot.firstFrame);
    }
}

void StoryboardView::toggleShotVisible(int shotIndex)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }
    const StoryboardShot& shot = mShots[shotIndex];
    if (shot.groupId >= 0)
    {
        mEditor->layers()->setGroupVisible(shot.groupId, !shot.visible);
    }
    else
    {
        Layer* anchor = mEditor->object()->findLayerById(shot.anchorLayerId);
        if (anchor != nullptr)
        {
            mEditor->switchVisibilityOfLayer(mEditor->object()->getIndex(anchor));
        }
    }
    viewport()->update();
}

void StoryboardView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    const QPoint contentPos = event->pos() + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const int index = shotIndexAt(contentPos);
    mDragArmed = false;
    mDragSourceShot = -1;
    if (index < 0) { return; }

    QRect eyeRect;
    eyeRectFor(index, &eyeRect);
    if (eyeRect.contains(contentPos))
    {
        toggleShotVisible(index);
        return;
    }

    // 点击即选中：选层（镜头顶层成员）+ 跳到镜头代表帧
    selectShot(index);

    if (!mOrderLocked && mShots.size() > 1)
    {
        mDragArmed = true;
        mDragSourceShot = index;
        mDragPressPos = event->pos();
    }
}

void StoryboardView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint contentPos = event->pos() + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());

    if (mDragging)
    {
        mDragPos = event->pos();
        // 并组判定：目标卡中心区（各50%）；否则按插入槽位
        const int index = shotIndexAt(contentPos);
        int merge = -1;
        if (index >= 0 && index != mDragSourceShot)
        {
            const QRect card = cardRect(index);
            const bool inCenterX = qAbs(contentPos.x() - card.center().x()) < card.width() / 4;
            const bool inCenterY = qAbs(contentPos.y() - card.center().y()) < card.height() / 4;
            if (inCenterX && inCenterY) { merge = index; }
        }
        mDragMergeShot = merge;
        mDragGap = (merge >= 0) ? -1 : gapIndexAt(contentPos);
        viewport()->update();
        return;
    }

    if (mDragArmed && (event->pos() - mDragPressPos).manhattanLength() > SB_DRAG_THRESHOLD)
    {
        mDragArmed = false;
        mDragging = true;
        mDragPos = event->pos();
        mDragGap = gapIndexAt(contentPos);
        mDragMergeShot = -1;
        viewport()->update();
        return;
    }

    // 悬浮高亮（眼睛热区 + 卡底）
    const int index = shotIndexAt(contentPos);
    bool hoverEye = false;
    if (index >= 0)
    {
        QRect eyeRect;
        eyeRectFor(index, &eyeRect);
        hoverEye = eyeRect.contains(contentPos);
    }
    if (index != mHoverShot || hoverEye != mHoverEye)
    {
        mHoverShot = index;
        mHoverEye = hoverEye;
        viewport()->update();
    }
}

void StoryboardView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }
    if (mDragging)
    {
        commitDrag();
    }
    mDragArmed = false;
    mDragging = false;
    mDragSourceShot = -1;
    mDragGap = -1;
    mDragMergeShot = -1;
    viewport()->update();
}

void StoryboardView::commitDrag()
{
    if (mEditor == nullptr || mDragSourceShot < 0 || mDragSourceShot >= mShots.size()) { return; }
    const StoryboardShot& source = mShots[mDragSourceShot];

    if (mDragMergeShot >= 0 && mDragMergeShot < mShots.size() && mDragMergeShot != mDragSourceShot)
    {
        // 并组：拖到目标卡中心 = 并入目标镜头（散层目标自动建组）
        const StoryboardShot& target = mShots[mDragMergeShot];
        mEditor->layers()->mergeShots(source.firstLayerIndex, source.layerCount, target.firstLayerIndex);
        return;
    }

    if (mDragGap < 0) { return; }
    const int n = mShots.size();
    const int gap = qBound(0, mDragGap, n); // 显示序插入位（0=最上方，n=最下方）
    int toStack;
    if (gap >= n)
    {
        // 显示序最下方 = 栈底（槽位 0）
        toStack = 0;
    }
    else
    {
        // 落到显示序第 gap 位 = 栈序插到该镜头块正上方（其槽位+块长）
        const StoryboardShot& below = mShots[gap];
        toStack = below.firstLayerIndex + below.layerCount;
    }
    mEditor->layers()->reorderLayerRange(source.firstLayerIndex, source.layerCount, toStack);
}

void StoryboardView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) { return; }
    const QPoint contentPos = event->pos() + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const int index = shotIndexAt(contentPos);
    if (index >= 0)
    {
        showEditDialog(index);
    }
}

void StoryboardView::playFromShot(int shotIndex)
{
    if (mEditor == nullptr) { return; }
    const StoryboardShot& shot = mShots[shotIndex];
    if (shot.lastFrame >= shot.firstFrame)
    {
        mEditor->scrubTo(shot.firstFrame);
    }
    mEditor->playback()->play();
}

void StoryboardView::applyShotColor(int shotIndex, int colorIndex)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }
    const StoryboardShot& shot = mShots[shotIndex];
    for (int m = shot.firstLayerIndex; m < shot.firstLayerIndex + shot.layerCount; ++m)
    {
        Layer* layer = mEditor->object()->getLayer(m);
        if (layer != nullptr)
        {
            layer->setColorIndex(colorIndex);
        }
    }
    mEditor->object()->modification();
    emit mEditor->updateTimeLine();
}

void StoryboardView::showRenameDialog(int shotIndex)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }
    const StoryboardShot& shot = mShots[shotIndex];
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("重命名镜头"), tr("镜头名称"),
                                               QLineEdit::Normal, shot.name, &ok);
    if (!ok || name.isEmpty() || name == shot.name) { return; }

    if (shot.groupId >= 0)
    {
        mEditor->layers()->renameGroup(shot.groupId, name); // 组操作自带单步撤销
    }
    else
    {
        Layer* anchor = mEditor->object()->findLayerById(shot.anchorLayerId);
        if (anchor != nullptr)
        {
            mEditor->layers()->renameLayer(anchor, name);
            mEditor->object()->modification();
            emit mEditor->updateTimeLine();
        }
    }
}

void StoryboardView::showEditDialog(int shotIndex)
{
    if (mEditor == nullptr || mEditor->object() == nullptr) { return; }
    const StoryboardShot& shot = mShots[shotIndex];
    Layer* anchor = mEditor->object()->findLayerById(shot.anchorLayerId);
    if (anchor == nullptr) { return; }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("编辑镜头 — %1").arg(shot.name));
    dialog.setMinimumWidth(360);

    QFormLayout* form = new QFormLayout(&dialog);
    QLineEdit* nameEdit = new QLineEdit(shot.name, &dialog);
    QLineEdit* actionEdit = new QLineEdit(anchor->storyboardAction(), &dialog);
    actionEdit->setPlaceholderText(tr("画面动作描述"));
    QLineEdit* dialogEdit = new QLineEdit(anchor->storyboardDialog(), &dialog);
    dialogEdit->setPlaceholderText(tr("角色对白"));
    QPlainTextEdit* notesEdit = new QPlainTextEdit(anchor->storyboardNotes(), &dialog);
    notesEdit->setPlaceholderText(tr("备注（镜头衔接/音效/提示等）"));
    notesEdit->setFixedHeight(72);
    form->addRow(tr("镜头名称："), nameEdit);
    form->addRow(tr("动作："), actionEdit);
    form->addRow(tr("对白："), dialogEdit);
    form->addRow(tr("备注："), notesEdit);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted) { return; }

    const QString newName = nameEdit->text().trimmed();
    if (!newName.isEmpty() && newName != shot.name)
    {
        if (shot.groupId >= 0)
        {
            mEditor->layers()->renameGroup(shot.groupId, newName);
        }
        else
        {
            mEditor->layers()->renameLayer(anchor, newName);
        }
    }
    anchor->setStoryboardAction(actionEdit->text().trimmed());
    anchor->setStoryboardDialog(dialogEdit->text().trimmed());
    anchor->setStoryboardNotes(notesEdit->toPlainText().trimmed());
    mEditor->object()->modification();
    emit mEditor->updateTimeLine();
}

void StoryboardView::contextMenuEvent(QContextMenuEvent* event)
{
    const QPoint contentPos = event->pos() + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const int index = shotIndexAt(contentPos);
    if (index < 0) { return; }
    const StoryboardShot& shot = mShots[index];

    QMenu menu(this);
    QAction* playAction = menu.addAction(tr("从此镜头播放"));
    menu.addSeparator();
    QAction* editAction = menu.addAction(tr("编辑注释…"));
    QAction* renameAction = menu.addAction(tr("重命名…"));
    menu.addSeparator();
    QAction* visibleAction = menu.addAction(shot.visible ? tr("隐藏镜头") : tr("显示镜头"));
    QMenu* colorMenu = menu.addMenu(tr("场次颜色"));
    QAction* noColor = colorMenu->addAction(tr("无颜色"));
    noColor->setCheckable(true);
    noColor->setChecked(shot.colorIndex < 0);
    QAction* colorActions[8] = {};
    for (int c = 0; c < 8; ++c)
    {
        colorActions[c] = colorMenu->addAction(tr("颜色 %1").arg(c + 1));
        colorActions[c]->setCheckable(true);
        colorActions[c]->setChecked(shot.colorIndex == c);
        QPixmap swatch(14, 14);
        swatch.fill(sbLabelColor(c));
        colorActions[c]->setIcon(QIcon(swatch));
    }
    menu.addSeparator();
    QAction* dissolveAction = nullptr;
    if (shot.groupId >= 0 && shot.layerCount > 1)
    {
        dissolveAction = menu.addAction(tr("解散为独立镜头"));
    }

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == nullptr) { return; }
    if (chosen == playAction) { playFromShot(index); }
    else if (chosen == editAction) { showEditDialog(index); }
    else if (chosen == renameAction) { showRenameDialog(index); }
    else if (chosen == visibleAction) { toggleShotVisible(index); }
    else if (chosen == noColor) { applyShotColor(index, -1); }
    else if (chosen == dissolveAction) { mEditor->layers()->dissolveGroup(shot.groupId); }
    else
    {
        for (int c = 0; c < 8; ++c)
        {
            if (chosen == colorActions[c]) { applyShotColor(index, c); return; }
        }
    }
}

void StoryboardView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void StoryboardView::leaveEvent(QEvent* event)
{
    QAbstractScrollArea::leaveEvent(event);
    mHoverShot = -1;
    mHoverEye = false;
    viewport()->update();
}

// ---------------------------------------------------------------------------
// StoryboardPanel
// ---------------------------------------------------------------------------

StoryboardPanel::StoryboardPanel(QWidget* parent) : BaseDockWidget(parent)
{
}

void StoryboardPanel::initUI()
{
    setWindowTitle(tr("分镜"));
    setTitle(tr("分镜"));

    QWidget* root = new QWidget(this);
    QVBoxLayout* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(4, 4, 4, 4);
    rootLay->setSpacing(4);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);

    mLockButton = new QPushButton(tr("锁定顺序"), root);
    mLockButton->setCheckable(true);
    mLockButton->setFixedHeight(24);
    mLockButton->setToolTip(tr("锁定后禁止拖动重排镜头（仍可编辑内容）"));
    toolbar->addWidget(mLockButton);

    mCameraButton = new QPushButton(tr("相机框"), root);
    mCameraButton->setCheckable(true);
    mCameraButton->setFixedHeight(24);
    mCameraButton->setToolTip(tr("在缩略图上叠加相机取景框（需相机图层）"));
    toolbar->addWidget(mCameraButton);

    mTimeModeButton = new QPushButton(QString(), root);
    mTimeModeButton->setFixedHeight(24);
    mTimeModeButton->setToolTip(tr("时长按帧/秒显示切换"));
    toolbar->addWidget(mTimeModeButton);

    mSizeSlider = new QSlider(Qt::Horizontal, root);
    mSizeSlider->setRange(80, 240);
    mSizeSlider->setFixedWidth(120);
    mSizeSlider->setToolTip(tr("调整镜头卡尺寸"));
    toolbar->addWidget(mSizeSlider);

    toolbar->addStretch();
    mStatusLabel = new QLabel(root);
    mStatusLabel->setStyleSheet(QStringLiteral("color:#8A8A94;"));
    toolbar->addWidget(mStatusLabel);

    rootLay->addLayout(toolbar);

    mView = new StoryboardView(root);
    rootLay->addWidget(mView, 1);
    setWidget(root);

    Editor* e = editor();
    Q_ASSERT(e != nullptr);
    mView->setEditor(e);

    // 偏好恢复
    QSettings settings(PENCIL2D, PENCIL2D);
    mSizeSlider->setValue(settings.value(QStringLiteral("Storyboard/CardWidth"), 160).toInt());
    mCameraButton->setChecked(settings.value(QStringLiteral("Storyboard/ShowCamera"), false).toBool());
    const bool showFrames = settings.value(QStringLiteral("Storyboard/ShowFrames"), true).toBool();
    mLockButton->setChecked(settings.value(QStringLiteral("Storyboard/LockOrder"), false).toBool());

    auto syncButtons = [this]
    {
        mTimeModeButton->setText(mView->showFrames() ? tr("按帧") : tr("按秒"));
    };
    // 初始应用
    mView->setCardWidth(mSizeSlider->value());
    mView->setShowCamera(mCameraButton->isChecked());
    mView->setShowFrames(showFrames);
    mView->setOrderLocked(mLockButton->isChecked());
    syncButtons();

    connect(mLockButton, &QPushButton::toggled, this, [this](bool on)
    {
        mView->setOrderLocked(on);
        QSettings s(PENCIL2D, PENCIL2D);
        s.setValue(QStringLiteral("Storyboard/LockOrder"), on);
    });
    connect(mCameraButton, &QPushButton::toggled, this, [this](bool on)
    {
        mView->setShowCamera(on);
        QSettings s(PENCIL2D, PENCIL2D);
        s.setValue(QStringLiteral("Storyboard/ShowCamera"), on);
    });
    connect(mTimeModeButton, &QPushButton::clicked, this, [this, syncButtons]
    {
        const bool frames = !mView->showFrames();
        mView->setShowFrames(frames);
        syncButtons();
        QSettings s(PENCIL2D, PENCIL2D);
        s.setValue(QStringLiteral("Storyboard/ShowFrames"), frames);
    });
    connect(mSizeSlider, &QSlider::valueChanged, this, [this](int value)
    {
        mView->setCardWidth(value);
        QSettings s(PENCIL2D, PENCIL2D);
        s.setValue(QStringLiteral("Storyboard/CardWidth"), value);
    });

    // 与编辑器同步（同 ExposureSheetPanel 接线集）
    connect(e, &Editor::scrubbed, this, [this](int frame) { mView->updateCurrentFrame(frame); });
    connect(e, &Editor::framesModified, this, [this] { mView->clearThumbCache(); mView->rebuildShots(); updateStatus(); });
    connect(e, &Editor::updateTimeLine, this, [this] { mView->rebuildShots(); updateStatus(); });
    connect(e, &Editor::updateLayerCount, this, [this] { mView->rebuildShots(); updateStatus(); });
    connect(e, &Editor::fpsChanged, this, [this](int fps)
    {
        mView->setFps(fps);
        mView->rebuildShots();
        updateStatus();
    });
    connect(e, &Editor::objectLoaded, this, [this, e]
    {
        mView->clearThumbCache();
        mView->rebuildShots();
        mView->updateCurrentFrame(e->currentFrame());
        updateStatus();
    });
    connect(e->layers(), &LayerManager::currentLayerChanged, this, [this](int) { mView->refreshHighlight(); });
    connect(e->layers(), &LayerManager::layerDeleted, this, [this](int) { mView->rebuildShots(); updateStatus(); });
    connect(e->layers(), &LayerManager::layerCountChanged, this, [this](int) { mView->rebuildShots(); updateStatus(); });

    mView->setFps(e->playback()->fps());
    mView->rebuildShots();
    mView->updateCurrentFrame(e->currentFrame());
    updateStatus();
}

void StoryboardPanel::updateUI()
{
    if (mView == nullptr) { return; }
    mView->rebuildShots();
    updateStatus();
}

void StoryboardPanel::updateStatus()
{
    if (mStatusLabel == nullptr || mView == nullptr) { return; }
    const int shots = mView->shotCount();
    const int frames = mView->totalFrames();
    if (mView->fps() <= 0)
    {
        mStatusLabel->setText(tr("%1 镜头 · %2 帧").arg(shots).arg(frames));
    }
    else
    {
        const qreal seconds = frames / static_cast<qreal>(mView->fps());
        mStatusLabel->setText(tr("%1 镜头 · %2 帧 · %3 秒")
                                  .arg(shots).arg(frames)
                                  .arg(QString::number(seconds, 'f', 1)));
    }
}
