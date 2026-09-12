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
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{

// alpha 低于 8 视为透明（与线稿/填色引擎的透明阈值一致）
constexpr uint8_t ALPHA_TRANSPARENT_MAX = 7;
// 闭运算方形核半径：宽度 < 2*半径+1（11px）的缝会被抓住
constexpr int CLOSE_RADIUS = 5;

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

// 沿单位方向 (dx,dy) 从 (x,y) 像素中心步进（Amanatides-Woo 体素遍历），
// 返回第一碰到的种子像素索引；走出画面仍未命中返回 -1（扫空，调用方回退）
int marchSeed(const int x, const int y, const double dx, const double dy,
              const std::vector<uint8_t>& seedMask, const int w, const int h)
{
    const double px = x + 0.5;
    const double py = y + 0.5;
    const int stepX = dx > 0.0 ? 1 : (dx < 0.0 ? -1 : 0);
    const int stepY = dy > 0.0 ? 1 : (dy < 0.0 ? -1 : 0);
    const double inf = std::numeric_limits<double>::max();
    // 除以带符号的 d：分子与 d 同号，保证步进时间为正
    double tMaxX = stepX != 0
        ? ((stepX > 0 ? x + 1.0 : static_cast<double>(x)) - px) / dx : inf;
    double tMaxY = stepY != 0
        ? ((stepY > 0 ? y + 1.0 : static_cast<double>(y)) - py) / dy : inf;
    const double tDX = stepX != 0 ? 1.0 / std::abs(dx) : inf;
    const double tDY = stepY != 0 ? 1.0 / std::abs(dy) : inf;

    int X = x;
    int Y = y;
    while (true)
    {
        if (tMaxX < tMaxY)
        {
            X += stepX;
            if (X < 0 || X >= w)
                return -1;
            tMaxX += tDX;
        }
        else
        {
            Y += stepY;
            if (Y < 0 || Y >= h)
                return -1;
            tMaxY += tDY;
        }
        const size_t idx = static_cast<size_t>(Y) * w + X;
        if (seedMask[idx] != 0)
            return static_cast<int>(idx);
    }
}

} // namespace

int HoleFiller::fillHoles(QImage& img, const int mode)
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

    // ---- 1. 镂空定位：限步测地膨胀（开运算重建），补出“真背景” ----
    // 老思路（透明区直接连到边界 = 背景）有个洞：色块中间的镂空若通过一条
    // 细缝（笔触缺口）与外界连通，会被整片判成背景——缝补上了、缝背后的
    // 镂空却留下。改为：先腐蚀透明掩码得到“宽区核心”（宽度 ≤ ~10px 的缝、
    // 细通道、小斑点被整条抹掉，其背后的镂空因此与外界断开）；再从核心
    // 出发只在透明像素内做 ≤ CLOSE_RADIUS 步的十字测地膨胀——核心区域
    // 连同自身边缘环完整补回，但走不出任何 ≥1px 的不透明墙，也不会顺着
    // 细通道爬超过 5px 深。补回的才是真背景；其余透明像素（缝本身、封闭
    // 镂空、窄通道背后的镂空、小斑点）全部进入填充范围，实现“图像区域
    // 内部没有任何镂空”。贴边 CLOSE_RADIUS 内的透明按开放处理（不封画布
    // 边上的缝，与旧闭运算的贴边保守语义一致）。
    std::vector<uint8_t> fillMask(n, 0);
    {
        std::vector<uint8_t> transMask(n);
        std::vector<uint8_t> eroded(n);
        for (size_t i = 0; i < n; ++i)
            transMask[i] = alpha[i] <= ALPHA_TRANSPARENT_MAX ? 1 : 0;
        boxMin(transMask, eroded, w, h, CLOSE_RADIUS); // min 滤波 = 透明掩码腐蚀

        // 核心的 8 连通标记；贴近边界（≤ CLOSE_RADIUS，补偿腐蚀的贴边置零）
        // 的核心连通域 = 背景核心
        std::vector<int32_t> comp(n, 0);
        std::vector<uint8_t> compIsBg(1, 0);
        std::vector<int32_t> stack;
        int32_t compCount = 0;
        for (int sy = 0; sy < h; ++sy)
        {
            for (int sx = 0; sx < w; ++sx)
            {
                const size_t s = static_cast<size_t>(sy) * w + sx;
                if (eroded[s] == 0 || comp[s] != 0)
                    continue;

                const int32_t id = ++compCount;
                compIsBg.push_back(0);
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
                    if (cx <= CLOSE_RADIUS || cx >= w - 1 - CLOSE_RADIUS
                        || cy <= CLOSE_RADIUS || cy >= h - 1 - CLOSE_RADIUS)
                    {
                        touchesBorder = true;
                    }
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
                            if (eroded[ni] != 0 && comp[ni] == 0)
                            {
                                comp[ni] = id;
                                stack.push_back(static_cast<int32_t>(ni));
                            }
                        }
                    }
                }
                compIsBg[static_cast<size_t>(id)] = touchesBorder ? 1 : 0;
            }
        }

        // 背景 = 贴边透明 ∪ 从背景核心出发、限深 CLOSE_RADIUS 步的透明域内
        // 十字测地膨胀（分层 BFS，任何不透明像素都是墙）
        std::vector<uint8_t> bgMask(n, 0);
        std::vector<int32_t> step(n, 0);
        std::vector<int32_t> queue;
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const size_t i = static_cast<size_t>(y) * w + x;
                if (transMask[i] == 0)
                    continue;
                const bool nearBorder = x <= CLOSE_RADIUS || x >= w - 1 - CLOSE_RADIUS
                    || y <= CLOSE_RADIUS || y >= h - 1 - CLOSE_RADIUS;
                const bool bgCore = eroded[i] != 0 && compIsBg[static_cast<size_t>(comp[i])] != 0;
                if (nearBorder || bgCore)
                {
                    bgMask[i] = 1;
                    step[i] = 0;
                    queue.push_back(static_cast<int32_t>(i));
                }
            }
        }
        for (size_t head = 0; head < queue.size(); ++head)
        {
            const int32_t cur = queue[head];
            if (step[cur] >= CLOSE_RADIUS)
                continue;
            const int cy = cur / w;
            const int cx = cur % w;
            const int nx4[4] = { cx - 1, cx + 1, cx, cx };
            const int ny4[4] = { cy, cy, cy - 1, cy + 1 };
            for (int k = 0; k < 4; ++k)
            {
                const int nx = nx4[k];
                const int ny = ny4[k];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h)
                    continue;
                const size_t ni = static_cast<size_t>(ny) * w + nx;
                if (transMask[ni] != 0 && bgMask[ni] == 0)
                {
                    bgMask[ni] = 1;
                    step[ni] = step[cur] + 1;
                    queue.push_back(static_cast<int32_t>(ni));
                }
            }
        }

        for (size_t i = 0; i < n; ++i)
            fillMask[i] = (transMask[i] != 0 && bgMask[i] == 0) ? 1 : 0;
    }
    std::vector<int32_t> dist(n, -1);

    // ---- 3. 多源 BFS：每个待填像素取最近有效像素的颜色（方向自适应）----
    // 填充范围 = 检测出的透明像素本身，绝不向半透明边缘膨胀：
    // 0 < alpha < 255 的抗锯齿边缘像素不进掩码、不被改写（防外溢）
    std::vector<uint32_t> color(n, 0); // 直通 RGB，随波前继承
    std::vector<uint8_t> seedMask(n, 0); // 有效参考色像素（方向模式的射线命中判定用）
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
            seedMask[i] = 1;
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

    // ---- 4b. 方向取色：所有待填像素沿用户指定方向（左/右/上/下）步进取第一参考色 ----
    if (mode == TakeLeft || mode == TakeRight || mode == TakeUp || mode == TakeDown)
    {
        double dx = 0.0, dy = 0.0;
        switch (mode)
        {
        case TakeLeft:  dx = -1.0; break;
        case TakeRight: dx = 1.0; break;
        case TakeUp:    dy = -1.0; break;
        case TakeDown:  dy = 1.0; break;
        }
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const size_t i = static_cast<size_t>(y) * w + x;
                if (fillMask[i] == 0 || dist[i] <= 0)
                    continue;
                const int hit = marchSeed(x, y, dx, dy, seedMask, w, h);
                if (hit >= 0)
                    color[i] = color[hit]; // 射线扫空（逃逸出画面）保留 BFS 最近色回退
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
