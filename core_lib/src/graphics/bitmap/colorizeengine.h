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
#ifndef COLORIZE_ENGINE_H
#define COLORIZE_ENGINE_H

#include <QImage>
#include <QRect>
#include <QRgb>
#include <QVector>
#include <functional>

/*
 * 智能填色引擎 — Krita「智能填色蒙版」(Colorize Mask / lazybrush) 算法移植。
 *
 * 算法源自 Krita (GPL-2.0-or-later) 的 KisWatershedWorker /
 * KisColorizeStrokeStrategy / KisLazyFillTools（Dmitry Kazakov, 2016-2017），
 * 重写为纯 QImage 管线：
 *
 *   buildHeightMap         线稿 → 高度图（255 = 线稿屏障）
 *   splitKeyStrokesByColor 彩色笔画图按颜色拆分为单色蒙版
 *   runWatershed           分水岭竞争填充 + 污染区清理
 *
 * 约定：传入的所有 QImage 尺寸必须一致，bounds 必须落在图像矩形内，
 * 计算只在 bounds 内进行；坐标全部使用图像本地坐标。
 */
namespace Colorize
{

struct FilteringOptions
{
    bool useEdgeDetection = false;  // LoG 边缘检测（软/灰线稿增强为清晰屏障）
    qreal edgeDetectionSize = 4.0;
    qreal fuzzyRadius = 0.0;        // 高斯闭缝半径（跨越线稿小缺口）
    qreal cleanUpAmount = 0.7;      // 污染区清理强度 [0..1]

    // 透明颜色（Krita transparentIndex 语义）：该颜色的笔画区域保持不填
    bool hasTransparentColor = false;
    QRgb transparentColor = 0;
};

struct KeyStroke
{
    QImage mask;                // Format_Grayscale8，>0 = 笔画覆盖
    QRgb color = 0;             // 非预乘颜色（isTransparent 时无意义）
    bool isTransparent = false; // 透明笔画：填充结果保持透明（保护区域）
};

/*
 * 线稿 → 高度图（Format_Grayscale8，255 = 屏障）。
 * lineArt 通常为 Format_ARGB32_Premultiplied，屏障强度取自 alpha 通道。
 */
QImage buildHeightMap(const QImage& lineArt, const QRect& bounds, const FilteringOptions& options);

/*
 * 彩色笔画图 → 单色笔画列表。按「反预乘后的精确 RGB」聚类，
 * 抗锯齿边缘像素归属其颜色组；返回顺序为面积降序（大面积笔画优先）。
 */
QVector<KeyStroke> splitKeyStrokesByColor(const QImage& strokesImage, const QRect& bounds);

/*
 * 分水岭填充。strokes 按值传入（算法会消费其中的蒙版副本）。
 * 返回 ARGB32_Premultiplied 结果（bounds 大小，未覆盖/透明笔画区为全透明）。
 * progress 以 0..100 回调，返回 false 则取消并返回空 QImage。
 */
QImage runWatershed(const QImage& heightMap,
                    QVector<KeyStroke> strokes,
                    const QRect& bounds,
                    qreal cleanUpAmount,
                    const std::function<bool(int)>& progress = std::function<bool(int)>());

/* 一步到位的组合入口。 */
QImage colorize(const QImage& lineArt,
                const QImage& strokesImage,
                const QRect& bounds,
                const FilteringOptions& options,
                const std::function<bool(int)>& progress = std::function<bool(int)>());

/* ===================== 跨帧色点搬运 ===================== */

struct TransportOptions
{
    int searchRadius = 48;     // 全局位移搜索半径（像素）
    int refineRadius = 8;      // 组级位移在全局位移邻域内的细化半径
    int patchRadiusMin = 8;    // 匹配块初始半径（无线稿时自适应增长）
    int patchRadiusMax = 32;   // 匹配块最大半径
    qreal motionPenalty = 6.0;  // 全局层偏向零位移的正则系数（压制孔径/切向歧义）
    qreal refinePenalty = 40.0; // 组级细化偏向全局位移的正则系数（形变噪声下无强证据不动）
};

/*
 * 色点跨帧搬运（"自动给下一帧上色"的传播步骤）：
 * 把帧A的彩色笔画图 strokesA 按线稿块匹配平移到帧B的坐标系。
 * 每个颜色组在全局位移邻域内细化各自的刚体平移（相邻动画帧以
 * 整体运动为主、局部形变为次）；锚点附近无线稿（纯平区）的组
 * 跟随全局位移。
 * 透明颜色语义由上层 colorize/options 处理，这里只搬运颜色笔画。
 * 返回尺寸与输入一致、可直接喂给 colorize() 的帧B笔画图。
 */
QImage transportStrokes(const QImage& lineArtA,
                        const QImage& strokesA,
                        const QImage& lineArtB,
                        const QRect& bounds,
                        const TransportOptions& options = TransportOptions());

}

#endif // COLORIZE_ENGINE_H
