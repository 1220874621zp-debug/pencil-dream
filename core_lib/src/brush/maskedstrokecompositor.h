/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef MASKEDSTROKECOMPOSITOR_H
#define MASKEDSTROKECOMPOSITOR_H

#include <QImage>
#include <QPoint>
#include <QRect>

#include "brushsettings.h"
#include "graphics/bitmap/washblend.h"

/**
 * 双笔尖合成器（Krita MaskingBrush / KisMaskingBrushRenderer 的移植）。
 *
 * 主笔尖与副笔尖沿同一轨迹各自独立撒 dab（各有自己的间距/压感曲线）：
 *  - 主 dab 按正常 wash/buildup 语义累积进"纯净主缓冲"；
 *  - 副 dab 以白色、满流量、union 累积进"覆盖蒙版"（Krita 白漆 +
 *    COMPOSITE_ALPHA_DARKEN 的等效语义：覆盖度只增不减）；
 *  - 任意时刻可取合成区域：对主笔迹的 alpha 逐像素做模式复合（只改
 *    alpha，不改颜色；burn/hard_mix 等公式的 alpha 专用变体）。
 *
 * 合成是"函数式"的（总是从两份累积状态重算，不做破坏性叠加），与 Krita
 * updateProjection 每 → 拷贝 strokeDevice 再复合 的做法一致。
 */
class MaskedStrokeCompositor
{
public:
    void begin(BrushMaskSettings::Mode mode);
    bool active() const { return mActive; }
    void end();
    void clear();

    /** 主 dab：正常笔刷合成语义落进纯净主缓冲，返回后可取 composedRegion 刷新显示 */
    void mainDab(const QImage& dab, const QPoint& topLeft, const DabPasteParams& params);
    /** 副 dab：白色 union 覆盖累积（忽略传入颜色，只取 alpha 形状） */
    void maskDab(const QImage& dab, const QPoint& topLeft);

    /** 取合成后的区域（钳到内部缓冲范围；越界返回空图）。outOrigin = 返回图左上角的画布坐标 */
    QImage composedRegion(const QRect& rect, QPoint& outOrigin) const;
    /** 内部缓冲的画布范围（空 = 尚无 dab） */
    QRect bounds() const;

    /** 对整图做双笔尖 alpha 复合（两图同原点；src=cover 的 alpha，dst=main 的 alpha） */
    static void applyMaskOpToImage(QImage& main, const QImage& cover, BrushMaskSettings::Mode mode);

    /** 单点 alpha 复合公式，src/dst ∈ [0,1]（KisMaskingBrushCompositeOp 各模式的 alpha 变体，float 域） */
    static qreal maskOp(BrushMaskSettings::Mode mode, qreal src, qreal dst);

    /** 纹理对 dab alpha 的复合公式（KisTextureOption：带 strength 的非软变体，float 域） */
    static qreal textureOp(int mode, qreal src, qreal dst, qreal strength);

private:
    void ensureBuffers(const QRect& rect);

    bool mActive = false;
    BrushMaskSettings::Mode mMode = BrushMaskSettings::Mode::Burn;
    QImage mMain;    // 纯净主笔迹累积（ARGB32_Premultiplied）
    QImage mCover;   // 白色覆盖蒙版（ARGB32_Premultiplied，alpha=union 覆盖度）
    QPoint mOrigin;  // 两缓冲共同的左上角画布坐标
    QRect mExtent;   // 蒙版范围 = 副笔尖 dab 矩形并集（范围内才复合，Krita mask extent）
};

#endif // MASKEDSTROKECOMPOSITOR_H
