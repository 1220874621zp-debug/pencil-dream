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
#include "brushsettings.h"

#include <QBuffer>
#include <QDomDocument>
#include <QtMath>

namespace
{

// QImage <-> base64 PNG（Krita 5 embedded_resources 同款做法：资源内嵌进预设 XML）
QString imageToBase64Png(const QImage& image)
{
    if (image.isNull()) {
        return QString();
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QString::fromLatin1(bytes.toBase64());
}

QImage base64PngToImage(const QString& base64)
{
    if (base64.isEmpty()) {
        return QImage();
    }
    const QByteArray bytes = QByteArray::fromBase64(base64.toLatin1());
    QImage image;
    image.loadFromData(bytes, "PNG");
    return image;
}

// alpha 加权平均灰度（Krita estimateImageAverage：auto mid point 的基准）
qreal estimateImageAverage(const QImage& image)
{
    qint64 lightnessSum = 0;
    qint64 alphaSum = 0;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb* pixel = reinterpret_cast<const QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x, ++pixel) {
            lightnessSum += qRound(qGray(*pixel) * qAlpha(*pixel) / 255.0);
            alphaSum += qAlpha(*pixel);
        }
    }
    return alphaSum == 0 ? 0.0 : 255.0 * qreal(lightnessSum) / alphaSum;
}

// luma 近似（Krita 纹理蒙版用 (11R+16G+5B)/32）
inline int textureLuma(QRgb c)
{
    return (11 * qRed(c) + 16 * qGreen(c) + 5 * qBlue(c)) / 32;
}

} // namespace

// ---------- BrushMaskSettings：值语义包装的副笔刷 ----------

BrushMaskSettings::~BrushMaskSettings() = default;

BrushMaskSettings::BrushMaskSettings(const BrushMaskSettings& other)
    : enabled(other.enabled)
    , sizeCoeff(other.sizeCoeff)
    , mode(other.mode)
    , sub(other.sub ? new BrushSettings(*other.sub) : nullptr)
{
}

BrushMaskSettings& BrushMaskSettings::operator=(const BrushMaskSettings& other)
{
    if (this != &other) {
        enabled = other.enabled;
        sizeCoeff = other.sizeCoeff;
        mode = other.mode;
        sub.reset(other.sub ? new BrushSettings(*other.sub) : nullptr);
    }
    return *this;
}

QString BrushMaskSettings::modeToString(Mode m)
{
    switch (m) {
    case Mode::Mult: return "mult";
    case Mode::Darken: return "darken";
    case Mode::Overlay: return "overlay";
    case Mode::Dodge: return "dodge";
    case Mode::LinearBurn: return "linear_burn";
    case Mode::LinearDodge: return "linear_dodge";
    case Mode::HardMix: return "hard_mix_photoshop";
    case Mode::HardMixSofter: return "hard_mix_softer_photoshop";
    case Mode::Subtract: return "subtract";
    case Mode::Burn:
    default: return "burn";
    }
}

BrushMaskSettings::Mode BrushMaskSettings::modeFromString(const QString& s, Mode fallback)
{
    if (s == "mult") return Mode::Mult;
    if (s == "darken") return Mode::Darken;
    if (s == "overlay") return Mode::Overlay;
    if (s == "dodge") return Mode::Dodge;
    if (s == "linear_burn") return Mode::LinearBurn;
    if (s == "linear_dodge") return Mode::LinearDodge;
    if (s == "hard_mix_photoshop") return Mode::HardMix;
    if (s == "hard_mix_softer_photoshop") return Mode::HardMixSofter;
    if (s == "subtract") return Mode::Subtract;
    if (s == "burn") return Mode::Burn;
    return fallback;
}

// ---------- 烘焙 ----------

void BrushSettings::bakeTipMask()
{
    tipMask = QImage();
    if (tipImage.isNull()) {
        return;
    }
    const QImage src = tipImage.convertToFormat(QImage::Format_ARGB32);

    // KisColorfulBrush::brushTipImage：灰度分段线性重映射（auto mid point 把
    // 平均灰度归一到 127；brightness 抬/压折点；contrast 拉斜率，=1 时二值化）
    const qreal midX = tipAutoMidPoint ? estimateImageAverage(src) : tipMidPoint;
    QImage adjusted = src;
    if (qAbs(midX - 127.0) > 0.1 || !qFuzzyIsNull(tipBrightness) || !qFuzzyIsNull(tipContrast)) {
        const qreal midY = tipBrightness > 0.0
                           ? 127.0 + (255.0 - 127.0) * tipBrightness
                           : 127.0 - 127.0 * (-tipBrightness);
        qreal loA = 0.0, hiA = 0.0, loB = 0.0, hiB = 255.0;
        if (!qFuzzyCompare(tipContrast, 1.0)) {
            if (tipContrast > 0.0) {
                loA = midY / (1.0 - tipContrast) / midX;
                hiA = (255.0 - midY) / (1.0 - tipContrast) / (255.0 - midX);
            } else {
                loA = midY * (1.0 + tipContrast) / midX;
                hiA = (255.0 - midY) * (1.0 + tipContrast) / (255.0 - midX);
            }
            loB = midY - midX * loA;
            hiB = midY - midX * hiA;
        }
        for (int y = 0; y < adjusted.height(); ++y) {
            QRgb* pixel = reinterpret_cast<QRgb*>(adjusted.scanLine(y));
            for (int x = 0; x < adjusted.width(); ++x, ++pixel) {
                int v = qGray(*pixel);
                v = v >= midX ? qMin(255, qRound(hiA * v + hiB))
                     : qMax(0, qRound(loA * v + loB));
                *pixel = qRgba(v, v, v, qAlpha(*pixel));
            }
        }
    }

    // mask = (255 - 灰度) × alpha / 255（KoColorSpaceTraits::fillGrayBrushWithColor：
    // 黑=不透明、白=透明——白底黑墨的书法笔尖天然正确）。
    // 存成"白色按 mask 预乘"的 ARGB32_Premultiplied（像素 = (a,a,a,a)），
    // 便于 QPainter 直接做缩放/旋转变换（变换后取 alpha 通道即掩码）
    tipMask = QImage(adjusted.size(), QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < adjusted.height(); ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(adjusted.scanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(tipMask.scanLine(y));
        for (int x = 0; x < adjusted.width(); ++x) {
            const int alpha = (255 - qGray(srcLine[x])) * qAlpha(srcLine[x]) / 255;
            dstLine[x] = qRgba(alpha, alpha, alpha, alpha);
        }
    }
}

void BrushTextureSettings::bake()
{
    bakedMask = QImage();
    if (pattern.isNull()) {
        return;
    }
    QImage src = pattern.convertToFormat(QImage::Format_ARGB32);
    if (scale > 0.0 && !qFuzzyCompare(scale, 1.0)) {
        const QSize scaled = src.size() * scale;
        if (scaled.width() >= 2 && scaled.height() >= 2) {
            src = src.scaled(scaled, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    }

    // KisTextureMaskInfo::recalculateMask：luma -> 亮度/对比度 -> 中性点两段线性 -> cutoff
    bakedMask = QImage(src.size(), QImage::Format_Alpha8);
    const int left = cutoffLeft;
    const int right = qMax(cutoffLeft + 1, cutoffRight);
    for (int y = 0; y < src.height(); ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(src.scanLine(y));
        quint8* dstLine = bakedMask.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const qreal a = qAlpha(srcLine[x]) / 255.0;
            qreal v = (textureLuma(srcLine[x]) / 255.0) * a + (1.0 - a); // 透明处当白
            v -= brightness;
            v = (v - 0.5) * contrast + 0.5;
            v = qBound(0.0, v, 1.0);
            if (invert) {
                v = 1.0 - v;
            }
            const qreal n = qBound(0.005, neutral, 0.995); // 中性点映射到 0.5
            v = (n >= 1.0 || v <= n) ? v / (2.0 * n) : 0.5 + (v - n) / (2.0 - 2.0 * n);
            v = qBound(0.0, v, 1.0);
            if (cutoffPolicy == 1 && (v * 255 < left || v * 255 > right)) {
                v = 0.0;   // 区间外不画
            } else if (cutoffPolicy == 2 && (v * 255 < left || v * 255 > right)) {
                v = 1.0;   // 区间外全画
            }
            dstLine[x] = quint8(qRound(v * 255.0));
        }
    }
}

// ---------- XML 序列化（v2，兼容 v1）----------

QDomElement BrushSettings::toXML(QDomDocument& doc) const
{
    QDomElement root = doc.createElement("PencilBrush");
    root.setAttribute("version", "2");
    root.setAttribute("name", name);

    QDomElement tip = doc.createElement("Tip");
    const QString shape = tipShape == TipShape::Circle ? "circle"
                         : tipShape == TipShape::Rectangle ? "rectangle" : "image";
    tip.setAttribute("shape", shape);
    tip.setAttribute("diameter", QString::number(diameter, 'f', 2));
    tip.setAttribute("ratio", QString::number(ratio, 'f', 3));
    tip.setAttribute("angle", QString::number(angle, 'f', 1));
    tip.setAttribute("hardness", QString::number(hardness, 'f', 3));
    if (tipShape == TipShape::Image && !tipImage.isNull()) {
        QDomElement image = doc.createElement("Image");
        image.setAttribute("autoMidPoint", tipAutoMidPoint ? "1" : "0");
        image.setAttribute("midPoint", QString::number(tipMidPoint, 'f', 1));
        image.setAttribute("brightness", QString::number(tipBrightness, 'f', 3));
        image.setAttribute("contrast", QString::number(tipContrast, 'f', 3));
        image.appendChild(doc.createTextNode(imageToBase64Png(tipImage)));
        tip.appendChild(image);
    }
    root.appendChild(tip);

    QDomElement stroke = doc.createElement("Stroke");
    stroke.setAttribute("opacity", QString::number(opacity, 'f', 3));
    stroke.setAttribute("flow", QString::number(flow, 'f', 3));
    stroke.setAttribute("spacingMode", spacingMode == SpacingMode::Auto ? "auto" : "fixed");
    stroke.setAttribute("spacing", QString::number(spacing, 'f', 3));
    stroke.setAttribute("autoSpacingCoeff", QString::number(autoSpacingCoeff, 'f', 2));
    stroke.setAttribute("eraser", eraser ? "1" : "0");
    stroke.setAttribute("scatter", QString::number(scatter, 'f', 3));
    stroke.setAttribute("paintingMode", paintingMode == PaintingMode::Wash ? "wash" : "buildup");
    stroke.setAttribute("blendMode",
                        blendMode == BlendMode::Multiply ? "multiply"
                        : blendMode == BlendMode::Screen ? "screen" : "normal");
    stroke.setAttribute("mirrorX", mirrorX ? "1" : "0");
    stroke.setAttribute("mirrorY", mirrorY ? "1" : "0");
    stroke.setAttribute("airbrush", airbrushEnabled ? "1" : "0");
    stroke.setAttribute("airbrushRate", QString::number(airbrushRate));
    root.appendChild(stroke);

    QDomElement dynamics = doc.createElement("Dynamics");
    dynamics.setAttribute("pressureSize", pressureSize ? "1" : "0");
    dynamics.setAttribute("sizeCurve", sizeCurve.toString());
    dynamics.setAttribute("pressureOpacity", pressureOpacity ? "1" : "0");
    dynamics.setAttribute("opacityCurve", opacityCurve.toString());
    root.appendChild(dynamics);

    if (mask.enabled && mask.sub) {
        QDomElement maskEl = doc.createElement("Mask");
        maskEl.setAttribute("enabled", "1");
        maskEl.setAttribute("sizeCoeff", QString::number(mask.sizeCoeff, 'f', 3));
        maskEl.setAttribute("mode", BrushMaskSettings::modeToString(mask.mode));
        // 副笔刷的 Tip/Stroke/Dynamics 直接挂在 Mask 下（复用同一序列化）
        const QDomElement subRoot = mask.sub->toXML(doc);
        for (QDomNode n = subRoot.firstChild(); !n.isNull(); n = n.nextSibling()) {
            maskEl.appendChild(n.cloneNode(true));
        }
        root.appendChild(maskEl);
    }

    if (texture.enabled && !texture.pattern.isNull()) {
        QDomElement texEl = doc.createElement("Texture");
        texEl.setAttribute("enabled", "1");
        texEl.setAttribute("mode", QString::number(texture.mode));
        texEl.setAttribute("strength", QString::number(texture.strength, 'f', 3));
        texEl.setAttribute("brightness", QString::number(texture.brightness, 'f', 3));
        texEl.setAttribute("contrast", QString::number(texture.contrast, 'f', 3));
        texEl.setAttribute("invert", texture.invert ? "1" : "0");
        texEl.setAttribute("neutral", QString::number(texture.neutral, 'f', 3));
        texEl.setAttribute("scale", QString::number(texture.scale, 'f', 3));
        texEl.setAttribute("offsetX", QString::number(texture.offsetX));
        texEl.setAttribute("offsetY", QString::number(texture.offsetY));
        texEl.setAttribute("cutoffPolicy", QString::number(texture.cutoffPolicy));
        texEl.setAttribute("cutoffLeft", QString::number(texture.cutoffLeft));
        texEl.setAttribute("cutoffRight", QString::number(texture.cutoffRight));
        QDomElement patternEl = doc.createElement("Pattern");
        patternEl.appendChild(doc.createTextNode(imageToBase64Png(texture.pattern)));
        texEl.appendChild(patternEl);
        root.appendChild(texEl);
    }

    if (colorSource == ColorSource::Pattern) {
        QDomElement cs = doc.createElement("ColorSource");
        cs.setAttribute("type", "pattern");
        root.appendChild(cs);
    }

    return root;
}

namespace
{

// 解析 <Tip>（主/副笔刷共用）
void parseTipElement(const QDomElement& tip, BrushSettings& s)
{
    const QString shape = tip.attribute("shape", "circle");
    s.tipShape = shape == "rectangle" ? BrushSettings::TipShape::Rectangle
                 : shape == "image" ? BrushSettings::TipShape::Image
                 : BrushSettings::TipShape::Circle;
    s.diameter = qBound(1.0, tip.attribute("diameter", "24").toDouble(), 200.0);
    s.ratio = qBound(0.05, tip.attribute("ratio", "1").toDouble(), 1.0);
    s.angle = qBound(0.0, tip.attribute("angle", "0").toDouble(), 360.0);
    s.hardness = qBound(0.01, tip.attribute("hardness", "0.65").toDouble(), 1.0);
    s.tipImage = QImage();
    s.tipMask = QImage();
    const QDomElement image = tip.firstChildElement("Image");
    if (!image.isNull()) {
        s.tipAutoMidPoint = image.attribute("autoMidPoint", "1") == "1";
        s.tipMidPoint = image.attribute("midPoint", "127").toDouble();
        s.tipBrightness = qBound(-1.0, image.attribute("brightness", "0").toDouble(), 1.0);
        s.tipContrast = qBound(-1.0, image.attribute("contrast", "0").toDouble(), 1.0);
        s.tipImage = base64PngToImage(image.text());
        s.bakeTipMask();
        if (s.tipImage.isNull()) {
            s.tipShape = BrushSettings::TipShape::Circle; // 资源损坏兜底
        }
    }
}

void parseStrokeElement(const QDomElement& stroke, BrushSettings& s)
{
    s.opacity = qBound(0.05, stroke.attribute("opacity", "1").toDouble(), 1.0);
    s.flow = qBound(0.01, stroke.attribute("flow", "1").toDouble(), 1.0);
    s.spacingMode = (stroke.attribute("spacingMode", "auto") == "fixed")
                    ? BrushSettings::SpacingMode::Fixed : BrushSettings::SpacingMode::Auto;
    s.spacing = qBound(0.02, stroke.attribute("spacing", "0.25").toDouble(), 5.0);
    s.autoSpacingCoeff = qBound(0.25, stroke.attribute("autoSpacingCoeff", "1").toDouble(), 5.0);
    s.eraser = stroke.attribute("eraser", "0") == "1";
    s.scatter = qBound(0.0, stroke.attribute("scatter", "0").toDouble(), 5.0);
    s.paintingMode = (stroke.attribute("paintingMode", "wash") == "buildup")
                     ? BrushSettings::PaintingMode::Buildup : BrushSettings::PaintingMode::Wash;
    const QString blend = stroke.attribute("blendMode", "normal");
    s.blendMode = (blend == "multiply") ? BrushSettings::BlendMode::Multiply
                  : (blend == "screen") ? BrushSettings::BlendMode::Screen
                  : BrushSettings::BlendMode::Normal;
    s.mirrorX = stroke.attribute("mirrorX", "0") == "1";
    s.mirrorY = stroke.attribute("mirrorY", "0") == "1";
    s.airbrushEnabled = stroke.attribute("airbrush", "0") == "1";
    s.airbrushRate = qBound(1, stroke.attribute("airbrushRate", "20").toInt(), 100);
}

void parseDynamicsElement(const QDomElement& dynamics, BrushSettings& s)
{
    s.pressureSize = dynamics.attribute("pressureSize", "1") == "1";
    s.sizeCurve = BrushCurve::fromString(dynamics.attribute("sizeCurve", "0,0;1,1;"));
    s.pressureOpacity = dynamics.attribute("pressureOpacity", "0") == "1";
    s.opacityCurve = BrushCurve::fromString(dynamics.attribute("opacityCurve", "0,0;1,1;"));
}

} // namespace

void BrushSettings::fromXML(const QDomElement& root)
{
    if (root.tagName() != "PencilBrush") {
        return;
    }

    name = root.attribute("name");

    QDomElement tip = root.firstChildElement("Tip");
    if (!tip.isNull()) {
        parseTipElement(tip, *this);
    }

    QDomElement stroke = root.firstChildElement("Stroke");
    if (!stroke.isNull()) {
        parseStrokeElement(stroke, *this);
    }

    QDomElement dynamics = root.firstChildElement("Dynamics");
    if (!dynamics.isNull()) {
        parseDynamicsElement(dynamics, *this);
    }

    // v2: 双笔尖
    QDomElement maskEl = root.firstChildElement("Mask");
    mask.enabled = false;
    mask.sub.reset();
    if (!maskEl.isNull() && maskEl.attribute("enabled", "0") == "1") {
        mask.sizeCoeff = qBound(0.05, maskEl.attribute("sizeCoeff", "1").toDouble(), 3.0);
        mask.mode = BrushMaskSettings::modeFromString(maskEl.attribute("mode", "burn"));
        BrushSettings sub;
        QDomElement subTip = maskEl.firstChildElement("Tip");
        if (!subTip.isNull()) parseTipElement(subTip, sub);
        QDomElement subStroke = maskEl.firstChildElement("Stroke");
        if (!subStroke.isNull()) parseStrokeElement(subStroke, sub);
        QDomElement subDynamics = maskEl.firstChildElement("Dynamics");
        if (!subDynamics.isNull()) parseDynamicsElement(subDynamics, sub);
        mask.sub = std::make_unique<BrushSettings>(sub);
        mask.enabled = true;
    }

    // v2: 纹理
    QDomElement texEl = root.firstChildElement("Texture");
    texture = BrushTextureSettings();
    if (!texEl.isNull() && texEl.attribute("enabled", "0") == "1") {
        texture.mode = qBound(0, texEl.attribute("mode", "15").toInt(), 15);
        if (texture.mode == 2 || texture.mode == 3) {
            texture.mode = 15; // LIGHTNESS/GRADIENT 无对应物，落到默认
        }
        texture.strength = qBound(0.0, texEl.attribute("strength", "1").toDouble(), 1.0);
        texture.brightness = qBound(-1.0, texEl.attribute("brightness", "0").toDouble(), 1.0);
        texture.contrast = texEl.attribute("contrast", "1").toDouble();
        texture.invert = texEl.attribute("invert", "0") == "1";
        texture.neutral = qBound(0.0, texEl.attribute("neutral", "0.5").toDouble(), 1.0);
        texture.scale = qBound(0.05, texEl.attribute("scale", "1").toDouble(), 16.0);
        texture.offsetX = texEl.attribute("offsetX", "0").toInt();
        texture.offsetY = texEl.attribute("offsetY", "0").toInt();
        texture.cutoffPolicy = qBound(0, texEl.attribute("cutoffPolicy", "0").toInt(), 2);
        texture.cutoffLeft = qBound(0, texEl.attribute("cutoffLeft", "0").toInt(), 254);
        texture.cutoffRight = qBound(1, texEl.attribute("cutoffRight", "255").toInt(), 255);
        const QDomElement patternEl = texEl.firstChildElement("Pattern");
        if (!patternEl.isNull()) {
            texture.pattern = base64PngToImage(patternEl.text());
            texture.bake();
            texture.enabled = !texture.pattern.isNull() && !texture.bakedMask.isNull();
        }
    }

    // v2: 颜色源
    colorSource = ColorSource::Plain;
    QDomElement cs = root.firstChildElement("ColorSource");
    if (!cs.isNull() && cs.attribute("type", "plain") == "pattern") {
        if (!texture.pattern.isNull()) {
            colorSource = ColorSource::Pattern;
        }
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
