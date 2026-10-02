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
#ifndef SHADOW_IMAGE_H
#define SHADOW_IMAGE_H

#include "bitmapimage.h"

#include "graphics/bitmap/shadowengine.h"

/*
 * 智能阴影图层的关键帧：继承 BitmapImage（分割线数据 = 位图图像，
 * 绘画/撤销/存取全部复用位图管线），旁挂阴影填充缓存。
 *
 * - 分割线：用户用标记色画的明暗交界线（即基类的 QImage）
 * - 填充缓存：ShadowFill::computeShadow 的计算结果（派生数据，
 *   不存盘、不进撤销），连同警告列表一起缓存
 */
class ShadowImage : public BitmapImage
{
public:
    ShadowImage() = default;
    explicit ShadowImage(const ShadowImage& rhs);

    ShadowImage* clone() const override;

    /** 任何修改（含撤销恢复）都使阴影缓存失效 */
    void setModified(bool b) override;

    bool needsUpdate() const { return mNeedsUpdate; }
    void setNeedsUpdate(bool b) { mNeedsUpdate = b; }

    /** 填充结果及其画布坐标范围（空图 = 没有可涂的阴影区域） */
    QImage shadowImage() const { return mShadow; }
    QRect shadowBounds() const { return mShadowBounds; }

    /** 计算时的图层结构代数（栈序变化会使缓存过期） */
    quint32 computedStructureGeneration() const { return mComputedStructureGeneration; }

    void setShadowResult(QImage result, QRect bounds, quint32 structureGeneration);

    /** 最近一次计算的警告（未切开分割线/无法判定），供面板显示 */
    QVector<ShadowFill::Warning> warnings() const { return mWarnings; }
    void setWarnings(QVector<ShadowFill::Warning> warnings) { mWarnings = warnings; }

private:
    QImage mShadow;
    QRect mShadowBounds;
    bool mNeedsUpdate = true;
    quint32 mComputedStructureGeneration = 0;
    QVector<ShadowFill::Warning> mWarnings;
};

#endif // SHADOW_IMAGE_H
