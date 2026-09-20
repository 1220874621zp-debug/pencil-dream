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
constexpr int OCCLUSION_SAMPLES = 16; // 径向遮挡采样数

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

/** uint8 缓冲的双线性采样（内容掩膜用） */
float bilinearSampleU8(const std::vector<uint8_t>& buf, const int w, const int h, double x, double y)
{
    x = std::min(static_cast<double>(w - 1), std::max(0.0, x));
    y = std::min(static_cast<double>(h - 1), std::max(0.0, y));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const double fx = x - x0;
    const double fy = y - y0;
    const double a = buf[static_cast<size_t>(y0) * w + x0];
    const double b = buf[static_cast<size_t>(y0) * w + x1];
    const double c = buf[static_cast<size_t>(y1) * w + x0];
    const double d = buf[static_cast<size_t>(y1) * w + x1];
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

/** 距离变换（chamfer 3-4 两趟近似欧氏）：白区（≥0.5）像素到最近非白像素的距离。
    SDF 伪高度场的来源：每个连通区域成丘（中心高），线稿/洞/画布空白是山谷（0）。 */
void distanceTransform(std::vector<float>& dist, const std::vector<float>& maskF, const int w, const int h)
{
    constexpr float kInf = 1e9f;
    const size_t count = maskF.size();
    dist.assign(count, 0.0f);
    for (size_t i = 0; i < count; ++i)
        dist[i] = maskF[i] >= 0.5f ? kInf : 0.0f;

    for (int y = 0; y < h; ++y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            float d = dist[row + x];
            if (x > 0)
                d = std::min(d, dist[row + x - 1] + 3.0f);
            if (y > 0)
            {
                const size_t up = row - w;
                d = std::min(d, dist[up + x] + 3.0f);
                if (x > 0)
                    d = std::min(d, dist[up + x - 1] + 4.0f);
                if (x < w - 1)
                    d = std::min(d, dist[up + x + 1] + 4.0f);
            }
            dist[row + x] = d;
        }
    }
    for (int y = h - 1; y >= 0; --y)
    {
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = w - 1; x >= 0; --x)
        {
            float d = dist[row + x];
            if (x < w - 1)
                d = std::min(d, dist[row + x + 1] + 3.0f);
            if (y < h - 1)
            {
                const size_t dn = row + w;
                d = std::min(d, dist[dn + x] + 3.0f);
                if (x < w - 1)
                    d = std::min(d, dist[dn + x + 1] + 4.0f);
                if (x > 0)
                    d = std::min(d, dist[dn + x - 1] + 4.0f);
            }
            dist[row + x] = d;
        }
    }
    for (float& v : dist)
        v /= 3.0f;
}

/** 简单阻塞：分离两趟 min/max 形态学（正值=阻塞/收缩、负值=扩展），r 像素 */
void morphChoke(std::vector<float>& buf, std::vector<float>& tmp, const int w, const int h, const int r, const bool choke)
{
    if (r <= 0)
        return;
    const auto fold = [choke](const float a, const float b) { return choke ? std::min(a, b) : std::max(a, b); };
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

/** 去椒盐：翻转 ≤maxArea px 的孤立连通域（白岛→洞、洞→白岛）。
    肤色等中间调骑在阈值上会撒椒盐，法线发射从每个噪点喷刺——先清干净；
    连通域判定保长度（1px 细线是长连通域，不会被翻）。 */
void despeckleMask(std::vector<float>& mask, const int w, const int h, const int maxArea)
{
    if (maxArea <= 0)
        return;
    const size_t count = static_cast<size_t>(w) * h;
    std::vector<uint8_t> visited(count, 0);
    std::vector<int> stack;
    std::vector<size_t> component;

    for (size_t seed = 0; seed < count; ++seed)
    {
        if (visited[seed] != 0)
            continue;
        const float value = mask[seed];
        stack.clear();
        component.clear();
        stack.push_back(static_cast<int>(seed));
        visited[seed] = 1;
        while (!stack.empty())
        {
            const int here = stack.back();
            stack.pop_back();
            component.push_back(static_cast<size_t>(here));
            const int hy = here / w;
            const int hx = here % w;
            for (int ddy = -1; ddy <= 1; ++ddy)
            {
                for (int ddx = -1; ddx <= 1; ++ddx)
                {
                    if (ddx == 0 && ddy == 0)
                        continue;
                    const int nx = hx + ddx;
                    const int ny = hy + ddy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h)
                        continue;
                    const size_t nb = static_cast<size_t>(ny) * w + nx;
                    if (visited[nb] != 0 || mask[nb] != value)
                        continue;
                    visited[nb] = 1;
                    stack.push_back(static_cast<int>(nb));
                }
            }
        }
        if (static_cast<int>(component.size()) <= maxArea)
        {
            const float flipped = value > 0.5f ? 0.0f : 1.0f;
            for (const size_t idx : component)
                mask[idx] = flipped;
        }
    }
}

/** 掩膜生成（黑透白不透 + 简单阻塞 + 去椒盐）：apply 与遮罩视图共用 */
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
    if (m.valid())
    {
        despeckleMask(m.maskF, m.w, m.h, 3);
    }
    return m;
}

/** 单通道混合（PS/AE 语义）：直通域按模式计算 → 按不透明度回混原色 → 重预乘。
    返回预乘分量（0..α，浮点，外层统一加权后再取整）。 */
double blendChannel(const int premul, const int alpha, const int s, const AutoShadowBlendMode mode, const double opacity)
{
    // 直通原色（预乘提升）
    double x;
    if (alpha >= 255)
        x = premul / 255.0;
    else if (alpha <= 0)
        return premul;
    else
        x = std::min(255.0, (premul * 255.0 + alpha * 0.5) / alpha) / 255.0;
    const double y = s / 255.0;

    double b; // 该模式的直通混合结果（0..1）
    switch (mode)
    {
    case AutoShadowBlendMode::Normal:
        b = y;
        break;
    case AutoShadowBlendMode::Multiply:
        b = x * y;
        break;
    case AutoShadowBlendMode::LinearBurn:
        b = std::max(0.0, x + y - 1.0);
        break;
    case AutoShadowBlendMode::Darken:
        b = std::min(x, y);
        break;
    case AutoShadowBlendMode::ColorBurn:
        b = y <= 0.0 ? 0.0 : 1.0 - std::min(1.0, (1.0 - x) / y);
        break;
    case AutoShadowBlendMode::Lighten:
        b = std::max(x, y);
        break;
    case AutoShadowBlendMode::Screen:
        b = x + y - x * y;
        break;
    case AutoShadowBlendMode::Overlay:
        b = x <= 0.5 ? 2.0 * x * y : 1.0 - 2.0 * (1.0 - x) * (1.0 - y);
        break;
    case AutoShadowBlendMode::SoftLight:
        b = (1.0 - 2.0 * y) * x * x + 2.0 * y * x;
        break;
    case AutoShadowBlendMode::HardLight:
        b = y <= 0.5 ? 2.0 * x * y : 1.0 - 2.0 * (1.0 - x) * (1.0 - y);
        break;
    case AutoShadowBlendMode::LinearDodge:
        b = std::min(1.0, x + y);
        break;
    default:
        b = x;
        break;
    }

    // 不透明度回混：out = 原色 + 不透明度·(混合结果−原色)，再预乘
    const double outStraight = x + opacity * (b - x);
    return outStraight * alpha;
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

    // 光源列表清洗：空列表回退默认单光；位置钳 ±10 防极端值，高度/强度钳有效域
    std::vector<AutoShadowLight> lightList;
    if (params.lights.isEmpty())
        lightList.push_back(AutoShadowLight{});
    else
        for (const auto& l : params.lights)
            lightList.push_back(l);
    const int maskThreshold = clampInt(params.maskThreshold, 1, 254);
    const int chokeMatte = clampInt(params.chokeMatte, -50, 50);
    const double gradientStrength = clampInt(params.gradientStrength, 0, 100) / 100.0;
    const double normalStrength = clampInt(params.normalStrength, 0, 100) / 100.0;
    const int formHeight = clampInt(params.formHeight, 1, 40);
    const int formSmooth = clampInt(params.formSmooth, 0, 40);
    const int occlusionRange = clampInt(params.occlusionStrength, 0, 100);
    const float feather = std::max(0.0f, static_cast<float>(params.edgeFeather));

    // 色带（反转=镜像）
    AutoShadowLevel levels[4];
    for (int i = 0; i < 4; ++i)
        levels[i] = params.levels[params.invertLevels ? 3 - i : i];

    // ── 掩膜生成段：去色阈值（黑透白不透）+ 简单阻塞
    const MatteData m = buildMatte(img, maskThreshold, chokeMatte);
    if (!m.valid())
        return 0;
    const int minX = m.minX, minY = m.minY, maxX = m.maxX, maxY = m.maxY;
    const size_t count = m.maskF.size();

    // 各光源像素坐标 + 伪空间 z（以 max(对角线, 光到内容距离) 为基的仰角比例——
    // 100≈45°仰角，越大越顶光；光放得越远仰角不塌）+ 归一化强度
    struct LightSetup
    {
        double px = 0.0, py = 0.0, z = 1.0, w = 1.0;
    };
    std::vector<LightSetup> lights;
    {
        const double cx = (minX + maxX) * 0.5;
        const double cy = (minY + maxY) * 0.5;
        const double diag = std::sqrt(static_cast<double>(w) * w + static_cast<double>(h) * h);
        for (const AutoShadowLight& l : lightList)
        {
            LightSetup s;
            s.px = std::min(10.0, std::max(-10.0, l.x)) * w;
            s.py = std::min(10.0, std::max(-10.0, l.y)) * h;
            const double screenDist = std::sqrt((s.px - cx) * (s.px - cx) + (s.py - cy) * (s.py - cy));
            s.z = std::max(1.0, clampInt(l.height, 1, 300) / 100.0 * std::max(diag, screenDist));
            s.w = clampInt(l.intensity, 0, 100) / 100.0;
            if (s.w > 0.0)
                lights.push_back(s); // 强度 0 = 该光源关闭，直接不参与
        }
        if (lights.empty())
        {
            // 全部光源强度为 0：保留一个位置但零照度（伪法线场全暗、渐变场取最小）
            LightSetup s;
            s.px = std::min(10.0, std::max(-10.0, lightList.front().x)) * w;
            s.py = std::min(10.0, std::max(-10.0, lightList.front().y)) * h;
            const double screenDist = std::sqrt((s.px - cx) * (s.px - cx) + (s.py - cy) * (s.py - cy));
            s.z = std::max(1.0, clampInt(lightList.front().height, 1, 300) / 100.0 * std::max(diag, screenDist));
            lights.push_back(s);
            lights.back().w = 0.0;
        }
    }

    // ── 场分量①：圆形渐变底场（白区内 到各光源距离按 r0/vmax 归一化 0..1，取各光源最近者）──
    std::vector<float> gradF(count, 0.0f);
    if (gradientStrength > 0.0)
    {
        std::vector<float> perLight(count, 0.0f);
        bool any = false;
        for (const LightSetup& ls : lights)
        {
            if (ls.w <= 0.0)
                continue;
            double minD2 = std::numeric_limits<double>::max();
            double maxD2 = 0.0;
            for (int y = minY; y <= maxY; ++y)
            {
                const size_t row = static_cast<size_t>(y) * w;
                const double dy = y - ls.py;
                for (int x = minX; x <= maxX; ++x)
                {
                    if (m.maskF[row + x] < 0.5f)
                        continue;
                    const double dx = x - ls.px;
                    const double d2 = dx * dx + dy * dy;
                    if (d2 < minD2)
                        minD2 = d2;
                    if (d2 > maxD2)
                        maxD2 = d2;
                }
            }
            if (maxD2 <= minD2)
                continue;
            const double r0 = std::sqrt(minD2);
            const double norm = 1.0 / (std::sqrt(maxD2) - r0);
            for (int y = minY; y <= maxY; ++y)
            {
                const size_t row = static_cast<size_t>(y) * w;
                const double dy = y - ls.py;
                for (int x = minX; x <= maxX; ++x)
                {
                    if (m.maskF[row + x] < 0.5f)
                        continue;
                    const double dx = x - ls.px;
                    perLight[row + x] = static_cast<float>(std::max(0.0, (std::sqrt(dx * dx + dy * dy) - r0) * norm));
                }
            }
            if (!any)
            {
                gradF = perLight; // 第一个有效光源直接铺底
                any = true;
            }
            else
            {
                for (size_t i = 0; i < count; ++i)
                    gradF[i] = std::min(gradF[i], perLight[i]); // 多光源：任一光照到即不受另一光的衰减
            }
        }
    }

    // ── 场分量②：SDF 伪法线 N·L（形体明暗交界线，主阴影场）──
    // 距离变换当伪高度场（连通区域成丘、线稿成谷）→ 圆滑 → 高度场法线 → 多光源照度。
    // 照度 = Σ 强度i·max(0, N·L_i)——各光源互补照明，所有光都照不到的坡面才全暗。
    // 明暗交界线横切形体、贴线阴影自动成立——AE Relight 类（AI 法线打光）的解析式近似。
    std::vector<float> normalF(count, 0.0f); // 阴影深度 = 1−照度
    if (normalStrength > 0.0)
    {
        std::vector<float> height;
        distanceTransform(height, m.maskF, w, h);
        {
            std::vector<float> tmp(count);
            gaussBlur(height, tmp, w, h, static_cast<float>(formSmooth));
        }
        const auto hAt = [&height, w, h](const int px, const int py) {
            const int cxx = std::min(w - 1, std::max(0, px));
            const int cyy = std::min(h - 1, std::max(0, py));
            return height[static_cast<size_t>(cyy) * w + cxx];
        };

        for (int y = minY; y <= maxY; ++y)
        {
            const size_t row = static_cast<size_t>(y) * w;
            for (int x = minX; x <= maxX; ++x)
            {
                if (m.maskF[row + x] < 0.5f)
                    continue;
                // N = normalize(−∂z/∂x, −∂z/∂y, 1)，z = formHeight·h（高度场法线，向外）
                const double gx = (hAt(x + 1, y) - hAt(x - 1, y)) * 0.5 * formHeight;
                const double gy = (hAt(x, y + 1) - hAt(x, y - 1)) * 0.5 * formHeight;
                const double nLen = std::sqrt(gx * gx + gy * gy + 1.0);
                const double hz = hAt(x, y) * formHeight;
                // 多光源：照度 = Σ 强度i·max(0, N·L_i)（表面点带伪高度 z）
                double illuminance = 0.0;
                for (const LightSetup& ls : lights)
                {
                    if (ls.w <= 0.0)
                        continue;
                    const double lx = ls.px - x;
                    const double ly = ls.py - y;
                    const double lz = ls.z - hz;
                    const double lLen = std::sqrt(lx * lx + ly * ly + lz * lz);
                    if (lLen <= 1e-6)
                    {
                        illuminance += ls.w;
                        continue;
                    }
                    const double ndl = ((-gx / nLen) * lx + (-gy / nLen) * ly + (1.0 / nLen) * lz) / lLen;
                    if (ndl > 0.0)
                        illuminance += ls.w * ndl;
                }
                normalF[row + x] = static_cast<float>(1.0 - std::min(1.0, illuminance));
            }
        }
    }

    // ── 映射段：黑透白不透门控 + 场分量合成（遮挡内联）→ 色阶（可选排线图案）──
    const double levelS[4][3] = {
        { qRed(levels[0].color), qGreen(levels[0].color), qBlue(levels[0].color) },
        { qRed(levels[1].color), qGreen(levels[1].color), qBlue(levels[1].color) },
        { qRed(levels[2].color), qGreen(levels[2].color), qBlue(levels[2].color) },
        { qRed(levels[3].color), qGreen(levels[3].color), qBlue(levels[3].color) },
    };

    // 排线图案（漫画网点）：固定角度斜线，投影坐标取模；线宽≈间距/3，线隙透出原图
    const bool hatch = params.hatch;
    const double hatchRad = qDegreesToRadians(static_cast<double>(clampInt(params.hatchAngle, 0, 180)));
    const double hatchCos = std::cos(hatchRad);
    const double hatchSin = std::sin(hatchRad);
    const double hatchSpace = std::max(1.0, static_cast<double>(clampInt(params.hatchSpacing, 1, 24)));
    const double hatchWidth = std::max(1.0, hatchSpace / 3.0);

    int changed = 0;
    for (int y = 0; y < h; ++y)
    {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        const size_t row = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x)
        {
            if (m.maskF[row + x] < 0.5f)
                continue; // 洞/线稿：完全不动（黑透白不透显示阴影）
            const QRgb px = line[x];
            const int alpha = qAlpha(px);

            double field = gradientStrength * gradF[row + x] + normalStrength * normalF[row + x];

            // 场分量③：径向遮挡——沿射向各光源采样掩膜，任一光路通畅即无遮挡
            // （取受阻最轻者）：洞在所有光源的背光侧才投出遮挡阴影
            if (occlusionRange > 0)
            {
                double minBlocked = 1.0;
                for (const LightSetup& ls : lights)
                {
                    if (ls.w <= 0.0)
                        continue;
                    const double dx = x - ls.px;
                    const double dy = y - ls.py;
                    const double len = std::sqrt(dx * dx + dy * dy);
                    if (len < 1.0)
                    {
                        minBlocked = 0.0; // 光源贴脸：无遮挡
                        break;
                    }
                    const double ux = -dx / len; // 指向光源
                    const double uy = -dy / len;
                    double acc = 0.0;
                    double weightSum = 0.0;
                    for (int i = 0; i < OCCLUSION_SAMPLES; ++i)
                    {
                        const double t = static_cast<double>(i) / static_cast<double>(OCCLUSION_SAMPLES - 1);
                        const double wgt = 1.0 - t; // tent：离本像素越远权重越低
                        const double off = t * occlusionRange;
                        const float cm = bilinearSampleU8(m.contentMask, w, h, x + ux * off, y + uy * off);
                        if (cm <= 0.0f)
                            continue; // 内容外不计入（空画布不挡光）
                        acc += wgt * cm * bilinearSample(m.maskF, w, h, x + ux * off, y + uy * off);
                        weightSum += wgt * cm;
                    }
                    const double blocked = weightSum > 1e-3 ? 1.0 - acc / weightSum : 0.0;
                    if (blocked < minBlocked)
                        minBlocked = blocked;
                    if (minBlocked <= 0.0)
                        break; // 已有通畅光路，再无遮挡
                }
                field += minBlocked;
            }

            const float F = static_cast<float>(std::min(1.0, std::max(0.0, field)) * 100.0);

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

            // 排线覆盖：该像素是否落在斜线上（线外=线隙，透出原图）
            double lineT = 1.0;
            if (hatch)
            {
                const double t = x * hatchCos + y * hatchSin;
                double m = std::fmod(t, hatchSpace);
                if (m < 0.0)
                    m += hatchSpace;
                lineT = m < hatchWidth ? 1.0 : 0.0;
            }

            double outR = 0.0, outG = 0.0, outB = 0.0;
            double coverageSum = 0.0;
            for (int i = 0; i < 4; ++i)
            {
                const double c = w[i] * lineT;
                if (c <= 0.0)
                    continue;
                coverageSum += c;
                const double levelOpacity = clampInt(levels[i].opacity, 0, 100) / 100.0;
                outR += c * blendChannel(qRed(px), alpha, static_cast<int>(levelS[i][0]), levels[i].mode, levelOpacity);
                outG += c * blendChannel(qGreen(px), alpha, static_cast<int>(levelS[i][1]), levels[i].mode, levelOpacity);
                outB += c * blendChannel(qBlue(px), alpha, static_cast<int>(levelS[i][2]), levels[i].mode, levelOpacity);
            }
            if (hatch)
            {
                // 线隙残量：透出原图（预乘原色直加）
                const double residual = std::max(0.0, 1.0 - coverageSum);
                outR += residual * qRed(px);
                outG += residual * qGreen(px);
                outB += residual * qBlue(px);
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
