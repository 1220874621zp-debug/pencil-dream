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

#include <QColor>
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

/* 两颜色 RGB 欧氏距离平方 */
inline int colorDistanceSq(QRgb a, QRgb b)
{
    const int dr = qRed(a) - qRed(b);
    const int dg = qGreen(a) - qGreen(b);
    const int db = qBlue(a) - qBlue(b);
    return dr * dr + dg * dg + db * db;
}
constexpr int MERGE_COLOR_DIST_SQ = 1200; // 灰系（低饱和）合并阈值 ≈每通道差20

/*
 * 颜色相似判定（相近色合并）：有彩色按色相（hue 差 ≤ 12° 判同色，
 * 明度/饱和度变体——画笔半透明叠色混出的深浅变体——全部并入主色，
 * 而用户刻意分开的相邻色相如红/橙/黄不会被误并）；低饱和（灰系）
 * 色相无意义，退回 RGB 距离判定。
 */
inline bool similarColors(QRgb a, QRgb b)
{
    int ha = 0, sa = 0, va = 0, hb = 0, sb = 0, vb = 0;
    QColor(a).getHsv(&ha, &sa, &va);
    QColor(b).getHsv(&hb, &sb, &vb);
    Q_UNUSED(va); Q_UNUSED(vb);
    if (sa < 40 || sb < 40)
        return colorDistanceSq(a, b) <= MERGE_COLOR_DIST_SQ;
    int dh = qAbs(ha - hb);
    if (dh > 180)
        dh = 360 - dh;
    return dh <= 12;
}



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
 * 笔画组主色分类（strokes 须面积降序，splitKeyStrokesByColor 的返回序）：
 * 1) 色相近似组并入首个相似主色（变体——画笔软边/流量的明暗中间色）；
 * 2) 余组中与 ≥2 个其它组空间相邻（覆盖膨胀 2px 相交）的组判为
 *    叠色混合带——半透明叠色/互补混出的中间色（红叠黄混橙、红叠绿
 *    混棕、互补混灰）必然同时贴着两个母色——并入面积最大的相邻组；
 *    刻意选的独立色点不贴别的色，保留；
 * 3) 其余组为主色。返回每组的主色组下标（主色指向自身）。
 * hasTransparent 时 transparentColor 组恒为主色（保护语义，不参与
 * 折叠也不作折叠目标）。
 */
QVector<int> classifyStrokeMasters(const QVector<KeyStroke>& strokes,
                                   QRgb transparentColor, bool hasTransparent);

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
 * 相近色组归并：同主色的明暗变体组（画笔半透明叠色混出）并入面积最大的
 * 相似主色组（蒙版逐像素取大），分水岭只按主色扩散，杜绝变体扩散成杂色区。
 * 透明标记色组钉在首位：其变体并入透明组而非普通主色（透明语义不丢）。
 * strokes 须为面积降序（splitKeyStrokesByColor 的返回序）。
 */
void mergeVariantStrokes(QVector<KeyStroke>& strokes, const FilteringOptions& options);

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

/*
 * 一步到位的组合入口。保证：每个封闭区域只按一组种子着色
 * （区域内种子覆盖最大者胜出，透明保护组豁免）——一个封闭
 * 区域只标记一个颜色点，平涂结果无区域内部多色斑。
 */
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
 * 色点跨帧搬运（区域锚点 + 颜色场映射）：
 * 落点 = 目标帧各分割区域锚点（保证标记落进封闭区域，每区域一个标记）；
 * 颜色 = 锚点经"源包围盒→目标包围盒"相对映射回源帧、采样源帧着色颜色场；
 * 颜色补漏：源帧每种颜色（面积≥64px）若未出现在目标标记中，把映射点
 * 所在区域的标记改成该颜色（替换而非叠加，维持一区一点）；
 * 邻近继承：采空（源帧透明）且无透明语义的封闭区域，继承质心最近
 * 已标区域的颜色——始终保证每个封闭区域有一个颜色点。
 * 采样为透明且 hasTransparent 时画透明标记色。
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
