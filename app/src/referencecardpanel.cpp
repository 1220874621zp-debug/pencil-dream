/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License.
version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "referencecardpanel.h"

#include <algorithm>

#include <QButtonGroup>
#include <QColorDialog>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "colormanager.h"
#include "editor.h"
#include "filedialog.h"
#include "tvptoolsdialog.h"

namespace
{
// 色块与名称条尺寸固定为屏幕像素，不随缩放变化（注释式悬浮层）
const qreal SWATCH_W = 64.0;
const qreal SWATCH_H = 40.0;
const qreal LABEL_H = 20.0;
const qreal LINE_COLOR_RGB[3] = { 229.0, 57.0, 53.0 };   // 标记线红

qreal pointToSegmentDistance(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal len2 = QPointF::dotProduct(ab, ab);
    if (len2 <= 0.0)
    {
        return QLineF(p, a).length();
    }
    const qreal t = qBound(0.0, QPointF::dotProduct(p - a, ab) / len2, 1.0);
    return QLineF(p, a + t * ab).length();
}
}

// --------------------------------------------------------------- 画布 ---+

ReferenceCardCanvas::ReferenceCardCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
}

void ReferenceCardCanvas::setMarkerMode(bool marker)
{
    if (mMarkerMode == marker) { return; }
    mMarkerMode = marker;
    mDraftPoints.clear();
    setFocus();   // Esc/回车需要键盘焦点
    updateCursor(mHoverPos);
    update();
}

void ReferenceCardCanvas::fitToWindow()
{
    if (mImage.isNull()) { return; }
    if (width() < 20 || height() < 20)
    {
        // 面板刚打开画布尺寸未就绪，等布局完成后再适配
        QTimer::singleShot(0, this, &ReferenceCardCanvas::fitToWindow);
        return;
    }
    // 右侧为色块列预留一列宽度，"图+色块列"整体在画布内居中
    const qreal columnW = SWATCH_W + 24.0;
    const qreal availW = qMax(80.0, width() - columnW);
    mScale = qBound(0.02,
                    0.95 * qMin(availW / static_cast<qreal>(mImage.width()),
                                height() / static_cast<qreal>(mImage.height())),
                    16.0);
    mPan = QPointF((width() - mImage.width() * mScale - columnW) / 2.0,
                   (height() - mImage.height() * mScale) / 2.0);
    update();
}

// ------------------------------------------------------------- 持久化 ---+

QString ReferenceCardCanvas::sidecarPathFor(const QString& imagePath)
{
    const QFileInfo info(imagePath);
    return info.absoluteDir().filePath(info.completeBaseName() + ".setcard.json");
}

QString ReferenceCardCanvas::sidecarPath() const
{
    return sidecarPathFor(mImagePath);
}

void ReferenceCardCanvas::saveSidecar()
{
    if (mImagePath.isEmpty()) { return; }

    QJsonObject root;
    root["version"] = 1;
    root["image"] = QDir::toNativeSeparators(mImagePath);

    QJsonArray swatchArray;
    for (const RefColorSwatch& s : std::as_const(mSwatches))
    {
        QJsonObject o;
        o["name"] = s.name;
        o["color"] = s.color.name();
        o["x"] = s.pos.x();
        o["y"] = s.pos.y();
        swatchArray.append(o);
    }
    root["swatches"] = swatchArray;

    QJsonArray lineArray;
    for (const RefMarkerLine& line : std::as_const(mLines))
    {
        QJsonArray points;
        for (const QPointF& p : line.points)
        {
            QJsonArray xy;
            xy.append(p.x());
            xy.append(p.y());
            points.append(xy);
        }
        QJsonObject o;
        o["points"] = points;
        lineArray.append(o);
    }
    root["lines"] = lineArray;

    QJsonObject view;
    view["scale"] = mScale;
    view["panX"] = mPan.x();
    view["panY"] = mPan.y();
    root["view"] = view;

    QFile file(sidecarPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "[设定卡片] 无法写入" << sidecarPath();
        return;
    }
    file.write(QJsonDocument(root).toJson());
}

bool ReferenceCardCanvas::loadSidecarData(const QString& jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) { return false; }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "[设定卡片] 预设解析失败" << jsonPath << parseError.errorString();
        return false;
    }
    const QJsonObject root = doc.object();

    mSwatches.clear();
    mSelected.clear();
    const QJsonArray swatchArray = root["swatches"].toArray();
    for (const QJsonValue& v : swatchArray)
    {
        const QJsonObject o = v.toObject();
        QColor color(o["color"].toString());
        if (!color.isValid()) { color = Qt::white; }
        RefColorSwatch s;
        s.color = color;
        s.name = o["name"].toString();
        s.pos = QPointF(o["x"].toDouble(), o["y"].toDouble());
        mSwatches.append(s);
    }

    mLines.clear();
    const QJsonArray lineArray = root["lines"].toArray();
    for (const QJsonValue& v : lineArray)
    {
        RefMarkerLine line;
        const QJsonArray points = v.toObject()["points"].toArray();
        for (const QJsonValue& pt : points)
        {
            const QJsonArray xy = pt.toArray();
            if (xy.size() >= 2)
            {
                line.points.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
            }
        }
        if (line.points.size() >= 2) { mLines.append(line); }
    }
    // 视图不恢复：载入后按当前窗口重新适配，保证图片大小与色块列排版合理
    return true;
}

bool ReferenceCardCanvas::loadImage(const QString& path, int extractCount)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
    {
        QMessageBox::warning(this, tr("设定卡片"),
                             tr("无法读取图片：%1").arg(reader.errorString()));
        return false;
    }

    mImage = image;
    mImagePath = QFileInfo(path).absoluteFilePath();
    mSwatches.clear();
    mLines.clear();
    mDraftPoints.clear();
    mSelected.clear();

    if (QFile::exists(sidecarPath()) && loadSidecarData(sidecarPath()))
    {
        // 旁路文件恢复色块/标记线，视图按当前窗口重新适配
        fitToWindow();
    }
    else
    {
        extractSwatches(extractCount);   // 内部含 fitToWindow
    }
    emit imageChanged(true);
    update();
    return true;
}

bool ReferenceCardCanvas::importPreset(const QString& jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, tr("导入预设"), tr("无法读取预设文件：%1").arg(jsonPath));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        QMessageBox::warning(this, tr("导入预设"),
                             tr("预设文件格式错误：%1").arg(parseError.errorString()));
        return false;
    }
    const QJsonObject root = doc.object();

    const QString imageField = root["image"].toString();
    if (imageField.isEmpty())
    {
        QMessageBox::warning(this, tr("导入预设"), tr("预设文件缺少图片路径。"));
        return false;
    }

    // 绝对路径优先，其次相对预设文件所在目录（整个文件夹搬走后仍可导入）
    QString resolved = imageField;
    QFileInfo info(resolved);
    if (!(info.isAbsolute() && info.exists()))
    {
        const QString rel = QFileInfo(jsonPath).absoluteDir().filePath(imageField);
        if (QFileInfo(rel).exists())
        {
            resolved = QFileInfo(rel).absoluteFilePath();
        }
    }

    QImageReader reader(resolved);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
    {
        QMessageBox::warning(this, tr("导入预设"),
                             tr("找不到或无法读取设定图：\n%1").arg(resolved));
        return false;
    }

    mImage = image;
    mImagePath = QFileInfo(resolved).absoluteFilePath();
    mDraftPoints.clear();
    mSelected.clear();
    if (!loadSidecarData(jsonPath))
    {
        mSwatches.clear();
        mLines.clear();
    }
    fitToWindow();
    emit imageChanged(true);
    update();
    return true;
}

void ReferenceCardCanvas::extractSwatches(int count)
{
    if (mImage.isNull()) { return; }

    const QList<QRgb> colors = PaletteExtractDialog::extractColors(mImage, count);
    if (colors.isEmpty()) { return; }

    mSwatches.clear();
    mSelected.clear();
    fitToWindow();

    // 初始列贴图片右缘、纵向等分图高：任何缩放下都与图片等比、保持紧凑
    const qreal colX = mImage.width() + 12.0 / mScale;
    const int n = colors.size();
    for (int i = 0; i < n; ++i)
    {
        RefColorSwatch s;
        s.color = QColor::fromRgb(colors[i]);
        s.name = tr("颜色%1").arg(i + 1);
        // 锚点是色块左上角：等分中心减半个块高（块高为屏幕像素，折回图坐标）
        s.pos = QPointF(colX, mImage.height() * (i + 0.5) / n - SWATCH_H / (2.0 * mScale));
        mSwatches.append(s);
    }
    saveSidecar();
    update();
}

// ------------------------------------------------------------- 命中测试 ---+

QRectF ReferenceCardCanvas::swatchRect(int index) const
{
    return QRectF(imageToWidget(mSwatches[index].pos),
                  QSizeF(SWATCH_W, SWATCH_H));
}

QRectF ReferenceCardCanvas::labelRect(int index) const
{
    const QString name = mSwatches[index].name;
    if (name.isEmpty()) { return QRectF(); }

    QFont f = font();
    if (f.pointSizeF() > 0) { f.setPointSizeF(f.pointSizeF() - 1.0); }
    else { f.setPixelSize(12); }
    const qreal textW = QFontMetricsF(f).horizontalAdvance(name) + 12.0;

    const QPointF topLeft = imageToWidget(mSwatches[index].pos);
    return QRectF(topLeft.x() + (SWATCH_W - textW) / 2.0,
                  topLeft.y() - LABEL_H - 4.0,
                  textW, LABEL_H);
}

int ReferenceCardCanvas::hitSwatch(const QPointF& widgetPos) const
{
    for (int i = mSwatches.size() - 1; i >= 0; --i)   // 后画的在上层
    {
        if (swatchRect(i).contains(widgetPos) || labelRect(i).contains(widgetPos))
        {
            return i;
        }
    }
    return -1;
}

int ReferenceCardCanvas::hitLine(const QPointF& widgetPos) const
{
    for (int i = mLines.size() - 1; i >= 0; --i)
    {
        const QVector<QPointF>& pts = mLines[i].points;
        for (int j = 1; j < pts.size(); ++j)
        {
            if (pointToSegmentDistance(widgetPos,
                                       imageToWidget(pts[j - 1]),
                                       imageToWidget(pts[j])) < 6.0)
            {
                return i;
            }
        }
    }
    return -1;
}

// --------------------------------------------------------------- 绘制 ---+

void ReferenceCardCanvas::ensureScaledCache()
{
    const qreal dpr = devicePixelRatioF();
    if (!mScaledCache.isNull() && qFuzzyCompare(mCacheScale, mScale) && qFuzzyCompare(mCacheDpr, dpr))
    {
        return;
    }
    const QSize target(qMax(1, qRound(mImage.width() * mScale * dpr)),
                       qMax(1, qRound(mImage.height() * mScale * dpr)));
    mScaledCache = QPixmap::fromImage(
        mImage.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    mScaledCache.setDevicePixelRatio(dpr);
    mCacheScale = mScale;
    mCacheDpr = dpr;
}

void ReferenceCardCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(38, 38, 42));
    painter.drawRect(rect());

    if (mImage.isNull())
    {
        painter.setPen(QColor(120, 120, 128));
        painter.setBrush(Qt::NoBrush);
        painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("点击工具栏「导入图片」导入设定图\n色块可拖动，单击应用颜色，双击改色"));
        return;
    }

    ensureScaledCache();
    painter.drawPixmap(mPan, mScaledCache);

    // 图幅描边
    painter.setPen(QPen(QColor(70, 70, 76), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(mPan, QSizeF(mImage.width() * mScale,
                                         mImage.height() * mScale)));

    painter.setRenderHint(QPainter::Antialiasing, true);

    // 已确认标记线：红色实线 + 首尾端点
    const QColor lineColor(static_cast<int>(LINE_COLOR_RGB[0]),
                           static_cast<int>(LINE_COLOR_RGB[1]),
                           static_cast<int>(LINE_COLOR_RGB[2]));
    QPen linePen(lineColor, 2);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    for (const RefMarkerLine& line : std::as_const(mLines))
    {
        if (line.points.size() < 2) { continue; }
        QVector<QPointF> mapped;
        mapped.reserve(line.points.size());
        for (const QPointF& p : line.points) { mapped.append(imageToWidget(p)); }
        painter.setPen(linePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(mapped.constData(), mapped.size());
        painter.setPen(Qt::NoPen);
        painter.setBrush(lineColor);
        painter.drawEllipse(mapped.first(), 2.5, 2.5);
        painter.drawEllipse(mapped.last(), 2.5, 2.5);
    }

    // 草稿线：实线段 + 到光标的虚线橡皮筋 + 顶点圆点
    if (!mDraftPoints.isEmpty())
    {
        QVector<QPointF> mapped;
        mapped.reserve(mDraftPoints.size());
        for (const QPointF& p : mDraftPoints) { mapped.append(imageToWidget(p)); }

        painter.setPen(linePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(mapped.constData(), mapped.size());

        if (mHoverPos.x() >= 0.0)
        {
            QPen dashPen(lineColor, 1, Qt::DashLine);
            painter.setPen(dashPen);
            painter.drawLine(mapped.last(), mHoverPos);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(lineColor);
        for (const QPointF& p : std::as_const(mapped))
        {
            painter.drawEllipse(p, 3.0, 3.0);
        }
    }

    // 色块与名称条
    QFont labelFont = font();
    if (labelFont.pointSizeF() > 0) { labelFont.setPointSizeF(labelFont.pointSizeF() - 1.0); }
    else { labelFont.setPixelSize(12); }
    painter.setFont(labelFont);

    for (int i = 0; i < mSwatches.size(); ++i)
    {
        const QRectF r = swatchRect(i);

        painter.setPen(QPen(QColor(20, 20, 24), 1));
        painter.setBrush(mSwatches[i].color);
        painter.drawRect(r);

        if (mSelected.contains(i) || (mDragging && i == mPressIndex))
        {
            painter.setPen(QPen(lineColor, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(r.adjusted(1.0, 1.0, -1.0, -1.0));
        }
        else if (i == mHoverSwatch)
        {
            painter.setPen(QPen(QColor(255, 255, 255, 210), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(r.adjusted(1.0, 1.0, -1.0, -1.0));
        }

        const QRectF lr = labelRect(i);
        if (!lr.isNull())
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 175));
            painter.drawRoundedRect(lr, 3.0, 3.0);
            painter.setPen(Qt::white);
            painter.setBrush(Qt::NoBrush);
            painter.drawText(lr, Qt::AlignCenter, mSwatches[i].name);
        }
    }

    // 框选矩形
    if (mRubberActive)
    {
        painter.setPen(QPen(QColor(120, 170, 255), 1, Qt::DashLine));
        painter.setBrush(QColor(120, 170, 255, 30));
        painter.drawRect(QRectF(mRubberStart, mRubberCur).normalized());
    }
}

// --------------------------------------------------------------- 交互 ---+

void ReferenceCardCanvas::updateCursor(const QPointF& pos)
{
    if (mMarkerMode) { setCursor(Qt::CrossCursor); return; }
    if (mPanning) { setCursor(Qt::ClosedHandCursor); return; }
    if (mDragging) { setCursor(Qt::ClosedHandCursor); return; }
    if (hitSwatch(pos) >= 0) { setCursor(Qt::SizeAllCursor); return; }
    setCursor(Qt::ArrowCursor);
}

void ReferenceCardCanvas::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    const QPointF pos = event->position();

    if (mMarkerMode)
    {
        if (event->button() == Qt::LeftButton)
        {
            mDraftPoints.append(widgetToImage(pos));
            update();
        }
        else if (event->button() == Qt::RightButton)
        {
            confirmDraft();
        }
        return;
    }

    if (event->button() == Qt::MiddleButton)
    {
        mPanning = true;
        mPanPressPos = pos;
        mPanPressPan = mPan;
        updateCursor(pos);
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        const int idx = hitSwatch(pos);
        if (idx >= 0)
        {
            mPressIndex = idx;
            mPressPos = pos;
            mDragging = false;
            mGrabOffset = widgetToImage(pos) - mSwatches[idx].pos;
            if (!mSelected.contains(idx))
            {
                mSelected.clear();
                mSelected.insert(idx);
            }
            // 整组拖动起点快照（含单选）
            mDragOrigins.clear();
            for (int i : std::as_const(mSelected))
            {
                mDragOrigins.append(qMakePair(i, mSwatches[i].pos));
            }
        }
        else
        {
            // 空白处左键＝框选色块
            mRubberActive = true;
            mRubberStart = pos;
            mRubberCur = pos;
        }
        updateCursor(pos);
        return;
    }

    if (event->button() == Qt::RightButton)
    {
        showContextMenu(pos, event->globalPosition().toPoint());
    }
}

void ReferenceCardCanvas::mouseMoveEvent(QMouseEvent* event)
{
    mHoverPos = event->position();

    if (mPressIndex >= 0 && (event->buttons() & Qt::LeftButton))
    {
        if (!mDragging && (mHoverPos - mPressPos).manhattanLength() > 4.0)
        {
            mDragging = true;
        }
        if (mDragging)
        {
            QPointF basePos(0.0, 0.0);
            for (const auto& origin : std::as_const(mDragOrigins))
            {
                if (origin.first == mPressIndex) { basePos = origin.second; }
            }
            const QPointF delta = (widgetToImage(mHoverPos) - mGrabOffset) - basePos;
            for (const auto& origin : std::as_const(mDragOrigins))
            {
                mSwatches[origin.first].pos = origin.second + delta;
            }
        }
    }
    else if (mRubberActive && (event->buttons() & Qt::LeftButton))
    {
        mRubberCur = mHoverPos;
    }
    else if (mPanning && (event->buttons() & Qt::MiddleButton))
    {
        mPan = mPanPressPan + (mHoverPos - mPanPressPos);
    }

    if (!mMarkerMode)
    {
        mHoverSwatch = hitSwatch(mHoverPos);
    }
    updateCursor(mHoverPos);
    update();
}

void ReferenceCardCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && mPressIndex >= 0)
    {
        if (mDragging)
        {
            saveSidecar();
        }
        else if (mEditor != nullptr && mPressIndex < mSwatches.size())
        {
            // 单击（未拖动）＝应用颜色到当前笔
            mEditor->color()->setFrontColor(mSwatches[mPressIndex].color);
        }
        mPressIndex = -1;
        mDragging = false;
        mDragOrigins.clear();
        update();
    }
    else if (event->button() == Qt::LeftButton && mRubberActive)
    {
        mRubberActive = false;
        const QRectF band = QRectF(mRubberStart, mRubberCur).normalized();
        if (band.width() < 3.0 && band.height() < 3.0)
        {
            mSelected.clear();   // 空白单击＝取消选择
        }
        else
        {
            mSelected.clear();
            for (int i = 0; i < mSwatches.size(); ++i)
            {
                const bool hitsBand = swatchRect(i).intersects(band)
                                      || (!labelRect(i).isNull() && labelRect(i).intersects(band));
                if (hitsBand) { mSelected.insert(i); }
            }
        }
        update();
    }
    else if (mPanning && event->button() == Qt::MiddleButton)
    {
        mPanning = false;
        updateCursor(event->position());
        update();
    }
}

void ReferenceCardCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (mMarkerMode || event->button() != Qt::LeftButton) { return; }

    const int idx = hitSwatch(event->position());
    if (idx < 0) { return; }

    const QColor color = QColorDialog::getColor(mSwatches[idx].color, this, tr("修改色块颜色"));
    if (color.isValid())
    {
        mSwatches[idx].color = color;
        saveSidecar();
        update();
    }
}

void ReferenceCardCanvas::wheelEvent(QWheelEvent* event)
{
    const qreal oldScale = mScale;
    const qreal newScale = (event->angleDelta().y() > 0) ? oldScale * 1.25 : oldScale * 0.8;
    const QPointF pos = event->position();
    // 缩放锚定光标：w = img*s + pan → pan' = pos - (pos - pan) * s'/s
    mPan = pos - (pos - mPan) * (newScale / oldScale);
    mScale = qBound(0.02, newScale, 16.0);
    update();
}

void ReferenceCardCanvas::keyPressEvent(QKeyEvent* event)
{
    if (!mMarkerMode)
    {
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
            && !mSelected.isEmpty())
        {
            deleteSelected();
            return;
        }
        if (event->key() == Qt::Key_Escape && !mSelected.isEmpty())
        {
            mSelected.clear();
            update();
            return;
        }
        QWidget::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Escape)
    {
        mDraftPoints.clear();
        update();
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        confirmDraft();
    }
    else if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)
             && !mDraftPoints.isEmpty())
    {
        mDraftPoints.removeLast();
        update();
    }
    else
    {
        QWidget::keyPressEvent(event);
    }
}

void ReferenceCardCanvas::deleteSelected()
{
    QList<int> indices = mSelected.values();
    std::sort(indices.begin(), indices.end(), [](int a, int b) { return a > b; });
    for (int i : indices)
    {
        if (i >= 0 && i < mSwatches.size()) { mSwatches.removeAt(i); }
    }
    mSelected.clear();
    mHoverSwatch = -1;
    saveSidecar();
    update();
}

void ReferenceCardCanvas::confirmDraft()
{
    if (mDraftPoints.size() >= 2)
    {
        RefMarkerLine line;
        line.points = mDraftPoints;
        mLines.append(line);
        saveSidecar();
    }
    mDraftPoints.clear();
    update();
}

void ReferenceCardCanvas::showContextMenu(const QPointF& pos, const QPoint& globalPos)
{
    QMenu menu(this);

    const int swIdx = hitSwatch(pos);
    if (swIdx >= 0)
    {
        QAction* renameAction = menu.addAction(tr("重命名"));
        QAction* colorAction = menu.addAction(tr("修改颜色"));
        menu.addSeparator();
        QAction* deleteAction = menu.addAction(tr("删除色块"));
        QAction* chosen = menu.exec(globalPos);

        if (chosen == renameAction)
        {
            bool ok = false;
            const QString name = QInputDialog::getText(this, tr("重命名"), tr("名称："),
                                                       QLineEdit::Normal,
                                                       mSwatches[swIdx].name, &ok);
            if (ok)
            {
                mSwatches[swIdx].name = name;
                saveSidecar();
                update();
            }
        }
        else if (chosen == colorAction)
        {
            const QColor color = QColorDialog::getColor(mSwatches[swIdx].color, this, tr("修改色块颜色"));
            if (color.isValid())
            {
                mSwatches[swIdx].color = color;
                saveSidecar();
                update();
            }
        }
        else if (chosen == deleteAction)
        {
            mSwatches.removeAt(swIdx);
            mSelected.clear();
            mHoverSwatch = -1;
            saveSidecar();
            update();
        }
        return;
    }

    const int lnIdx = hitLine(pos);
    if (lnIdx >= 0)
    {
        QAction* deleteAction = menu.addAction(tr("删除标记线"));
        if (menu.exec(globalPos) == deleteAction)
        {
            mLines.removeAt(lnIdx);
            saveSidecar();
            update();
        }
        return;
    }

    QAction* importAction = menu.addAction(tr("导入图片"));
    menu.addSeparator();
    QAction* fitAction = menu.addAction(tr("适应窗口"));
    QAction* chosen = menu.exec(globalPos);
    if (chosen == importAction)
    {
        emit requestImport();
    }
    else if (chosen == fitAction)
    {
        fitToWindow();
    }
}

// --------------------------------------------------------------- 面板 ---+

ReferenceCardPanel::ReferenceCardPanel(QWidget* parent)
    : BaseDockWidget(parent)
{
}

void ReferenceCardPanel::initUI()
{
    setWindowTitle(tr("设定卡片"));
    setTitle(tr("设定卡片"));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // 单排工具栏：文件操作 + 工具切换 + 视图
    mImportButton = new QPushButton(tr("导入图片"), central);
    mPresetButton = new QPushButton(tr("导入预设"), central);
    mExtractButton = new QPushButton(tr("提取色卡"), central);
    mExtractButton->setEnabled(false);
    mSelectToolButton = new QPushButton(tr("选择"), central);
    mMarkerToolButton = new QPushButton(tr("标记线"), central);
    mFitButton = new QPushButton(tr("适应窗口"), central);
    mSelectToolButton->setCheckable(true);
    mMarkerToolButton->setCheckable(true);
    mSelectToolButton->setChecked(true);
    auto* toolGroup = new QButtonGroup(this);
    toolGroup->setExclusive(true);
    toolGroup->addButton(mSelectToolButton);
    toolGroup->addButton(mMarkerToolButton);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);
    toolbar->addWidget(mImportButton);
    toolbar->addWidget(mPresetButton);
    toolbar->addWidget(mExtractButton);
    toolbar->addWidget(mSelectToolButton);
    toolbar->addWidget(mMarkerToolButton);
    toolbar->addWidget(mFitButton);

    mCanvas = new ReferenceCardCanvas(central);
    mCanvas->setEditor(editor());

    layout->addLayout(toolbar);
    layout->addWidget(mCanvas, 1);

    setWidget(central);

    connect(mImportButton, &QPushButton::clicked, this, &ReferenceCardPanel::importImage);
    connect(mPresetButton, &QPushButton::clicked, this, &ReferenceCardPanel::importPreset);
    connect(mExtractButton, &QPushButton::clicked, this, &ReferenceCardPanel::reextractSwatches);
    connect(mFitButton, &QPushButton::clicked, mCanvas, &ReferenceCardCanvas::fitToWindow);
    connect(mSelectToolButton, &QPushButton::toggled, this, [this](bool checked)
    {
        if (checked) { mCanvas->setMarkerMode(false); }
    });
    connect(mMarkerToolButton, &QPushButton::toggled, this, [this](bool checked)
    {
        if (checked) { mCanvas->setMarkerMode(true); }
    });
    connect(mCanvas, &ReferenceCardCanvas::requestImport, this, &ReferenceCardPanel::importImage);
    connect(mCanvas, &ReferenceCardCanvas::imageChanged, mExtractButton, &QPushButton::setEnabled);
}

void ReferenceCardPanel::importImage()
{
    const QString path = FileDialog::getOpenFileName(this, FileType::IMAGE, tr("导入设定图"));
    if (path.isEmpty()) { return; }

    int count = mLastCount;
    if (!QFile::exists(ReferenceCardCanvas::sidecarPathFor(path)))
    {
        // 没有旁路数据才会提取，先问数量；取消则放弃导入
        bool ok = false;
        count = QInputDialog::getInt(this, tr("提取色卡"), tr("颜色数量："),
                                     mLastCount, 1, 20, 1, &ok);
        if (!ok) { return; }
        mLastCount = count;
    }
    mCanvas->loadImage(path, count);
}

void ReferenceCardPanel::importPreset()
{
    QSettings settings("Pencil", "Pencil");
    const QString lastDir = settings.value("referenceCardPresetDir").toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("导入预设"), lastDir,
                                                      tr("设定卡片预设 (*.setcard.json)"));
    if (path.isEmpty()) { return; }
    settings.setValue("referenceCardPresetDir", QFileInfo(path).absolutePath());
    mCanvas->importPreset(path);
}

void ReferenceCardPanel::reextractSwatches()
{
    bool ok = false;
    const int count = QInputDialog::getInt(this, tr("提取色卡"), tr("颜色数量："),
                                           mLastCount, 1, 20, 1, &ok);
    if (!ok) { return; }
    mLastCount = count;
    mCanvas->extractSwatches(count);
}
