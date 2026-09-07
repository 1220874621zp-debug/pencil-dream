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
#include "brushengine.h"

#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QtMath>

#include "graphics/bitmap/washblend.h"

namespace
{

// 参考 Krita KisCircleMaskGenerator::valueAt / KisRectMaskGenerator：
// 返回归一化距离 n（0=中心，>=1=笔尖外）
qreal normalizedDistance(qreal dx, qreal dy, qreal rx, qreal ry,
                         BrushSettings::TipShape shape)
{
    if (shape == BrushSettings::TipShape::Rectangle) {
        return qMax(qAbs(dx) / rx, qAbs(dy) / ry);
    }
    const qreal nx = dx / rx;
    const qreal ny = dy / ry;
    return qSqrt(nx * nx + ny * ny);
}

// 归一化坐标外扩 1 像素再算（Krita 的 antialiasEdges 技巧，保证硬边也有 1px 过渡）
qreal normalizedDistanceAA(qreal dx, qreal dy, qreal rx, qreal ry,
                           BrushSettings::TipShape shape)
{
    const qreal ax = qAbs(dx) + 1.0;
    const qreal ay = qAbs(dy) + 1.0;
    if (shape == BrushSettings::TipShape::Rectangle) {
        return qMax(ax / rx, ay / ry);
    }
    const qreal nx = ax / rx;
    const qreal ny = ay / ry;
    return qSqrt(nx * nx + ny * ny);
}

/**
 * 生成一个上好色的 dab 图。
 * 数学移植自 Krita 圆形/矩形掩码生成器：
 *   n  = 归一化距离；n >= 1 → 全透明
 *   nf = n / hardness（实心核比例，对应 Krita 的 fade 系数的倒数关系）
 *   不透明度 = 1 - n * (nf - 1) / (nf - n)
 * subPixelX/Y ∈ {0, 0.5}：落点的半像素余数，烤进掩码中心位置
 * （Krita KisAutoBrush 的 subPixel 做法），合成侧因此整数对齐免重采样。
 */
QImage makeDabImage(const BrushSettings& settings, const QColor& color,
                    qreal diameter, qreal subPixelX, qreal subPixelY)
{
    const qreal major = qMax<qreal>(1, diameter);
    const qreal minor = qMax<qreal>(1, qRound(diameter * settings.ratio));
    const qreal rad = qDegreesToRadians(settings.angle);
    const qreal cosA = qCos(rad);
    const qreal sinA = qSin(rad);

    // 旋转后的外接矩形；尺寸保持偶数，让"图中心 + 半像素"仍是整数落点
    const qreal boxW = major * qAbs(cosA) + minor * qAbs(sinA);
    const qreal boxH = major * qAbs(sinA) + minor * qAbs(cosA);
    int imgW = qCeil(boxW) + 2;
    int imgH = qCeil(boxH) + 2;
    if (imgW & 1) ++imgW;
    if (imgH & 1) ++imgH;

    const qreal cx = imgW * 0.5 + subPixelX;
    const qreal cy = imgH * 0.5 + subPixelY;
    const qreal rx = major * 0.5;
    const qreal ry = minor * 0.5;
    const qreal hardness = qBound(0.01, settings.hardness, 1.0);

    const int cr = color.red();
    const int cg = color.green();
    const int cb = color.blue();
    const int ca = color.alpha();

    QImage dab(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    dab.fill(Qt::transparent);

    for (int py = 0; py < imgH; ++py) {
        QRgb* line = reinterpret_cast<QRgb*>(dab.scanLine(py));
        const qreal dy = py + 0.5 - cy;
        for (int px = 0; px < imgW; ++px) {
            const qreal dx = px + 0.5 - cx;

            // 逆旋转回笔尖坐标系
            const qreal xr = dx * cosA + dy * sinA;
            const qreal yr = -dx * sinA + dy * cosA;

            const qreal n = normalizedDistance(xr, yr, rx, ry, settings.tipShape);
            if (n >= 1.0) {
                continue;
            }

            const qreal nf = normalizedDistanceAA(xr, yr, rx * hardness, ry * hardness,
                                                  settings.tipShape);
            qreal alpha = 1.0;
            if (nf > 1.0) {
                alpha = 1.0 - n * (nf - 1.0) / (nf - n);
            }
            if (alpha <= 0.0) {
                continue;
            }

            const int a = qRound(alpha * 255.0 * ca / 255.0);
            line[px] = qPremultiply(qRgba(cr, cg, cb, a));
        }
    }
    return dab;
}

} // namespace

void BrushEngine::beginStroke(const QPointF& point, qreal pressure, const QColor& color,
                              const DabPainter& painter)
{
    mStrokeActive = true;
    mColor = color;
    mLastPoint = point;
    mLastPressure = pressure;
    mDabCache.clear();

    // 起笔先落一个 dab（Krita 行为：单击即出点）
    paintDab(point, pressure, painter);
    mRemainingDistance = spacingFor(dabDiameterAt(pressure));
}

void BrushEngine::strokeTo(const QPointF& point, qreal pressure, const DabPainter& painter)
{
    if (!mStrokeActive) {
        return;
    }
    const QLineF segment(mLastPoint, point);
    const qreal dist = segment.length();
    if (dist <= 0.0) {
        return;
    }

    // 沿线段按间距撒 dab；间距随 dab 直径变化（自动间距）
    while (mRemainingDistance <= dist) {
        const qreal t = mRemainingDistance / dist;
        const QPointF pos = mLastPoint + (point - mLastPoint) * t;
        const qreal pr = mLastPressure + (pressure - mLastPressure) * t;
        paintDab(pos, pr, painter);
        mRemainingDistance += spacingFor(dabDiameterAt(pr));
    }
    mRemainingDistance -= dist;

    mLastPoint = point;
    mLastPressure = pressure;
}

void BrushEngine::dabAt(const QPointF& point, qreal pressure, const DabPainter& painter)
{
    paintDab(point, pressure, painter);
}

void BrushEngine::endStroke()
{
    mStrokeActive = false;
    mDabCache.clear();
}

qreal BrushEngine::dabDiameterAt(qreal pressure) const
{
    const qreal scale = mSettings.pressureSize ? mSettings.sizeCurve.value(pressure) : 1.0;
    return qMax(1.0, mSettings.diameter * scale);
}

qreal BrushEngine::dabOpacityAt(qreal pressure) const
{
    const qreal scale = mSettings.pressureOpacity ? mSettings.opacityCurve.value(pressure) : 1.0;
    return qBound(0.0, mSettings.opacity * scale, 1.0);
}

qreal BrushEngine::spacingFor(qreal dabDiameter) const
{
    qreal spacing = 0.0;
    if (mSettings.spacingMode == BrushSettings::SpacingMode::Auto) {
        // Krita calcAutoSpacing：小笔不断点、大笔不卡的平方根间距
        spacing = mSettings.autoSpacingCoeff
                  * (dabDiameter < 1.0 ? dabDiameter : qSqrt(dabDiameter));
    } else {
        spacing = dabDiameter * mSettings.spacing;
    }
    return qBound(0.35, spacing, qMax(0.7, dabDiameter * 2.0));
}

void BrushEngine::paintDab(const QPointF& point, qreal pressure, const DabPainter& painter)
{
    if (!painter) {
        return;
    }

    // 直径量化到 4% 步长（Krita dab 缓存容差的同思路）：压感连续变化时
    // 只有跨步长才重生成掩码，配合 2x2 子像素桶把每笔的缓存规模压在
    // ~100 张以内（200px 软笔压感全程扫一遍也从 200 张降到 ~25 张）
    const qreal step = qMax(1.0, mSettings.diameter * 0.04);
    const int gridIndex = qMax(1, qRound(dabDiameterAt(pressure) / step));
    const qreal diameter = gridIndex * step;

    // 落点吸附半像素网格（误差 ≤0.25px，AA 边缘下不可见），余数作为
    // 子像素偏移烤进掩码；topLeft 因此恒为整数坐标
    const int snappedX = qRound(point.x() * 2.0);
    const int snappedY = qRound(point.y() * 2.0);
    const int subBucketX = snappedX & 1;
    const int subBucketY = snappedY & 1;

    DabRequest request;
    request.opacity = dabOpacityAt(pressure);
    request.dab = cachedDab((gridIndex << 2) | (subBucketX << 1) | subBucketY,
                            diameter, subBucketX * 0.5, subBucketY * 0.5);
    // 掩码中心在图内位于 (imgW/2 + subX)，落图后对准吸附点 (snappedX/2)
    request.topLeft = QPoint((snappedX >> 1) - request.dab.width() / 2,
                             (snappedY >> 1) - request.dab.height() / 2);
    painter(request);
}

const QImage& BrushEngine::cachedDab(quint32 cacheKey, qreal diameter,
                                     qreal subPixelX, qreal subPixelY)
{
    auto it = mDabCache.find(cacheKey);
    if (it == mDabCache.end()) {
        // 保险丝：极端压感抖动下防止缓存无限增长（正常一笔远到不了）
        if (mDabCache.size() > 96) {
            mDabCache.clear();
        }
        it = mDabCache.insert(cacheKey,
                              makeDabImage(mSettings, mColor, diameter, subPixelX, subPixelY));
    }
    return it.value();
}

QImage BrushEngine::renderStrokePreview(const BrushSettings& settings, const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(39, 39, 42));

    BrushSettings preview = settings;
    preview.diameter = qBound(3.0, qMin(settings.diameter, size.height() * 0.42), 200.0);
    if (preview.ratio < 1.0 && preview.angle != 0.0) {
        // 扁笔尖旋转后更高，稍微再压一点避免溢出
        preview.diameter = qMax(3.0, preview.diameter * 0.8);
    }

    QImage strokeLayer(size, QImage::Format_ARGB32_Premultiplied);
    strokeLayer.fill(Qt::transparent);

    BrushEngine engine;
    engine.setSettings(preview);

    const auto painter = [&strokeLayer](const DabRequest& dab) {
        washBlendImage(strokeLayer, dab.dab, dab.topLeft, dab.opacity);
    };

    QPainterPath path;
    path.moveTo(0.10 * size.width(), 0.70 * size.height());
    path.cubicTo(0.38 * size.width(), 0.02 * size.height(),
                 0.58 * size.width(), 1.06 * size.height(),
                 0.90 * size.width(), 0.30 * size.height());

    const int samples = 80;
    engine.mStrokeActive = true;
    engine.mColor = QColor(238, 238, 238);
    engine.mLastPoint = path.pointAtPercent(0.0);
    engine.mLastPressure = 0.15;
    engine.paintDab(engine.mLastPoint, 0.15, painter);
    engine.mRemainingDistance = engine.spacingFor(engine.dabDiameterAt(0.15));

    for (int i = 1; i <= samples; ++i) {
        const qreal t = qreal(i) / samples;
        const qreal pressure = 0.15 + 0.85 * qSin(t * M_PI);
        engine.strokeTo(path.pointAtPercent(t), pressure, painter);
    }

    QPainter composer(&image);
    composer.drawImage(0, 0, strokeLayer);
    composer.end();
    return image;
}
