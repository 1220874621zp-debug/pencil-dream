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
#ifndef BRUSHSETTINGS_H
#define BRUSHSETTINGS_H

#include <QDomDocument>
#include <QDomElement>
#include <QString>

#include "brushcurve.h"

/**
 * 笔刷参数模型，参考 Krita KisPaintOpSettings / KisBrushModel 的精简版。
 * 一支笔刷 = 笔尖(Tip) + 描边(Stroke) + 动态(Dynamics) 三组参数，
 * 可整体序列化为 XML，作为预设存储（预设文件 = PNG 缩略图 + 内嵌 XML）。
 */
struct BrushSettings
{
    enum class TipShape
    {
        Circle,
        Rectangle
    };

    enum class SpacingMode
    {
        Auto,   // 间距 = 系数 * sqrt(直径)，小笔不断点、大笔不卡（Krita 默认）
        Fixed   // 间距 = 直径 * spacing
    };

    // ---- 笔尖 ----
    QString name;               // 预设名（序列化进 XML 根属性）
    TipShape tipShape = TipShape::Circle;
    qreal diameter = 24.0;      // 像素，1..200
    qreal ratio = 1.0;          // 椭圆短轴/长轴，0.05..1
    qreal angle = 0.0;          // 笔尖旋转角度，0..360
    qreal hardness = 0.65;      // 硬度 0.01..1：实心核占半径的比例（= Krita 1-fade）

    // ---- 描边 ----
    qreal opacity = 1.0;        // 笔刷不透明度 0.05..1（橡皮预设=擦除强度）
    SpacingMode spacingMode = SpacingMode::Auto;
    qreal spacing = 0.25;       // 固定间距（直径的比例）0.02..5
    qreal autoSpacingCoeff = 1.0;
    bool eraser = false;        // 橡皮预设：dab alpha 经 DestinationOut 变成擦除量

    // ---- 动态（压感）----
    bool pressureSize = true;
    BrushCurve sizeCurve;       // 压感 → 大小比例
    bool pressureOpacity = false;
    BrushCurve opacityCurve;    // 压感 → 不透明度比例

    QDomElement toXML(QDomDocument& doc) const;
    void fromXML(const QDomElement& element);

    QString toXMLString() const;
    static bool fromXMLString(const QString& xml, BrushSettings& out);
};

#endif // BRUSHSETTINGS_H
