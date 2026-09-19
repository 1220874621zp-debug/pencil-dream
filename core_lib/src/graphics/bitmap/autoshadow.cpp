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
#include "autoshadow.h"

#include <QImage>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{

constexpr int ALPHA_MIN = 16; // α≥此值视为不透明内容

int clampInt(const int v, const int lo, const int hi)
{
    return std::min(hi, std::max(lo, v));
}

float smoothStep(const float e0, const float e1, const float x)
{
    if (e1 <= e0)
        return x < e0 ? 0.0f : 1.0f;
    const float t = std::min(1.0f, std::max(0.0f, (x - e0) / (e1 - e0)));
    return t * t * (3.0f - 2.0f * t);
}

/** 双线性采样（边界钳位） */
float bilinearSample(const std::vector<float>& buf, const int w, const int h, double x, double y)
{
    x = std::min(static_cast<double>(w - 1), std::max(0.0, x));
    y = std::min(static_cast<double>(h - 1), std::max(0.0, y));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const double fx = x - x0;
    const double fy = y - y0;
    const float a = buf[static_cast<size_t>(y0) * w + x0];
    const float b = buf[static_cast<size_t>(y0) * w + x1];
    const float c = buf[static_cast<size_t>(y1) * w + x0];
    const float d = buf[static_cast<size_t>(y1) * w + x1];
    return static_cast<float>((a * (1.0 - fx) + b * fx) * (1.0 - fy) + (c * (1.0 - fx) + d * fx) * fy);
}

/** ── 高斯模糊：3 次盒式模糊近似（分离前缀和，O(N) 与半径无关）── */

std::vector<int> boxesForGauss(const float sigma, const int boxCount)
{
    const float wIdeal = std::sqrt((12.0f * sigma * sigma / static_cast<float>(boxCount)) + 1.0f);
    int wl = static_cast<int>(std::floor(wIdeal));
    if (wl % 2 == 0)
        --wl;
    if (wl < 1)
        wl = 1;
    const int wu = wl + 2;
    const float mIdeal = (12.0f * sigma * sigma
                          - static_cast<float>(boxCount) * wl * wl
                          - static_cast<float>(boxCount) * wl
                          - static_cast<float>(boxCount) / 4.0f)
                         / static_cast<float>(-4 * wl - 4);
    const int m = static_cast<int>(std::round(mIdeal));
    std::vector<int> sizes;
    for (int i = 0; i < boxCount; ++i)
        sizes.push_back(i < m ? wl : wu);
    return sizes;
}

void boxBlurH(std::vector<float>& buf, std::vector<float>& tmp, const int w, const int h, const int r)
{
    if (r <= 0)
        return;
    const float inv = 1.0f / static_cast<float>(2 * r + 1);
    for (int y = 0; y < h; ++y)
    {
        const float* line = &buf[static_cast<size_t>(y) * w];
        float* out = &tmp[static_cast<size_t>(y) * w];
        float acc = line[0] * static_cast<float>(r + 1);
        for (int i = 1; i <= r; ++i)
            acc += line[std::min(i, w - 1)];
        for (int x = 0; x < w; ++x)
        {
            out[x] = acc * inv;
            acc += line[std::min(x + r + 1, w - 1)] - line[std::max(x - r, 0)];
        }
    }
    std::swap(buf, tmp);
}

void boxBlurV(std::vector<float>& buf, std::vector<float>& tmp, const int w, const int h, const int r)
{
    if (r <= 0)
        return;
    const float inv = 1.0f / static_cast<float>(2 * r + 1);
    for (int x = 0; x < w; ++x)
    {
        float acc = buf[static_cast<size_t>(x)] * static_cast<float>(r + 1);
        for (int i = 1; i <= r; ++i)
            acc += buf[static_cast<size_t>(std::min(i, h - 1)) * w + x];
        for (int y = 0; y < h; ++y)
        {
            tmp[static_cast<size_t>(y) * w + x] = acc * inv;
            acc += buf[static_cast<size_t>(std::min(y + r + 1, h - 1)) * w + x]
                 - buf[static_cast<size_t>(std::max(y - r, 0)) * w + x];
        }
    }
    std::swap(buf, tmp);
}

void gaussBlur(std::vector<float>& buf, std::vector<float>& tmp, const int w, const int h, const float sigma)
{
    const std::vector<int> boxes = boxesForGauss(sigma, 3);
    for (const int size : boxes)
    {
        const int r = (size - 1) / 2;
        if (r <= 0)
            continue;
        boxBlurH(buf, tmp, w, h, r);
        boxBlurV(buf, tmp, w, h, r);
    }
}

/** 单通道混合（预乘域）：mode 决定公式，返回混合后的预乘分量（浮点，外层统一加权后再取整） */
double blendChannel(const int premul, const int alpha, const int s, const AutoShadowBlendMode mode)
{
    switch (mode)
    {
    case AutoShadowBlendMode::Multiply:
        return premul * static_cast<double>(s) / 255.0;
    case AutoShadowBlendMode::Normal:
        return s * static_cast<double>(alpha) / 255.0;
    case AutoShadowBlendMode::LinearBurn:
    {
        if (alpha >= 255)
            return std::max(0, premul + s - 255);
        const int straight = std::min(255, (premul * 255 + alpha / 2) / alpha);
        return std::max(0, std::min(255, straight + s - 255)) * static_cast<double>(alpha) / 255.0;
    }
    }
    return premul;
}

/** 简单阻塞：分离两趟 min/max 形态学（正值=阻塞/收缩、负值=扩展），r 像素 */
void morphChoke(std::vector<float>& buf, std::vector<float>& tmp, const int w, const int h, const int r, const bool choke)
{
    if (r <= 0)
        return;
    const auto fold = [choke](const float a, const float b) { return choke ? std::min(a, b) : std::max(a, b); };
    // 水平趟：buf → tmp
    for (int y = 0; y < h; ++y)
    {
        const float* src = &buf[static_cast<size_t>(y) * w];
        float* dst = &tmp[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x)
        {
            float acc = src[std::max(0, x - r)];
            for (int i = std::max(0, x - r) + 1; i <= std::min(w - 1, x + r); ++i)
                acc = fold(acc, src[i]);
            dst[x] = acc;
        }
    }
    // 垂直趟：tmp → buf
    for (int y = 0; y < h; ++y)
    {
        float* dst = &buf[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x)
        {
            float acc = tmp[static_cast<size_t>(std::max(0, y - r)) * w + x];
            for (int i = std::max(0, y - r) + 1; i <= std::min(h - 1, y + r); ++i)
                acc = fold(acc, tmp[static_cast<size_t>(i) * w + x]);
            dst[x] = acc;
        }
    }
}

/** 掩膜生成（黑透白不透 + 简单阻塞）：apply 与遮罩视图共用 */
struct MatteData
{
    int w = 0;
    int h = 0;
    int minX = 0, minY = 0, maxX = -1, maxY = -1; // maxY<0 = 无内容
    std::vector<uint8_t> contentMask; // 原图内容（α≥16）
    std::vector<float> maskF;         // 阻塞后的掩膜（1=不透明白，0=透明黑/洞）

    bool valid() const { return maxX >= 0; }
};

MatteData buildMatte(const QImage& img, const int maskThreshold, const int chokeMatte)
{
    MatteData m;
    m.w = img.width();
    m.h = img.height();
    if (m.w <= 0 || m.h <= 0)
        return m;
    m.contentMask.assign(static_cast<size_t>(m.w) * m.h, 0);
    m.maskF.assign(static_cast<size_t>(m.w) * m.h, 0.0f);
    m.minX = m.w; m.minY = m.h; m.maxX = -1; m.maxY = -1;

    for (int y = 0; y < m.h; ++y)
    {
        const auto* line = reinterpret_cast<const QRgb*>(img.scanLine(y));
        const size_t row = static_cast<size_t>(y) * m.w;
        for (int x = 0; x < m.w; ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a < ALPHA_MIN)
                continue;
            m.contentMask[row + x] = 1;
            const auto lift = [a](const int premul) { return std::min(255, (premul * 255 + a / 2) / a); };
            const int gray = (299 * lift(qRed(px)) + 587 * lift(qGreen(px)) + 114 * lift(qBlue(px))) / 1000;
            if (gray >= maskThreshold)
                m.maskF[row + x] = 1.0f;
            m.minX = std::min(m.minX, x);
            m.maxX = std::max(m.maxX, x);
            m.minY = std::min(m.minY, y);
            m.maxY = std::max(m.maxY, y);
        }
    }

    if (chokeMatte != 0 && m.valid())
    {
        std::vector<float> tmp(m.maskF.size());
        morphChoke(m.maskF, tmp, m.w, m.h, std::abs(chokeMatte), chokeMatte > 0);
    }
    return m;
}

} // namespace

namespace AutoShadow
{

int apply(QImage& img, const AutoShadowParams& params)
{
    if (img.isNull() || img.format() != QImage::Format_ARGB32_Premultiplied)
        return 0;
    const int w = img.width();
    const int h = img.height();
    if (w <= 0 || h <= 0)
        return 0;

    // 阈值清洗：递增且落在 1..100（乱序输入按就近可用值收敛，预览与实跑同规则）
    const int t1 = clampInt(params.thresholds[0], 1, 98);
    const int t2 = clampInt(params.thresholds[1], t1 + 1, 99);
    const int t3 = clampInt(params.thresholds[2], t2 + 1, 100);
    const double t1f = t1, t2f = t2, t3f = t3;

    // 光源=图像归一化坐标（可越界放远光），钳到 ±10 防极端值
    const double lightX = std::min(10.0, std::max(-10.0, params.lightX)) * w;
    const double lightY = std::min(10.0, std::max(-10.0, params.lightY)) * h;
    const int maskThreshold = clampInt(params.maskThreshold, 1, 254);
    const int chokeMatte = clampInt(params.chokeMatte, -50, 50);
    const double distance = std::max(1.0, static_cast<double>(params.shadowDistance));
    const int blurSize = clampInt(params.shadowSize, 0, 200);
    const float feather = std::max(0.0f, static_cast<float>(params.edgeFeather));

    // 色带（反转=镜像）
    AutoShadowLevel levels[4];
    for (int i = 0; i < 4; ++i)
    {
        const AutoShadowLevel& src = params.levels[params.invertLevels ? 3 - i : i];
        levels[i].color = src.color;
        levels[i].mode = src.mode;
    }

    // ── 掩膜生成段：去色阈值（黑透白不透）+ 简单阻塞
    const MatteData m = buildMatte(img, maskThreshold, chokeMatte);
    if (!m.valid())
        return 0;
    const std::vector<uint8_t>& contentMask = m.contentMask;
    const std::vector<float>& maskF = m.maskF;
    const int minX = m.minX, minY = m.minY, maxX = m.maxX, maxY = m.maxY;

    // ── 内阴影场段：取样掩膜 = 掩膜沿背光方向平移（逐像素径向，光源远≈平行），再高斯模糊
    const size_t count = maskF.size();
    std::vector<float> shifted(count, 0.0f);
    for (int y = minY; y <= maxY; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = minX; x <= maxX; ++x)
        {
            if (contentMask[row + x] == 0)
                continue;
            const double dx = x - lightX;
            const double dy = y - lightY;
            const double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0)
            {
                // 光源就在像素上：该像素正对光，取样自身（掩膜=1 → 无阴影）
                shifted[row + x] = maskF[row + x];
                continue;
            }
            shifted[row + x] = bilinearSample(maskF, w, h,
                                              x + dx / len * distance,
                                              y + dy / len * distance);
        }
    }
    if (blurSize > 0)
    {
        std::vector<float> tmp(count);
        gaussBlur(shifted, tmp, w, h, std::max(1.0f, blurSize / 2.0f));
    }

    // ── 映射段：shadow = mask − blur(shift)，钳 0..1 → 场值 0..100 → 色阶权重
    const double levelS[4][3] = {
        { qRed(levels[0].color), qGreen(levels[0].color), qBlue(levels[0].color) },
        { qRed(levels[1].color), qGreen(levels[1].color), qBlue(levels[1].color) },
        { qRed(levels[2].color), qGreen(levels[2].color), qBlue(levels[2].color) },
        { qRed(levels[3].color), qGreen(levels[3].color), qBlue(levels[3].color) },
    };

    int changed = 0;
    for (int y = 0; y < h; ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            if (contentMask[row + x] == 0)
                continue;
            const double shadowRaw = maskF[row + x] - shifted[row + x];
            const float F = static_cast<float>(std::min(1.0, std::max(0.0, shadowRaw)) * 100.0);
            const QRgb px = line[x];
            const int alpha = qAlpha(px);

            // 色阶权重 w0..w3（和恒为 1）：三条越界进度 s1/s2/s3 链式组合
            double s1, s2, s3;
            const auto ramp = [](const double lo, const double hi, const double v) {
                return std::min(1.0, std::max(0.0, (v - lo) / (hi - lo)));
            };
            if (params.smooth)
            {
                s1 = ramp(0.0, t1f, F);
                s2 = ramp(t1f, t2f, F);
                s3 = ramp(t2f, t3f, F);
            }
            else
            {
                const float half = feather * 0.5f;
                s1 = smoothStep(t1f - half, t1f + half, F);
                s2 = smoothStep(t2f - half, t2f + half, F);
                s3 = smoothStep(t3f - half, t3f + half, F);
            }
            const double w[4] = {
                1.0 - s1,
                s1 * (1.0 - s2),
                s2 * (1.0 - s3),
                s3,
            };

            // 各阶独立混合后按权重加权（模式不同也能连续过渡），一次取整
            double outR = 0.0, outG = 0.0, outB = 0.0;
            for (int i = 0; i < 4; ++i)
            {
                if (w[i] <= 0.0)
                    continue;
                outR += w[i] * blendChannel(qRed(px), alpha, static_cast<int>(levelS[i][0]), levels[i].mode);
                outG += w[i] * blendChannel(qGreen(px), alpha, static_cast<int>(levelS[i][1]), levels[i].mode);
                outB += w[i] * blendChannel(qBlue(px), alpha, static_cast<int>(levelS[i][2]), levels[i].mode);
            }
            const QRgb out = qRgba(qRound(outR), qRound(outG), qRound(outB), alpha);
            if (out != px)
            {
                line[x] = out;
                ++changed;
            }
        }
    }
    return changed;
}

QImage renderMattePreview(const QImage& img, const AutoShadowParams& params)
{
    QImage out;
    if (img.isNull() || img.format() != QImage::Format_ARGB32_Premultiplied)
        return out;
    const MatteData m = buildMatte(img,
                                   clampInt(params.maskThreshold, 1, 254),
                                   clampInt(params.chokeMatte, -50, 50));
    if (!m.valid())
        return out;

    out = QImage(m.w, m.h, QImage::Format_ARGB32_Premultiplied);
    out.fill(qRgb(0, 0, 0)); // 黑=透明
    for (int y = 0; y < m.h; ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        const size_t row = static_cast<size_t>(y) * m.w;
        for (int x = 0; x < m.w; ++x)
        {
            if (m.maskF[row + x] > 0.5f)
                line[x] = qRgb(255, 255, 255); // 白=不透明
        }
    }
    return out;
}

} // namespace AutoShadow
