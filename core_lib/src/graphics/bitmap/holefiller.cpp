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
#include "holefiller.h"

#include <QImage>
#include <QRgb>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{

// alpha 低于 8 视为透明（与线稿/填色引擎的透明阈值一致）
constexpr uint8_t ALPHA_TRANSPARENT_MAX = 7;
// 闭运算方形核半径：宽度 < 2*半径+1（11px）的缝会被抓住
constexpr int CLOSE_RADIUS = 5;
// 填充范围向非完全不透明像素膨胀的像素数（吸收抗锯齿毛边）
constexpr int FRINGE_GROW = 2;

// 预乘像素 → 直通 RGB（种子颜色须反预乘，否则半透明源会整体偏暗）
uint32_t unpremultiplyRgb(const uint8_t alpha, const QRgb premul)
{
    if (alpha == 0)
        return 0;
    if (alpha == 255)
        return premul | 0xFF000000u;
    const auto lift = [alpha](const int v) {
        return std::min(255, (v * 255 + alpha / 2) / alpha);
    };
    return qRgb(lift(qRed(premul)), lift(qGreen(premul)), lift(qBlue(premul)));
}

// 分离式盒状最大值滤波（先水平后垂直，等价方形核膨胀；边界外按 0）
void boxMax(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst,
            const int w, const int h, const int r)
{
    std::vector<uint8_t> tmp(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
    {
        const uint8_t* row = &src[static_cast<size_t>(y) * w];
        uint8_t* out = &tmp[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x)
        {
            uint8_t v = 0;
            const int x1 = std::min(w - 1, x + r);
            for (int i = std::max(0, x - r); i <= x1; ++i)
                v = std::max(v, row[i]);
            out[x] = v;
        }
    }
    for (int x = 0; x < w; ++x)
    {
        for (int y = 0; y < h; ++y)
        {
            uint8_t v = 0;
            const int y1 = std::min(h - 1, y + r);
            for (int i = std::max(0, y - r); i <= y1; ++i)
                v = std::max(v, tmp[static_cast<size_t>(i) * w + x]);
            dst[static_cast<size_t>(y) * w + x] = v;
        }
    }
}

// 分离式盒状最小值滤波（等价方形核腐蚀；窗口越界一侧按 0 计，
// 即贴边内容被保守腐蚀——与 scipy binary_closing 的 border_value=0 语义一致，
// 否则贴着画布边的透明窄带会被误判成细缝）
void boxMin(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst,
            const int w, const int h, const int r)
{
    std::vector<uint8_t> tmp(static_cast<size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y)
    {
        const uint8_t* row = &src[static_cast<size_t>(y) * w];
        uint8_t* out = &tmp[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x)
        {
            if (x < r || x > w - 1 - r)
            {
                out[x] = 0;
                continue;
            }
            uint8_t v = 255;
            for (int i = x - r; i <= x + r; ++i)
                v = std::min(v, row[i]);
            out[x] = v;
        }
    }
    for (int x = 0; x < w; ++x)
    {
        for (int y = 0; y < h; ++y)
        {
            const size_t di = static_cast<size_t>(y) * w + x;
            if (y < r || y > h - 1 - r)
            {
                dst[di] = 0;
                continue;
            }
            uint8_t v = 255;
            for (int i = y - r; i <= y + r; ++i)
                v = std::min(v, tmp[static_cast<size_t>(i) * w + x]);
            dst[di] = v;
        }
    }
}

} // namespace

int HoleFiller::fillHoles(QImage& img)
{
    if (img.format() != QImage::Format_ARGB32_Premultiplied || img.isNull())
        return 0;

    const int w = img.width();
    const int h = img.height();
    if (w <= 2 || h <= 2)
        return 0;
    const size_t n = static_cast<size_t>(w) * h;

    std::vector<uint8_t> alpha(n);
    bool hasTransparent = false;
    for (int y = 0; y < h; ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < w; ++x)
        {
            const uint8_t a = static_cast<uint8_t>(qAlpha(line[x]));
            alpha[static_cast<size_t>(y) * w + x] = a;
            hasTransparent = hasTransparent || a <= ALPHA_TRANSPARENT_MAX;
        }
    }
    if (!hasTransparent)
        return 0;

    // ---- 1. 镂空定位：8 连通，触边连通域 = 背景，其余 = 封闭镂空 ----
    std::vector<int32_t> comp(n, 0); // 0 = 非透明
    std::vector<uint8_t> compTouchesBorder(1, 0); // 下标 = 连通域 id
    std::vector<int32_t> stack;
    int32_t compCount = 0;
    for (int sy = 0; sy < h; ++sy)
    {
        for (int sx = 0; sx < w; ++sx)
        {
            const size_t s = static_cast<size_t>(sy) * w + sx;
            if (alpha[s] > ALPHA_TRANSPARENT_MAX || comp[s] != 0)
                continue;

            const int32_t id = ++compCount;
            compTouchesBorder.push_back(0);
            bool touchesBorder = false;
            comp[s] = id;
            stack.clear();
            stack.push_back(static_cast<int32_t>(s));
            while (!stack.empty())
            {
                const int32_t cur = stack.back();
                stack.pop_back();
                const int cy = cur / w;
                const int cx = cur % w;
                if (cx == 0 || cx == w - 1 || cy == 0 || cy == h - 1)
                    touchesBorder = true;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    const int ny = cy + dy;
                    if (ny < 0 || ny >= h)
                        continue;
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int nx = cx + dx;
                        if (nx < 0 || nx >= w)
                            continue;
                        const size_t ni = static_cast<size_t>(ny) * w + nx;
                        if (alpha[ni] <= ALPHA_TRANSPARENT_MAX && comp[ni] == 0)
                        {
                            comp[ni] = id;
                            stack.push_back(static_cast<int32_t>(ni));
                        }
                    }
                }
            }
            compTouchesBorder[static_cast<size_t>(id)] = touchesBorder ? 1 : 0;
        }
    }

    std::vector<uint8_t> fillMask(n, 0);
    for (size_t i = 0; i < n; ++i)
    {
        if (comp[i] != 0 && compTouchesBorder[static_cast<size_t>(comp[i])] == 0)
            fillMask[i] = 1;
    }
    comp = std::vector<int32_t>(); // 释放连通域标记，腾出内存
    std::vector<int32_t> dist(n, -1);

    // ---- 2. 细缝检测：闭运算（膨胀→腐蚀），闭出来的透明像素 = 窄缝 ----
    {
        std::vector<uint8_t> opaqueMask(n);
        std::vector<uint8_t> dilated(n);
        std::vector<uint8_t> closed(n);
        for (size_t i = 0; i < n; ++i)
            opaqueMask[i] = alpha[i] > ALPHA_TRANSPARENT_MAX ? 1 : 0;
        boxMax(opaqueMask, dilated, w, h, CLOSE_RADIUS);
        boxMin(dilated, closed, w, h, CLOSE_RADIUS);
        for (size_t i = 0; i < n; ++i)
        {
            if (closed[i] != 0 && opaqueMask[i] == 0)
                fillMask[i] = 1;
        }
    }

    // ---- 3. 毛边吸收：掩码向非完全不透明像素膨胀 2px（绝不碰全不透明笔画）----
    for (int iter = 0; iter < FRINGE_GROW; ++iter)
    {
        std::vector<uint8_t> grown(fillMask);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const size_t i = static_cast<size_t>(y) * w + x;
                if (fillMask[i] != 0 || alpha[i] == 255)
                    continue;
                const bool nextToMask =
                    (x > 0 && fillMask[i - 1] != 0) ||
                    (x < w - 1 && fillMask[i + 1] != 0) ||
                    (y > 0 && fillMask[i - w] != 0) ||
                    (y < h - 1 && fillMask[i + w] != 0);
                if (nextToMask)
                    grown[i] = 1;
            }
        }
        fillMask.swap(grown);
    }

    // ---- 4. 多源 BFS：每个待填像素取最近有效像素的颜色（方向自适应）----
    std::vector<uint32_t> color(n, 0); // 直通 RGB，随波前继承
    std::vector<int32_t> queue;
    for (int y = 0; y < h; ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < w; ++x)
        {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (fillMask[i] != 0 || alpha[i] <= ALPHA_TRANSPARENT_MAX)
                continue;
            dist[i] = 0;
            color[i] = unpremultiplyRgb(alpha[i], line[x]);
            bool nextToMask = false;
            for (int dy = -1; dy <= 1 && !nextToMask; ++dy)
            {
                const int ny = y + dy;
                if (ny < 0 || ny >= h)
                    continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= w)
                        continue;
                    if (fillMask[static_cast<size_t>(ny) * w + nx] != 0)
                    {
                        nextToMask = true;
                        break;
                    }
                }
            }
            if (nextToMask)
                queue.push_back(static_cast<int32_t>(i));
        }
    }

    for (size_t head = 0; head < queue.size(); ++head)
    {
        const int32_t cur = queue[head];
        const int cy = cur / w;
        const int cx = cur % w;
        for (int dy = -1; dy <= 1; ++dy)
        {
            const int ny = cy + dy;
            if (ny < 0 || ny >= h)
                continue;
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int nx = cx + dx;
                if (nx < 0 || nx >= w)
                    continue;
                const size_t ni = static_cast<size_t>(ny) * w + nx;
                if (fillMask[ni] != 0 && dist[ni] < 0)
                {
                    dist[ni] = dist[cur] + 1;
                    color[ni] = color[cur];
                    queue.push_back(static_cast<int32_t>(ni));
                }
            }
        }
    }

    // ---- 5. 写回：填充像素一律不透明（alpha=255 时预乘与直通一致）----
    int filled = 0;
    for (int y = 0; y < h; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (fillMask[i] != 0 && dist[i] > 0)
            {
                const QRgb c = color[i];
                line[x] = qRgb(qRed(c), qGreen(c), qBlue(c));
                ++filled;
            }
        }
    }
    return filled;
}
