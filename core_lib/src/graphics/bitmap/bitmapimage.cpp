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

*/
#include "bitmapimage.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainterPath>
#include <algorithm>
#include "util.h"

#include "blitrect.h"
#include "tile.h"
#include "tiledbuffer.h"

BitmapImage::BitmapImage()
{
    registerOwner();
}

BitmapImage::BitmapImage(const BitmapImage& a) : KeyFrame(a)
{
    mEnableAutoCrop = a.mEnableAutoCrop;
    mOpacity = a.mOpacity;
    d = std::make_shared<SharedData>(*a.d); // 深拷贝共享块（QImage 内部 COW 与旧行为一致）
    registerOwner();
}

BitmapImage::BitmapImage(const QRect& rectangle, const QColor& color)
{
    d->bounds = rectangle;
    d->image = QImage(d->bounds.size(), QImage::Format_ARGB32_Premultiplied);
    d->image.fill(color.rgba());
    d->minBound = false;
    registerOwner();
}

BitmapImage::BitmapImage(const QPoint& topLeft, const QImage& image)
{
    d->bounds = QRect(topLeft, image.size());
    d->minBound = true;
    d->image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    registerOwner();
}

BitmapImage::BitmapImage(const QPoint& topLeft, const QString& path)
{
    setFileName(path);
    d->image = QImage();

    d->bounds = QRect(topLeft, QSize(-1, 0));
    d->minBound = true;
    setModified(false);
    registerOwner();
}

BitmapImage::~BitmapImage()
{
    unregisterOwner();
}

void BitmapImage::registerOwner()
{
    d->owners.push_back(this);
}

void BitmapImage::unregisterOwner()
{
    auto& owners = d->owners;
    owners.erase(std::remove(owners.begin(), owners.end(), this), owners.end());
}

void BitmapImage::rebindSharedData(std::shared_ptr<SharedData> newData)
{
    unregisterOwner();
    d = std::move(newData);
    registerOwner();
}

BitmapImage* BitmapImage::createInstance()
{
    BitmapImage* instance = new BitmapImage();
    instance->mEnableAutoCrop = mEnableAutoCrop;
    instance->mOpacity = mOpacity;
    instance->setPos(pos());
    instance->rebindSharedData(d);
    return instance;
}

void BitmapImage::breakInstance()
{
    if (!isInstanceShared()) { return; }
    rebindSharedData(std::make_shared<SharedData>(*d));
    modification();
}

void BitmapImage::shareDataFrom(BitmapImage* source)
{
    Q_ASSERT(source != nullptr);
    if (source == this || d == source->d) { return; }
    rebindSharedData(source->d);
}

void BitmapImage::modification()
{
    setModified(true);
}

void BitmapImage::setModified(bool b)
{
    if (!b)
    {
        // 存盘成功后逐帧清脏（saveKeyFrameFile 各自调），不广播：
        // 广播 false 会把尚未落盘的成员也标成已存，编辑丢失
        KeyFrame::setModified(false);
        return;
    }
    // 置脏广播全组（见头文件说明）；显式限定基类实现防递归
    for (BitmapImage* owner : d->owners)
    {
        owner->KeyFrame::setModified(true);
    }
}

void BitmapImage::setImage(QImage* img)
{
    Q_ASSERT(img && img->format() == QImage::Format_ARGB32_Premultiplied);
    d->image = *img;
    d->minBound = false;

    modification();
}

BitmapImage& BitmapImage::operator=(const BitmapImage& a)
{
    if (this == &a)
    {
        return *this; // a self-assignment
    }

    KeyFrame::operator=(a);
    // 写穿共享块而非重绑：实例帧经 replaceKeyFrame()（撤销快照回放）走这里，
    // 写穿让全组一起还原；普通帧 d 独占，写穿与整体替换等价
    *d = *a.d;
    mOpacity = a.mOpacity;
    modification();
    return *this;
}

BitmapImage* BitmapImage::clone() const
{
    BitmapImage* b = new BitmapImage(*this);
    b->setFileName(""); // don't link to the file of the source bitmap image

    const bool validKeyFrame = !fileName().isEmpty();
    if (validKeyFrame && !isModified())
    {
        // This bitmapImage is temporarily unloaded.
        // since it's not in the memory, we need to copy the linked png file to prevent data loss.
        QFileInfo finfo(fileName());
        Q_ASSERT(finfo.isAbsolute());
        Q_ASSERT(QFile::exists(fileName()));

        QString newFilePath;
        do
        {
            newFilePath = QString("%1/temp-%2.%3")
                .arg(finfo.canonicalPath())
                .arg(uniqueString(12))
                .arg(finfo.suffix());
        }
        while (QFile::exists(newFilePath));

        b->setFileName(newFilePath);
        bool ok = QFile::copy(fileName(), newFilePath);
        Q_ASSERT(ok);
        qDebug() << "COPY>" << fileName();
    }
    return b;
}

void BitmapImage::loadFile()
{
    if (!fileName().isEmpty() && !isLoaded())
    {
        d->image = QImage(fileName()).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        d->bounds.setSize(d->image.size());
        d->minBound = false;
    }
}

void BitmapImage::unloadFile()
{
    if (isModified() == false && !fileName().isEmpty())
    {
        d->image = QImage();
    }
}

bool BitmapImage::isLoaded() const
{
    if (d->image.isNull()) { return false; }

    return d->image.width() == d->bounds.width();
}

quint64 BitmapImage::memoryUsage()
{
    if (!d->image.isNull())
    {
        return imageSize(d->image);
    }
    return 0;
}

void BitmapImage::paintImage(QPainter& painter)
{
    painter.drawImage(d->bounds.topLeft(), *image());
}

void BitmapImage::paintImage(QPainter& painter, QImage& image, QRect sourceRect, QRect destRect)
{
    painter.drawImage(QRect(d->bounds.topLeft(), destRect.size()),
                      image,
                      sourceRect);
}

QImage* BitmapImage::image()
{
    loadFile();
    return &d->image;
}

BitmapImage BitmapImage::copy()
{
    return BitmapImage(d->bounds.topLeft(), *image());
}

BitmapImage BitmapImage::copy(QRect rectangle)
{
    if (rectangle.isEmpty() || d->bounds.isEmpty()) return BitmapImage();

    QRect intersection2 = rectangle.translated(-d->bounds.topLeft());

    BitmapImage result(rectangle.topLeft(), image()->copy(intersection2));
    return result;
}

BitmapImage BitmapImage::copy(QPolygonF polygon)
{
    if (polygon.size() < 3 || d->bounds.isEmpty()) return BitmapImage();

    QRect bounding = polygon.boundingRect().toAlignedRect();
    if (bounding.isEmpty()) return BitmapImage();

    BitmapImage rectCopy = copy(bounding);
    if (rectCopy.width() <= 0 || rectCopy.height() <= 0) return BitmapImage();

    // Mask the copied region to the polygon shape (anti-aliased edge,
    // matching Krita's default anti-aliased outline selections)
    QPainter painter(rectCopy.image());
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath maskPath;
    maskPath.addPolygon(polygon.translated(-QPointF(bounding.topLeft())));
    maskPath.setFillRule(Qt::OddEvenFill);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.fillPath(maskPath, QColor(0, 0, 0, 255));
    painter.end();

    return rectCopy;
}

void BitmapImage::paste(BitmapImage* bitmapImage, QPainter::CompositionMode cm)
{
    if(bitmapImage->width() <= 0 || bitmapImage->height() <= 0)
    {
        return;
    }

    setCompositionModeBounds(bitmapImage, cm);

    QImage* image2 = bitmapImage->image();

    QPainter painter(image());
    painter.setCompositionMode(cm);
    painter.drawImage(bitmapImage->d->bounds.topLeft() - d->bounds.topLeft(), *image2);
    painter.end();

    modification();
}

void BitmapImage::paste(const TiledBuffer* tiledBuffer, QPainter::CompositionMode cm, const QPainterPath* selectionClip)
{
    if(tiledBuffer->bounds().width() <= 0 || tiledBuffer->bounds().height() <= 0)
    {
        return;
    }
    extend(tiledBuffer->bounds());

    QPainter painter(image());

    painter.setCompositionMode(cm);
    auto const tiles = tiledBuffer->tiles();
    if (selectionClip != nullptr && !selectionClip->isEmpty()) {
        // constrain the stroke to the active selection (lasso/rect)
        painter.translate(-d->bounds.topLeft());
        painter.setClipPath(*selectionClip);
        for (const Tile* item : tiles) {
            painter.drawPixmap(item->pos(), item->pixmap());
        }
    } else {
        for (const Tile* item : tiles) {
            const QPixmap& tilePixmap = item->pixmap();
            const QPoint& tilePos = item->pos();
            painter.drawPixmap(tilePos-d->bounds.topLeft(), tilePixmap);
        }
    }
    painter.end();

    modification();
}

void BitmapImage::moveTopLeft(QPoint point)
{
    d->bounds.moveTopLeft(point);
    // Size is unchanged so there is no need to update d->bounds
    modification();
}

void BitmapImage::transform(QRect newBoundaries, bool smoothTransform)
{
    d->bounds = newBoundaries;
    newBoundaries.moveTopLeft(QPoint(0, 0));
    QImage newImage(d->bounds.size(), QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&newImage);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothTransform);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(newImage.rect(), QColor(0, 0, 0, 0));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(newBoundaries, *image());
    painter.end();
    d->image = newImage;

    modification();
}

BitmapImage BitmapImage::transformed(QRect selection, QTransform transform, bool smoothTransform)
{
    Q_ASSERT(!selection.isEmpty());

    BitmapImage selectedPart = copy(selection);

    // Get the transformed image
    QImage transformedImage;
    if (smoothTransform)
    {
        transformedImage = selectedPart.image()->transformed(transform, Qt::SmoothTransformation);
    }
    else
    {
        transformedImage = selectedPart.image()->transformed(transform);
    }
    return BitmapImage(transform.mapRect(selection).normalized().topLeft(), transformedImage);
}

BitmapImage BitmapImage::transformed(QPolygonF selection, QTransform transform, bool smoothTransform)
{
    if (selection.size() < 3) { return BitmapImage(); }

    BitmapImage selectedPart = copy(selection);
    if (selectedPart.width() <= 0 || selectedPart.height() <= 0) { return BitmapImage(); }

    QImage transformedImage;
    if (smoothTransform)
    {
        transformedImage = selectedPart.image()->transformed(transform, Qt::SmoothTransformation);
    }
    else
    {
        transformedImage = selectedPart.image()->transformed(transform);
    }
    return BitmapImage(transform.mapRect(selection.boundingRect()).normalized().topLeft().toPoint(), transformedImage);
}

BitmapImage BitmapImage::transformed(QRect newBoundaries, bool smoothTransform)
{
    BitmapImage transformedImage(newBoundaries, QColor(0, 0, 0, 0));
    QPainter painter(transformedImage.image());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothTransform);
    newBoundaries.moveTopLeft(QPoint(0, 0));
    painter.drawImage(newBoundaries, *image());
    painter.end();
    return transformedImage;
}

/** Update image bounds.
 *
 *  @param[in] newBoundaries the new bounds
 *
 *  Sets this image's bounds to rectangle.
 *  Modifies d->bounds and crops d->image.
 */
void BitmapImage::updateBounds(QRect newBoundaries)
{
    // Check to make sure changes actually need to be made
    if (d->bounds == newBoundaries) return;

    QImage newImage(newBoundaries.size(), QImage::Format_ARGB32_Premultiplied);
    newImage.fill(Qt::transparent);
    if (!newImage.isNull())
    {
        QPainter painter(&newImage);
        painter.drawImage(d->bounds.topLeft() - newBoundaries.topLeft(), d->image);
        painter.end();
    }
    d->image = newImage;
    d->bounds = newBoundaries;
    d->minBound = false;

    modification();
}

void BitmapImage::extend(const QPoint &p)
{
    if (!d->bounds.contains(p))
    {
        extend(QRect(p, QSize(1, 1)));
    }
}

void BitmapImage::extend(QRect rectangle)
{
    if (rectangle.width() <= 0) rectangle.setWidth(1);
    if (rectangle.height() <= 0) rectangle.setHeight(1);
    if (d->bounds.contains(rectangle))
    {
        // Do nothing
    }
    else
    {
        QRect newBoundaries = d->bounds.united(rectangle).normalized();
        QImage newImage(newBoundaries.size(), QImage::Format_ARGB32_Premultiplied);
        newImage.fill(Qt::transparent);
        if (!newImage.isNull())
        {
            QPainter painter(&newImage);
            painter.drawImage(d->bounds.topLeft() - newBoundaries.topLeft(), *image());
            painter.end();
        }
        d->image = newImage;
        d->bounds = newBoundaries;

        modification();
    }
}

/** Updates the bounds after a drawImage operation with the composition mode cm.
 *
 *  @param[in] source The source image used for the drawImage call.
 *  @param[in] cm The composition mode that will be used for the draw image
 *
 *  @see BitmapImage::setCompositionModeBounds(BitmapImage, QPainter::CompositionMode)
 */
void BitmapImage::setCompositionModeBounds(BitmapImage *source, QPainter::CompositionMode cm)
{
    if (source)
    {
        setCompositionModeBounds(source->d->bounds, source->d->minBound, cm);
    }
}

/** Updates the bounds after a draw operation with the composition mode cm.
 *
 * @param[in] sourceBounds The bounds of the source used for drawcall.
 * @param[in] isSourceMinBounds Is sourceBounds the minimal bounds for the source image
 * @param[in] cm The composition mode that will be used for the draw image
 *
 * For a call to draw image of a QPainter (initialized with d->image) with an argument
 * of source, this function intelligently calculates the bounds. It will attempt to
 * preserve minimum bounds based on the composition mode.
 *
 * This works baed on the principle that some minimal bounds can be determined
 * solely by the minimal bounds of this and source, depending on the value of cm.
 * Some composition modes only expand, or have no affect on the bounds.
 *
 * @warning The draw operation described by the arguments of this
 *          function needs to be called after this function is run,
 *          or the bounds will be out of sync. If d->bounds is null,
 *          no draw operation needs to be performed.
 */
void BitmapImage::setCompositionModeBounds(QRect sourceBounds, bool isSourceMinBounds, QPainter::CompositionMode cm)
{
    QRect newBoundaries;
    switch(cm)
    {
    case QPainter::CompositionMode_Destination:
    case QPainter::CompositionMode_SourceAtop:
        // The Destination and SourceAtop modes
        // do not change the bounds from destination.
        newBoundaries = d->bounds;
        // d->minBound remains the same
        break;
    case QPainter::CompositionMode_SourceIn:
    case QPainter::CompositionMode_DestinationIn:
    case QPainter::CompositionMode_Clear:
    case QPainter::CompositionMode_DestinationOut:
        // The bounds of the result of SourceIn, DestinationIn, Clear, and DestinationOut
        // modes are no larger than the destination bounds
        newBoundaries = d->bounds;
        d->minBound = false;
        break;
    default:
        // If it's not one of the above cases, create a union of the two bounds.
        // This contains the minimum bounds, if both the destination and source
        // use their respective minimum bounds.
        newBoundaries = d->bounds.united(sourceBounds);
        d->minBound = d->minBound && isSourceMinBounds;
    }

    updateBounds(newBoundaries);
}

/** Removes any transparent borders by reducing the boundaries.
 *
 *  This function reduces the bounds of an image until the top and
 *  bottom rows, and the left and right columns of pixels each
 *  contain at least one pixel with a non-zero alpha value
 *  (i.e. non-transparent pixel). Both d->bounds and
 *  the size of #d->image are updated.
 *
 *  @pre d->bounds.size() == d->image->size()
 *  @post Either the first and last rows and columns all contain a
 *        pixel with alpha > 0 or d->bounds.isEmpty() == true
 *  @post isMinimallyBounded() == true
 */
void BitmapImage::autoCrop()
{
    if (!mEnableAutoCrop) return;
    if (d->bounds.isEmpty()) return; // Exit if current bounds are null
    if (d->image.isNull()) return;

    Q_ASSERT(d->bounds.size() == d->image.size());

    // Exit if already min bounded
    if (d->minBound) return;

    // Get image properties
    const int width = d->image.width();

    // Relative top and bottom row indices (inclusive)
    int relTop = 0;
    int relBottom = d->bounds.height() - 1;

    // Check top row
    bool isEmpty = true; // Used to track if a non-transparent pixel has been found
    while (isEmpty && relTop <= relBottom) // Loop through rows
    {
        // Point cursor to the first pixel in the current top row
        const QRgb* cursor = reinterpret_cast<const QRgb*>(d->image.constScanLine(relTop));
        for (int col = 0; col < width; col++) // Loop through pixels in row
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if (qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in row relTop,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to the next pixel in the row
            cursor++;
        }
        if (isEmpty)
        {
            // If the row we just checked was empty, increase relTop
            // to remove the empty row from the top of the bounding box
            ++relTop;
        }
    }

    // Check bottom row
    isEmpty = true; // Reset isEmpty
    while (isEmpty && relBottom >= relTop) // Loop through rows
    {
        // Point cursor to the first pixel in the current bottom row
        const QRgb* cursor = reinterpret_cast<const QRgb*>(d->image.constScanLine(relBottom));
        for (int col = 0; col < width; col++) // Loop through pixels in row
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if(qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in row relBottom,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to the next pixel in the row
            ++cursor;
        }
        if (isEmpty)
        {
            // If the row we just checked was empty, decrease relBottom
            // to remove the empty row from the bottom of the bounding box
            --relBottom;
        }
    }

    // Relative left and right column indices (inclusive)
    int relLeft = 0;
    int relRight = d->bounds.width()-1;

    // Check left column - find minimum transparent span at start of each row
    int minLeft = d->bounds.width();
    for (int row = relTop; row <= relBottom; ++row)
    {
        const QRgb* cursor = reinterpret_cast<const QRgb*>(d->image.constScanLine(row));
        for (int col = 0; col < minLeft; ++col)
        {
            if (qAlpha(*cursor) != 0)
            {
                minLeft = col;
                break;
            }
            ++cursor;
        }
    }
    relLeft = minLeft;

    // Check right column - find minimum transparent span at end of each row
    int minRight = 0;
    for (int row = relTop; row <= relBottom; ++row)
    {
        const QRgb* cursor = reinterpret_cast<const QRgb*>(d->image.constScanLine(row)) + d->bounds.width() - 1;
        for (int col = d->bounds.width() - 1; col > minRight; --col)
        {
            if (qAlpha(*cursor) != 0)
            {
                minRight = col;
                break;
            }
            --cursor;
        }
    }
    relRight = minRight;

    if (relTop > relBottom || relLeft > relRight)
    {
        clear();
        return;
    }
    //qDebug() << "Original" << d->bounds;
    //qDebug() << "Autocrop" << relLeft << relTop << relRight - d->bounds.width() + 1 << relBottom - d->bounds.height() + 1;
    // Update d->bounds and d->image if necessary
    updateBounds(d->bounds.adjusted(relLeft, relTop, relRight - d->bounds.width() + 1, relBottom - d->bounds.height() + 1));

    //qDebug() << "New bounds" << d->bounds;

    d->minBound = true;
}

QRgb BitmapImage::pixel(int x, int y)
{
    return pixel(QPoint(x, y));
}

QRgb BitmapImage::pixel(QPoint p)
{
    QRgb result = qRgba(0, 0, 0, 0); // black
    if (d->bounds.contains(p))
        result = image()->pixel(p - d->bounds.topLeft());
    return result;
}

void BitmapImage::setPixel(int x, int y, QRgb color)
{
    setPixel(QPoint(x, y), color);
}

void BitmapImage::setPixel(QPoint p, QRgb color)
{
    setCompositionModeBounds(QRect(p, QSize(1,1)), true, QPainter::CompositionMode_SourceOver);
    if (d->bounds.contains(p))
    {
        image()->setPixel(p - d->bounds.topLeft(), color);
    }
    modification();
}

void BitmapImage::fillNonAlphaPixels(const QRgb color)
{
    if (d->bounds.isEmpty()) { return; }

    BitmapImage fill(bounds(), color);
    paste(&fill, QPainter::CompositionMode_SourceIn);
}

void BitmapImage::drawLine(QPointF P1, QPointF P2, QPen pen, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = 2 + pen.width();
    setCompositionModeBounds(QRect(P1.toPoint(), P2.toPoint()).normalized().adjusted(-width, -width, width, width), true, cm);
    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.drawLine(P1 - d->bounds.topLeft(), P2 - d->bounds.topLeft());
        painter.end();
    }
    modification();
}

void BitmapImage::drawRect(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    setCompositionModeBounds(rectangle.adjusted(-width, -width, width, width).toRect(), true, cm);
    if (brush.style() == Qt::RadialGradientPattern)
    {
        QRadialGradient* gradient = (QRadialGradient*)brush.gradient();
        gradient->setCenter(gradient->center() - d->bounds.topLeft());
        gradient->setFocalPoint(gradient->focalPoint() - d->bounds.topLeft());
    }
    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);

        // Adjust the brush rectangle to be bigger than the bounds itself,
        // otherwise there will be artifacts shown in some cases when smudging
        painter.drawRect(rectangle.translated(-d->bounds.topLeft()).adjusted(-1, -1, 1, 1));
        painter.end();
    }
    modification();
}

void BitmapImage::drawEllipse(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    setCompositionModeBounds(rectangle.adjusted(-width, -width, width, width).toRect(), true, cm);
    if (brush.style() == Qt::RadialGradientPattern)
    {
        QRadialGradient* gradient = (QRadialGradient*)brush.gradient();
        gradient->setCenter(gradient->center() - d->bounds.topLeft());
        gradient->setFocalPoint(gradient->focalPoint() - d->bounds.topLeft());
    }
    if (!image()->isNull())
    {
        QPainter painter(image());

        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setCompositionMode(cm);
        painter.drawEllipse(rectangle.translated(-d->bounds.topLeft()));
        painter.end();
    }
    modification();
}

void BitmapImage::drawPath(QPainterPath path, QPen pen, QBrush brush,
                           QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    // qreal inc = 1.0 + width / 20.0;

    setCompositionModeBounds(path.controlPointRect().adjusted(-width, -width, width, width).toRect(), true, cm);

    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setTransform(QTransform().translate(-d->bounds.left(), -d->bounds.top()));
        painter.setWorldMatrixEnabled(true);
        if (path.length() > 0)
        {
            /*
            for (int pt = 0; pt < path.elementCount() - 1; pt++)
            {
                qreal dx = path.elementAt(pt + 1).x - path.elementAt(pt).x;
                qreal dy = path.elementAt(pt + 1).y - path.elementAt(pt).y;
                qreal m = sqrt(dx*dx + dy*dy);
                qreal factorx = dx / m;
                qreal factory = dy / m;
                for (float h = 0.f; h < m; h += inc)
                {
                    qreal x = path.elementAt(pt).x + factorx * h;
                    qreal y = path.elementAt(pt).y + factory * h;
                    painter.drawPoint(QPointF(x, y));
                }
            }
            */
            painter.drawPath( path );
        }
        else
        {
            // forces drawing when points are coincident (mousedown)
            painter.drawPoint(static_cast<int>(path.elementAt(0).x), static_cast<int>(path.elementAt(0).y));
        }
        painter.end();
    }
    modification();
}

BitmapImage* BitmapImage::scanToTransparent(BitmapImage *img, const int threshold, const bool redEnabled, const bool greenEnabled, const bool blueEnabled)
{
    Q_ASSERT(img != nullptr);

    QRgb rgba = img->constScanLine(img->left(), img->top());
    if (qAlpha(rgba) == 0)
        return img;

    for (int x = img->left(); x <= img->right(); x++)
    {
        for (int y = img->top(); y <= img->bottom(); y++)
        {
            rgba = img->constScanLine(x, y);

            if (qAlpha(rgba) == 0)
                break;

            const int grayValue = qGray(rgba);
            const int redValue = qRed(rgba);
            const int greenValue = qGreen(rgba);
            const int blueValue = qBlue(rgba);
            if (grayValue >= threshold)
            {   // IF Threshold or above
                img->scanLine(x, y, transp);
            }
            else if (redValue > greenValue + COLORDIFF &&
                     redValue > blueValue + COLORDIFF &&
                     redValue > grayValue + GRAYSCALEDIFF)
            {   // IF Red line
                if (redEnabled)
                {
                    img->scanLine(x, y, redline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else if (greenValue > redValue + COLORDIFF &&
                     greenValue > blueValue + COLORDIFF &&
                     greenValue > grayValue + GRAYSCALEDIFF)
            {   // IF Green line
                if (greenEnabled)
                {
                    img->scanLine(x, y, greenline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else if (blueValue > redValue + COLORDIFF &&
                     blueValue > greenValue + COLORDIFF &&
                     blueValue > grayValue + GRAYSCALEDIFF)
            {   // IF Blue line
                if (blueEnabled)
                {
                    img->scanLine(x, y, blueline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else
            {   // okay, so it is in grayscale graduation area
                if (grayValue >= LOW_THRESHOLD)
                {
                    const qreal factor = static_cast<qreal>(threshold - grayValue) / static_cast<qreal>(threshold - LOW_THRESHOLD);
                    img->scanLine(x , y, qRgba(0, 0, 0, static_cast<int>(threshold * factor)));
                }
                else // grayValue < LOW_THRESHOLD
                {
                    img->scanLine(x , y, blackline);
                }
            }
        }
    }
    img->modification();
    return img;
}

Status BitmapImage::writeFile(const QString& filename)
{
    DebugDetails dd;
    dd << "BitmapImage::writeFile";
    dd << QString("&nbsp;&nbsp;filename = ").append(filename);

    QImageWriter writer(filename);
    if (!d->image.isNull())
    {
        bool b = writer.write(d->image);
        if (b) {
            return Status::OK;
        } else {
            dd << QString("&nbsp;&nbsp;Error: %1 (Code %2)").arg(writer.errorString()).arg(static_cast<int>(writer.error()));
            return Status(Status::FAIL, dd);
        }
    }

    if (bounds().isEmpty())
    {
        QFile f(filename);
        if(f.exists())
        {
            bool b = f.remove();
            if (!b) {
                dd << "&nbsp;&nbsp;Error: Image is empty but unable to remove file.";
                return Status::FAIL;
            }
        }

        // The frame is likely empty, act like there's no file name
        // so we don't end up writing to it later.
        setFileName("");
    }
    return Status::SAFE;
}

void BitmapImage::clear()
{
    d->image = QImage(); // null image
    d->bounds = QRect(0, 0, 0, 0);
    d->minBound = true;
    // 清空=内容已变更：断开旧文件，否则渲染层 loadFile() 会把旧图从磁盘加载回来
    // （复活旧像素，且 d->bounds 只重设 size 不重设 topLeft → 内容整体偏移到画布中心）
    setFileName("");
    modification();
}

QRgb BitmapImage::constScanLine(int x, int y) const
{
    QRgb result = QRgb();
    if (d->bounds.contains(x, y)) {
        result = *(reinterpret_cast<const QRgb*>(d->image.constScanLine(y - d->bounds.top())) + x - d->bounds.left());
    }
    return result;
}

void BitmapImage::scanLine(int x, int y, QRgb color)
{
    if (!d->bounds.contains(x, y)) {
        return;
    }
    // Make sure color is premultiplied before calling
    *(reinterpret_cast<QRgb*>(image()->scanLine(y - d->bounds.top())) + x - d->bounds.left()) = color;
}

void BitmapImage::clear(QRect rectangle)
{
    QRect clearRectangle = d->bounds.intersected(rectangle);
    clearRectangle.moveTopLeft(clearRectangle.topLeft() - d->bounds.topLeft());

    setCompositionModeBounds(clearRectangle, true, QPainter::CompositionMode_Clear);

    QPainter painter(image());
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(clearRectangle, QColor(0, 0, 0, 0));
    painter.end();

    modification();
}

void BitmapImage::clear(QPolygonF polygon)
{
    if (polygon.size() < 3 || d->bounds.isEmpty()) { return; }

    QRect bounding = polygon.boundingRect().toAlignedRect().intersected(d->bounds);
    if (bounding.isEmpty()) { return; }

    setCompositionModeBounds(bounding, true, QPainter::CompositionMode_Clear);

    QPainter painter(image());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-d->bounds.topLeft());
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    QPainterPath clearPath;
    clearPath.addPolygon(polygon);
    clearPath.setFillRule(Qt::OddEvenFill);
    painter.fillPath(clearPath, QColor(0, 0, 0, 0));
    painter.end();

    modification();
}

