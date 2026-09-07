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
#include <QtNumeric>

namespace
{
    // Grid cell size (source pixels) for forward-mapped lattices
    const int CellSize = 16;

    qreal cross(const QPointF& a, const QPointF& b)
    {
        return a.x() * b.y() - a.y() * b.x();
    }

    /** Inverse bilinear: dst quad (A=topLeft, B=topRight, D=bottomRight,
     *  C=bottomLeft) with edge vectors e1=AB, e2=AC, e3=BD-AC, and an
     *  axis-aligned source cell. Finds (mu, nu) in [0,1]^2 for dst point p
     *  relative to A; false when outside the quad. */
    bool inverseBilinear(const QPointF& a, const QPointF& c, const QPointF& d,
                         const QPointF& p, QPointF& uv)
    {
        const qreal eps = 1e-9;

        const qreal k2 = cross(d, c);
        const qreal k1 = cross(a, c) + cross(p, d);
        const qreal k0 = cross(p, a);

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

    QRgb sampleImage(const QImage& img, qreal x, qreal y, bool smooth)
    {
        return smooth ? bilinearSample(img, x, y) : nearestSample(img, x, y);
    }

    struct LatticeGeometry
    {
        int cols = 0;
        int rows = 0;
        QVector<QPointF> points; // (cols+1)*(rows+1), row-major
    };

    LatticeGeometry makeLattice(int srcW, int srcH)
    {
        LatticeGeometry g;
        g.cols = qMax(1, (srcW + CellSize - 1) / CellSize);
        g.rows = qMax(1, (srcH + CellSize - 1) / CellSize);
        for (int r = 0; r <= g.rows; ++r)
        {
            for (int c = 0; c <= g.cols; ++c)
            {
                g.points << QPointF(qreal(c * srcW) / g.cols, qreal(r * srcH) / g.rows);
            }
        }
        return g;
    }

    /** Forward-grid renderer: warps src by mapping its lattice points to
     *  `mapped` (same layout as makeLattice). Shared by MLS warp and cage. */
    QImage renderWarpedLattice(const QImage& srcImage,
                               const LatticeGeometry& lattice,
                               const QVector<QPointF>& mapped,
                               bool smoothSampling,
                               QPointF* newOffset)
    {
        const QImage src = (srcImage.format() == QImage::Format_ARGB32_Premultiplied)
                           ? srcImage
                           : srcImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        const int srcW = src.width();
        const int srcH = src.height();
        if (srcW <= 0 || srcH <= 0) { return src; }

        const int cols = lattice.cols;
        const int rows = lattice.rows;

        qreal minX = std::numeric_limits<qreal>::max();
        qreal minY = std::numeric_limits<qreal>::max();
        qreal maxX = std::numeric_limits<qreal>::lowest();
        qreal maxY = std::numeric_limits<qreal>::lowest();
        for (const QPointF& pt : mapped)
        {
            // NaN from degenerate math near a control point collapses to 0
            // in the min/max fold below, so it cannot poison the bounds
            minX = qMin(minX, qIsFinite(pt.x()) ? pt.x() : 0.0);
            minY = qMin(minY, qIsFinite(pt.y()) ? pt.y() : 0.0);
            maxX = qMax(maxX, qIsFinite(pt.x()) ? pt.x() : 0.0);
            maxY = qMax(maxY, qIsFinite(pt.y()) ? pt.y() : 0.0);
        }
        int minXi = qFloor(minX) - 1;
        int minYi = qFloor(minY) - 1;
        int maxXi = qCeil(maxX) + 1;
        int maxYi = qCeil(maxY) + 1;

        // sanity clamp: a degenerate lattice must not allocate gigabytes
        const int kMaxExtent = 65536;
        minXi = qMax(minXi, -kMaxExtent);
        minYi = qMax(minYi, -kMaxExtent);
        maxXi = qMin(maxXi, minXi + kMaxExtent);
        maxYi = qMin(maxYi, minYi + kMaxExtent);

        QImage dst(QSize(maxXi - minXi + 1, maxYi - minYi + 1), QImage::Format_ARGB32_Premultiplied);
        if (dst.isNull()) { return src; }
        dst.fill(Qt::transparent);
        if (newOffset) { *newOffset = QPointF(minXi, minYi); }

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                const QPointF& A = mapped[r * (cols + 1) + c];
                const QPointF& B = mapped[r * (cols + 1) + c + 1];
                const QPointF& D = mapped[(r + 1) * (cols + 1) + c + 1];
                const QPointF& C = mapped[(r + 1) * (cols + 1) + c];

                const qreal srcX0 = qreal(c * srcW) / cols;
                const qreal srcY0 = qreal(r * srcH) / rows;
                const qreal srcX1 = qreal((c + 1) * srcW) / cols;
                const qreal srcY1 = qreal((r + 1) * srcH) / rows;

                if (!qIsFinite(A.x()) || !qIsFinite(A.y()) || !qIsFinite(B.x()) || !qIsFinite(B.y()) ||
                    !qIsFinite(C.x()) || !qIsFinite(C.y()) || !qIsFinite(D.x()) || !qIsFinite(D.y()))
                {
                    continue; // degenerate cell: drop it
                }

                const QPointF e1 = B - A;
                const QPointF e2 = C - A;
                const QPointF e3 = (D - C) - (B - A);

                const QRect cellBounds = QPolygonF({A, B, D, C}).boundingRect().toAlignedRect();

                for (int y = cellBounds.top(); y <= cellBounds.bottom(); ++y)
                {
                    if (y < minYi || y > maxYi) { continue; }
                    QRgb* dstLine = reinterpret_cast<QRgb*>(dst.scanLine(y - minYi));
                    for (int x = cellBounds.left(); x <= cellBounds.right(); ++x)
                    {
                        if (x < minXi || x > maxXi) { continue; }

                        QPointF uv;
                        if (!inverseBilinear(e1, e2, e3, QPointF(x, y) - A, uv))
                            continue;

                        const qreal sx = srcX0 + uv.x() * (srcX1 - srcX0);
                        const qreal sy = srcY0 + uv.y() * (srcY1 - srcY0);

                        dstLine[x - minXi] = sampleImage(src, sx, sy, smoothSampling);
                    }
                }
            }
        }

        return dst;
    }

    // ---- Green Coordinates (faithful port of KisGreenCoordinatesMath) ----
    qreal dotProduct(const QPointF& a, const QPointF& b)
    {
        return a.x() * b.x() + a.y() * b.y();
    }

    qreal normLength(const QPointF& a)
    {
        return std::hypot(a.x(), a.y());
    }

    QPointF leftUnitNormal(const QPointF& a)
    {
        QPointF result = (a.x() != 0.0) ? QPointF(-a.y() / a.x(), 1.0) : QPointF(-1.0, 0.0);
        const qreal length = normLength(result);
        result *= ((cross(a, result) >= 0) ? 1.0 : -1.0) / length;
        return -result;
    }

    int polygonDirection(const QVector<QPointF>& polygon)
    {
        qreal doubleSum = 0;
        const int numPoints = polygon.size();
        for (int i = 1; i <= numPoints; i++)
        {
            const int prev = i - 1;
            const int next = (i == numPoints) ? 0 : i;
            doubleSum += (polygon[next].x() - polygon[prev].x()) * (polygon[next].y() + polygon[prev].y());
        }
        return doubleSum >= 0 ? 1 : -1;
    }

    struct GreenCage
    {
        struct Coords
        {
            QVector<qreal> psi; // per edge
            QVector<qreal> phi; // per vertex
        };

        QVector<qreal> originalCageEdgeSizes;
        QVector<Coords> precalc;
        int cageDirection = 0;

        static void precalculateOneEdge(const QPointF& pt, const QPointF& v1, const QPointF& v2,
                                        qreal* edgePsi, qreal* vertex1Phi, qreal* vertex2Phi,
                                        int dir)
        {
            const QPointF a = v2 - v1;
            const QPointF b = v1 - pt;
            const qreal Q = dotProduct(a, a);
            const qreal S = dotProduct(b, b);
            const qreal R = dotProduct(2.0 * a, b);
            if (Q <= 0.0) { return; }

            const QPointF unitA = a / normLength(a);
            const QPointF inward = dir * QPointF(-unitA.y(), unitA.x());
            const qreal BA = dotProduct(b, inward);
            const qreal SRT = std::sqrt(4.0 * S * Q - R * R);
            if (SRT <= 0.0 || S <= 0.0 || S + Q + R <= 0.0) { return; }

            const qreal L0 = std::log(S);
            const qreal L1 = std::log(S + Q + R);
            const qreal A0 = std::atan(R / SRT) / SRT;
            const qreal A1 = std::atan((2.0 * Q + R) / SRT) / SRT;
            const qreal A10 = A1 - A0;
            const qreal L10 = L1 - L0;

            // Krita flips the psi sign relative to the paper ("magicMultiplier")
            static const qreal magicMultiplier = -1.0;

            *edgePsi += -magicMultiplier * normLength(a) / (4.0 * M_PI) *
                ((4.0 * S - (R * R) / Q) * A10 + R / (2.0 * Q) * L10 + L1 - 2.0);

            *vertex2Phi += -BA / (2.0 * M_PI) * (L10 / (2.0 * Q) - A10 * R / Q);
            *vertex1Phi += BA / (2.0 * M_PI) * (L10 / (2.0 * Q) - A10 * (2.0 + R / Q));
        }

        void build(const QVector<QPointF>& originalCage, const QVector<QPointF>& points)
        {
            cageDirection = polygonDirection(originalCage);
            const int numCagePoints = originalCage.size();

            originalCageEdgeSizes.resize(numCagePoints);
            for (int i = 1; i <= numCagePoints; i++)
            {
                const int endIndex = (i != numCagePoints) ? i : 0;
                originalCageEdgeSizes[i - 1] =
                    normLength(originalCage[endIndex] - originalCage[i - 1]);
            }

            precalc.resize(points.size());
            for (int i = 0; i < points.size(); i++)
            {
                precalc[i].psi.resize(numCagePoints);
                precalc[i].phi.resize(numCagePoints);
                for (int e = 1; e <= numCagePoints; e++)
                {
                    const int endIndex = (e != numCagePoints) ? e : 0;
                    precalculateOneEdge(points[i], originalCage[e - 1], originalCage[endIndex],
                                        &precalc[i].psi[e - 1],
                                        &precalc[i].phi[e - 1],
                                        &precalc[i].phi[endIndex],
                                        cageDirection);
                }
            }
        }

        QPointF transformedPoint(int pointIndex, const QVector<QPointF>& transformedCage) const
        {
            const int numCagePoints = transformedCage.size();
            const int dir = polygonDirection(transformedCage);

            QPointF result;
            for (int i = 0; i < numCagePoints; i++)
            {
                result += precalc[pointIndex].phi[i] * transformedCage[i];

                const QPointF edge = transformedCage[(i + 1) % numCagePoints] - transformedCage[i];
                const qreal scaleCoeff = normLength(edge) / originalCageEdgeSizes[i];
                result += precalc[pointIndex].psi[i] * (scaleCoeff * dir * leftUnitNormal(edge));
            }
            return result;
        }
    };
}

namespace MlsWarp
{

QPointF warpTransformMath(WarpType type,
                          const QPointF& v,
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
        {
            // exact hit: Krita returns the moved point directly
            return q[i];
        }

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

    if (type == WarpType::Affine)
    {
        qreal A0 = 0, A1 = 0, A3 = 0;
        for (int i = 0; i < nbPoints; ++i)
        {
            pHat[i] = p[i] - pStar;
            qHat[i] = q[i] - qStar;
            A0 += w[i] * pHat[i].x() * pHat[i].x();
            A3 += w[i] * pHat[i].y() * pHat[i].y();
            A1 += w[i] * pHat[i].x() * pHat[i].y();
        }
        const qreal A2 = A1;
        const qreal det = A0 * A3 - A1 * A2;
        if (std::abs(det) < 1e-20) { return v; }

        const qreal inv0 = A3 / det;
        const qreal inv1 = -A1 / det;
        const qreal inv3 = A0 / det;

        const QPointF t = v - pStar;
        const qreal ax = t.x() * inv0 + t.y() * inv1;
        const qreal ay = t.x() * inv1 + t.y() * inv3;

        QPointF res = qStar;
        for (int j = 0; j < nbPoints; ++j)
        {
            const qreal Aj = ax * pHat[j].x() + ay * pHat[j].y();
            res += w[j] * Aj * qHat[j];
        }
        return res;
    }

    if (type == WarpType::Similitude)
    {
        qreal mu_s = 0;
        QPointF resTmp(0, 0);
        for (int i = 0; i < nbPoints; ++i)
        {
            pHat[i] = p[i] - pStar;
            qHat[i] = q[i] - qStar;

            mu_s += w[i] * dotProduct(pHat[i], pHat[i]);

            const qreal qx = w[i] * qHat[i].x();
            const qreal qy = w[i] * qHat[i].y();

            resTmp += QPointF(qx * pHat[i].x() + qy * pHat[i].y(),
                              qx * pHat[i].y() - qy * pHat[i].x());
        }
        if (mu_s < 1e-20) { return v + (qStar - pStar); }
        resTmp /= mu_s;

        const QPointF vDiff = v - pStar;
        return QPointF(resTmp.x() * vDiff.x() + resTmp.y() * vDiff.y(),
                       resTmp.x() * vDiff.y() - resTmp.y() * vDiff.x()) + qStar;
    }

    // rigid
    QVector2D resTmp(0, 0);
    for (int i = 0; i < nbPoints; ++i)
    {
        pHat[i] = p[i] - pStar;
        qHat[i] = q[i] - qStar;

        const qreal qx = w[i] * qHat[i].x();
        const qreal qy = w[i] * qHat[i].y();

        resTmp += QVector2D(qx * pHat[i].x() + qy * pHat[i].y(),
                            qx * pHat[i].y() - qy * pHat[i].x());
    }

    if (resTmp.lengthSquared() < 1e-20)
    {
        return v + (qStar - pStar);
    }

    const QPointF fArrow = resTmp.normalized().toPointF();
    const QVector2D vDiff(v - pStar);
    return QPointF(fArrow.x() * vDiff.x() + fArrow.y() * vDiff.y(),
                   fArrow.x() * vDiff.y() - fArrow.y() * vDiff.x()) + qStar;
}

QImage warpImage(const QImage& srcImage,
                 const QVector<QPointF>& origPoints,
                 const QVector<QPointF>& transfPoints,
                 qreal alpha,
                 bool smoothSampling,
                 QPointF* newOffset,
                 WarpType type)
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

    bool identity = true;
    for (int i = 0; i < origPoints.size(); ++i)
    {
        if (origPoints[i] != transfPoints[i]) { identity = false; break; }
    }
    if (identity) { return srcImage; }

    const LatticeGeometry lattice = makeLattice(srcImage.width(), srcImage.height());

    QVector<QPointF> mapped;
    mapped.reserve(lattice.points.size());
    for (const QPointF& pt : lattice.points)
    {
        mapped << warpTransformMath(type, pt, origPoints, transfPoints, alpha);
    }

    return renderWarpedLattice(srcImage, lattice, mapped, smoothSampling, newOffset);
}

QImage cageWarpImage(const QImage& srcImage,
                     const QVector<QPointF>& origCage,
                     const QVector<QPointF>& transfCage,
                     bool smoothSampling,
                     QPointF* newOffset)
{
    if (srcImage.isNull() || origCage.size() < 3
        || origCage.size() != transfCage.size())
    {
        return srcImage;
    }

    bool identity = true;
    for (int i = 0; i < origCage.size(); ++i)
    {
        if (origCage[i] != transfCage[i]) { identity = false; break; }
    }
    if (identity) { return srcImage; }

    const LatticeGeometry lattice = makeLattice(srcImage.width(), srcImage.height());

    // precalculate green coordinates for lattice points inside the cage,
    // points outside stay fixed (KisCageTransformWorker behavior)
    const QPolygonF cagePoly(origCage);

    QVector<int> pointIndex(lattice.points.size(), -1);
    QVector<QPointF> innerPoints;
    for (int i = 0; i < lattice.points.size(); i++)
    {
        if (cagePoly.containsPoint(lattice.points[i], Qt::OddEvenFill))
        {
            pointIndex[i] = innerPoints.size();
            innerPoints << lattice.points[i];
        }
    }

    GreenCage green;
    if (!innerPoints.isEmpty())
    {
        green.build(origCage, innerPoints);
    }

    QVector<QPointF> mapped;
    mapped.reserve(lattice.points.size());
    for (int i = 0; i < lattice.points.size(); i++)
    {
        if (pointIndex[i] >= 0)
        {
            mapped << green.transformedPoint(pointIndex[i], transfCage);
        }
        else
        {
            mapped << lattice.points[i];
        }
    }

    return renderWarpedLattice(srcImage, lattice, mapped, smoothSampling, newOffset);
}

void liquifyDab(QImage& workImage,
                const QImage& original,
                const QPointF& base,
                qreal sigma,
                LiquifyOp op,
                const QPointF& dabVector,
                qreal amount)
{
    if (workImage.isNull() || sigma <= 0) { return; }

    const int w = workImage.width();
    const int h = workImage.height();

    const qreal maxDist = 3.0 * sigma;
    const int x0 = qMax(0, qFloor(base.x() - maxDist));
    const int y0 = qMax(0, qFloor(base.y() - maxDist));
    const int x1 = qMin(w - 1, qCeil(base.x() + maxDist));
    const int y1 = qMin(h - 1, qCeil(base.y() + maxDist));
    if (x0 > x1 || y0 > y1) { return; }

    const qreal inv2SigmaSq = 1.0 / (2.0 * sigma * sigma);
    const qreal angle = 2.0 * M_PI * amount;

    if (op == LiquifyOp::Undo)
    {
        if (original.isNull()) { return; }
        for (int y = y0; y <= y1; ++y)
        {
            QRgb* workLine = reinterpret_cast<QRgb*>(workImage.scanLine(y));
            const QRgb* origLine = reinterpret_cast<const QRgb*>(original.constScanLine(y));
            for (int x = x0; x <= x1; ++x)
            {
                const QPointF diff = QPointF(x, y) - base;
                const qreal lambda = std::exp(-(diff.x() * diff.x() + diff.y() * diff.y()) * inv2SigmaSq);
                if (lambda < 1e-4) { continue; }

                // blend back toward the pre-stroke image
                const qreal t = qBound<qreal>(0.0, amount * lambda, 1.0);
                const QRgb a = workLine[x];
                const QRgb b = origLine[x];
                workLine[x] = qRgba(qRound(qRed(a) + (qRed(b) - qRed(a)) * t),
                                    qRound(qGreen(a) + (qGreen(b) - qGreen(a)) * t),
                                    qRound(qBlue(a) + (qBlue(b) - qBlue(a)) * t),
                                    qRound(qAlpha(a) + (qAlpha(b) - qAlpha(a)) * t));
            }
        }
        return;
    }

    // geometric ops: inverse mapping by fixed-point iteration
    // (the displacement field is smooth, 4 iterations converge comfortably)
    for (int y = y0; y <= y1; ++y)
    {
        QRgb* workLine = reinterpret_cast<QRgb*>(workImage.scanLine(y));
        for (int x = x0; x <= x1; ++x)
        {
            const QPointF dstPt(x, y);
            QPointF srcPt = dstPt;

            for (int it = 0; it < 4; ++it)
            {
                const QPointF diff = srcPt - base;
                const qreal lambda = std::exp(-(diff.x() * diff.x() + diff.y() * diff.y()) * inv2SigmaSq);

                switch (op)
                {
                case LiquifyOp::Move:
                case LiquifyOp::Offset:
                    srcPt = dstPt - lambda * dabVector;
                    break;
                case LiquifyOp::Scale:
                {
                    const qreal s = 1.0 + amount * lambda;
                    srcPt = base + (dstPt - base) / (std::abs(s) < 0.01 ? 0.01 : s);
                    break;
                }
                case LiquifyOp::Rotate:
                {
                    const qreal a = -angle * lambda;
                    const QPointF r = dstPt - base;
                    srcPt = base + QPointF(std::cos(a) * r.x() - std::sin(a) * r.y(),
                                           std::sin(a) * r.x() + std::cos(a) * r.y());
                    break;
                }
                default:
                    break;
                }
            }

            workLine[x] = bilinearSample(workImage, srcPt.x(), srcPt.y());
        }
    }
}

QImage perspectiveWarpImage(const QImage& srcImage,
                            const QPolygonF& dstQuad,
                            bool smoothSampling,
                            QPointF* newOffset)
{
    if (srcImage.isNull() || dstQuad.size() != 4) { return srcImage; }

    QPolygonF srcQuad;
    srcQuad << QPointF(0, 0)
            << QPointF(srcImage.width(), 0)
            << QPointF(srcImage.width(), srcImage.height())
            << QPointF(0, srcImage.height());

    QTransform t;
    if (!QTransform::quadToQuad(srcQuad, dstQuad, t))
    {
        return srcImage;
    }

    const QImage result = srcImage.transformed(
        t, smoothSampling ? Qt::SmoothTransformation : Qt::FastTransformation);

    if (newOffset)
    {
        *newOffset = t.map(QRectF(QPointF(0, 0), QSizeF(srcImage.size()))).boundingRect().topLeft();
    }
    return result;
}

} // namespace MlsWarp
