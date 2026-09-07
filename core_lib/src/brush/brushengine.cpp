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
 */
QImage makeDabImage(const BrushSettings& settings, const QColor& color, int diameterPx)
{
    const qreal major = qMax<qreal>(1, diameterPx);
    const qreal minor = qMax<qreal>(1, qRound(diameterPx * settings.ratio));
    const qreal rad = qDegreesToRadians(settings.angle);
    const qreal cosA = qCos(rad);
    const qreal sinA = qSin(rad);

    // 旋转后的外接矩形
    const qreal boxW = major * qAbs(cosA) + minor * qAbs(sinA);
    const qreal boxH = major * qAbs(sinA) + minor * qAbs(cosA);
    const int imgW = qCeil(boxW) + 2;
    const int imgH = qCeil(boxH) + 2;

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
        const qreal dy = py + 0.5 - imgH * 0.5;
        for (int px = 0; px < imgW; ++px) {
            const qreal dx = px + 0.5 - imgW * 0.5;

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
    DabRequest request;
    request.center = point;
    request.opacity = dabOpacityAt(pressure);
    request.dab = cachedDab(qMax(1, qRound(dabDiameterAt(pressure))));
    painter(request);
}

const QImage& BrushEngine::cachedDab(int diameterPx)
{
    auto it = mDabCache.find(diameterPx);
    if (it == mDabCache.end()) {
        it = mDabCache.insert(diameterPx, makeDabImage(mSettings, mColor, diameterPx));
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
        washBlendImage(strokeLayer, dab.dab,
                       QPointF(dab.center.x() - dab.dab.width() * 0.5,
                               dab.center.y() - dab.dab.height() * 0.5),
                       dab.opacity);
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
