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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
#ifndef LAYERSPLITTER_H
#define LAYERSPLITTER_H

#include <QImage>
#include <QRgb>

#include <vector>

#include "colordistance.h"

struct LayerSplitParams
{
    int fuzziness = 20;           // 0..200，Lab ΔE 尺度；0=精确色匹配
    bool disregardOpacity = true; // 匹配键剥掉像素透明度（写入仍保留原α）
    bool sortLayers = true;       // 结果按面积大者排前（大在上）

    // 动作级选项（算法不使用）：拆完隐藏源图层
    bool hideOriginal = false;
};

/** 拆分图层颜色：Krita「Split Layer」移植（贪心顺序聚类）。

 * 语义（忠实移植 plugins/extensions/layersplit/layersplit.cpp）：
 *  - 逐像素顺序找第一个 ΔE≤fuzziness 的桶（先到先得，桶色不更新均值）；
 *  - 命中把原始像素（含原α）写入该桶画布同坐标；
 *  - 透明像素跳过；disregardOpacity 只影响精确匹配（fuzziness=0）的键，
 *    ΔE 路径本身不含 α（与 Krita 的 KoColor::difference 一致）；
 *  - 桶数上限 256（照片类输入防御，超限 processFrame 返回 false）。
 *
 * 多帧流式：逐帧喂 processFrame（同色跨帧归同一桶，桶序=全局首现序），
 * takeBucketImage 取走当前帧该桶画布（取后重置为空，内存峰值=单帧画布数）。
 */
class LayerSplitter
{
public:
    struct Bucket
    {
        QRgb keyColor = 0; // 匹配键色（disregardOpacity 时 α=255）
        QImage image;      // 与源帧同尺寸，其余像素全透明
        int pixels = 0;    // 该桶像素数
    };

    explicit LayerSplitter(const LayerSplitParams& params);

    /** 单帧便捷接口：拆一张图，桶已按排序规则排好（面积降序/发现序）。
     * 桶超上限返回 false（outBuckets 填入已建桶）。 */
    static bool splitImage(const QImage& img, const LayerSplitParams& params,
                           std::vector<Bucket>& outBuckets);

    // ---- 多帧流式接口 ----

    /** 喂一帧（Format_ARGB32_Premultiplied）。false=桶超上限，调用方应中止。 */
    bool processFrame(const QImage& img);

    int bucketCount() const;
    QRgb bucketKeyColor(int index) const;
    int bucketPixels(int index) const; // 跨帧累计

    /** 取走当前帧 index 桶画布（该帧无像素则返回 null 图；取后画布重置）。 */
    QImage takeBucketImage(int index);

    /** 最终建层顺序：桶索引序列（sortLayers=true 按面积降序，否则发现序）。 */
    std::vector<int> sortedBucketOrder() const;

private:
    int findBucket(const int r, const int g, const int b, const QRgb exactKey) const;

    struct BucketState
    {
        QRgb keyColor = 0;
        ColorDistance::LabF keyLab;
        int pixels = 0;
        QImage frameImage; // 当前帧画布（惰性创建）
    };

    LayerSplitParams mParams;
    std::vector<BucketState> mBuckets;
    QImage mFrameSize; // 当前帧尺寸模板（空=还没喂过帧）
};

#endif // LAYERSPLITTER_H
