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
#ifndef BRUSHCURVE_H
#define BRUSHCURVE_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtMath>
#include <algorithm>

/**
 * 笔刷动态映射曲线（压感 → 大小/不透明度比例）。
 * 参考 Krita KisCubicCurve 的轻量实现：
 *   - 控制点 + 单调三次样条（Fritsch–Carlson，不会过冲）
 *   - 预烘焙 256 项查找表，取值 O(1)
 *   - 字符串序列化 "x,y;x,y;..."，可直接存进预设 XML
 */
class BrushCurve
{
public:
    BrushCurve()
    {
        setPoints({ QPointF(0.0, 0.0), QPointF(1.0, 1.0) });
    }

    void setPoints(const QList<QPointF>& points)
    {
        QList<QPointF> sorted = points;
        std::sort(sorted.begin(), sorted.end(),
                  [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });

        // 去掉重复横坐标并把数值夹进 0..1
        QList<QPointF> clean;
        for (const QPointF& p : sorted) {
            QPointF c(qBound(0.0, p.x(), 1.0), qBound(0.0, p.y(), 1.0));
            if (clean.isEmpty() || c.x() - clean.last().x() > 1e-6) {
                clean.append(c);
            }
        }
        if (clean.isEmpty()) {
            clean.append(QPointF(0.0, 0.0));
        }
        if (clean.first().x() > 0.0) {
            clean.prepend(QPointF(0.0, clean.first().y()));
        }
        if (clean.last().x() < 1.0) {
            clean.append(QPointF(1.0, clean.last().y()));
        }
        mPoints = clean;
        bakeLookupTable();
    }

    const QList<QPointF>& points() const { return mPoints; }

    bool isIdentity() const
    {
        return mPoints.size() == 2
               && qFuzzyCompare(mPoints.first().x(), 0.0)
               && qFuzzyCompare(mPoints.first().y(), 0.0)
               && qFuzzyCompare(mPoints.last().x(), 1.0)
               && qFuzzyCompare(mPoints.last().y(), 1.0);
    }

    qreal value(qreal x) const
    {
        if (x <= 0.0) return mLUT.first();
        if (x >= 1.0) return mLUT.last();
        const qreal pos = x * (LUT_SIZE - 1);
        const int i = int(pos);
        const qreal frac = pos - i;
        return mLUT[i] + (mLUT[i + 1] - mLUT[i]) * frac;
    }

    QString toString() const
    {
        QString out;
        for (const QPointF& p : mPoints) {
            out += QString::number(p.x(), 'f', 4) + "," + QString::number(p.y(), 'f', 4) + ";";
        }
        return out;
    }

    static BrushCurve fromString(const QString& str)
    {
        QList<QPointF> pts;
        const QStringList tokens = str.split(';', Qt::SkipEmptyParts);
        for (const QString& token : tokens) {
            const QStringList xy = token.split(',');
            if (xy.size() == 2) {
                pts.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
            }
        }
        if (pts.size() < 2) {
            return BrushCurve();
        }
        BrushCurve curve;
        curve.setPoints(pts);
        return curve;
    }

private:
    enum { LUT_SIZE = 256 };

    void bakeLookupTable()
    {
        const int n = mPoints.size();
        QVector<qreal> dx(n - 1), slope(n - 1), tangent(n);

        for (int i = 0; i < n - 1; ++i) {
            dx[i] = mPoints[i + 1].x() - mPoints[i].x();
            const qreal dy = mPoints[i + 1].y() - mPoints[i].y();
            slope[i] = (dx[i] > 1e-9) ? dy / dx[i] : 0.0;
        }

        // Fritsch–Carlson 切线：保证样条不过冲（单调区间保持单调）
        tangent[0] = slope[0];
        tangent[n - 1] = slope[n - 2];
        for (int i = 1; i < n - 1; ++i) {
            if (slope[i - 1] * slope[i] <= 0.0) {
                tangent[i] = 0.0;
            } else {
                const qreal w1 = 2.0 * dx[i] + dx[i - 1];
                const qreal w2 = dx[i] + 2.0 * dx[i - 1];
                tangent[i] = (w1 + w2) / (w1 / slope[i - 1] + w2 / slope[i]);
            }
        }

        mLUT.resize(LUT_SIZE);
        for (int i = 0; i < LUT_SIZE; ++i) {
            const qreal x = qreal(i) / (LUT_SIZE - 1);

            int seg = 0;
            while (seg < n - 2 && mPoints[seg + 1].x() < x) {
                ++seg;
            }

            const QPointF& p0 = mPoints[seg];
            const QPointF& p1 = mPoints[seg + 1];
            const qreal h = p1.x() - p0.x();
            if (h <= 1e-9) {
                mLUT[i] = p1.y();
                continue;
            }
            const qreal t = (x - p0.x()) / h;
            const qreal t2 = t * t;
            const qreal t3 = t2 * t;
            const qreal h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
            const qreal h10 = t3 - 2.0 * t2 + t;
            const qreal h01 = -2.0 * t3 + 3.0 * t2;
            const qreal h11 = t3 - t2;
            mLUT[i] = h00 * p0.y() + h10 * h * tangent[seg]
                      + h01 * p1.y() + h11 * h * tangent[seg + 1];
        }
    }

    QList<QPointF> mPoints;
    QVector<qreal> mLUT;
};

#endif // BRUSHCURVE_H
