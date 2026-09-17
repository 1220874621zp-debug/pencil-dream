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

Ported from Krita's LayerSplit plugin (SPDX-FileCopyrightText: 2014 Boudewijn Rempt,
LGPL-2.0-or-later): 贪心顺序聚类按颜色拆层。
*/
#include "layersplitter.h"

#include <algorithm>

namespace
{

constexpr int MAX_BUCKETS = 256;

// 预乘分量 → 直通（与 holefiller/colortoalpha 同式）
int liftChannel(const int premul, const int alpha)
{
    return std::min(255, (premul * 255 + alpha / 2) / alpha);
}

} // namespace

LayerSplitter::LayerSplitter(const LayerSplitParams& params)
    : mParams(params)
{
}

int LayerSplitter::findBucket(const int r, const int g, const int b, const QRgb exactKey) const
{
    if (mParams.fuzziness <= 0)
    {
        for (size_t i = 0; i < mBuckets.size(); ++i)
        {
            if (mBuckets[i].keyColor == exactKey)
                return static_cast<int>(i);
        }
        return -1;
    }

    const ColorDistance::LabF lab = ColorDistance::rgbToLab(r, g, b);
    const double fuzz = mParams.fuzziness;
    for (size_t i = 0; i < mBuckets.size(); ++i)
    {
        if (ColorDistance::deltaE(lab, mBuckets[i].keyLab) <= fuzz)
            return static_cast<int>(i);
    }
    return -1;
}

bool LayerSplitter::processFrame(const QImage& img)
{
    if (img.isNull() || img.format() != QImage::Format_ARGB32_Premultiplied)
        return true; // 契约外输入：安全跳过（与 colortoalpha 同守卫语义）

    mFrameSize = QImage(); // 各桶画布按本帧尺寸惰性重建
    const QSize size = img.size();

    for (int y = 0; y < img.height(); ++y)
    {
        const auto* line = reinterpret_cast<const QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;

            const int r = liftChannel(qRed(px), a);
            const int g = liftChannel(qGreen(px), a);
            const int b = liftChannel(qBlue(px), a);
            // 精确模式的键（disregardOpacity 时剥α）；ΔE 路径只看 RGB
            const QRgb exactKey = mParams.disregardOpacity ? qRgb(r, g, b) : qRgba(r, g, b, a);

            const int hit = findBucket(r, g, b, exactKey);
            if (hit >= 0)
            {
                BucketState& bucket = mBuckets[hit];
                if (bucket.frameImage.isNull())
                {
                    bucket.frameImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
                    bucket.frameImage.fill(Qt::transparent); // QImage 构造不清零，未写区域须为透明
                }
                reinterpret_cast<QRgb*>(bucket.frameImage.scanLine(y))[x] = px; // 原像素含原α
                ++bucket.pixels;
            }
            else
            {
                if (static_cast<int>(mBuckets.size()) >= MAX_BUCKETS)
                    return false;
                BucketState bucket;
                bucket.keyColor = exactKey;
                bucket.keyLab = ColorDistance::rgbToLab(r, g, b);
                bucket.pixels = 1;
                bucket.frameImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
                bucket.frameImage.fill(Qt::transparent);
                reinterpret_cast<QRgb*>(bucket.frameImage.scanLine(y))[x] = px;
                mBuckets.push_back(std::move(bucket));
            }
        }
    }
    return true;
}

int LayerSplitter::bucketCount() const
{
    return static_cast<int>(mBuckets.size());
}

QRgb LayerSplitter::bucketKeyColor(const int index) const
{
    return mBuckets.at(index).keyColor;
}

int LayerSplitter::bucketPixels(const int index) const
{
    return mBuckets.at(index).pixels;
}

QImage LayerSplitter::takeBucketImage(const int index)
{
    return std::move(mBuckets.at(index).frameImage);
}

std::vector<int> LayerSplitter::sortedBucketOrder() const
{
    std::vector<int> order(mBuckets.size());
    for (size_t i = 0; i < mBuckets.size(); ++i)
        order[i] = static_cast<int>(i);

    if (mParams.sortLayers)
    {
        std::stable_sort(order.begin(), order.end(), [this](const int x, const int y) {
            return mBuckets[x].pixels > mBuckets[y].pixels; // 面积降序：大者在上
        });
    }
    return order;
}

bool LayerSplitter::splitImage(const QImage& img, const LayerSplitParams& params,
                               std::vector<Bucket>& outBuckets)
{
    LayerSplitter splitter(params);
    if (!splitter.processFrame(img))
        return false;

    outBuckets.clear();
    for (const int index : splitter.sortedBucketOrder())
    {
        Bucket bucket;
        bucket.keyColor = splitter.bucketKeyColor(index);
        bucket.image = splitter.takeBucketImage(index);
        bucket.pixels = splitter.bucketPixels(index);
        outBuckets.push_back(std::move(bucket));
    }
    return true;
}
