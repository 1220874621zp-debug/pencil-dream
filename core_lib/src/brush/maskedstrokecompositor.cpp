/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "maskedstrokecompositor.h"

#include <QPainter>
#include <QtMath>

namespace
{

inline qreal clamp01(qreal v)
{
    return qBound(0.0, v, 1.0);
}

// CFOverlay（KoCompositeOpFunctions）：柔光叠底的双段公式
inline qreal overlayChannel(qreal src, qreal dst)
{
    return dst <= 0.5 ? 2.0 * src * dst : 1.0 - 2.0 * (1.0 - src) * (1.0 - dst);
}

// colorDodgeAlpha（KisMaskingBrushCompositeOp.h:180）：alpha 域的色减淡变体
inline qreal colorDodgeAlpha(qreal src, qreal dst)
{
    if (src >= 0.9995) {
        return dst <= 0.0005 ? 0.0 : 1.0;
    }
    return clamp01(dst / (1.0 - src));
}

// CFColorBurn（KoCompositeOpFunctions.h:328）：dst=1 是稳定点；整数域 src=0 → 0
inline qreal colorBurnAlpha(qreal src, qreal dst)
{
    if (dst >= 0.9995) {
        return 1.0;
    }
    if (src <= 0.0005) {
        return 0.0;
    }
    return 1.0 - clamp01((1.0 - dst) / src);
}

} // namespace

void MaskedStrokeCompositor::begin(BrushMaskSettings::Mode mode)
{
    mMode = mode;
    mActive = true;
    mMain = QImage();
    mCover = QImage();
    mOrigin = QPoint(0, 0);
    mExtent = QRect();
}

void MaskedStrokeCompositor::end()
{
    mActive = false;
}

void MaskedStrokeCompositor::clear()
{
    mActive = false;
    mMain = QImage();
    mCover = QImage();
    mOrigin = QPoint(0, 0);
    mExtent = QRect();
}

void MaskedStrokeCompositor::ensureBuffers(const QRect& rect)
{
    if (mMain.isNull()) {
        const QSize size = rect.size().grownBy(QMargins(64, 64, 64, 64));
        mOrigin = rect.topLeft() - QPoint(64, 64);
        mMain = QImage(size, QImage::Format_ARGB32_Premultiplied);
        mMain.fill(Qt::transparent);
        mCover = QImage(size, QImage::Format_ARGB32_Premultiplied);
        mCover.fill(Qt::transparent);
        return;
    }
    const QRect current(mOrigin, mMain.size());
    if (current.contains(rect)) {
        return;
    }
    // 扩容：新范围 + 64px 余量，旧内容拷贝
    const QRect grown = rect.united(current).marginsAdded(QMargins(64, 64, 64, 64));
    QImage newMain(grown.size(), QImage::Format_ARGB32_Premultiplied);
    newMain.fill(Qt::transparent);
    QImage newCover(grown.size(), QImage::Format_ARGB32_Premultiplied);
    newCover.fill(Qt::transparent);
    QPainter mainPainter(&newMain);
    mainPainter.drawImage(current.topLeft() - grown.topLeft(), mMain);
    mainPainter.end();
    QPainter coverPainter(&newCover);
    coverPainter.drawImage(current.topLeft() - grown.topLeft(), mCover);
    coverPainter.end();
    mOrigin = grown.topLeft();
    mMain = newMain;
    mCover = newCover;
}

void MaskedStrokeCompositor::mainDab(const QImage& dab, const QPoint& topLeft,
                                     const DabPasteParams& params)
{
    if (!mActive || dab.isNull()) {
        return;
    }
    ensureBuffers(QRect(topLeft, dab.size()));
    washBlendImage(mMain, dab, topLeft - mOrigin, params);
}

void MaskedStrokeCompositor::maskDab(const QImage& dab, const QPoint& topLeft)
{
    if (!mActive || dab.isNull()) {
        return;
    }
    const QRect dabRect(topLeft, dab.size());
    ensureBuffers(dabRect);
    // 蒙版范围 = 副笔尖 dab 矩形的并集（Krita 只在 mask extent 上跑复合：
    // 范围外主笔迹原样保留；范围内但未覆盖处按公式处理——burn 类会清零）
    mExtent = mExtent.isNull() ? dabRect : mExtent.united(dabRect);
    // 白色 union 累积：buildup + 满流量 → alpha 朝并集演化（Krita 白漆 ALPHA_DARKEN）
    DabPasteParams params;
    params.opacity = 1.0;
    params.flow = 1.0;
    params.buildup = true;
    washBlendImage(mCover, dab, topLeft - mOrigin, params);
}

QRect MaskedStrokeCompositor::bounds() const
{
    if (mMain.isNull()) {
        return QRect();
    }
    return QRect(mOrigin, mMain.size());
}

QImage MaskedStrokeCompositor::composedRegion(const QRect& rect, QPoint& outOrigin) const
{
    outOrigin = QPoint();
    if (mMain.isNull() || !mActive) {
        return QImage();
    }
    const QRect clipped = rect.intersected(bounds());
    if (clipped.isEmpty()) {
        return QImage();
    }
    QImage region(clipped.size(), QImage::Format_ARGB32_Premultiplied);
    region.fill(Qt::transparent);
    const QPoint localBase = clipped.topLeft() - mOrigin;

    // 范围内逐像素复合；范围外主笔迹原样拷贝
    const auto fillRange = [&](int y, int x0, int x1, bool compose) {
        QRgb* line = reinterpret_cast<QRgb*>(region.scanLine(y));
        const QRgb* mainLine = reinterpret_cast<const QRgb*>(mMain.constScanLine(localBase.y() + y));
        const QRgb* coverLine = reinterpret_cast<const QRgb*>(mCover.constScanLine(localBase.y() + y));
        for (int x = x0; x <= x1; ++x) {
            const QRgb dp = mainLine[localBase.x() + x];
            if (!compose) {
                line[x] = dp;
                continue;
            }
            const int src = qAlpha(coverLine[localBase.x() + x]);
            const int dst = qAlpha(dp);
            if (src == 0 && dst == 0) {
                continue;
            }
            const int newA = qRound(maskOp(mMode, src / 255.0, dst / 255.0) * 255.0);
            if (newA == dst || dst == 0) {
                line[x] = dp;
            } else if (newA == 0) {
                line[x] = 0;
            } else {
                // 保色调改 alpha：非预乘分量按新 alpha 重预乘
                const qreal s = newA / qreal(dst);
                line[x] = qPremultiply(qRgba(qMin(255, qRound(qRed(dp) * s)),
                                             qMin(255, qRound(qGreen(dp) * s)),
                                             qMin(255, qRound(qBlue(dp) * s)),
                                             newA));
            }
        }
    };

    const QRect inExtent = mExtent.isNull() ? QRect() : clipped.intersected(mExtent);
    for (int y = 0; y < clipped.height(); ++y) {
        const int canvasY = clipped.y() + y;
        const bool rowInExtent = !inExtent.isEmpty()
                                 && canvasY >= inExtent.top()
                                 && canvasY <= inExtent.bottom();
        if (!rowInExtent) {
            fillRange(y, 0, clipped.width() - 1, false);
            continue;
        }
        const int inLeft = qMax(0, inExtent.left() - clipped.x());
        const int inRight = qMin(clipped.width() - 1, inExtent.right() - clipped.x());
        if (inLeft > 0) {
            fillRange(y, 0, inLeft - 1, false);
        }
        if (inRight < clipped.width() - 1) {
            fillRange(y, inRight + 1, clipped.width() - 1, false);
        }
        fillRange(y, inLeft, inRight, true);
    }
    outOrigin = clipped.topLeft();
    return region;
}

void MaskedStrokeCompositor::applyMaskOpToImage(QImage& main, const QImage& cover,
                                                BrushMaskSettings::Mode mode)
{
    if (main.isNull() || cover.isNull()) {
        return;
    }
    Q_ASSERT(main.format() == QImage::Format_ARGB32_Premultiplied
             && cover.format() == QImage::Format_ARGB32_Premultiplied);
    const QRect overlap = main.rect().intersected(cover.rect());
    for (int y = overlap.top(); y <= overlap.bottom(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(main.scanLine(y));
        const QRgb* coverLine = reinterpret_cast<const QRgb*>(cover.constScanLine(y));
        for (int x = overlap.left(); x <= overlap.right(); ++x) {
            const int src = qAlpha(coverLine[x]);
            const QRgb dp = line[x];
            const int dst = qAlpha(dp);
            if (src == 0 && dst == 0) {
                continue;
            }
            const int newA = qRound(maskOp(mode, src / 255.0, dst / 255.0) * 255.0);
            if (newA == dst || dst == 0) {
                continue;
            }
            if (newA == 0) {
                line[x] = 0;
            } else {
                const qreal s = newA / qreal(dst);
                line[x] = qPremultiply(qRgba(qMin(255, qRound(qRed(dp) * s)),
                                             qMin(255, qRound(qGreen(dp) * s)),
                                             qMin(255, qRound(qBlue(dp) * s)),
                                             newA));
            }
        }
    }
}

qreal MaskedStrokeCompositor::maskOp(BrushMaskSettings::Mode mode, qreal src, qreal dst)
{
    switch (mode) {
    case BrushMaskSettings::Mode::Mult:
        return src * dst;
    case BrushMaskSettings::Mode::Darken:
        return qMin(src, dst);
    case BrushMaskSettings::Mode::Overlay:
        return overlayChannel(src, dst);
    case BrushMaskSettings::Mode::Dodge:
        return colorDodgeAlpha(src, dst);
    case BrushMaskSettings::Mode::LinearBurn:
        return qMax(0.0, src + dst - 1.0);
    case BrushMaskSettings::Mode::LinearDodge:
        return dst <= 0.0 ? 0.0 : qMin(1.0, src + dst);
    case BrushMaskSettings::Mode::HardMix:
        return src + dst > 1.0 ? 1.0 : 0.0;
    case BrushMaskSettings::Mode::HardMixSofter:
        return clamp01(3.0 * dst - 2.0 * (1.0 - src));
    case BrushMaskSettings::Mode::Subtract:
        return qMax(0.0, dst - src);
    case BrushMaskSettings::Mode::Burn:
    default:
        return colorBurnAlpha(src, dst);
    }
}

qreal MaskedStrokeCompositor::textureOp(int mode, qreal src, qreal dst, qreal strength)
{
    switch (mode) {
    case 0:  // MULTIPLY
        return dst + (src * dst - dst) * strength;
    case 1:  // SUBTRACT
        return qMax(0.0, dst - src - (1.0 - strength));
    case 4:  // DARKEN
        return qMin(src, dst * strength);
    case 5:  // OVERLAY
        return overlayChannel(src, dst * strength);
    case 6:  // COLOR_DODGE
        return colorDodgeAlpha(src, dst * strength);
    case 7:  // COLOR_BURN
        return colorBurnAlpha(src, dst * strength);
    case 8:  // LINEAR_DODGE
        return dst <= 0.0 ? 0.0 : qMin(1.0, src + dst * strength);
    case 9:  // LINEAR_BURN
        return qMax(0.0, src + dst * strength - 1.0);
    case 10: // HARD_MIX_PHOTOSHOP
        return src + dst * strength > 1.0 ? 1.0 : 0.0;
    case 11: // HARD_MIX_SOFTER_PHOTOSHOP
        return clamp01(3.0 * dst * strength - 2.0 * (1.0 - src));
    case 12: { // HEIGHT
        const qreal s1 = 0.99 * strength;
        return clamp01(dst / (1.0 - s1) - (src + (1.0 - s1)));
    }
    case 13: { // LINEAR_HEIGHT
        const qreal s1 = 0.99 * strength;
        const qreal m = dst / (1.0 - s1);
        return clamp01(qMax(m * (1.0 - src), m - src));
    }
    case 14: // HEIGHT_PHOTOSHOP
        return clamp01(dst * 10.0 * strength - src);
    case 15: { // LINEAR_HEIGHT_PHOTOSHOP
        const qreal m = dst * 10.0 * strength;
        return clamp01(qMax((1.0 - src) * m, m - src));
    }
    default:
        return dst;
    }
}
