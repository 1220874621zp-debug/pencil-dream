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
#ifndef BRUSHSETTINGS_H
#define BRUSHSETTINGS_H

#include <QDomDocument>
#include <QDomElement>
#include <QImage>
#include <QString>
#include <memory>

#include "brushcurve.h"

struct BrushSettings;

/**
 * 笔刷纹理设置（Krita KisTextureOption 的移植）。
 * 图案在载入时烘焙成 Alpha8 蒙版（KisTextureMaskInfo::recalculateMask 的公式），
 * 绘制时按 dab 的画布坐标平铺采样（锚定画布，不随笔刷走），对 dab 的 alpha
 * 做模式复合（与双笔尖同一套公式，带 strength 的非软变体）。
 */
struct BrushTextureSettings
{
    bool enabled = false;
    int mode = 15;              // Krita TexturingMode 编号（本引擎支持除 LIGHTNESS/GRADIENT 外的全部）
    qreal strength = 1.0;       // 0..1
    qreal brightness = 0.0;     // 烘焙期亮度偏移
    qreal contrast = 1.0;       // 烘焙期对比度（(v-0.5)*contrast+0.5）
    bool invert = false;
    qreal neutral = 0.5;        // 中性点：纹理值==neutral 对 dab 无影响
    qreal scale = 1.0;          // 图案缩放（烘焙期生效）
    int offsetX = 0;            // 采样偏移（画布坐标）
    int offsetY = 0;
    int cutoffPolicy = 0;       // 0=无 1=区间外不画(alpha=0) 2=区间外全画(alpha=1)
    int cutoffLeft = 0;         // 0..255
    int cutoffRight = 255;

    QImage pattern;             // 原始图案（序列化用）
    QImage bakedMask;           // 烘焙后的 Alpha8 蒙版（运行时）

    void bake();                // pattern + 上述参数 -> bakedMask
};

/** 双笔尖（Krita MaskingBrush 的移植）：副笔尖沿同一轨迹独立撒 dab，
 *  累积成覆盖蒙版，对主笔迹的 alpha 逐像素做模式复合（只改 alpha 不改颜色） */
struct BrushMaskSettings
{
    enum class Mode
    {
        Mult, Darken, Overlay, Dodge, Burn, LinearBurn,
        LinearDodge, HardMix, HardMixSofter, Subtract
    };

    static QString modeToString(Mode m);
    static Mode modeFromString(const QString& s, Mode fallback = Mode::Burn);

    bool enabled = false;
    qreal sizeCoeff = 1.0;      // 副笔尖直径 = 主直径 × 系数（Krita MasterSizeCoeff）
    Mode mode = Mode::Burn;
    std::unique_ptr<BrushSettings> sub; // 副笔刷完整参数（值语义拷贝）

    ~BrushMaskSettings();
    BrushMaskSettings() = default;
    BrushMaskSettings(const BrushMaskSettings& other);
    BrushMaskSettings& operator=(const BrushMaskSettings& other);
};

/**
 * 笔刷参数模型，参考 Krita KisPaintOpSettings / KisBrushModel 的精简版。
 * 一支笔刷 = 笔尖(Tip) + 描边(Stroke) + 动态(Dynamics) 三组参数，
 * 可整体序列化为 XML，作为预设存储（预设文件 = PNG 缩略图 + 内嵌 XML）。
 *
 * v2 扩展（Krita 5 移植）：图像笔尖(Image) / 双笔尖(Mask) / 纹理(Texture) /
 * 图案颜色源(ColorSource)。图像资源以 base64 PNG 内嵌在 XML 里（同 Krita 5
 * embedded_resources 的做法），预设仍是单文件。
 */
struct BrushSettings
{
    enum class TipShape
    {
        Circle,
        Rectangle,
        Image      // 自定义图像笔尖：亮度→mask（黑=不透明，白=透明，GIMP/Krita 语义）
    };

    enum class SpacingMode
    {
        Auto,   // 间距 = 系数 * sqrt(直径)，小笔不断点、大笔不卡（Krita 默认）
        Fixed   // 间距 = 直径 * spacing
    };

    enum class PaintingMode
    {
        Wash,     // 涂抹：alpha 向不透明度收敛，同笔不越叠越深（Krita WASH/ALPHA_DARKEN）
        Buildup   // 叠加：每个 dab 直接累积，反复描会变深（Krita BUILDUP）
    };

    enum class BlendMode
    {
        Normal,   // 正常
        Multiply, // 正片叠底（笔尖混合，对底色逐像素相乘）
        Screen    // 滤色
    };

    enum class ColorSource
    {
        Plain,    // 纯色（前景色）
        Pattern,  // 图案上色：按 dab 画布坐标平铺采样纹理图案作为笔色（Krita KoPatternColorSource）
        Clone     // 仿制上色（Panto）：按"画布坐标−偏移−源图原点"从源图采样，越界透明不平铺；
                  // 源图经 BrushEngine::setCloneSource 由工具层起笔时快照注入（不入预设 XML）
    };

    // ---- 笔尖 ----
    QString name;               // 预设名（序列化进 XML 根属性）
    TipShape tipShape = TipShape::Circle;
    qreal diameter = 24.0;      // 像素，1..200（图像笔尖=长边像素尺寸）
    qreal ratio = 1.0;          // 椭圆短轴/长轴，0.05..1（图像笔尖=纵向压缩）
    qreal angle = 0.0;          // 笔尖旋转角度，0..360
    qreal hardness = 0.65;      // 硬度 0.01..1：实心核占半径的比例（= Krita MaskGenerator 的 hfade/vfade）

    // 图像笔尖（tipShape == Image 时有效）
    QImage tipImage;            // 原始导入图
    bool tipAutoMidPoint = true;   // 烘焙时 midX 取 alpha 加权平均灰度（Krita AutoAdjustMidPoint）
    qreal tipMidPoint = 127.0;     // 手动折点（autoMidPoint=false 时）
    qreal tipBrightness = 0.0;     // KisColorfulBrush 亮度调整，-1..1
    qreal tipContrast = 0.0;       // 对比度调整，-1..1（1=二值化）
    QImage tipMask;             // 烘焙后的 Alpha8 蒙版：值=(255-灰度)×alpha/255

    // ---- 描边 ----
    qreal opacity = 1.0;        // 笔刷不透明度 0.05..1（橡皮预设=擦除强度）
    qreal flow = 1.0;           // 流量 0.01..1：涂抹模式下在"并集"与"收敛"间插值（Krita flow）
    SpacingMode spacingMode = SpacingMode::Auto;
    qreal spacing = 0.25;       // 固定间距（直径的比例）0.02..5
    qreal autoSpacingCoeff = 1.0;
    bool eraser = false;        // 橡皮预设：dab alpha 经 DestinationOut 变成擦除量

    // ---- 笔尖 ----
    qreal scatter = 0.0;        // 散布 0..5：dab 落点随机偏移量（×直径）
    PaintingMode paintingMode = PaintingMode::Wash;
    BlendMode blendMode = BlendMode::Normal;
    bool mirrorX = false;       // 水平镜像绘画（围绕画布中心）
    bool mirrorY = false;       // 垂直镜像绘画

    // ---- 喷枪 ----
    bool airbrushEnabled = false;
    int airbrushRate = 20;      // 每秒 dab 数 1..100

    // ---- 动态（压感）----
    bool pressureSize = true;
    BrushCurve sizeCurve;       // 压感 → 大小比例
    bool pressureOpacity = false;
    BrushCurve opacityCurve;    // 压感 → 不透明度比例

    // ---- v2 扩展 ----
    BrushMaskSettings mask;         // 双笔尖
    BrushTextureSettings texture;   // 笔刷纹理
    ColorSource colorSource = ColorSource::Plain; // 颜色源（Pattern 用 texture.pattern）

    void bakeTipMask();         // tipImage + 调整参数 -> tipMask（fromXML/UI 导入后调用）

    QDomElement toXML(QDomDocument& doc) const;
    void fromXML(const QDomElement& element);

    QString toXMLString() const;
    static bool fromXMLString(const QString& xml, BrushSettings& out);
};

#endif // BRUSHSETTINGS_H
