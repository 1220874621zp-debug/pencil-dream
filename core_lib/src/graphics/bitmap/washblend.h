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
#ifndef WASHBLEND_H
#define WASHBLEND_H

#include <QImage>
#include <QPointF>
#include <QtMath>

/**
 * Krita wash 语义的图像盖章：逐像素取"更不透明"的一方（预乘 ARGB32）。
 * QPainter 的 CompositionMode_Lighten 只对颜色取 max、alpha 仍按
 * SourceOver 累积（重叠会变深），做不出 wash，故手写像素混合。
 * 同一笔内半透明 dab 重叠不叠加变深、交叉不留洞。
 *
 * dab 落点按亚像素坐标做双线性 4 抽头采样——若把落点吸附到整数像素，
 * 斜线笔画的边缘会量化成台阶（锯齿）。
 */
inline void washBlendImage(QImage& dst, const QImage& src, const QPointF& topLeft, qreal opacity = 1.0)
{
    if (dst.isNull() || src.isNull() || opacity <= 0.0) {
        return;
    }
    Q_ASSERT(src.format() == QImage::Format_ARGB32_Premultiplied);
    if (dst.format() != QImage::Format_ARGB32_Premultiplied) {
        dst = dst.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    const int srcW = src.width();
    const int srcH = src.height();

    const int x0 = qMax(0, int(qFloor(topLeft.x())));
    const int y0 = qMax(0, int(qFloor(topLeft.y())));
    const int x1 = qMin(dst.width(), int(qCeil(topLeft.x())) + srcW);
    const int y1 = qMin(dst.height(), int(qCeil(topLeft.y())) + srcH);

    for (int y = y0; y < y1; ++y) {
        auto dstLine = reinterpret_cast<QRgb*>(dst.scanLine(y));

        // 目标像素中心 (x+0.5, y+0.5) 映射到源图像素索引空间
        const qreal v = (y + 0.5) - topLeft.y() - 0.5;
        const int j0 = int(qFloor(v));
        const qreal fv = v - j0;

        for (int x = x0; x < x1; ++x) {
            const qreal u = (x + 0.5) - topLeft.x() - 0.5;
            const int i0 = int(qFloor(u));
            const qreal fu = u - i0;

            const qreal w00 = (1.0 - fu) * (1.0 - fv);
            const qreal w10 = fu * (1.0 - fv);
            const qreal w01 = (1.0 - fu) * fv;
            const qreal w11 = fu * fv;

            QRgb taps[4] = { 0, 0, 0, 0 };
            const int xs[4] = { i0, i0 + 1, i0, i0 + 1 };
            const int ys[4] = { j0, j0, j0 + 1, j0 + 1 };
            const qreal ws[4] = { w00, w10, w01, w11 };

            qreal r = 0.0, g = 0.0, b = 0.0, a = 0.0;
            for (int k = 0; k < 4; ++k) {
                if (xs[k] < 0 || xs[k] >= srcW || ys[k] < 0 || ys[k] >= srcH || ws[k] == 0.0) {
                    continue;
                }
                const QRgb s = reinterpret_cast<const QRgb*>(src.constScanLine(ys[k]))[xs[k]];
                r += qRed(s) * ws[k];
                g += qGreen(s) * ws[k];
                b += qBlue(s) * ws[k];
                a += qAlpha(s) * ws[k];
            }

            const quint32 sa = qRound(a * opacity);
            if (sa == 0) {
                continue;
            }
            // 预乘格式下各分量按不透明度缩放/双线性组合后仍是合法预乘值
            const QRgb scaled = (sa << 24)
                                | (qRound(r * opacity) << 16)
                                | (qRound(g * opacity) << 8)
                                | qRound(b * opacity);

            const QRgb d = dstLine[x];
            dstLine[x] = (sa >= qAlpha(d)) ? scaled : d;
        }
    }
}

#endif // WASHBLEND_H
