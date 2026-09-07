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
#include "brushsettings.h"

#include <QtMath>

QDomElement BrushSettings::toXML(QDomDocument& doc) const
{
    QDomElement root = doc.createElement("PencilBrush");
    root.setAttribute("version", "1");
    root.setAttribute("name", name);

    QDomElement tip = doc.createElement("Tip");
    tip.setAttribute("shape", tipShape == TipShape::Circle ? "circle" : "rectangle");
    tip.setAttribute("diameter", QString::number(diameter, 'f', 2));
    tip.setAttribute("ratio", QString::number(ratio, 'f', 3));
    tip.setAttribute("angle", QString::number(angle, 'f', 1));
    tip.setAttribute("hardness", QString::number(hardness, 'f', 3));
    root.appendChild(tip);

    QDomElement stroke = doc.createElement("Stroke");
    stroke.setAttribute("opacity", QString::number(opacity, 'f', 3));
    stroke.setAttribute("spacingMode", spacingMode == SpacingMode::Auto ? "auto" : "fixed");
    stroke.setAttribute("spacing", QString::number(spacing, 'f', 3));
    stroke.setAttribute("autoSpacingCoeff", QString::number(autoSpacingCoeff, 'f', 2));
    root.appendChild(stroke);

    QDomElement dynamics = doc.createElement("Dynamics");
    dynamics.setAttribute("pressureSize", pressureSize ? "1" : "0");
    dynamics.setAttribute("sizeCurve", sizeCurve.toString());
    dynamics.setAttribute("pressureOpacity", pressureOpacity ? "1" : "0");
    dynamics.setAttribute("opacityCurve", opacityCurve.toString());
    root.appendChild(dynamics);

    return root;
}

void BrushSettings::fromXML(const QDomElement& root)
{
    if (root.tagName() != "PencilBrush") {
        return;
    }

    name = root.attribute("name");

    QDomElement tip = root.firstChildElement("Tip");
    if (!tip.isNull()) {
        tipShape = (tip.attribute("shape", "circle") == "rectangle")
                   ? TipShape::Rectangle : TipShape::Circle;
        diameter = qBound(1.0, tip.attribute("diameter", "24").toDouble(), 200.0);
        ratio = qBound(0.05, tip.attribute("ratio", "1").toDouble(), 1.0);
        angle = qBound(0.0, tip.attribute("angle", "0").toDouble(), 360.0);
        hardness = qBound(0.01, tip.attribute("hardness", "0.65").toDouble(), 1.0);
    }

    QDomElement stroke = root.firstChildElement("Stroke");
    if (!stroke.isNull()) {
        opacity = qBound(0.05, stroke.attribute("opacity", "1").toDouble(), 1.0);
        spacingMode = (stroke.attribute("spacingMode", "auto") == "fixed")
                      ? SpacingMode::Fixed : SpacingMode::Auto;
        spacing = qBound(0.02, stroke.attribute("spacing", "0.25").toDouble(), 5.0);
        autoSpacingCoeff = qBound(0.25, stroke.attribute("autoSpacingCoeff", "1").toDouble(), 5.0);
    }

    QDomElement dynamics = root.firstChildElement("Dynamics");
    if (!dynamics.isNull()) {
        pressureSize = dynamics.attribute("pressureSize", "1") == "1";
        sizeCurve = BrushCurve::fromString(dynamics.attribute("sizeCurve", "0,0;1,1;"));
        pressureOpacity = dynamics.attribute("pressureOpacity", "0") == "1";
        opacityCurve = BrushCurve::fromString(dynamics.attribute("opacityCurve", "0,0;1,1;"));
    }
}

QString BrushSettings::toXMLString() const
{
    QDomDocument doc;
    doc.appendChild(toXML(doc));
    return doc.toString(-1); // 紧凑输出，适合塞进 PNG 文本块
}

bool BrushSettings::fromXMLString(const QString& xml, BrushSettings& out)
{
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        return false;
    }
    QDomElement root = doc.documentElement();
    if (root.tagName() != "PencilBrush") {
        return false;
    }
    out.fromXML(root);
    return true;
}
