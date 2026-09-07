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
 * dab 盖章参数（Krita 像素笔刷的合成模型，参考 KoCompositeOpAlphaDarken/
 * KoCompositeOpOver + 每笔尖混合模式）：
 *  - Wash（涂抹）：alpha 只朝 opacity 收敛，同笔重叠不越叠越深；
 *    边缘的重叠渐变会相互填补 → 边缘平滑（Krita WASH/间接层同款公式）
 *  - Buildup（叠加）：每个 dab 按并集累积，反复描逐渐变深（Krita 默认 BUILDUP）
 *  - flow：涂抹模式下在"零流量并集"与"满流量收敛"间插值（Krita flow 语义）
 *  - blendMode：笔尖混合模式，先对底色混合再参与 alpha 合成
 *  - erase：擦除方向（绘制缓冲的 alpha 收缩，配合终局 DestinationOut）
 *
 * 子像素定位在 dab 生成阶段烤进掩码（BrushEngine::paintDab 的半像素
 * 网格吸附），这里只做整数对齐写入，无重采样。
 */
struct DabPasteParams
{
    qreal opacity = 1.0;  // 笔刷不透明度 0..1
    qreal flow = 1.0;     // 流量 0..1
    bool erase = false;   // 擦除方向
    bool buildup = false; // false=涂抹(Wash) true=叠加(Buildup)
    int blendMode = 0;    // 0=正常 1=正片叠底 2=滤色
};

inline void washBlendImage(QImage& dst, const QImage& src, const QPoint& topLeft,
                           const DabPasteParams& params = DabPasteParams())
{
    if (dst.isNull() || src.isNull() || params.opacity <= 0.0 || params.flow <= 0.0) {
        return;
    }
    Q_ASSERT(src.format() == QImage::Format_ARGB32_Premultiplied);
    if (dst.format() != QImage::Format_ARGB32_Premultiplied) {
        dst = dst.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    const qreal opacity = qBound(0.0, params.opacity, 1.0);
    const qreal flow = qBound(0.0, params.flow, 1.0);
    const int op255 = qRound(255 * opacity);

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
    if (qAlpha(srcBits[best]) <= 0) {
        return;
    }
    const int maxAlpha = qAlpha(srcBits[best]);
    const int colR = qRound(qBound(0.0, ((srcBits[best] >> 16) & 0xFF) * 255.0 / maxAlpha, 255.0));
    const int colG = qRound(qBound(0.0, ((srcBits[best] >> 8) & 0xFF) * 255.0 / maxAlpha, 255.0));
    const int colB = qRound(qBound(0.0, (srcBits[best] & 0xFF) * 255.0 / maxAlpha, 255.0));

    for (int y = y0; y < y1; ++y) {
        const QRgb* s = reinterpret_cast<const QRgb*>(src.constScanLine(y - topLeft.y())) + (x0 - topLeft.x());
        QRgb* d = reinterpret_cast<QRgb*>(dst.scanLine(y)) + x0;
        for (int x = x0; x < x1; ++x, ++s, ++d) {
            const int msk = qAlpha(*s); // 掩码 alpha（0..255）
            if (msk == 0) {
                continue;
            }
            const QRgb dp = *d;
            const int dA = qAlpha(dp);

            if (params.erase) {
                // ---- 擦除：缓冲 alpha 收缩，终局 DestinationOut 生效 ----
                int aNew;
                if (params.buildup) {
                    // 叠加擦：每 dab 乘性削减
                    aNew = dA - qRound(dA * (msk * opacity * flow) / 255.0);
                } else {
                    // 涂抹擦：向 (255-op255) 收敛，alpha 只降不升
                    const int floorA = 255 - op255;
                    const int aFull = dA > floorA
                                      ? floorA + qRound((dA - floorA) * (255 - msk * opacity) / 255.0)
                                      : dA;
                    aNew = dA + qRound((aFull - dA) * flow);
                }
                if (aNew >= dA) {
                    continue; // 没擦掉（或已到底）
                }
                if (dA == 0) {
                    continue;
                }
                // 预乘各分量按 alpha 比例缩放
                const int scale = qRound(255.0 * aNew / dA);
                *d = (aNew << 24)
                     | (qRound(qRed(dp) * scale / 255.0) << 16)
                     | (qRound(qGreen(dp) * scale / 255.0) << 8)
                     | qRound(qBlue(dp) * scale / 255.0);
                continue;
            }

            // ---- 绘制 ----
            // 有效源 alpha = 掩码 × 不透明度
            const int sA = qRound(msk * opacity);

            // 笔尖混合模式：对底色混合出本 dab 的源色
            int sr = colR, sg = colG, sb = colB;
            if (params.blendMode != 0 && dA > 0) {
                // 目标非预乘色
                const int ur = qRound(qRed(dp) * 255.0 / dA);
                const int ug = qRound(qGreen(dp) * 255.0 / dA);
                const int ub = qRound(qBlue(dp) * 255.0 / dA);
                if (params.blendMode == 1) { // 正片叠底
                    sr = colR * ur / 255; sg = colG * ug / 255; sb = colB * ub / 255;
                } else { // 滤色
                    sr = 255 - (255 - colR) * (255 - ur) / 255;
                    sg = 255 - (255 - colG) * (255 - ug) / 255;
                    sb = 255 - (255 - colB) * (255 - ub) / 255;
                }
            }

            // alpha 演化（Krita KoCompositeOpAlphaDarken / Over 的 flow 插值形式）
            int aNew;
            if (params.buildup) {
                // 叠加：朝并集累积（flow 插值），反复描趋向全不透明
                const int aUnion = dA + sA - dA * sA / 255;
                aNew = dA + qRound((aUnion - dA) * flow);
            } else {
                // 涂抹：满流量朝 opacity 收敛（同笔封顶不加深），
                //        零流量只并集；flow 在两者间插值
                const int aFull = dA + qRound((op255 - dA) * msk / 255.0);
                const int aZero = dA + sA - dA * sA / 255;
                aNew = aZero + qRound((aFull - aZero) * flow);
            }
            if (aNew <= dA) {
                continue; // alpha 只增不减
            }

            // 颜色：dst 通道按有效源 alpha 向源色插值（Krita dst[i]=lerp(dst,src,srcAlpha')）
            if (dA == 0) {
                *d = qPremultiply(qRgba(sr, sg, sb, aNew));
            } else {
                const qreal k = sA / 255.0;
                const qreal uR = qRed(dp) / (dA / 255.0);
                const qreal uG = qGreen(dp) / (dA / 255.0);
                const qreal uB = qBlue(dp) / (dA / 255.0);
                const int nR = qRound(uR + (sr - uR) * k);
                const int nG = qRound(uG + (sg - uG) * k);
                const int nB = qRound(uB + (sb - uB) * k);
                *d = qPremultiply(qRgba(qBound(0, nR, 255), qBound(0, nG, 255),
                                        qBound(0, nB, 255), aNew));
            }
        }
    }
}

#endif // WASHBLEND_H
