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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
#ifndef COLORDISTANCE_H
#define COLORDISTANCE_H

#include <QRgb>

#include <array>
#include <cmath>

/** 感知色差基建：sRGB → CIELab（D65）→ ΔE（CIE76）。

 * 语义对齐 Krita KoColorSpace::difference() 的 lcms 实现
 * （plugins/color/lcms2engine/LcmsColorSpace.h：Lab ΔE，0..255）。
 * 与 ICC PCS(D50) 的数值偏差 <1~2，Pencil 无色彩管理可接受。
 * 使用方：颜色转透明度（colortoalpha）、拆分图层颜色（layersplitter）。
 */
namespace ColorDistance
{

struct LabF
{
    double L = 0.0;
    double a = 0.0;
    double b = 0.0;
};

// sRGB 分量 → 线性光（256 级 LUT，避免每像素 3 次 pow）
inline const std::array<double, 256>& srgbLinearLut()
{
    static const std::array<double, 256> lut = [] {
        std::array<double, 256> v{};
        for (int i = 0; i < 256; ++i)
        {
            const double c = i / 255.0;
            v[i] = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
        }
        return v;
    }();
    return lut;
}

// sRGB → CIELab（D65 白点，标准公式）
inline LabF rgbToLab(const int r, const int g, const int b)
{
    const auto& lut = srgbLinearLut();
    const double R = lut[r], G = lut[g], B = lut[b];

    const double X = 0.4124564 * R + 0.3575761 * G + 0.1804375 * B;
    const double Y = 0.2126729 * R + 0.7151522 * G + 0.0721750 * B;
    const double Z = 0.0193339 * R + 0.1191920 * G + 0.9503041 * B;

    constexpr double Xn = 0.95047, Yn = 1.0, Zn = 1.08883;
    const auto f = [](const double t) {
        return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0;
    };
    const double fx = f(X / Xn), fy = f(Y / Yn), fz = f(Z / Zn);
    return { 116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz) };
}

inline LabF rgbToLab(const QRgb c)
{
    return rgbToLab(qRed(c), qGreen(c), qBlue(c));
}

// CIE76 ΔE：sqrt(ΔL²+Δa²+Δb²)。需要 0..255 时调用方自行 clamp。
inline double deltaE(const LabF& x, const LabF& y)
{
    const double dL = x.L - y.L;
    const double da = x.a - y.a;
    const double db = x.b - y.b;
    return std::sqrt(dL * dL + da * da + db * db);
}

} // namespace ColorDistance

#endif // COLORDISTANCE_H
