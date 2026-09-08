/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

Adapted from Krita's kis_selection_filters.cpp (GPL-2.0-or-later).
The grow/shrink disc algorithms originate from GIMP
(jaycox@gimp.org), as noted in the Krita source.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "fillfilters.h"

#include <QtMath>
#include <QtGlobal>

#include <memory>

namespace FillFilters
{

namespace
{

constexpr quint8 MAX_SELECTED = 255;
constexpr quint8 MIN_SELECTED = 0;

inline int rint(qreal v)
{
    return static_cast<int>(v < 0.0 ? v - 0.5 : v + 0.5);
}

/** Holds the y coordinates of a circular structuring element, indexed from
 *  -xRadius to +xRadius (inclusive). */
void computeBorder(qint32* circ, qint32 xradius, qint32 yradius)
{
    qint32 i;
    qint32 diameter = xradius * 2 + 1;
    double tmp;

    for (i = 0; i < diameter; i++) {
        if (i > xradius)
            tmp = (i - xradius) - 0.5;
        else if (i < xradius)
            tmp = (xradius - i) - 0.5;
        else
            tmp = 0.0;

        double divisor = static_cast<double>(xradius);
        if (divisor == 0.0) {
            divisor = 1.0;
        }
        circ[i] = rint(yradius * sqrt(xradius * xradius - tmp * tmp) / divisor);
    }
}

template<typename T>
void rotatePointers(T** p, quint32 n)
{
    T* p0 = p[0];
    for (quint32 i = 0; i < n - 1; i++) {
        p[i] = p[i + 1];
    }
    p[n - 1] = p0;
}

/** Dilate with a disc of the given radius; `pickMax` = false erodes instead. */
void discMorphology(QVector<quint8>& mask, int width, int height,
                    int xRadius, int yRadius, bool pickMax)
{
    if (xRadius <= 0 || yRadius <= 0) return;

    const int w = width;
    const int h = height;

    // caches the region's pixel data
    QVector<QVector<quint8>> buf(yRadius + 1);
    for (int i = 0; i < yRadius + 1; i++) {
        buf[i].resize(w);
    }
    // caches the extreme values for each column
    QVector<quint8*> max(w + 2 * xRadius);
    QVector<quint8> buffer((w + 2 * xRadius) * (yRadius + 1));
    // Outside the region is unselected for both dilation and erosion, as in
    // Krita's fill-tool usage of these filters
    for (int i = 0; i < w + 2 * xRadius; i++) {
        if (i < xRadius)
            max[i] = buffer.data();
        else if (i < w + xRadius)
            max[i] = buffer.data() + (yRadius + 1) * (i - xRadius);
        else
            max[i] = buffer.data() + (yRadius + 1) * (w + xRadius - 1);

        for (int j = 0; j < yRadius + 1; j++)
            max[i][j] = 0;
    }
    // offset the max pointer by xRadius so the range of the array
    // is [-xRadius] to [w + xRadius]
    quint8** maxPtr = max.data() + xRadius;

    QVector<quint8> out(w);

    QVector<qint32> circData(2 * xRadius + 1);
    qint32* circ = circData.data() + xRadius;
    computeBorder(circData.data(), xRadius, yRadius);

    auto pick = pickMax ? [](quint8 a, quint8 b) { return qMax(a, b); }
                        : [](quint8 a, quint8 b) { return qMin(a, b); };

    quint8* bufPtrs[16];
    Q_ASSERT(yRadius + 1 <= 16);
    for (int i = 0; i < yRadius + 1; i++) {
        bufPtrs[i] = buf[i].data();
    }

    memset(bufPtrs[0], 0, w);
    for (int i = 0; i < yRadius && i < h; i++) { // load top of image
        memcpy(bufPtrs[i + 1], mask.data() + i * w, w);
    }

    for (int x = 0; x < w; x++) { // set up extreme values for top of image
        maxPtr[x][0] = bufPtrs[0][x];
        for (int j = 1; j < yRadius + 1; j++) {
            maxPtr[x][j] = pick(bufPtrs[j][x], maxPtr[x][j - 1]);
        }
    }

    for (int y = 0; y < h; y++) {
        rotatePointers(bufPtrs, yRadius + 1);
        if (y < h - yRadius) {
            memcpy(bufPtrs[yRadius], mask.data() + (y + yRadius) * w, w);
        } else {
            memset(bufPtrs[yRadius], 0, w);
        }
        for (int x = 0; x < w; x++) { /* update extreme-value array */
            for (int i = yRadius; i > 0; i--) {
                maxPtr[x][i] = pick(pick(maxPtr[x][i - 1], bufPtrs[i - 1][x]), bufPtrs[i][x]);
            }
            maxPtr[x][0] = bufPtrs[0][x];
        }
        qint32 lastExtreme = maxPtr[0][circ[-1]];
        qint32 lastIndex = 1;
        for (int x = 0; x < w; x++) { /* render scan line */
            lastIndex--;
            if (lastIndex >= 0) {
                if (lastExtreme == (pickMax ? 255 : 0)) {
                    out[x] = pickMax ? 255 : 0;
                } else {
                    lastExtreme = pickMax ? 0 : 255;
                    for (qint32 i = xRadius; i >= 0; i--)
                        if (pickMax ? (lastExtreme < maxPtr[x + i][circ[i]])
                                    : (lastExtreme > maxPtr[x + i][circ[i]])) {
                            lastExtreme = maxPtr[x + i][circ[i]];
                            lastIndex = i;
                        }
                    out[x] = static_cast<quint8>(lastExtreme);
                }
            } else {
                lastIndex = xRadius;
                lastExtreme = maxPtr[x + xRadius][circ[xRadius]];
                for (qint32 i = xRadius - 1; i >= -xRadius; i--)
                    if (pickMax ? (lastExtreme < maxPtr[x + i][circ[i]])
                                : (lastExtreme > maxPtr[x + i][circ[i]])) {
                        lastExtreme = maxPtr[x + i][circ[i]];
                        lastIndex = i;
                    }
                out[x] = static_cast<quint8>(lastExtreme);
            }
        }
        memcpy(mask.data() + y * w, out.data(), w);
    }
}

} // anonymous namespace

void growSelection(QVector<quint8>& mask, int width, int height, int radius)
{
    discMorphology(mask, width, height, radius, radius, true);
}

void shrinkSelection(QVector<quint8>& mask, int width, int height, int radius)
{
    discMorphology(mask, width, height, radius, radius, false);
}

void featherSelection(QVector<quint8>& mask, int width, int height, int radius)
{
    if (radius <= 0) return;

    // compute the horizontal kernel (Krita's KisFeatherSelectionFilter)
    const uint kernelSize = radius * 2 + 1;
    QVector<qreal> kernel(kernelSize);

    const qreal multiplicand = 1.0 / (2.0 * M_PI * radius * radius);
    const qreal exponentMultiplicand = 1.0 / (2.0 * radius * radius);

    qreal sum = 0.0;
    for (uint x = 0; x < kernelSize; x++) {
        const uint xDistance = qAbs(static_cast<int>(radius) - static_cast<int>(x));
        kernel[x] = multiplicand * exp(-static_cast<qreal>((xDistance * xDistance) + (radius * radius)) * exponentMultiplicand);
        sum += kernel[x];
    }
    for (uint x = 0; x < kernelSize; x++) {
        kernel[x] /= sum;
    }

    // separable convolution with repeated (clamped) borders, as in Krita's
    // BORDER_REPEAT convolution border
    QVector<quint8> tmp(width * height);
    for (int y = 0; y < height; ++y) {
        const quint8* row = mask.constData() + y * width;
        for (int x = 0; x < width; ++x) {
            qreal acc = 0.0;
            for (int k = 0; k < static_cast<int>(kernelSize); ++k) {
                const int sx = qBound(0, x + k - radius, width - 1);
                acc += kernel[k] * row[sx];
            }
            tmp[y * width + x] = static_cast<quint8>(qRound(acc));
        }
    }
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            qreal acc = 0.0;
            for (int k = 0; k < static_cast<int>(kernelSize); ++k) {
                const int sy = qBound(0, y + k - radius, height - 1);
                acc += kernel[k] * tmp[sy * width + x];
            }
            mask[y * width + x] = static_cast<quint8>(qRound(acc));
        }
    }
}

namespace
{

// Antialias filter parameters, from Krita's KisAntiAliasSelectionFilter
constexpr qint32 edgeThreshold = 4;
constexpr qint32 numSteps = 30;
constexpr qint32 antialiasOffsets[numSteps] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2
};
constexpr qint32 horizontalBorderSize = 2;
constexpr qint32 verticalBorderSize = 40;
constexpr qint32 numberOfScanlines = 2 * verticalBorderSize + 1;
constexpr qint32 currentScanlineIndex = verticalBorderSize;
constexpr quint8 defaultPixel = MIN_SELECTED; // outside the region: unselected

struct AntialiasContext
{
    quint8* scanlines[numberOfScanlines];
    quint32 bytesPerPaddedScanline;

    // Bounds-checked sample: Krita's original relies on the edge breaking
    // the span search before it can leave the buffer; we make that safety
    // explicit instead of trusting pixel values.
    inline quint8 sample(qint32 row, qint32 col) const
    {
        if (row < 0 || row >= numberOfScanlines ||
            col < 0 || col >= static_cast<qint32>(bytesPerPaddedScanline)) {
            return defaultPixel;
        }
        return scanlines[row][col];
    }
};

bool findSpanExtreme(AntialiasContext& ctx, qint32 x, qint32 pixelOffset,
                     qint32 rowMultiplier, qint32 colMultiplier, qint32 direction,
                     qint32 pixelAvg, qint32 scaledGradient, qint32 currentPixelDiff,
                     qint32* spanEndDistance, qint32* pixelDiff, bool* spanExtremeValid)
{
    *spanEndDistance = 0;
    *spanExtremeValid = true;
    for (qint32 i = 0; i < numSteps; ++i) {
        *spanEndDistance += antialiasOffsets[i];
        const qint32 row1 = currentScanlineIndex + (direction * *spanEndDistance * rowMultiplier);
        const qint32 col1 = x + horizontalBorderSize + (direction * *spanEndDistance * colMultiplier);
        const qint32 row2 = row1 + pixelOffset * colMultiplier;
        const qint32 col2 = col1 + pixelOffset * rowMultiplier;
        const quint8 pixel1 = ctx.sample(row1, col1);
        const quint8 pixel2 = ctx.sample(row2, col2);
        // Get how different are these edge pixels from the current pixels and
        // stop searching if they are too different
        *pixelDiff = ((pixel1 + pixel2) >> 1) - pixelAvg;
        if (qAbs(*pixelDiff) > scaledGradient) {
            // If this is the end of the span then check if the corner belongs
            // to a jagged border or to a right angled part of the shape
            qint32 pixelDiff2;
            if ((currentPixelDiff < 0 && *pixelDiff < 0) || (currentPixelDiff > 0 && *pixelDiff > 0)) {
                const qint32 row3 = row2 + pixelOffset * colMultiplier;
                const qint32 col3 = col2 + pixelOffset * rowMultiplier;
                const quint8 pixel3 = ctx.sample(row3, col3);
                pixelDiff2 = ((pixel2 + pixel3) >> 1) - pixelAvg;
            } else {
                const qint32 row3 = row1 - pixelOffset * colMultiplier;
                const qint32 col3 = col1 - pixelOffset * rowMultiplier;
                const quint8 pixel3 = ctx.sample(row3, col3);
                pixelDiff2 = ((pixel1 + pixel3) >> 1) - pixelAvg;
            }
            *spanExtremeValid = !(qAbs(pixelDiff2) > scaledGradient);
            break;
        }
    }
    return true;
}

bool getInterpolationValue(qint32 negativeSpanEndDistance,
                           qint32 positiveSpanEndDistance,
                           qint32 negativePixelDiff,
                           qint32 positivePixelDiff,
                           qint32 currentPixelDiff,
                           bool negativeSpanExtremeValid,
                           bool positiveSpanExtremeValid,
                           qint32* interpolationValue)
{
    // Since we search a limited number of steps in each direction of the
    // current pixel, the end pixel of the span may still belong to the edge.
    // So we check for that, and if that's the case we must not smooth the
    // current pixel
    const bool pixelDiffLessThanZero = currentPixelDiff < 0;
    quint32 distance;
    if (negativeSpanEndDistance < positiveSpanEndDistance) {
        if (!negativeSpanExtremeValid) {
            return false;
        }
        // The pixel is closer to the negative end
        const bool spanEndPixelDiffLessThanZero = negativePixelDiff < 0;
        if (pixelDiffLessThanZero == spanEndPixelDiffLessThanZero) {
            return false;
        }
        distance = negativeSpanEndDistance;
    } else {
        if (!positiveSpanExtremeValid) {
            return false;
        }
        // The pixel is closer to the positive end
        const bool spanEndPixelDiffLessThanZero = positivePixelDiff < 0;
        if (pixelDiffLessThanZero == spanEndPixelDiffLessThanZero) {
            return false;
        }
        distance = positiveSpanEndDistance;
    }
    const qint32 spanLength = positiveSpanEndDistance + negativeSpanEndDistance;
    *interpolationValue = ((distance << 8) / spanLength) + 128;
    return *interpolationValue >= 0;
}

} // anonymous namespace

void antialiasSelection(QVector<quint8>& mask, int width, int height)
{
    // Size of a scanline
    const quint32 bytesPerScanline = width + 2 * horizontalBorderSize;
    // Size of a scanline padded to a multiple of 8
    const quint32 bytesPerPaddedScanline = ((bytesPerScanline + 7) / 8) * 8;

    // This buffer contains the number of consecutive scanlines needed to
    // process the current scanline
    QVector<quint8> buffer(bytesPerPaddedScanline * numberOfScanlines);
    buffer.fill(defaultPixel);

    AntialiasContext ctx;
    ctx.bytesPerPaddedScanline = bytesPerPaddedScanline;
    for (quint32 i = 0; i < numberOfScanlines; ++i) {
        ctx.scanlines[i] = buffer.data() + i * bytesPerPaddedScanline;
    }

    // Initialize the scanlines: `verticalBorderSize` default rows on top,
    // then the first image rows with default side borders
    const quint32 numberOfFirstRows = qMin(static_cast<qint32>(height),
                                           numberOfScanlines - verticalBorderSize);
    for (quint32 i = verticalBorderSize; i < verticalBorderSize + numberOfFirstRows; ++i) {
        memcpy(ctx.scanlines[i] + horizontalBorderSize,
               mask.constData() + (i - verticalBorderSize) * width, width);
    }

    // Buffer that contains the current output scanline
    QVector<quint8> antialiasedScanline(width);

    // Main loop
    for (int y = 0; y < height; ++y)
    {
        // Move to the next scanline
        if (y > 0) {
            // Update scanline pointers
            std::rotate(std::begin(ctx.scanlines), std::begin(ctx.scanlines) + 1, std::end(ctx.scanlines));
            // Copy the next scanline
            if (y < height - verticalBorderSize) {
                memset(ctx.scanlines[numberOfScanlines - 1], defaultPixel, bytesPerScanline);
                memcpy(ctx.scanlines[numberOfScanlines - 1] + horizontalBorderSize,
                       mask.constData() + (y + verticalBorderSize) * width, width);
            } else {
                memset(ctx.scanlines[numberOfScanlines - 1], defaultPixel, bytesPerScanline);
            }
        }
        // Process the pixels in the current scanline
        for (int x = 0; x < width; ++x)
        {
            // Get the current pixel and neighbors
            const quint8* pixelRowM = ctx.scanlines[currentScanlineIndex] + x + horizontalBorderSize;
            const quint8* pixelRowN = ctx.scanlines[currentScanlineIndex - 1] + x + horizontalBorderSize;
            const quint8* pixelRowS = ctx.scanlines[currentScanlineIndex + 1] + x + horizontalBorderSize;
            const qint32 pixelNW = *(pixelRowN - 1);
            const qint32 pixelN  = *(pixelRowN    );
            const qint32 pixelNE = *(pixelRowN + 1);
            const qint32 pixelW  = *(pixelRowM - 1);
            const qint32 pixelM  = *(pixelRowM    );
            const qint32 pixelE  = *(pixelRowM + 1);
            const qint32 pixelSW = *(pixelRowS - 1);
            const qint32 pixelS  = *(pixelRowS    );
            const qint32 pixelSE = *(pixelRowS + 1);
            // Get the gradients
            const qint32 rowNSum = (pixelNW >> 2) + (pixelN >> 1) + (pixelNE >> 2);
            const qint32 rowMSum = (pixelW  >> 2) + (pixelM >> 1) + (pixelE  >> 2);
            const qint32 rowSSum = (pixelSW >> 2) + (pixelS >> 1) + (pixelSE >> 2);
            const qint32 colWSum = (pixelNW >> 2) + (pixelW >> 1) + (pixelSW >> 2);
            const qint32 colMSum = (pixelN  >> 2) + (pixelM >> 1) + (pixelS  >> 2);
            const qint32 colESum = (pixelNE >> 2) + (pixelE >> 1) + (pixelSE >> 2);
            const qint32 gradientN = qAbs(rowMSum - rowNSum);
            const qint32 gradientS = qAbs(rowSSum - rowMSum);
            const qint32 gradientW = qAbs(colMSum - colWSum);
            const qint32 gradientE = qAbs(colESum - colMSum);
            // Get the maximum gradient
            const qint32 maxGradientNS = qMax(gradientN, gradientS);
            const qint32 maxGradientWE = qMax(gradientW, gradientE);
            const qint32 maxGradient = qMax(maxGradientNS, maxGradientWE);
            // Return early if the gradient is below some threshold (given by
            // the value below which the jagged edge is not noticeable)
            if (maxGradient < edgeThreshold) {
                antialiasedScanline[x] = pixelM;
                continue;
            }
            // Collect some info about the pixel and neighborhood
            qint32 neighborPixel, gradient;
            qint32 pixelOffset, rowMultiplier, colMultiplier;
            if (maxGradientNS > maxGradientWE) {
                // Horizontal span
                if (gradientN > gradientS) {
                    // The edge is formed with the top pixel
                    neighborPixel = pixelN;
                    gradient = gradientN;
                    pixelOffset = -1;
                } else {
                    // The edge is formed with the bottom pixel
                    neighborPixel = pixelS;
                    gradient = gradientS;
                    pixelOffset = 1;
                }
                rowMultiplier = 0;
                colMultiplier = 1;
            } else {
                // Vertical span
                if (gradientW > gradientE) {
                    // The edge is formed with the left pixel
                    neighborPixel = pixelW;
                    gradient = gradientW;
                    pixelOffset = -1;
                } else {
                    // The edge is formed with the right pixel
                    neighborPixel = pixelE;
                    gradient = gradientE;
                    pixelOffset = 1;
                }
                rowMultiplier = 1;
                colMultiplier = 0;
            }
            // Find the span extremes
            const qint32 pixelAvg = (neighborPixel + pixelM) >> 1;
            const qint32 currentPixelDiff = pixelM - pixelAvg;
            qint32 negativePixelDiff = 0, positivePixelDiff = 0;
            qint32 negativeSpanEndDistance = 0, positiveSpanEndDistance = 0;
            bool negativeSpanExtremeValid = false, positiveSpanExtremeValid = false;

            findSpanExtreme(ctx, x, pixelOffset, rowMultiplier, colMultiplier, -1, pixelAvg,
                            gradient >> 2, currentPixelDiff, &negativeSpanEndDistance,
                            &negativePixelDiff, &negativeSpanExtremeValid);
            findSpanExtreme(ctx, x, pixelOffset, rowMultiplier, colMultiplier, 1, pixelAvg,
                            gradient >> 2, currentPixelDiff, &positiveSpanEndDistance,
                            &positivePixelDiff, &positiveSpanExtremeValid);

            // Get the interpolation value for this pixel given the span extent
            // and perform linear interpolation between the current pixel and
            // the edge neighbor
            qint32 interpolationValue;
            if (!getInterpolationValue(negativeSpanEndDistance, positiveSpanEndDistance,
                                       negativePixelDiff, positivePixelDiff, currentPixelDiff,
                                       negativeSpanExtremeValid, positiveSpanExtremeValid,
                                       &interpolationValue)) {
                antialiasedScanline[x] = pixelM;
            } else {
                antialiasedScanline[x] = neighborPixel + ((pixelM - neighborPixel) * interpolationValue >> 8);
            }
        }
        // Copy the scanline data to the mask
        memcpy(mask.data() + y * width, antialiasedScanline.constData(), width);
    }
}

void growUntilDarkestPixel(QVector<quint8>& mask, const QImage& reference,
                           int width, int height, int radius)
{
    Q_ASSERT(reference.size() == QSize(width, height));

    // Copy the original selection. We will grow this adaptively until the
    // darkest or more opaque pixels or until the maximum grow is reached.
    QVector<quint8> adaptive = mask;
    // Grow the original selection normally. At the end this selection will be
    // masked with the adaptively grown mask.
    growSelection(mask, width, height, radius);

    Q_ASSERT(reference.format() == QImage::Format_ARGB32_Premultiplied);

    const auto opacityOf = [](QRgb px) { return qAlpha(px); };
    const auto intensityOf = [](QRgb px) {
        const QRgb unpre = qUnpremultiply(px);
        return qGray(qRed(unpre), qGreen(unpre), qBlue(unpre));
    };

    // Test if a pixel can be selected, given an already-masked neighbor.
    const auto testSelectPixel = [&](QPoint p, QPoint n) -> bool {
        if (adaptive[n.y() * width + n.x()] != MIN_SELECTED) {
            const QRgb npx = reinterpret_cast<const QRgb*>(reference.constScanLine(n.y()))[n.x()];
            const quint8 nOpacity = opacityOf(npx);
            const QRgb ppx = reinterpret_cast<const QRgb*>(reference.constScanLine(p.y()))[p.x()];
            const quint8 pOpacity = opacityOf(ppx);
            if (pOpacity >= nOpacity) {
                // Special case for when the neighbor pixel is fully transparent.
                // In that case do not compare the intensity
                if (nOpacity == MIN_SELECTED) {
                    return true;
                }
                // If the opacity test passes we still have to perform the
                // intensity test
                if (intensityOf(ppx) <= intensityOf(npx)) {
                    return true;
                }
            }
        }
        return false;
    };

    const auto inside = [&](int x, int y) {
        return x >= 0 && x < width && y >= 0 && y < height;
    };

    // Top-left to bottom-right pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const QPoint p(x, y);
            if (adaptive[y * width + x] == MIN_SELECTED && mask[y * width + x] != MIN_SELECTED) {
                bool selected = false;
                const QPoint candidates[] = {
                    QPoint(x - 1, y - 1), QPoint(x, y - 1), QPoint(x + 1, y - 1),
                    QPoint(x - 1, y)
                };
                for (const QPoint& n : candidates) {
                    // The neighbor sets differ for the first row and the
                    // first/last pixel of each row, exactly as in Krita's
                    // implementation; testing the superset only grows into
                    // pixels the scan has already visited, so the result is
                    // identical and the edge cases need no special casing.
                    if (n.y() >= y && !(n.y() == y && n.x() < x)) continue; // not "already visited"
                    if (!inside(n.x(), n.y())) continue;
                    if (testSelectPixel(p, n)) {
                        selected = true;
                        break;
                    }
                }
                if (selected) {
                    adaptive[y * width + x] = MAX_SELECTED;
                }
            }
        }
    }

    // Bottom-right to top-left pass
    for (int y = height - 1; y >= 0; --y) {
        for (int x = width - 1; x >= 0; --x) {
            const QPoint p(x, y);
            if (adaptive[y * width + x] == MIN_SELECTED && mask[y * width + x] != MIN_SELECTED) {
                bool selected = false;
                const QPoint candidates[] = {
                    QPoint(x + 1, y + 1), QPoint(x, y + 1), QPoint(x - 1, y + 1),
                    QPoint(x + 1, y)
                };
                for (const QPoint& n : candidates) {
                    if (n.y() <= y && !(n.y() == y && n.x() > x)) continue; // not "already visited"
                    if (!inside(n.x(), n.y())) continue;
                    if (testSelectPixel(p, n)) {
                        selected = true;
                        break;
                    }
                }
                if (selected) {
                    adaptive[y * width + x] = MAX_SELECTED;
                }
            }
        }
    }

    // Combine the adaptively grown mask with the normally grown mask: the
    // adaptive mask erases pixels of the normal growth
    for (int i = 0; i < width * height; ++i) {
        mask[i] *= (adaptive[i] != MIN_SELECTED);
    }
}

} // namespace FillFilters
