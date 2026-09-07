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
#ifndef BRUSHENGINE_H
#define BRUSHENGINE_H

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QSize>

#include "brushsettings.h"

#include <functional>

/**
 * 位图笔刷引擎（参考 Krita 像素笔刷 KisBrushOp 的轻量移植）。
 *
 * 一次描边的流程：
 *   beginStroke() → strokeTo() × N → endStroke()
 * strokeTo 沿线段按间距切分 dab（Krita KisPaintOpUtils::paintLine 的
 * 距离累加算法 + 自动间距），每个 dab 通过回调交给宿主落到画布上。
 *
 * dab 是一张 ARGB32_Premultiplied 的上色小图（笔尖掩码 × 颜色）。
 * 直径量化到 4% 步长缓存（Krita dab 缓存容差的同思路），压感微调
 * 直接复用缓存图；落点吸附半像素网格，余下的子像素偏移在生成时
 * 烤进掩码（Krita subPixel 的做法），合成侧因此整数对齐免重采样。
 */
class BrushEngine
{
public:
    struct DabRequest
    {
        QPoint topLeft;   // dab 左上角（画布整数坐标，子像素已烤进 dab 图）
        QImage dab;       // 上色后的 dab 图（已含笔尖形状与颜色）
        qreal opacity = 1.0; // 该 dab 的不透明度
    };

    using DabPainter = std::function<void(const DabRequest&)>;

    void setSettings(const BrushSettings& settings) { mSettings = settings; }
    const BrushSettings& settings() const { return mSettings; }

    void beginStroke(const QPointF& point, qreal pressure, const QColor& color, const DabPainter& painter);
    void strokeTo(const QPointF& point, qreal pressure, const DabPainter& painter);
    void dabAt(const QPointF& point, qreal pressure, const DabPainter& painter);
    void endStroke();
    bool isStrokeActive() const { return mStrokeActive; }

    /** 压感(0..1)在该点的 dab 直径（像素） */
    qreal dabDiameterAt(qreal pressure) const;
    /** 压感(0..1)在该点的 dab 不透明度(0..1) */
    qreal dabOpacityAt(qreal pressure) const;

    /** 渲染预设缩略图：深色底 + 一条带压感变化的 S 形描边 */
    static QImage renderStrokePreview(const BrushSettings& settings, const QSize& size);

private:
    qreal spacingFor(qreal dabDiameter) const;
    void paintDab(const QPointF& point, qreal pressure, const DabPainter& painter);
    const QImage& cachedDab(quint32 cacheKey, qreal diameter, qreal subPixelX, qreal subPixelY);

    BrushSettings mSettings;
    QColor mColor;
    QHash<quint32, QImage> mDabCache;

    bool mStrokeActive = false;
    QPointF mLastPoint;
    qreal mLastPressure = 1.0;
    qreal mRemainingDistance = 0.0; // 距离下一个 dab 还差的距离
};

#endif // BRUSHENGINE_H
