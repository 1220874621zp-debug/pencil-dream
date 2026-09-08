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
#ifndef COLORIZE_IMAGE_H
#define COLORIZE_IMAGE_H

#include "bitmapimage.h"

/*
 * 智能填色图层的关键帧：继承 BitmapImage（笔画数据 = 位图图像，
 * 绘画/撤销/存取全部复用位图管线），旁挂着色缓存。
 *
 * - 笔画：用户用画笔在填色层上画出的彩色笔画（即基类的 QImage）
 * - 着色缓存：Colorize::colorize 的计算结果（派生数据，不存盘、不进撤销）
 */
class ColorizeImage : public BitmapImage
{
public:
    ColorizeImage() = default;
    explicit ColorizeImage(const ColorizeImage& rhs);

    ColorizeImage* clone() const override;

    /** 任何修改（含撤销恢复）都使着色缓存失效 */
    void setModified(bool b) override;

    bool needsUpdate() const { return mNeedsUpdate; }
    void setNeedsUpdate(bool b) { mNeedsUpdate = b; }

    /** 着色结果及其画布坐标范围（空图 = 尚未计算/无需显示） */
    QImage coloringImage() const { return mColoring; }
    QRect coloringBounds() const { return mColoringBounds; }

    void setColoringResult(QImage result, QRect bounds);

private:
    QImage mColoring;
    QRect mColoringBounds;
    bool mNeedsUpdate = true;
};

#endif // COLORIZE_IMAGE_H
