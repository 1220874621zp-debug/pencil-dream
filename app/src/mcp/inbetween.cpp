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

// 3-4 chamfer 距离变换（单位=1/3 像素；水平/垂直步长3，对角步长4）
void chamferDistance(const QVector<quint8>& binary, int w, int h, QVector<qint32>& out)
{
    const qint32 INF = std::numeric_limits<qint32>::max() / 4;
    out.fill(INF);

    const int n = w * h;
    for (int i = 0; i < n; ++i)
        if (binary[i] != 0)
            out[i] = 0;

    for (int y = 0; y < h; ++y)
    {
        const int row = y * w;
        for (int x = 0; x < w; ++x)
        {
            const int i = row + x;
            qint32 d = out[i];
            if (d == 0)
                continue;
            if (x > 0)
                d = std::min(d, out[i - 1] + 3);
            if (y > 0)
            {
                d = std::min(d, out[i - w] + 3);
                if (x > 0)
                    d = std::min(d, out[i - w - 1] + 4);
                if (x < w - 1)
                    d = std::min(d, out[i - w + 1] + 4);
            }
            out[i] = d;
        }
    }

    for (int y = h - 1; y >= 0; --y)
    {
        const int row = y * w;
        for (int x = w - 1; x >= 0; --x)
        {
            const int i = row + x;
            qint32 d = out[i];
            if (d == 0)
                continue;
            if (x < w - 1)
                d = std::min(d, out[i + 1] + 3);
            if (y < h - 1)
            {
                d = std::min(d, out[i + w] + 3);
                if (x < w - 1)
                    d = std::min(d, out[i + w + 1] + 4);
                if (x > 0)
                    d = std::min(d, out[i + w - 1] + 4);
            }
            out[i] = d;
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

// 移除面积小于 minArea 的连通域（8连通，直接在 alpha mask 上操作）
void denoiseMask(QVector<quint8>& mask, int w, int h, int minArea)
{
    const int n = w * h;
    QVector<quint8> visited(n, 0);
    QVector<int> stack;
    QVector<int> component;

    for (int start = 0; start < n; ++start)
    {
        if (mask[start] == 0 || visited[start] != 0)
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
                    if (mask[j] != 0 && visited[j] == 0)
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
                mask[i] = 0;
        }
    }
}

QVector<quint8> binarize(const QImage& image)
{
    const int n = image.width() * image.height();
    QVector<quint8> binary(n);
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x)
            binary[y * image.width() + x] = (qAlpha(line[x]) > 16) ? 1 : 0;
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
    const int n = w * h;

    const QImage ia = (a.format() == QImage::Format_ARGB32_Premultiplied) ? a : a.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage ib = (b.format() == QImage::Format_ARGB32_Premultiplied) ? b : b.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QVector<qint32> distA(n), distB(n);
    chamferDistance(binarize(ia), w, h, distA);
    chamferDistance(binarize(ib), w, h, distB);

    // 距离场插值（换算回像素单位）
    QVector<qreal> mid(n);
    for (int i = 0; i < n; ++i)
        mid[i] = ((1.0 - t) * distA[i] + t * distB[i]) / 3.0;

    for (int pass = 0; pass < options.blurPasses; ++pass)
        boxBlur(mid, w, h);

    // 等值带 → alpha（0.75px 抗锯齿过渡带）
    const qreal eps = qMax(0.5, options.epsilon);
    const qreal aa = 0.75;
    QVector<quint8> mask(n);
    QVector<qreal> alpha(n);
    for (int i = 0; i < n; ++i)
    {
        alpha[i] = qBound(0.0, (eps + aa - mid[i]) / (2.0 * aa), 1.0);
        mask[i] = (alpha[i] > 0.5) ? 1 : 0;
    }

    if (options.denoiseArea > 0)
        denoiseMask(mask, w, h, options.denoiseArea);

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    const QRgb stroke = qPremultiply(options.strokeColor.rgba());
    const int sr = qRed(stroke), sg = qGreen(stroke), sb = qBlue(stroke);
    for (int y = 0; y < h; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        const int row = y * w;
        for (int x = 0; x < w; ++x)
        {
            const int i = row + x;
            if (mask[i] == 0)
            {
                line[x] = 0;
            }
            else
            {
                const int al = qRound(alpha[i] * 255);
                // 预乘格式直接按比例缩放笔画色
                line[x] = qRgba(sr * al / 255, sg * al / 255, sb * al / 255, al);
            }
        }
    }
    return out;
}

} // namespace Inbetween
