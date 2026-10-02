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
#include <QRegion>

#include "brushsettings.h"
#include "graphics/bitmap/washblend.h"

/**
 * 双笔尖合成器（Krita MaskingBrush / KisMaskingBrushRenderer 的移植）。
 *
 * 主笔尖与副笔尖沿同一轨迹各自独立撒 dab（各有自己的间距/压感曲线）：
 *  - 主 dab 按正常 wash/buildup 语义累积进"纯净主缓冲"；
 *  - 副 dab 用副笔刷自己的 opacity/flow/涂料模式累积白色进"覆盖蒙版"
 *    （Krita 蒙版投影 = 副预设完整语义：墨沁T类满不透明度饱和成并集，
 *    霓裳染TD类 5% 不透明度只形成 ~5% 覆盖 → burn 深切出宽飞白）；
 *  - 任意时刻可取合成区域：对主笔迹的 alpha 逐像素做模式复合（只改
 *    alpha，不改颜色；burn/hard_mix 等公式的 alpha 专用变体）。
 *
 * 复合范围 = 蒙版 dab 矩形的并集（QRegion，逐 dab 局部带状区域）——
 * 与 Krita updateProjection 按蒙版 dirty rect 逐个复合一致。矩形并集
 * 之外的主笔迹原样保留：burn/hard_mix 在 src=0 处会把半透明像素清零，
 * 若像包围盒那样全范围复合，会把蒙版带之外所有浓淡渐变/软边烧穿。
 *
 * 合成是"函数式"的（总是从两份累积状态重算，不做破坏性叠加），与 Krita
 * updateProjection 每次拷贝 strokeDevice 再复合 的做法一致。
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
    /** 副 dab：白色累积进覆盖蒙版（忽略传入颜色，只取 alpha 形状；opacity/flow/涂料模式 = 副笔刷自己的） */
    void maskDab(const QImage& dab, const QPoint& topLeft,
                 qreal opacity = 1.0, qreal flow = 1.0, bool buildup = true);

    /** 取合成后的区域（钳到内部缓冲范围；越界返回空图）。outOrigin = 返回图左上角的画布坐标 */
    QImage composedRegion(const QRect& rect, QPoint& outOrigin) const;
    /** 内部缓冲的画布范围（空 = 尚无 dab） */
    QRect bounds() const;

    /** 对整图做双笔尖 alpha 复合，只在 extent（蒙版 dab 矩形并集，两图同原点坐标系）内逐像素跑 */
    static void applyMaskOpToImage(QImage& main, const QImage& cover, const QRegion& extent,
                                   BrushMaskSettings::Mode mode);

    /** 单点 alpha 复合公式，src/dst ∈ [0,1]（KisMaskingBrushCompositeOp 各模式的 alpha 变体，float 域） */
    static qreal maskOp(BrushMaskSettings::Mode mode, qreal src, qreal dst);

    /** 纹理对 dab alpha 的复合公式（KisTextureOption：带 strength 的非软变体，float 域） */
    static qreal textureOp(int mode, qreal src, qreal dst, qreal strength);

private:
    void ensureBuffers(const QRect& rect);

    bool mActive = false;
    BrushMaskSettings::Mode mMode = BrushMaskSettings::Mode::Burn;
    QImage mMain;    // 纯净主笔迹累积（ARGB32_Premultiplied）
    QImage mCover;   // 白色覆盖蒙版（ARGB32_Premultiplied，alpha=副笔刷累积覆盖度）
    QPoint mOrigin;  // 两缓冲共同的左上角画布坐标
    QRegion mExtent; // 复合范围 = 蒙版 dab 矩形并集（逐 dab 局部区域，Krita mask extent）
};

#endif // MASKEDSTROKECOMPOSITOR_H
