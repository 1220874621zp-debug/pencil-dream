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
#include "catch.hpp"

#include "layersplitter.h"

#include <QImage>
#include <QRgb>

namespace
{

QImage makeImage(const int w, const int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    return img;
}

// 直接写预乘像素（不透明色预乘与直通一致）
void fillRect(QImage& img, const int x0, const int y0, const int x1, const int y1, const QRgb premul)
{
    for (int y = y0; y <= y1; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = x0; x <= x1; ++x)
            line[x] = premul;
    }
}

void fillRectStraight(QImage& img, const int x0, const int y0, const int x1, const int y1, const QRgb straight)
{
    fillRect(img, x0, y0, x1, y1, qPremultiply(straight));
}

} // namespace

TEST_CASE("LayerSplitter-flat-colors")
{
    SECTION("三色平涂：恰三桶、像素数正确、桶内像素与原图逐位一致")
    {
        QImage img = makeImage(9, 3);
        fillRect(img, 0, 0, 2, 2, qRgb(255, 0, 0));    // 9 px 红
        fillRect(img, 3, 0, 5, 2, qRgb(0, 200, 0));    // 9 px 绿
        fillRect(img, 6, 0, 8, 2, qRgb(0, 0, 255));    // 9 px 蓝

        LayerSplitParams p; // fuzziness=20 默认
        std::vector<LayerSplitter::Bucket> buckets;
        REQUIRE(LayerSplitter::splitImage(img, p, buckets));
        REQUIRE(buckets.size() == 3);

        int totalPixels = 0;
        for (const auto& bucket : buckets) totalPixels += bucket.pixels;
        REQUIRE(totalPixels == 27);

        // 每个原图不透明像素恰落入一个桶且逐位一致
        for (int y = 0; y < img.height(); ++y)
        {
            for (int x = 0; x < img.width(); ++x)
            {
                const QRgb src = reinterpret_cast<const QRgb*>(img.scanLine(y))[x];
                if (qAlpha(src) == 0) continue;
                int hits = 0;
                for (const auto& bucket : buckets)
                {
                    if (qAlpha(reinterpret_cast<const QRgb*>(bucket.image.scanLine(y))[x]) != 0)
                    {
                        REQUIRE(reinterpret_cast<const QRgb*>(bucket.image.scanLine(y))[x] == src);
                        ++hits;
                    }
                }
                REQUIRE(hits == 1);
            }
        }
    }

    SECTION("全透明图：零桶")
    {
        std::vector<LayerSplitter::Bucket> buckets;
        REQUIRE(LayerSplitter::splitImage(makeImage(8, 8), LayerSplitParams{}, buckets));
        REQUIRE(buckets.empty());
    }
}

TEST_CASE("LayerSplitter-fuzziness")
{
    SECTION("模糊度0：相近色各成一桶，同色归同桶")
    {
        QImage img = makeImage(4, 1);
        fillRect(img, 0, 0, 0, 0, qRgb(200, 30, 30));
        fillRect(img, 1, 0, 1, 0, qRgb(203, 31, 29)); // ΔE 很小但非零
        fillRect(img, 2, 0, 2, 0, qRgb(200, 30, 30)); // 与首像素同色

        LayerSplitParams p;
        p.fuzziness = 0;
        std::vector<LayerSplitter::Bucket> buckets;
        REQUIRE(LayerSplitter::splitImage(img, p, buckets));
        REQUIRE(buckets.size() == 2);
        REQUIRE(buckets[0].pixels == 2); // 两块同色
    }

    SECTION("模糊度20：扫描噪点归并成一桶")
    {
        QImage img = makeImage(3, 1);
        fillRect(img, 0, 0, 0, 0, qRgb(200, 30, 30));
        fillRect(img, 1, 0, 1, 0, qRgb(203, 31, 29));
        fillRect(img, 2, 0, 2, 0, qRgb(198, 29, 32));

        LayerSplitParams p;
        p.fuzziness = 20;
        std::vector<LayerSplitter::Bucket> buckets;
        REQUIRE(LayerSplitter::splitImage(img, p, buckets));
        REQUIRE(buckets.size() == 1);
        REQUIRE(buckets[0].pixels == 3);
    }
}

TEST_CASE("LayerSplitter-opacity")
{
    SECTION("忽略透明度：同色不同α归一桶且各写原α")
    {
        // 用 255 的整数因子 α（170/85），保证预乘↔直通往返精确（预乘存储固有 ±1 舍入）
        QImage img = makeImage(3, 1);
        fillRectStraight(img, 0, 0, 0, 0, qRgba(204, 51, 51, 255));
        fillRectStraight(img, 1, 0, 1, 0, qRgba(204, 51, 51, 170));
        fillRectStraight(img, 2, 0, 2, 0, qRgba(204, 51, 51, 85));

        LayerSplitParams p;
        p.fuzziness = 0;
        p.disregardOpacity = true;
        std::vector<LayerSplitter::Bucket> buckets;
        REQUIRE(LayerSplitter::splitImage(img, p, buckets));
        REQUIRE(buckets.size() == 1);
        const QRgb* line = reinterpret_cast<const QRgb*>(buckets[0].image.scanLine(0));
        REQUIRE(qAlpha(line[0]) == 255);
        REQUIRE(qAlpha(line[1]) == 170);
        REQUIRE(qAlpha(line[2]) == 85);
    }

    SECTION("不忽略透明度+精确匹配：同色不同α各成一桶")
    {
        QImage img = makeImage(3, 1);
        fillRectStraight(img, 0, 0, 0, 0, qRgba(204, 51, 51, 255));
        fillRectStraight(img, 1, 0, 1, 0, qRgba(204, 51, 51, 170));
        fillRectStraight(img, 2, 0, 2, 0, qRgba(204, 51, 51, 85));

        LayerSplitParams p;
        p.fuzziness = 0;
        p.disregardOpacity = false;
        std::vector<LayerSplitter::Bucket> buckets;
        REQUIRE(LayerSplitter::splitImage(img, p, buckets));
        REQUIRE(buckets.size() == 3);
    }
}

TEST_CASE("LayerSplitter-first-match")
{
    // 大模糊度下中间色落入先建桶（红），即使离蓝更近——Krita 先到先得语义
    QImage img = makeImage(3, 1);
    fillRect(img, 0, 0, 0, 0, qRgb(255, 0, 0));   // 先建红桶
    fillRect(img, 1, 0, 1, 0, qRgb(0, 0, 255));   // 再建蓝桶
    fillRect(img, 2, 0, 2, 0, qRgb(128, 0, 128)); // 紫：距红≈114、距蓝≈65，都≤150

    LayerSplitParams p;
    p.fuzziness = 150;
    std::vector<LayerSplitter::Bucket> buckets;
    REQUIRE(LayerSplitter::splitImage(img, p, buckets));
    REQUIRE(buckets.size() == 2);
    REQUIRE(buckets[0].pixels == 2); // 红+紫
    REQUIRE(buckets[1].pixels == 1); // 蓝
}

TEST_CASE("LayerSplitter-sort")
{
    QImage img = makeImage(6, 1);
    fillRect(img, 0, 0, 0, 0, qRgb(10, 200, 10));  // 1 px
    fillRect(img, 1, 0, 5, 0, qRgb(200, 10, 10));  // 5 px

    LayerSplitParams p;
    p.sortLayers = true;
    std::vector<LayerSplitter::Bucket> buckets;
    REQUIRE(LayerSplitter::splitImage(img, p, buckets));
    REQUIRE(buckets.size() == 2);
    REQUIRE(buckets[0].pixels > buckets[1].pixels); // 面积降序
}

TEST_CASE("LayerSplitter-bucket-limit")
{
    // fuzziness=0 + 300 种不同色 → 超过 256 桶上限（qRgb 通道会回绕，用双通道组合保证互异）
    QImage img = makeImage(300, 1);
    for (int x = 0; x < 300; ++x)
        fillRect(img, x, 0, x, 0, qRgb(x, x / 2, 0));

    LayerSplitParams p;
    p.fuzziness = 0;
    std::vector<LayerSplitter::Bucket> buckets;
    REQUIRE_FALSE(LayerSplitter::splitImage(img, p, buckets));
}

TEST_CASE("LayerSplitter-multiframe")
{
    SECTION("同色跨帧归同一桶，每帧画布独立")
    {
        QImage f1 = makeImage(2, 1);
        fillRect(f1, 0, 0, 0, 0, qRgb(255, 0, 0));
        fillRect(f1, 1, 0, 1, 0, qRgb(0, 0, 255));

        QImage f2 = makeImage(2, 1);
        fillRect(f2, 0, 0, 0, 0, qRgb(255, 0, 0));   // 红：应归桶0
        fillRect(f2, 1, 0, 1, 0, qRgb(0, 200, 0));   // 绿：新桶

        LayerSplitter splitter(LayerSplitParams{});
        REQUIRE(splitter.processFrame(f1));
        REQUIRE(splitter.processFrame(f2));
        REQUIRE(splitter.bucketCount() == 3);
        REQUIRE(splitter.bucketPixels(0) == 2); // 红跨帧累计
    }

    SECTION("takeBucketImage 后画布重置（下一帧从空开始）")
    {
        QImage f1 = makeImage(1, 1);
        fillRect(f1, 0, 0, 0, 0, qRgb(255, 0, 0));
        QImage f2 = makeImage(1, 1);
        fillRect(f2, 0, 0, 0, 0, qRgb(0, 0, 255)); // 换色：桶0 本帧无像素

        LayerSplitter splitter(LayerSplitParams{});
        REQUIRE(splitter.processFrame(f1));
        REQUIRE_FALSE(splitter.takeBucketImage(0).isNull());
        REQUIRE(splitter.processFrame(f2));
        REQUIRE(splitter.takeBucketImage(0).isNull()); // 红 桶本帧空
        REQUIRE_FALSE(splitter.takeBucketImage(1).isNull());
    }
}

TEST_CASE("LayerSplitter-format-guard")
{
    QImage img(4, 4, QImage::Format_ARGB32); // 非预乘：契约外输入，安全跳过
    img.fill(Qt::white);
    LayerSplitter splitter(LayerSplitParams{});
    REQUIRE(splitter.processFrame(img));
    REQUIRE(splitter.bucketCount() == 0);
}
