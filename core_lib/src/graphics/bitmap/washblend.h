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
 *  - 橡皮复用同一路径：缓冲 alpha 累积 = 擦除量（涂抹=封顶 opacity，
 *    叠加=越擦越净），ScribbleArea 的实时预览与终局 paste 用
 *    DestinationOut 反转语义，故这里不需要 erase 分支
 *
 * 子像素定位在 dab 生成阶段烤进掩码（BrushEngine::paintDab 的半像素
 * 网格吸附），这里只做整数对齐写入，无重采样。
 */
struct DabPasteParams
{
    qreal opacity = 1.0;  // 笔刷不透明度 0..1
    qreal flow = 1.0;     // 流量 0..1
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

/**
 * Krita 混合笔刷（smudge/Smearing）核心公式，KisColorSmudgeStrategyBase 的化简：
 *   new(x) = lerp( tile(x), canvas(x − Δ), rate · mask(x) )
 * tile = 笔画缓冲瓦片（预乘）；canvas = 图层+缓冲的合成采样图（同画布坐标，
 * 内容沿笔画链式更新 → 拖尾）；Δ = 相邻 dab 中心位移（采样自上一 dab 位置）；
 * mask = 笔尖掩码的 alpha（决定边缘软硬与中心作用强度——Krita 默认
 * 混合预设 fade 0.9，边缘极软）。位移为亚像素，采样双线性插值。
 */
inline void smudgeBlendImage(QImage& tile, const QPoint& tileOrigin,
                             const QImage& canvas, const QPoint& canvasOrigin,
                             const QImage& mask, const QPoint& maskOrigin,
                             const QPointF& delta, qreal rate)
{
    if (tile.isNull() || canvas.isNull() || mask.isNull() || rate <= 0.0) {
        return;
    }
    Q_ASSERT(tile.format() == QImage::Format_ARGB32_Premultiplied
             && canvas.format() == QImage::Format_ARGB32_Premultiplied);
    rate = qBound(0.0, rate, 1.0);

    const int cw = canvas.width(), ch = canvas.height();
    for (int my = 0; my < mask.height(); ++my) {
        const QRgb* mrow = reinterpret_cast<const QRgb*>(mask.constScanLine(my));
        for (int mx = 0; mx < mask.width(); ++mx) {
            const qreal m = qAlpha(mrow[mx]) / 255.0;
            const qreal a = rate * m;
            if (a <= 0.0) {
                continue;
            }
            const int tx = maskOrigin.x() + mx - tileOrigin.x();
            const int ty = maskOrigin.y() + my - tileOrigin.y();
            if (tx < 0 || tx >= tile.width() || ty < 0 || ty >= tile.height()) {
                continue;
            }
            // 采样画布像素中心 (x+0.5) − Δ 处的内容，双线性
            const qreal sx = maskOrigin.x() + mx - delta.x() - canvasOrigin.x();
            const qreal sy = maskOrigin.y() + my - delta.y() - canvasOrigin.y();
            const int ix = int(qFloor(sx)), iy = int(qFloor(sy));
            const qreal fx = sx - ix, fy = sy - iy;
            auto sampleAt = [&](int x, int y) -> QRgb {
                if (x < 0 || x >= cw || y < 0 || y >= ch) {
                    return 0;
                }
                return reinterpret_cast<const QRgb*>(canvas.constScanLine(y))[x];
            };
            const QRgb s00 = sampleAt(ix, iy), s10 = sampleAt(ix + 1, iy);
            const QRgb s01 = sampleAt(ix, iy + 1), s11 = sampleAt(ix + 1, iy + 1);
            const qreal w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy);
            const qreal w01 = (1 - fx) * fy, w11 = fx * fy;
            const qreal sr = qRed(s00) * w00 + qRed(s10) * w10 + qRed(s01) * w01 + qRed(s11) * w11;
            const qreal sg = qGreen(s00) * w00 + qGreen(s10) * w10 + qGreen(s01) * w01 + qGreen(s11) * w11;
            const qreal sb = qBlue(s00) * w00 + qBlue(s10) * w10 + qBlue(s01) * w01 + qBlue(s11) * w11;
            const qreal sa = qAlpha(s00) * w00 + qAlpha(s10) * w10 + qAlpha(s01) * w01 + qAlpha(s11) * w11;
            if (sa <= 0.5 && qRound(sa) == 0) {
                continue; // 采样处无内容，不引入透明
            }
            // 预乘各通道线性插值合法
            QRgb* d = reinterpret_cast<QRgb*>(tile.scanLine(ty)) + tx;
            const qreal ia = 1.0 - a;
            *d = qRgba(qRound(qRed(*d) * ia + sr * a),
                       qRound(qGreen(*d) * ia + sg * a),
                       qRound(qBlue(*d) * ia + sb * a),
                       qRound(qAlpha(*d) * ia + sa * a));
        }
    }
}

#endif // WASHBLEND_H
