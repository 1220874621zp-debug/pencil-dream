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

#include "inbetween.h"

#include <QVector>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace
{

// 距离场条目：3-4 chamfer 距离（单位=1/3像素）+ 最近线像素坐标
struct DtEntry
{
    qint32 dist = std::numeric_limits<qint32>::max() / 4;
    qint32 srcX = -1;
    qint32 srcY = -1;
};

// 3-4 chamfer 距离变换 + 最近源点传播（两遍扫描）
void chamferWithSources(const QVector<quint8>& binary, int w, int h, QVector<DtEntry>& out)
{
    out.resize(static_cast<int>(binary.size()));

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            DtEntry& e = out[y * w + x];
            if (binary[y * w + x] != 0)
            {
                e.dist = 0;
                e.srcX = x;
                e.srcY = y;
            }
        }

    auto relax = [&](int i, int j, qint32 step)
    {
        if (out[j].dist + step < out[i].dist)
        {
            out[i].dist = out[j].dist + step;
            out[i].srcX = out[j].srcX;
            out[i].srcY = out[j].srcY;
        }
    };

    for (int y = 0; y < h; ++y)
    {
        const int row = y * w;
        for (int x = 0; x < w; ++x)
        {
            const int i = row + x;
            if (x > 0)
                relax(i, i - 1, 3);
            if (y > 0)
            {
                relax(i, i - w, 3);
                if (x > 0)
                    relax(i, i - w - 1, 4);
                if (x < w - 1)
                    relax(i, i - w + 1, 4);
            }
        }
    }

    for (int y = h - 1; y >= 0; --y)
    {
        const int row = y * w;
        for (int x = w - 1; x >= 0; --x)
        {
            const int i = row + x;
            if (x < w - 1)
                relax(i, i + 1, 3);
            if (y < h - 1)
            {
                relax(i, i + w, 3);
                if (x < w - 1)
                    relax(i, i + w + 1, 4);
                if (x > 0)
                    relax(i, i + w - 1, 4);
            }
        }
    }
}

// 3x3 盒式平滑（边界取有效邻域）
void boxBlur(QVector<qreal>& field, int w, int h)
{
    QVector<qreal> src = field;
    for (int y = 0; y < h; ++y)
    {
        const int y0 = (y > 0 ? y - 1 : y) * w;
        const int y1 = y * w;
        const int y2 = (y < h - 1 ? y + 1 : y) * w;
        for (int x = 0; x < w; ++x)
        {
            const int x0 = x > 0 ? x - 1 : x;
            const int x2 = x < w - 1 ? x + 1 : x;
            field[y1 + x] = (src[y0 + x0] + src[y0 + x] + src[y0 + x2] +
                             src[y1 + x0] + src[y1 + x] + src[y1 + x2] +
                             src[y2 + x0] + src[y2 + x] + src[y2 + x2]) / 9.0;
        }
    }
}

// 移除面积小于 minArea 的连通域（8连通，在 alpha 场上操作）
void denoiseField(QVector<qreal>& alpha, int w, int h, int minArea)
{
    const int n = w * h;
    QVector<quint8> visited(n, 0);
    QVector<int> stack;
    QVector<int> component;

    for (int start = 0; start < n; ++start)
    {
        if (alpha[start] <= 0.5 || visited[start] != 0)
            continue;

        stack.clear();
        component.clear();
        stack.append(start);
        visited[start] = 1;
        while (!stack.isEmpty())
        {
            const int i = stack.takeLast();
            component.append(i);
            const int x = i % w;
            const int y = i / w;
            for (int dy = -1; dy <= 1; ++dy)
            {
                const int ny = y + dy;
                if (ny < 0 || ny >= h)
                    continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int nx = x + dx;
                    if (nx < 0 || nx >= w)
                        continue;
                    const int j = ny * w + nx;
                    if (alpha[j] > 0.5 && visited[j] == 0)
                    {
                        visited[j] = 1;
                        stack.append(j);
                    }
                }
            }
        }

        if (component.size() < minArea)
        {
            for (int i : component)
                alpha[i] = 0.0;
        }
    }
}

QVector<quint8> binarize(const QImage& image)
{
    const int w = image.width();
    QVector<quint8> binary(static_cast<size_t>(w) * image.height());
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.scanLine(y));
        for (int x = 0; x < w; ++x)
            binary[static_cast<int>(y) * w + x] = (qAlpha(line[x]) > 16) ? 1 : 0;
    }
    return binary;
}

} // namespace

namespace Inbetween
{

QImage interpolate(const QImage& a, const QImage& b, qreal t, const Options& options)
{
    if (a.size() != b.size() || a.width() == 0 || a.height() == 0)
        return QImage();

    t = qBound(0.0, t, 1.0);
    const int w = a.width();
    const int h = a.height();

    const QImage ia = (a.format() == QImage::Format_ARGB32_Premultiplied) ? a : a.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage ib = (b.format() == QImage::Format_ARGB32_Premultiplied) ? b : b.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // 每像素到对面线稿最近笔画的距离与最近点（用于对应点位移）
    QVector<DtEntry> nearB, nearA;
    chamferWithSources(binarize(ib), w, h, nearB);
    chamferWithSources(binarize(ia), w, h, nearA);

    // 中间帧线条 = 双向对应点的 t 位移处撒圆盘：
    //   A 的线像素 a → a + t·(nearestB(a) − a)
    //   B 的线像素 b → b + (1−t)·(nearestA(b) − b)
    // 两图重合时位移为零（中间帧=原图）；平移时落在比例位置；
    // 无"无符号距离场相加插值"在两线之间恒为常数的退化问题。
    QVector<qreal> alpha(static_cast<size_t>(w) * h, 0.0);
    const qreal radius = qMax(0.6, options.epsilon);

    auto stamp = [&](qreal cx, qreal cy)
    {
        const int x0 = qMax(0, int(cx - radius) - 1);
        const int x1 = qMin(w - 1, int(cx + radius) + 1);
        const int y0 = qMax(0, int(cy - radius) - 1);
        const int y1 = qMin(h - 1, int(cy + radius) + 1);
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                const qreal d = qSqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                const qreal value = qBound(0.0, radius + 0.5 - d, 1.0);
                qreal& slot = alpha[y * w + x];
                if (value > slot)
                    slot = value;
            }
        }
    };

    for (int y = 0; y < h; ++y)
    {
        const QRgb* lineA = reinterpret_cast<const QRgb*>(ia.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            if (qAlpha(lineA[x]) <= 16)
                continue;
            const DtEntry& e = nearB[y * w + x];
            if (e.srcX < 0)
                continue;
            stamp(x + t * (e.srcX - x), y + t * (e.srcY - y));
        }
    }

    for (int y = 0; y < h; ++y)
    {
        const QRgb* lineB = reinterpret_cast<const QRgb*>(ib.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            if (qAlpha(lineB[x]) <= 16)
                continue;
            const DtEntry& e = nearA[y * w + x];
            if (e.srcX < 0)
                continue;
            stamp(x + (1.0 - t) * (e.srcX - x), y + (1.0 - t) * (e.srcY - y));
        }
    }

    for (int pass = 0; pass < options.blurPasses; ++pass)
        boxBlur(alpha, w, h);

    if (options.denoiseArea > 0)
        denoiseField(alpha, w, h, options.denoiseArea);

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    const QColor stroke = options.strokeColor;
    const int sr = stroke.red(), sg = stroke.green(), sb = stroke.blue();
    for (int y = 0; y < h; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        const int row = y * w;
        for (int x = 0; x < w; ++x)
        {
            const int al = qRound(alpha[row + x] * 255);
            if (al <= 0)
                line[x] = 0;
            else
                line[x] = qRgba(sr * al / 255, sg * al / 255, sb * al / 255, al);
        }
    }
    return out;
}

} // namespace Inbetween
