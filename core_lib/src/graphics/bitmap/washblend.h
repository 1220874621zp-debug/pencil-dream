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
 * Krita wash 语义的整数对齐盖章：逐像素取"更不透明"的一方（预乘 ARGB32）。
 * QPainter 的 CompositionMode_Lighten 只对颜色取 max、alpha 仍按
 * SourceOver 累积（重叠会变深），做不出 wash，故手写像素混合。
 * 同一笔内半透明 dab 重叠不叠加变深、交叉不留洞。
 *
 * 子像素定位在 dab 生成阶段烤进掩码（BrushEngine::paintDab 的半像素
 * 网格吸附），这里只做整数对齐的单次比较写入，无重采样。
 *
 * dab 的 RGB = 颜色 × 掩码 alpha（生成时预乘烤死），所以输出像素只依赖
 * 源 alpha 字节——按 alpha 查表（256 项）即得整个输出像素，热循环退化为
 * "取 alpha → 查表 → 比较 → 可能写一次"。
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
    opacity = qBound(0.0, opacity, 1.0);

    const int x0 = qMax(0, topLeft.x());
    const int y0 = qMax(0, topLeft.y());
    const int x1 = qMin(dst.width(), topLeft.x() + src.width());
    const int y1 = qMin(dst.height(), topLeft.y() + src.height());
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    // 从最不透明的源像素反推笔色（src = qPremultiply(color, a) 的整数近似）
    const QRgb* srcBits = reinterpret_cast<const QRgb*>(src.constBits());
    const int srcCount = src.width() * src.height();
    int best = 0;
    for (int i = 1; i < srcCount; ++i) {
        if (qAlpha(srcBits[i]) > qAlpha(srcBits[best])) {
            best = i;
        }
    }
    const int maxAlpha = qAlpha(srcBits[best]);
    if (maxAlpha <= 0) {
        return;
    }
    const int cr = qRound(qBound(0.0, ((srcBits[best] >> 16) & 0xFF) * 255.0 / maxAlpha, 255.0));
    const int cg = qRound(qBound(0.0, ((srcBits[best] >> 8) & 0xFF) * 255.0 / maxAlpha, 255.0));
    const int cb = qRound(qBound(0.0, (srcBits[best] & 0xFF) * 255.0 / maxAlpha, 255.0));

    // alpha 字节 → 缩放不透明度后的输出 alpha / 整个输出像素（预乘）
    quint32 outAlphaLut[256];
    quint32 outPixelLut[256];
    for (int a = 0; a <= 255; ++a) {
        const int scaled = qRound(a * opacity);
        outAlphaLut[a] = static_cast<quint32>(scaled);
        outPixelLut[a] = qPremultiply(qRgba(cr, cg, cb, scaled));
    }

    const int srcRowSkip = src.width() - (x1 - x0);
    const QRgb* s = reinterpret_cast<const QRgb*>(src.constScanLine(y0 - topLeft.y()))
                    + (x0 - topLeft.x());
    for (int y = y0; y < y1; ++y) {
        QRgb* d = reinterpret_cast<QRgb*>(dst.scanLine(y)) + x0;
        for (int x = x0; x < x1; ++x, ++s, ++d) {
            const quint32 outA = outAlphaLut[qAlpha(*s)];
            if (outA >= qAlpha(*d)) {
                *d = outPixelLut[qAlpha(*s)];
            }
        }
        s += srcRowSkip;
    }
}

#endif // WASHBLEND_H
