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
#include <QRandomGenerator>

#include "graphics/bitmap/washblend.h"
#include "maskedstrokecompositor.h"

namespace
{

inline int floorMod(int n, int m)
{
    return ((n % m) + m) % m;
}

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

/** 图像笔尖 dab（定义见下；掩码经 QTransform 缩放/旋转后上色） */
QImage makeImageDabImage(const BrushSettings& settings, const QColor& color,
                         qreal diameter, qreal subPixelX, qreal subPixelY);

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
    if (settings.tipShape == BrushSettings::TipShape::Image && !settings.tipMask.isNull()) {
        return makeImageDabImage(settings, color, diameter, subPixelX, subPixelY);
    }
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

/**
 * 图像笔尖 dab（Krita png_brush/预定义笔尖的移植）。
 * tipMask 是"白色按掩码预乘"的 ARGB 图（alpha=掩码）。变换矩阵与 Krita
 * KisQImagePyramid::baseBrushTransform 同构：以图像中心为基准，先非均匀
 * 缩放（scale=直径/max(tipW,tipH)，ratio 压纵向）再旋转，子像素偏移烤进
 * 平移；SmoothPixmapTransform 做双线性重采样。缩放/旋转后外接矩形定图
 * 尺寸（保持偶数对齐落点），上色 = 掩码 alpha × 笔色。
 */
QImage makeImageDabImage(const BrushSettings& settings, const QColor& color,
                         qreal diameter, qreal subPixelX, qreal subPixelY)
{
    const QImage& tip = settings.tipMask;
    const qreal tipMax = qMax<qreal>(1, qMax(tip.width(), tip.height()));
    const qreal scale = qMax<qreal>(0.001, diameter) / tipMax;
    const qreal rad = qDegreesToRadians(settings.angle);
    const qreal cosA = qAbs(qCos(rad));
    const qreal sinA = qAbs(qSin(rad));
    const qreal w = tip.width() * scale;
    const qreal h = tip.height() * scale * settings.ratio;

    int imgW = qCeil(w * cosA + h * sinA) + 2;
    int imgH = qCeil(w * sinA + h * cosA) + 2;
    if (imgW & 1) ++imgW;
    if (imgH & 1) ++imgH;

    QImage alphaImg(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    alphaImg.fill(Qt::transparent);
    {
        QPainter painter(&alphaImg);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.translate(imgW * 0.5 + subPixelX, imgH * 0.5 + subPixelY);
        painter.rotate(settings.angle);
        painter.scale(scale, scale * settings.ratio);
        painter.translate(-tip.width() * 0.5, -tip.height() * 0.5);
        painter.drawImage(QPointF(0.0, 0.0), tip);
    }

    const int cr = color.red();
    const int cg = color.green();
    const int cb = color.blue();
    const int ca = color.alpha();
    QImage dab(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    dab.fill(Qt::transparent);
    for (int py = 0; py < imgH; ++py) {
        const QRgb* src = reinterpret_cast<const QRgb*>(alphaImg.constScanLine(py));
        QRgb* line = reinterpret_cast<QRgb*>(dab.scanLine(py));
        for (int px = 0; px < imgW; ++px, ++src, ++line) {
            const int a = qAlpha(*src);
            if (a == 0) {
                continue;
            }
            *line = qPremultiply(qRgba(cr, cg, cb, a * ca / 255));
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
    mStrokeTimer.start();
    mLastDabTimeMs = 0;

    // 混合笔刷：首 dab 只定位不作画（kis_colorsmudgeop.cpp:196-199），
    // 采样基准由宿主在回调里记录
    if (!mSmudgeMode) {
        // 起笔先落一个 dab（Krita 行为：单击即出点）
        paintDab(point, pressure, painter);
    }
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

    // 散布：dab 落点加随机偏移（Krita KisScatterOption），间距仍按未散布路径计
    const qreal diameterRaw = dabDiameterAt(pressure);
    QPointF effectivePoint = point;
    if (mSettings.scatter > 0.0) {
        effectivePoint += scatterOffset(diameterRaw);
    }

    // 直径量化到 4% 步长（Krita dab 缓存容差的同思路）：压感连续变化时
    // 只有跨步长才重生成掩码，配合 2x2 子像素桶把每笔的缓存规模压在
    // ~100 张以内（200px 软笔压感全程扫一遍也从 200 张降到 ~25 张）
    const qreal step = qMax(1.0, mSettings.diameter * 0.04);
    const int gridIndex = qMax(1, qRound(diameterRaw / step));
    const qreal diameter = gridIndex * step;

    // 落点吸附半像素网格（误差 ≤0.25px，AA 边缘下不可见），余数作为
    // 子像素偏移烤进掩码；topLeft 因此恒为整数坐标
    const int snappedX = qRound(effectivePoint.x() * 2.0);
    const int snappedY = qRound(effectivePoint.y() * 2.0);
    const int subBucketX = snappedX & 1;
    const int subBucketY = snappedY & 1;

    const QImage& dab = cachedDab((gridIndex << 2) | (subBucketX << 1) | subBucketY,
                                  diameter, subBucketX * 0.5, subBucketY * 0.5);
    // 掩码中心在图内位于 (imgW/2 + subX)，落图后对准吸附点 (snappedX/2)
    const QPoint topLeft((snappedX >> 1) - dab.width() / 2,
                         (snappedY >> 1) - dab.height() / 2);
    emitDab(dab, topLeft, pressure, painter);
    mLastDabTimeMs = mStrokeTimer.elapsed();
}

void BrushEngine::setCloneSource(const QImage& source, const QPoint& sourceTopLeft,
                                 const QPointF& offset)
{
    // 采样按非预乘通道取色（qRed/qGreen/qBlue/qAlpha 为真值，与 Pattern 同式）；
    // BitmapImage 内部是 ARGB32_Premultiplied，这里统一转出
    mCloneSource = source.isNull() ? QImage()
                                   : source.convertToFormat(QImage::Format_ARGB32);
    mCloneTopLeft = sourceTopLeft;
    mCloneOffset = offset;
}

void BrushEngine::emitDab(const QImage& dab, const QPoint& topLeft, qreal pressure,
                          const DabPainter& painter)
{
    DabRequest request;
    request.dab = positionAppliedDab(dab, topLeft);
    request.topLeft = topLeft;
    request.opacity = dabOpacityAt(pressure);
    request.flow = qBound(0.01, mSettings.flow, 1.0);
    request.buildup = mSettings.paintingMode == BrushSettings::PaintingMode::Buildup;
    request.blendMode = static_cast<int>(mSettings.blendMode);
    request.perPixelColor = mSettings.colorSource == BrushSettings::ColorSource::Pattern
                            || mSettings.colorSource == BrushSettings::ColorSource::Clone;
    painter(request);

    // 镜像绘画：围绕对称中心再盖一枚翻转发（Krita mirror）。
    // 纹理/图案按画布位置生效 → 对翻转后的 dab 在其落点重新应用位置效果
    if (mMirrorCenterValid && (mSettings.mirrorX || mSettings.mirrorY)) {
        DabRequest mirror = request;
        mirror.dab = dab;
        QPoint tl = topLeft;
        if (mSettings.mirrorX) {
            tl.setX(qRound(2.0 * mMirrorCenter.x()) - (topLeft.x() + dab.width()));
            mirror.dab = dab.mirrored(true, false);
        }
        if (mSettings.mirrorY) {
            tl.setY(qRound(2.0 * mMirrorCenter.y()) - (topLeft.y() + dab.height()));
            mirror.dab = mirror.dab.mirrored(false, true);
        }
        mirror.topLeft = tl;
        mirror.dab = positionAppliedDab(mirror.dab, tl);
        painter(mirror);
    }
}

QImage BrushEngine::positionAppliedDab(const QImage& dab, const QPoint& topLeft) const
{
    const bool textureOn = mSettings.texture.enabled && !mSettings.texture.bakedMask.isNull();
    const bool patternOn = mSettings.colorSource == BrushSettings::ColorSource::Pattern
                           && !mSettings.texture.pattern.isNull();
    const bool cloneOn = mSettings.colorSource == BrushSettings::ColorSource::Clone
                         && !mCloneSource.isNull();
    if (!textureOn && !patternOn && !cloneOn) {
        return dab;
    }

    // 纹理/图案都锚定画布坐标：按 dab 落点平铺采样（Krita offset % maskSize）
    const QImage& texMask = mSettings.texture.bakedMask;
    const QImage& pattern = mSettings.texture.pattern;
    const int tw = textureOn ? texMask.width() : 1;
    const int th = textureOn ? texMask.height() : 1;
    const int pw = patternOn ? pattern.width() : 1;
    const int ph = patternOn ? pattern.height() : 1;
    const int cloneOx = qRound(mCloneOffset.x()) + mCloneTopLeft.x();
    const int cloneOy = qRound(mCloneOffset.y()) + mCloneTopLeft.y();

    QImage out = dab.copy();
    for (int y = 0; y < out.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        const int canvasY = topLeft.y() + y;
        for (int x = 0; x < out.width(); ++x, ++line) {
            const int a = qAlpha(*line);
            if (a == 0) {
                continue;
            }
            const int canvasX = topLeft.x() + x;
            int newA = a;
            if (textureOn) {
                const int tx = floorMod(canvasX - mSettings.texture.offsetX, tw);
                const int ty = floorMod(canvasY - mSettings.texture.offsetY, th);
                const qreal src = texMask.constScanLine(ty)[tx] / 255.0;
                newA = qRound(MaskedStrokeCompositor::textureOp(
                                  mSettings.texture.mode, src, a / 255.0,
                                  mSettings.texture.strength) * 255.0);
                if (newA <= 0) {
                    *line = 0;
                    continue;
                }
            }
            if (cloneOn) {
                // 仿制颜色源（Panto）：源图按 画布坐标−偏移−原点 带界采样，
                // 越界透明（不平铺）；alpha = 笔尖墨量 × 源像素 alpha
                const int sx = canvasX - cloneOx;
                const int sy = canvasY - cloneOy;
                if (sx < 0 || sy < 0 || sx >= mCloneSource.width() || sy >= mCloneSource.height()) {
                    *line = 0;
                    continue;
                }
                const QRgb sc = reinterpret_cast<const QRgb*>(
                    mCloneSource.constScanLine(sy))[sx];
                const int na = newA * qAlpha(sc) / 255;
                if (na <= 0) {
                    *line = 0;
                    continue;
                }
                *line = qPremultiply(qRgba(qRed(sc), qGreen(sc), qBlue(sc), na));
            } else if (patternOn) {
                // 图案颜色源（KoPatternColorSource）：颜色 = 图案在该画布位置的像素
                const QRgb pc = reinterpret_cast<const QRgb*>(
                    pattern.constScanLine(floorMod(canvasY, ph)))[floorMod(canvasX, pw)];
                *line = qPremultiply(qRgba(qRed(pc), qGreen(pc), qBlue(pc), newA));
            } else if (newA != a) {
                const qreal s = newA / qreal(a);
                *line = qPremultiply(qRgba(qRound(qRed(*line) * s),
                                           qRound(qGreen(*line) * s),
                                           qRound(qBlue(*line) * s),
                                           newA));
            }
        }
    }
    return out;
}

QPointF BrushEngine::scatterOffset(qreal diameter) const
{
    // 均匀散布 ±scatter×直径/2（两轴独立）
    QRandomGenerator* rng = QRandomGenerator::global();
    const qreal spread = mSettings.scatter * diameter;
    return QPointF((rng->generateDouble() - 0.5) * spread,
                   (rng->generateDouble() - 0.5) * spread);
}

void BrushEngine::airbrushTick(const DabPainter& painter)
{
    if (!mStrokeActive || !mSettings.airbrushEnabled || !painter) {
        return;
    }
    const qint64 intervalMs = qMax<qint64>(1, 1000 / qBound(1, mSettings.airbrushRate, 100));
    const qint64 now = mStrokeTimer.elapsed();
    // 静止时按速率补 dab；移动中的 dab 由 spacing 负责（paintDab 会重置时钟）
    while (now - mLastDabTimeMs >= intervalMs) {
        paintDab(mLastPoint, mLastPressure, painter);
    }
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
    // 缩略图要确定性：关掉随机散布/喷枪/镜像（形状语义不变）
    preview.scatter = 0.0;
    preview.airbrushEnabled = false;
    preview.mirrorX = preview.mirrorY = false;

    QImage strokeLayer(size, QImage::Format_ARGB32_Premultiplied);
    strokeLayer.fill(Qt::transparent);

    BrushEngine engine;
    engine.setSettings(preview);

    const auto painter = [&strokeLayer](const DabRequest& dab) {
        DabPasteParams params;
        params.opacity = dab.opacity;
        params.flow = dab.flow;
        params.buildup = dab.buildup;
        params.blendMode = dab.blendMode;
        params.perPixelColor = dab.perPixelColor;
        washBlendImage(strokeLayer, dab.dab, dab.topLeft, params);
    };

    QPainterPath path;
    path.moveTo(0.10 * size.width(), 0.70 * size.height());
    path.cubicTo(0.38 * size.width(), 0.02 * size.height(),
                 0.58 * size.width(), 1.06 * size.height(),
                 0.90 * size.width(), 0.30 * size.height());

    const auto runStroke = [&path, &size](BrushEngine& engine, const DabPainter& painter) {
        const int samples = 80;
        engine.mStrokeActive = true;
        engine.mLastPoint = path.pointAtPercent(0.0);
        engine.mLastPressure = 0.15;
        engine.paintDab(engine.mLastPoint, 0.15, painter);
        engine.mRemainingDistance = engine.spacingFor(engine.dabDiameterAt(0.15));
        for (int i = 1; i <= samples; ++i) {
            const qreal t = qreal(i) / samples;
            const qreal pressure = 0.15 + 0.85 * qSin(t * M_PI);
            engine.strokeTo(path.pointAtPercent(t), pressure, painter);
        }
    };
    engine.mColor = QColor(238, 238, 238);
    runStroke(engine, painter);

    // 双笔尖：副笔尖沿同一轨迹画白色 union 覆盖层，对主笔迹做 alpha 复合
    // （KisMaskingBrushRenderer::updateProjection 的预览版）
    if (settings.mask.enabled && settings.mask.sub) {
        QImage cover(size, QImage::Format_ARGB32_Premultiplied);
        cover.fill(Qt::transparent);
        BrushEngine maskEngine;
        BrushSettings sub = *settings.mask.sub;
        sub.diameter = qMax(1.0, preview.diameter * settings.mask.sizeCoeff);
        sub.scatter = 0.0;
        sub.airbrushEnabled = false;
        sub.mirrorX = sub.mirrorY = false;
        sub.eraser = false;
        sub.texture = BrushTextureSettings();
        sub.colorSource = BrushSettings::ColorSource::Plain;
        sub.mask.enabled = false;
        sub.mask.sub.reset();
        maskEngine.setSettings(sub);

        const auto coverPainter = [&cover](const DabRequest& dab) {
            DabPasteParams params;
            params.opacity = 1.0;
            params.flow = 1.0;
            params.buildup = true;
            washBlendImage(cover, dab.dab, dab.topLeft, params);
        };
        maskEngine.mColor = Qt::white;
        runStroke(maskEngine, coverPainter);

        MaskedStrokeCompositor::applyMaskOpToImage(strokeLayer, cover, settings.mask.mode);
    }

    QPainter composer(&image);
    composer.drawImage(0, 0, strokeLayer);
    composer.end();
    return image;
}
