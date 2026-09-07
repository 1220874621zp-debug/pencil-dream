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

#include "mlswarp.h"

#include <QVarLengthArray>
#include <QVector2D>
#include <QtMath>
#include <QPainter>
#include <cmath>
#include <limits>

namespace
{
    // Grid cell size (source pixels) for the forward-mapped lattice.
    const int CellSize = 16;

    qreal crossProduct(const QPointF& a, const QPointF& b)
    {
        return a.x() * b.y() - a.y() * b.x();
    }

    /** Inverse bilinear: given dst quad (A,B,D,C as A=topLeft, B=topRight,
     *  D=bottomRight, C=bottomLeft) and an axis-aligned source cell
     *  (srcBase, srcW, srcH), finds (mu, nu) in [0,1]² for dst point p
     *  relative to A. Returns false when the solve lands outside the quad. */
    bool inverseBilinear(const QPointF& a, const QPointF& c, const QPointF& d,
                         const QPointF& p, QPointF& uv)
    {
        const qreal eps = 1e-9;

        const qreal k2 = crossProduct(d, c);
        const qreal k1 = crossProduct(a, c) + crossProduct(p, d);
        const qreal k0 = crossProduct(p, a);

        qreal nu = -1;
        if (std::abs(k2) < eps)
        {
            if (std::abs(k1) < eps) { return false; }
            nu = -k0 / k1;
        }
        else
        {
            const qreal disc = k1 * k1 - 4.0 * k2 * k0;
            if (disc < 0) { return false; }
            const qreal sq = std::sqrt(disc);
            const qreal nu1 = (-k1 - sq) / (2.0 * k2);
            const qreal nu2 = (-k1 + sq) / (2.0 * k2);
            nu = (nu1 >= 0.0 && nu1 <= 1.0) ? nu1 : nu2;
        }
        if (nu < 0.0 || nu > 1.0) { return false; }

        // p = mu * (a + nu*d) + nu * c  =>  solve mu per axis, pick the
        // dominant denominator for stability.
        const qreal dx = a.x() + nu * d.x();
        const qreal dy = a.y() + nu * d.y();
        qreal mu = -1;
        if (std::abs(dx) >= std::abs(dy))
        {
            if (std::abs(dx) < eps) { return false; }
            mu = (p.x() - nu * c.x()) / dx;
        }
        else
        {
            if (std::abs(dy) < eps) { return false; }
            mu = (p.y() - nu * c.y()) / dy;
        }
        if (mu < 0.0 || mu > 1.0) { return false; }

        uv = QPointF(mu, nu);
        return true;
    }

    QRgb bilinearSample(const QImage& img, qreal x, qreal y)
    {
        const int w = img.width();
        const int h = img.height();

        x = qBound<qreal>(0.0, x, w - 1);
        y = qBound<qreal>(0.0, y, h - 1);

        const int x0 = qMin(int(x), w - 2 < 0 ? 0 : w - 2);
        const int y0 = qMin(int(y), h - 2 < 0 ? 0 : h - 2);
        const int x1 = x0 + 1;
        const int y1 = y0 + 1;

        const qreal fx = x - x0;
        const qreal fy = y - y0;

        const QRgb* line0 = reinterpret_cast<const QRgb*>(img.constScanLine(y0));
        const QRgb* line1 = reinterpret_cast<const QRgb*>(img.constScanLine(y1));

        // ARGB32_Premultiplied: channels interpolate linearly, alpha included
        const qreal p00[4] = { qRed(line0[x0]), qGreen(line0[x0]), qBlue(line0[x0]), qAlpha(line0[x0]) };
        const qreal p10[4] = { qRed(line0[x1]), qGreen(line0[x1]), qBlue(line0[x1]), qAlpha(line0[x1]) };
        const qreal p01[4] = { qRed(line1[x0]), qGreen(line1[x0]), qBlue(line1[x0]), qAlpha(line1[x0]) };
        const qreal p11[4] = { qRed(line1[x1]), qGreen(line1[x1]), qBlue(line1[x1]), qAlpha(line1[x1]) };

        int out[4];
        for (int i = 0; i < 4; i++)
        {
            const qreal top = p00[i] * (1.0 - fx) + p10[i] * fx;
            const qreal bottom = p01[i] * (1.0 - fx) + p11[i] * fx;
            out[i] = qRound(top * (1.0 - fy) + bottom * fy);
        }
        return qRgba(out[0], out[1], out[2], out[3]);
    }

    QRgb nearestSample(const QImage& img, qreal x, qreal y)
    {
        const int sx = qBound(0, qRound(x), img.width() - 1);
        const int sy = qBound(0, qRound(y), img.height() - 1);
        return reinterpret_cast<const QRgb*>(img.constScanLine(sy))[sx];
    }
}

namespace MlsWarp
{

QPointF rigidTransformMath(const QPointF& v,
                           const QVector<QPointF>& p,
                           const QVector<QPointF>& q,
                           qreal alpha)
{
    const int nbPoints = p.size();
    if (nbPoints == 0 || nbPoints != q.size()) { return v; }

    QVarLengthArray<qreal, 32> w(nbPoints);
    qreal sumWi = 0;
    QPointF pStar(0, 0), qStar(0, 0);
    QVarLengthArray<QPointF, 32> pHat(nbPoints), qHat(nbPoints);

    for (int i = 0; i < nbPoints; ++i)
    {
        if (v == p[i])
            return q[i];

        QVector2D tmp(p[i] - v);
        const qreal lenSq = tmp.lengthSquared();
        if (lenSq < 1e-12)
        {
            w[i] = std::numeric_limits<qreal>::max() / nbPoints;
        }
        else
        {
            w[i] = 1.0 / std::pow(lenSq, alpha);
        }
        pStar += w[i] * p[i];
        qStar += w[i] * q[i];
        sumWi += w[i];
    }
    pStar /= sumWi;
    qStar /= sumWi;

    QVector2D resTmp(0, 0);
    for (int i = 0; i < nbPoints; ++i)
    {
        pHat[i] = p[i] - pStar;
        qHat[i] = q[i] - qStar;

        const qreal qx = w[i] * qHat[i].x();
        const qreal qy = w[i] * qHat[i].y();
        const qreal px = pHat[i].x();
        const qreal py = pHat[i].y();

        resTmp += QVector2D(qx * px + qy * py, qx * py - qy * px);
    }

    if (resTmp.lengthSquared() < 1e-20)
    {
        return v + (qStar - pStar);
    }

    QPointF fArrow = resTmp.normalized().toPointF();
    QVector2D vMinusPStar(v - pStar);
    QPointF res(fArrow.x() * vMinusPStar.x() + fArrow.y() * vMinusPStar.y(),
                fArrow.x() * vMinusPStar.y() - fArrow.y() * vMinusPStar.x());
    res += qStar;

    return res;
}

QImage warpImage(const QImage& srcImage,
                 const QVector<QPointF>& origPoints,
                 const QVector<QPointF>& transfPoints,
                 qreal alpha,
                 bool smoothSampling,
                 QPointF* newOffset)
{
    if (srcImage.isNull() || origPoints.isEmpty()
        || origPoints.size() != transfPoints.size())
    {
        return srcImage;
    }

    if (origPoints.size() == 1)
    {
        return srcImage;
    }

    // identity shortcut
    bool identity = true;
    for (int i = 0; i < origPoints.size(); ++i)
    {
        if (origPoints[i] != transfPoints[i]) { identity = false; break; }
    }
    if (identity) { return srcImage; }

    const QImage src = (srcImage.format() == QImage::Format_ARGB32_Premultiplied)
                       ? srcImage
                       : srcImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const int srcW = src.width();
    const int srcH = src.height();
    if (srcW <= 0 || srcH <= 0) { return src; }

    const int cols = qMax(1, (srcW + CellSize - 1) / CellSize);
    const int rows = qMax(1, (srcH + CellSize - 1) / CellSize);

    // Forward-map the lattice: latticePt(x,y) maps through MLS.
    QVector<QPointF> lattice((cols + 1) * (rows + 1));
    for (int r = 0; r <= rows; ++r)
    {
        for (int c = 0; c <= cols; ++c)
        {
            const QPointF srcPt(qreal(c * srcW) / cols, qreal(r * srcH) / rows);
            lattice[r * (cols + 1) + c] = rigidTransformMath(srcPt, origPoints, transfPoints, alpha);
        }
    }

    // Destination bounds from all mapped lattice points (+ margin)
    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::lowest();
    qreal maxY = std::numeric_limits<qreal>::lowest();
    for (const QPointF& pt : lattice)
    {
        minX = qMin(minX, pt.x());
        minY = qMin(minY, pt.y());
        maxX = qMax(maxX, pt.x());
        maxY = qMax(maxY, pt.y());
    }
    const int minXi = qFloor(minX) - 1;
    const int minYi = qFloor(minY) - 1;
    const int maxXi = qCeil(maxX) + 1;
    const int maxYi = qCeil(maxY) + 1;
    minX = minXi;
    minY = minYi;
    maxX = maxXi;
    maxY = maxYi;

    QImage dst(QSize(maxXi - minXi + 1, maxYi - minYi + 1), QImage::Format_ARGB32_Premultiplied);
    dst.fill(Qt::transparent);
    if (newOffset) { *newOffset = QPointF(minXi, minYi); }

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const QPointF& A = lattice[r * (cols + 1) + c];
            const QPointF& B = lattice[r * (cols + 1) + c + 1];
            const QPointF& D = lattice[(r + 1) * (cols + 1) + c + 1];
            const QPointF& C = lattice[(r + 1) * (cols + 1) + c];

            // source cell in image coords
            const qreal srcX0 = qreal(c * srcW) / cols;
            const qreal srcY0 = qreal(r * srcH) / rows;
            const qreal srcX1 = qreal((c + 1) * srcW) / cols;
            const qreal srcY1 = qreal((r + 1) * srcH) / rows;

            const QPointF e1 = B - A; // a
            const QPointF e2 = C - A; // c(edge)
            const QPointF e3 = (D - C) - (B - A); // d = BD - AC

            const QRect cellBounds = QPolygonF({A, B, D, C}).boundingRect().toAlignedRect();

            for (int y = cellBounds.top(); y <= cellBounds.bottom(); ++y)
            {
                QRgb* dstLine = reinterpret_cast<QRgb*>(dst.scanLine(y - minYi));
                for (int x = cellBounds.left(); x <= cellBounds.right(); ++x)
                {
                    if (x < minX || x > maxX || y < minY || y > maxY) { continue; }

                    QPointF uv;
                    if (!inverseBilinear(e1, e2, e3, QPointF(x, y) - A, uv))
                        continue;

                    const qreal sx = srcX0 + uv.x() * (srcX1 - srcX0);
                    const qreal sy = srcY0 + uv.y() * (srcY1 - srcY0);

                    dstLine[x - minXi] = smoothSampling ? bilinearSample(src, sx, sy)
                                                       : nearestSample(src, sx, sy);
                }
            }
        }
    }

    return dst;
}

} // namespace MlsWarp
