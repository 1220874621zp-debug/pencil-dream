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
#include "catch.hpp"

#include "colortoalpha.h"

#include <QColor>
#include <QImage>
#include <QRgb>

namespace
{

QImage makeImage(const int w, const int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    return img;
}

// 直接写预乘像素（不透明色预乘与直通一致），保证断言精确
void fillRect(QImage& img, const int x0, const int y0, const int x1, const int y1, const QRgb premul)
{
    for (int y = y0; y <= y1; ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = x0; x <= x1; ++x)
            line[x] = premul;
    }
}

// 半透明像素：直通色预乘后写入
void fillRectStraight(QImage& img, const int x0, const int y0, const int x1, const int y1,
                      const QRgb straight)
{
    fillRect(img, x0, y0, x1, y1, qPremultiply(straight));
}

// 结果叠回目标色，返回合成后的直通 RGB（用于往返性校验）
QRgb compositeOver(const QRgb premulPx, const QRgb targetStraight)
{
    const int a = qAlpha(premulPx);
    if (a == 0)
        return targetStraight;
    const auto lift = [a](const int v) { return qMin(255, (v * 255 + a / 2) / a); };
    const double af = a / 255.0;
    const auto mix = [af, &lift](const int c, const int t) {
        return static_cast<int>(qRound(lift(c) * af + t * (1.0 - af)));
    };
    return qRgb(mix(qRed(premulPx), qRed(targetStraight)),
                mix(qGreen(premulPx), qGreen(targetStraight)),
                mix(qBlue(premulPx), qBlue(targetStraight)));
}

} // namespace

TEST_CASE("ColorToAlpha-white-background")
{
    SECTION("全白图整图转透明")
    {
        QImage img = makeImage(16, 8);
        fillRect(img, 0, 0, 15, 7, qRgb(255, 255, 255));
        ColorToAlphaParams p; // 默认白 + 100
        const int changed = ColorToAlpha::apply(img, p);
        REQUIRE(changed == 16 * 8);
        for (int y = 0; y < img.height(); ++y)
        {
            const QRgb* line = reinterpret_cast<const QRgb*>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x)
                REQUIRE(qAlpha(line[x]) == 0);
        }
    }

    SECTION("白底黑线：背景透明、黑线逐位不变")
    {
        QImage img = makeImage(16, 16);
        fillRect(img, 0, 0, 15, 15, qRgb(255, 255, 255));
        fillRect(img, 7, 0, 8, 15, qRgb(0, 0, 0)); // 中间竖线，ΔE=100≥阈值→全保留
        const int changed = ColorToAlpha::apply(img, ColorToAlphaParams{});
        REQUIRE(changed == 16 * 16 - 2 * 16);
        for (int y = 0; y < 16; ++y)
        {
            const QRgb* line = reinterpret_cast<const QRgb*>(img.scanLine(y));
            REQUIRE(qAlpha(line[5]) == 0);          // 白背景
            REQUIRE(line[7] == qRgba(0, 0, 0, 255)); // 黑线逐位不变
        }
    }
}

TEST_CASE("ColorToAlpha-gray-ramp")
{
    // 不透明灰阶：ΔE 随灰度升高递减 → α 递减；数值与 Lab 公式对齐（容差带防公式微调脆断）
    QImage img = makeImage(3, 1);
    fillRect(img, 0, 0, 0, 0, qRgb(64, 64, 64));
    fillRect(img, 1, 0, 1, 0, qRgb(128, 128, 128));
    fillRect(img, 2, 0, 2, 0, qRgb(192, 192, 192));

    ColorToAlpha::apply(img, ColorToAlphaParams{});
    const QRgb* line = reinterpret_cast<const QRgb*>(img.scanLine(0));
    const int a64 = qAlpha(line[0]);
    const int a128 = qAlpha(line[1]);
    const int a192 = qAlpha(line[2]);
    CHECK(a64 > a128);
    CHECK(a128 > a192);
    CHECK(a64 >= 178);  CHECK(a64 <= 195);  // ΔE≈72.9
    CHECK(a128 >= 110); CHECK(a128 <= 130); // ΔE≈46.4
    CHECK(a192 >= 50);  CHECK(a192 <= 65);  // ΔE≈22.4
}

TEST_CASE("ColorToAlpha-roundtrip")
{
    QImage img = makeImage(3, 1);
    const int grays[3] = { 64, 128, 192 };
    for (int i = 0; i < 3; ++i)
        fillRect(img, i, 0, i, 0, qRgb(grays[i], grays[i], grays[i]));

    ColorToAlpha::apply(img, ColorToAlphaParams{});
    const QRgb* line = reinterpret_cast<const QRgb*>(img.scanLine(0));
    for (int i = 0; i < 3; ++i)
    {
        const QRgb back = compositeOver(line[i], qRgb(255, 255, 255));
        INFO("gray=" << grays[i] << " back r=" << qRed(back));
        REQUIRE(qAbs(qRed(back) - grays[i]) <= 10);
        REQUIRE(qAbs(qGreen(back) - grays[i]) <= 10);
        REQUIRE(qAbs(qBlue(back) - grays[i]) <= 10);
    }
}

TEST_CASE("ColorToAlpha-alpha-only-reduce")
{
    SECTION("全透明像素逐位不动")
    {
        QImage img = makeImage(4, 4);
        img.fill(QRgb(0x00123456)); // 非零 RGB 的全透明像素（预乘下罕见但允许）
        const QImage before = img.copy();
        REQUIRE(ColorToAlpha::apply(img, ColorToAlphaParams{}) == 0);
        REQUIRE(img == before);
    }

    SECTION("透明度只降不升：更透明的像素保持原透明度")
    {
        QImage img = makeImage(1, 1);
        fillRectStraight(img, 0, 0, 0, 0, qRgba(128, 128, 128, 76)); // α≈0.3，ΔE≈46<100
        ColorToAlpha::apply(img, ColorToAlphaParams{});
        const QRgb px = reinterpret_cast<const QRgb*>(img.scanLine(0))[0];
        // 算出的新透明度 ≈0.46 高于原 0.3 → alpha 保持 76（颜色按新透明度反推变深属正常）
        REQUIRE(qAlpha(px) == 76);
    }
}

TEST_CASE("ColorToAlpha-colored-target")
{
    SECTION("彩色目标：同色转透明、差异大的色不动")
    {
        QImage img = makeImage(2, 1);
        fillRect(img, 0, 0, 0, 0, qRgb(255, 0, 0));     // 红
        fillRect(img, 1, 0, 1, 0, qRgb(255, 255, 255)); // 白（与红 ΔE 远超 100）

        ColorToAlphaParams p;
        p.targetColor = qRgb(255, 0, 0);
        ColorToAlpha::apply(img, p);

        const QRgb* line = reinterpret_cast<const QRgb*>(img.scanLine(0));
        REQUIRE(qAlpha(line[0]) == 0);                 // 红被剥掉
        REQUIRE(line[1] == qRgba(255, 255, 255, 255)); // 白逐位不变
    }
}

TEST_CASE("ColorToAlpha-threshold-bounds")
{
    SECTION("阈值=1：只有完全等于目标色的像素转透明")
    {
        QImage img = makeImage(2, 1);
        fillRect(img, 0, 0, 0, 0, qRgb(255, 255, 255));
        fillRect(img, 1, 0, 1, 0, qRgb(250, 250, 250)); // ΔE≈2.3≥1 → 保持

        ColorToAlphaParams p;
        p.threshold = 1;
        const int changed = ColorToAlpha::apply(img, p);
        REQUIRE(changed == 1);
        const QRgb* line = reinterpret_cast<const QRgb*>(img.scanLine(0));
        REQUIRE(qAlpha(line[0]) == 0);
        REQUIRE(line[1] == qRgba(250, 250, 250, 255));
    }
}

TEST_CASE("ColorToAlpha-format-guard")
{
    QImage img(4, 4, QImage::Format_ARGB32); // 非预乘：契约外输入，应安全返回 0
    img.fill(Qt::white);
    REQUIRE(ColorToAlpha::apply(img, ColorToAlphaParams{}) == 0);
}
