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
#ifndef BITMAP_IMAGE_H
#define BITMAP_IMAGE_H

#include <QPainter>
#include <QPainterPath>
#include "keyframe.h"
#include <QtMath>
#include <QHash>
#include <memory>

class TiledBuffer;

class BitmapImage : public KeyFrame
{
public:
    const QRgb transp = qRgba(0, 0, 0, 0);
    const QRgb blackline = qRgba(1, 1, 1, 255);
    const QRgb redline = qRgba(254,0,0,255);
    const QRgb greenline = qRgba(0,254,0,255);
    const QRgb blueline = qRgba(0,0,254,255);

    BitmapImage();
    BitmapImage(const BitmapImage&);
    BitmapImage(const QRect &rectangle, const QColor& color);
    BitmapImage(const QPoint& topLeft, const QImage& image);
    BitmapImage(const QPoint& topLeft, const QString& path);

    ~BitmapImage() override;
    BitmapImage& operator=(const BitmapImage& a);

    BitmapImage* clone() const override;

    /** 实例帧（Instance）：新建一个与本帧共用同一共享数据块的壳。
     *  之后任一成员的内容/边界修改全组同步；曝光长度、透明度等帧级属性各自独立。 */
    BitmapImage* createInstance();

    /** 解除实例：深拷贝一份自己的共享块，此后与本组其它成员互不影响（像素不变）。 */
    void breakInstance();

    /** 让本帧改用 source 的共享块（载入归并恢复实例链、解除实例撤销用）。 */
    void shareDataFrom(BitmapImage* source);

    /** 是否与其它帧共用共享块（实例组成员数 >= 2）。 */
    bool isInstanceShared() const { return d->owners.size() > 1; }

    bool sharesDataWith(const BitmapImage* other) const { return d == other->d; }

    /** 同组全部成员（含自身）。返回拷贝，调用方持有期间组结构不变即可信。 */
    std::vector<BitmapImage*> instanceMembers() const { return d->owners; }

    /** 共享块标识（仅用于同一次遍历内分组比较，无持久含义）。 */
    const void* sharedDataId() const { return d.get(); }

    /** 置脏广播到实例组全体成员：存盘按帧跳过未修改帧（needSaveFrame），
     *  若只置被编辑帧，其它成员的文件保持陈旧内容，重开工程即丢编辑。 */
    void modification() override;
    void setModified(bool b) override;

    /** Loads the backing image data from disk and into memory if it exists but hasn't been loaded yet */
    void loadFile() override;

    /** Unloads the image data from memory if there's valid backing data on disk and the latest state has been stored */
    void unloadFile() override;

    /** Checks whether the keyframe holds valid image data.
     *
     *  @note This does not verify that the data is loaded from a backing file.
     *  @return `true` if the keyframe holds a valid image, otherwise returns `false`
    */
    bool isLoaded() const override;
    quint64 memoryUsage() override;

    void paintImage(QPainter& painter);
    void paintImage(QPainter &painter, QImage &image, QRect sourceRect, QRect destRect);

    QImage* image();
    void    setImage(QImage* pImg);

    BitmapImage copy();
    BitmapImage copy(QRect rectangle);
    BitmapImage copy(QPolygonF polygon);
    void paste(BitmapImage*, QPainter::CompositionMode cm = QPainter::CompositionMode_SourceOver);
    void paste(const TiledBuffer* tiledBuffer, QPainter::CompositionMode cm = QPainter::CompositionMode_SourceOver, const QPainterPath* selectionClip = nullptr);

    void moveTopLeft(QPoint point);
    void moveTopLeft(QPointF point) { moveTopLeft(point.toPoint()); }
    void transform(QRect rectangle, bool smoothTransform);
    void transform(QRectF rectangle, bool smoothTransform) { transform(rectangle.toRect(), smoothTransform); }
    BitmapImage transformed(QRect selection, QTransform transform, bool smoothTransform);
    BitmapImage transformed(QRect rectangle, bool smoothTransform);
    BitmapImage transformed(QRectF rectangle, bool smoothTransform) { return transformed(rectangle.toRect(), smoothTransform); }
    BitmapImage transformed(QPolygonF selection, QTransform transform, bool smoothTransform);

    bool contains(QPoint P) { return d->bounds.contains(P); }
    bool contains(QPointF P) { return contains(P.toPoint()); }
    void autoCrop();

    QRgb pixel(int x, int y);
    QRgb pixel(QPoint p);
    void setPixel(int x, int y, QRgb color);
    void setPixel(QPoint p, QRgb color);
    void fillNonAlphaPixels(const QRgb color);

    QRgb constScanLine(int x, int y) const;
    void scanLine(int x, int y, QRgb color);
    void clear();
    void clear(QRect rectangle);
    void clear(QRectF rectangle) { clear(rectangle.toRect()); }
    void clear(QPolygonF polygon);

    void drawLine(QPointF P1, QPointF P2, QPen pen, QPainter::CompositionMode cm, bool antialiasing);
    void drawRect(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing);
    void drawEllipse(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing);
    void drawPath(QPainterPath path, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing);

    QPoint topLeft() { autoCrop(); return d->bounds.topLeft(); }
    QPoint topRight() { autoCrop(); return d->bounds.topRight(); }
    QPoint bottomLeft() { autoCrop(); return d->bounds.bottomLeft(); }
    QPoint bottomRight() { autoCrop(); return d->bounds.bottomRight(); }
    int left() { autoCrop(); return d->bounds.left(); }
    int right() { autoCrop(); return d->bounds.right(); }
    int top() { autoCrop(); return d->bounds.top(); }
    int bottom() { autoCrop(); return d->bounds.bottom(); }
    int width() { autoCrop(); return d->bounds.width(); }
    int height() { autoCrop(); return d->bounds.height(); }
    QSize size() { autoCrop(); return d->bounds.size(); }

    BitmapImage* scanToTransparent(BitmapImage* img, int threshold, bool redEnabled, bool greenEnabled, bool blueEnabled);

    QRect& bounds() { autoCrop(); return d->bounds; }

    /** Determines if the BitmapImage is minimally bounded.
     *
     *  A BitmapImage is minimally bounded if all edges contain
     *  at least 1 non-transparent pixel (alpha > 0). In other words,
     *  the size of the image cannot be decreased without
     *  cropping visible data.
     *
     *  @return True only if bounds() is the minimal bounding box
     *          for the contained image.
     */
    bool isMinimallyBounded() const { return d->minBound; }
    void enableAutoCrop(bool b) { mEnableAutoCrop = b; }
    void setOpacity(qreal opacity) { mOpacity = opacity; }
    qreal getOpacity() const { return mOpacity; }

    Status writeFile(const QString& filename);

    /** Compare colors for the purposes of flood filling
     *
     *  Calculates the Eulcidian difference of the RGB channels
     *  of the image and compares it to the tolerance
     *
     *  @param[in] newColor The first color to compare
     *  @param[in] oldColor The second color to compare
     *  @param[in] tolerance The threshold limit between a matching and non-matching color
     *  @param[in,out] cache Contains a mapping of previous results of compareColor with rule that
     *                 cache[someColor] = compareColor(someColor, oldColor, tolerance)
     *
     *  @return Returns true if the colors have a similarity below the tolerance level
     *          (i.e. if Eulcidian distance squared is <= tolerance)
     */
    static inline bool compareColor(QRgb newColor, QRgb oldColor, int tolerance, QHash<QRgb, bool> *cache)
    {
        // Handle trivial case
        if (newColor == oldColor) return true;

        if(cache && cache->contains(newColor)) return cache->value(newColor);

        // Get Eulcidian distance between colors
        // Not an accurate representation of human perception,
        // but it's the best any image editing program ever does
        int diffRed = static_cast<int>(qPow(qRed(oldColor) - qRed(newColor), 2));
        int diffGreen = static_cast<int>(qPow(qGreen(oldColor) - qGreen(newColor), 2));
        int diffBlue = static_cast<int>(qPow(qBlue(oldColor) - qBlue(newColor), 2));
        // This may not be the best way to handle alpha since the other channels become less relevant as
        // the alpha is reduces (ex. QColor(0,0,0,0) is the same as QColor(255,255,255,0))
        int diffAlpha = static_cast<int>(qPow(qAlpha(oldColor) - qAlpha(newColor), 2));

        bool isSimilar = (diffRed + diffGreen + diffBlue + diffAlpha) <= tolerance;

        if(cache)
        {
            Q_ASSERT(cache->contains(isSimilar) ? isSimilar == (*cache)[newColor] : true);
            (*cache)[newColor] = isSimilar;
        }

        return isSimilar;
    }

protected:
    void updateBounds(QRect rectangle);
    void extend(const QPoint& p);
    void extend(QRect rectangle);

    void setCompositionModeBounds(BitmapImage *source, QPainter::CompositionMode cm);
    void setCompositionModeBounds(QRect sourceBounds, bool isSourceMinBounds, QPainter::CompositionMode cm);

private:
    /** 帧图像数据共享块。普通帧独占一块；实例帧（Instance）由 createInstance()
     *  建链后多个壳共用同一块——内容与边界写穿共享块即全组同步。
     *  拷贝构造/赋值恒为深拷贝语义，共享只经 createInstance()/shareDataFrom() 建立，
     *  否则撤销快照（BitmapReplaceCommand 按值持有）会被别名破坏。 */
    struct SharedData
    {
        QImage image;
        QRect bounds {0, 0, 0, 0};

        /** @see isMinimallyBounded() */
        bool minBound = true;

        /** 同组成员登记簿（含自身）。置脏广播与实例归属判断的事实源；
         *  由壳的构造/析构/重绑维护，不参与拷贝语义。 */
        std::vector<BitmapImage*> owners;

        SharedData() = default;
        SharedData(const SharedData& o) : image(o.image), bounds(o.bounds), minBound(o.minBound) {}
        SharedData& operator=(const SharedData& o)
        {
            if (this != &o) { image = o.image; bounds = o.bounds; minBound = o.minBound; }
            return *this;
        }
    };
    std::shared_ptr<SharedData> d = std::make_shared<SharedData>();

    bool mEnableAutoCrop = false;

    void registerOwner();
    void unregisterOwner();
    void rebindSharedData(std::shared_ptr<SharedData> newData);

    const int LOW_THRESHOLD = 30; // threshold for images to be given transparency
    const int COLORDIFF = 5;      // difference in color values to decide color
    const int GRAYSCALEDIFF = 15; // difference in grasycale values to decide color

    qreal mOpacity = 1.0;
};

#endif
