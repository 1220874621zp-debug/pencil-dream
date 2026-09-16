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
    int searchRadius = 48;    // 全局位移搜索半径（像素）
    int refineRadius = 8;     // 组级位移在全局位移邻域内的细化半径
    int patchRadiusMin = 8;   // 匹配块初始半径（无线稿时自适应增长）
    int patchRadiusMax = 32;  // 匹配块最大半径
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

/*
 * 色点跨帧搬运（包围盒相对映射版）：帧A色点按其在帧A线稿包围盒内的
 * 相对位置 (u,v)，映射到帧B线稿包围盒的同相对位置，每组重画标准大小
 * 的实心标记点（颜色保留、不搬像素）。适合"角色整体移动+缩放"型动画
 * （包围盒位移即整体位移，无匹配歧义）；非刚性形变时落点为近似，
 * 可在目标帧手动修正色点。
 */
QImage transportStrokesByBounds(const QImage& lineArtA,
                                const QImage& strokesA,
                                const QImage& lineArtB,
                                const QRect& bounds);

/*
 * 背景透明包裹（描边形式）：
 * 沿线稿包围盒边缘画一圈指定颜色（通常为标记透明的背景色）的实心描边
 * （厚 3px，向外扩 1px 避压线稿边缘）。连通背景接触描边即整体受保护；
 * 被残线完全封闭、不接触描边的背景口袋不受保护（与手绘描边同限）。
 * 返回与 lineArt 同尺寸、仅含描边的 ARGB32_Premultiplied 图。
 */
QImage makeBackgroundWrap(const QImage& lineArt, const QRect& bounds, QRgb color);

/* ===================== 封闭区域分割与区域级搬运 ===================== */

/** 一个封闭区域（线稿屏障外的连通域） */
struct RegionLabel
{
    QRect bounds;             // 区域包围盒（图内坐标）
    QPoint centroid;          // 质心（跨帧邻近匹配用）
    QPoint anchor;            // 区域内离质心最近的像素（标记点落点）
    qint64 area = 0;
    bool touchesEdge = false; // 接触计算域边缘（开放背景区域）
};

struct RegionSegmentation
{
    QVector<qint32> labelOf;      // 每像素标签号（0 = 屏障），bounds 行优先
    QVector<RegionLabel> regions; // 下标 = 标签号 - 1
    QRect bounds;
};

/*
 * 线稿封闭区域分割：以（含闭缝/边缘检测滤波的）线稿为屏障对 bounds
 * 做连通域标记。fuzzyRadius 可先桥接线稿小缺口，减少区域误连通。
 */
RegionSegmentation segmentRegions(const QImage& lineArt, const QRect& bounds,
                                  const FilteringOptions& options = FilteringOptions());

/*
 * 色点跨帧搬运（区域邻近映射）：
 * 源帧各封闭区域的颜色从其着色结果（纯色平涂）锚点采样，目标帧每个
 * 区域按"质心最近"继承源帧有色区域颜色，在区域锚点画标准标记点——
 * 每区域只标一色；新增区域拿最近区域颜色（后续可手动修正）。
 * 着色结果为透明的区域：hasTransparent 时画透明标记色，否则不标。
 * coloringA 为平铺到与 lineArtA 同尺寸画布的源帧着色结果。
 */
QImage transportStrokesByRegions(const QImage& lineArtA,
                                 const QImage& coloringA,
                                 const QImage& lineArtB,
                                 const QRect& bounds,
                                 const FilteringOptions& options = FilteringOptions(),
                                 QRgb transparentColor = 0,
                                 bool hasTransparent = false);

}

#endif // COLORIZE_ENGINE_H
