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
#include <QPoint>
#include <QtMath>

/**
 * Krita wash 语义的图像盖章：逐像素取"更不透明"的一方（预乘 ARGB32）。
 * QPainter 的 CompositionMode_Lighten 只对颜色取 max、alpha 仍按
 * SourceOver 累积（重叠会变深），做不出 wash，故手写像素混合。
 * 同一笔内半透明 dab 重叠不叠加变深、交叉不留洞。
 */
inline void washBlendImage(QImage& dst, const QImage& src, const QPoint& topLeft, qreal opacity = 1.0)
{
    if (dst.isNull() || src.isNull() || opacity <= 0.0) {
        return;
    }
    Q_ASSERT(src.format() == QImage::Format_ARGB32_Premultiplied);
    if (dst.format() != QImage::Format_ARGB32_Premultiplied) {
        dst = dst.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    const int x0 = qMax(0, topLeft.x());
    const int y0 = qMax(0, topLeft.y());
    const int x1 = qMin(dst.width(), topLeft.x() + src.width());
    const int y1 = qMin(dst.height(), topLeft.y() + src.height());

    for (int y = y0; y < y1; ++y) {
        auto dstLine = reinterpret_cast<QRgb*>(dst.scanLine(y));
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(src.constScanLine(y - topLeft.y()));

        for (int x = x0; x < x1; ++x) {
            const QRgb s = srcLine[x - topLeft.x()];
            const int srcAlpha = qAlpha(s);
            if (srcAlpha == 0) {
                continue;
            }

            QRgb scaled = s;
            if (opacity < 1.0) {
                // 预乘格式下按不透明度整体缩放各分量，仍是合法预乘值
                const quint32 sr = qRound(qRed(s) * opacity);
                const quint32 sg = qRound(qGreen(s) * opacity);
                const quint32 sb = qRound(qBlue(s) * opacity);
                const quint32 sa = qRound(srcAlpha * opacity);
                scaled = (sa << 24) | (sr << 16) | (sg << 8) | sb;
            }

            const QRgb d = dstLine[x];
            dstLine[x] = (qAlpha(scaled) >= qAlpha(d)) ? scaled : d;
        }
    }
}

#endif // WASHBLEND_H
