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
#include "shadowengine.h"

#include "colorizeengine.h"

#include <QColor>
#include <QHash>
#include <QPoint>
#include <QSet>

#include <algorithm>
#include <limits>

namespace ShadowFill
{
namespace
{

// 禁用线颜色匹配容差（反预乘后 RGB 距离平方）：容忍抗锯齿边缘像素
// 反预乘的取整误差，同时不会把红/黄这类刻意分开的标记色混起来
constexpr int DISABLED_COLOR_DIST_SQ = 3000;

/*
 * 分割线蒙版（Format_Grayscale8，bounds 大小）：笔画非零 alpha 的像素，
 * 剔除禁用色。>0 = 分割线（屏障）。
 */
QImage buildDividerMask(const QImage& strokes, const QRect& bounds,
                        const QVector<QRgb>& disabledColors)
{
    QImage mask(bounds.size(), QImage::Format_Grayscale8);
    mask.fill(0);
    const bool filterColors = !disabledColors.isEmpty();

    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(strokes.constScanLine(y));
        uchar* dstLine = mask.scanLine(y - bounds.top());
        for (int x = bounds.left(); x <= bounds.right(); ++x)
        {
            const QRgb px = srcLine[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;

            if (filterColors)
            {
                // 反预乘后比较（抗锯齿像素颜色不变，只有 alpha 变）
                const QRgb c = qRgb(qBound(0, qRed(px) * 255 / a, 255),
                                    qBound(0, qGreen(px) * 255 / a, 255),
                                    qBound(0, qBlue(px) * 255 / a, 255));
                bool disabled = false;
                for (QRgb dc : disabledColors)
                {
                    if (Colorize::colorDistanceSq(c, dc) <= DISABLED_COLOR_DIST_SQ)
                    {
                        disabled = true;
                        break;
                    }
                }
                if (disabled)
                    continue;
            }
            dstLine[x - bounds.left()] = static_cast<uchar>(a);
        }
    }
    return mask;
}

/*
 * Grayscale8 形态学闭运算（box 结构元，水平/垂直两趟可分离）：
 * 膨胀时域外按 0、腐蚀时域外按 255——不人为吃掉贴边像素。
 * 桥接分割线线尾没搭到线稿的小缺口。
 */
void morphCloseGray8(QImage& mask, int radius)
{
    const int w = mask.width();
    const int h = mask.height();
    if (w <= 0 || h <= 0 || radius < 1)
        return;

    const auto boxMax = [&](QImage& img) {
        QImage tmp(img.size(), QImage::Format_Grayscale8);
        tmp.fill(0);
        for (int y = 0; y < h; ++y)
        {
            const uchar* s = img.constScanLine(y);
            uchar* d = tmp.scanLine(y);
            for (int x = 0; x < w; ++x)
            {
                uchar v = 0;
                for (int xx = qMax(0, x - radius); xx <= qMin(w - 1, x + radius); ++xx)
                    v = qMax(v, s[xx]);
                d[x] = v;
            }
        }
        for (int x = 0; x < w; ++x)
            for (int y = 0; y < h; ++y)
            {
                uchar v = 0;
                for (int yy = qMax(0, y - radius); yy <= qMin(h - 1, y + radius); ++yy)
                    v = qMax(v, tmp.constScanLine(yy)[x]);
                img.scanLine(y)[x] = v;
            }
    };
    const auto boxMin = [&](QImage& img) {
        QImage tmp(img.size(), QImage::Format_Grayscale8);
        tmp.fill(255);
        for (int y = 0; y < h; ++y)
        {
            const uchar* s = img.constScanLine(y);
            uchar* d = tmp.scanLine(y);
            for (int x = 0; x < w; ++x)
            {
                uchar v = 255;
                for (int xx = qMax(0, x - radius); xx <= qMin(w - 1, x + radius); ++xx)
                    v = qMin(v, s[xx]);
                d[x] = v;
            }
        }
        for (int x = 0; x < w; ++x)
            for (int y = 0; y < h; ++y)
            {
                uchar v = 255;
                for (int yy = qMax(0, y - radius); yy <= qMin(h - 1, y + radius); ++yy)
                    v = qMin(v, tmp.constScanLine(yy)[x]);
                img.scanLine(y)[x] = v;
            }
    };
    boxMax(mask);
    boxMin(mask);
}

bool maskHasAny(const QImage& mask)
{
    for (int y = 0; y < mask.height(); ++y)
    {
        const uchar* line = mask.constScanLine(y);
        for (int x = 0; x < mask.width(); ++x)
            if (line[x] > 0)
                return true;
    }
    return false;
}

/*
 * 合成 Pass B 的屏障图：线稿副本上，把分割线像素的 alpha 取 max 并入
 * （颜色丢弃——buildHeightMap 只消费 alpha）。
 */
QImage compositeBarrier(const QImage& lineArt, const QImage& divider, const QRect& bounds)
{
    QImage combined = (lineArt.format() == QImage::Format_ARGB32_Premultiplied)
                          ? lineArt.copy()
                          : lineArt.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const uchar* dLine = divider.constScanLine(y - bounds.top());
        QRgb* cLine = reinterpret_cast<QRgb*>(combined.scanLine(y));
        for (int x = bounds.left(); x <= bounds.right(); ++x)
        {
            const int dv = dLine[x - bounds.left()];
            if (dv == 0)
                continue;
            const int na = qMax(qAlpha(cLine[x]), static_cast<int>(dv));
            cLine[x] = qPremultiply(qRgba(0, 0, 0, na));
        }
    }
    return combined;
}

} // namespace

Result computeShadow(const QImage& lineArt,
                     const QImage& strokes,
                     const QRect& bounds,
                     const Params& params,
                     const QVector<QRgb>& transparentMarkerColors)
{
    Result result;
    if (bounds.isEmpty() || lineArt.size() != strokes.size())
        return result;
    // 契约守卫：bounds 必须落在图像矩形内（越界=内部 scanLine 直接崩）
    if (!QRect(QPoint(0, 0), lineArt.size()).contains(bounds))
        return result;

    // ① 分割线蒙版 + 闭缝
    QImage divider = buildDividerMask(strokes, bounds, transparentMarkerColors);
    const int closeR = qRound(params.gapRadius);
    if (closeR >= 1)
        morphCloseGray8(divider, closeR);
    if (!maskHasAny(divider))
        return result;

    // ② 双层分割：图形（线稿屏障）与子区域（线稿+分割线屏障）
    const Colorize::RegionSegmentation shapeSeg = Colorize::segmentRegions(lineArt, bounds);
    const QImage combined = compositeBarrier(lineArt, divider, bounds);
    const Colorize::RegionSegmentation subSeg = Colorize::segmentRegions(combined, bounds);

    const int w = bounds.width();
    const auto labelA = [&](int x, int y) -> qint32 {
        if (!bounds.contains(x, y))
            return 0;
        return shapeSeg.labelOf[static_cast<size_t>((y - bounds.top()) * w + (x - bounds.left()))];
    };

    const int nSub = subSeg.regions.size();

    // 子区域 → 图形（锚点恒在子区域内部；图形屏障 ⊂ 子区域屏障，
    // 故锚点也必在图形域内，采样标签可靠）
    QVector<qint32> parentOf(nSub, 0);
    QHash<qint32, QVector<int>> shapeChildren;
    for (int r = 0; r < nSub; ++r)
    {
        const QPoint a = subSeg.regions[r].anchor;
        const qint32 parent = labelA(a.x(), a.y());
        parentOf[r] = parent;
        if (parent > 0)
            shapeChildren[parent].append(r);
    }

    // 分割线连通域标注（8 连通）+（组件, 图形）像素计数：
    // "未切开"警告跟随线条主体所在图形——画过头的越界尾巴（方帽伸出
    // 轮廓几像素、线穿过墙探进邻图形）不单独告警
    QVector<qint32> compOf(static_cast<size_t>(w) * bounds.height(), 0);
    qint32 compCount = 0;
    QHash<qint32, QHash<qint32, qint64>> compShapeTally;
    QVector<QPoint> compQueue;
    for (int y = 0; y < bounds.height(); ++y)
    {
        const uchar* dLine = divider.constScanLine(y);
        for (int x = 0; x < w; ++x)
        {
            if (dLine[x] == 0 || compOf[static_cast<size_t>(y * w + x)] != 0)
                continue;
            ++compCount;
            compOf[static_cast<size_t>(y * w + x)] = compCount;
            compQueue.append(QPoint(x, y));
            for (int head = 0; head < compQueue.size(); ++head)
            {
                const QPoint pt = compQueue[head];
                const qint32 l = labelA(bounds.left() + pt.x(), bounds.top() + pt.y());
                if (l > 0)
                    ++compShapeTally[compCount][l];
                static const int kDx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
                static const int kDy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
                for (int k = 0; k < 8; ++k)
                {
                    const int nx = pt.x() + kDx[k], ny = pt.y() + kDy[k];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= bounds.height())
                        continue;
                    if (divider.constScanLine(ny)[nx] == 0)
                        continue;
                    if (compOf[static_cast<size_t>(ny * w + nx)] != 0)
                        continue;
                    compOf[static_cast<size_t>(ny * w + nx)] = compCount;
                    compQueue.append(QPoint(nx, ny));
                }
            }
            compQueue.clear();
        }
    }
    // ③ 方向判定：同图形子区域沿所选轴比较质心——阴影=极端组（与极端值
    //    相差 ≤ 阈值的所有子区域，十字切割并列的整列一起中选）；极端组
    //    =全部子区域（分割线与所选方向垂直/并列，方向无信息）→ 整组交给
    //    空间邻近继承与兜底
    enum State
    {
        Unknown = 0,
        IsShadow,
        IsNotShadow
    };
    QVector<State> state(nSub, Unknown);

    const bool axisX = (params.direction == DirLeft || params.direction == DirRight);
    const bool takeMax = (params.direction == DirRight || params.direction == DirDown);

    // (沿轴坐标, 次轴坐标, 子区域下标)——次轴用于兜底的确定性并列破缺
    const auto buildCoords = [&](const QVector<int>& children)
    {
        QVector<QPair<QPair<qint64, qint64>, int>> coords;
        for (int idx : children)
        {
            const QPoint c = subSeg.regions[idx].centroid;
            const qint64 axisCoord = axisX ? c.x() : c.y();
            const qint64 otherCoord = axisX ? c.y() : c.x();
            coords.append(qMakePair(qMakePair(axisCoord, otherCoord), idx));
        }
        std::sort(coords.begin(), coords.end());
        return coords;
    };

    for (auto it = shapeChildren.constBegin(); it != shapeChildren.constEnd(); ++it)
    {
        const QVector<int>& children = it.value();
        if (children.size() < 2)
            continue;

        const auto coords = buildCoords(children);
        const Colorize::RegionLabel& shape = shapeSeg.regions[it.key() - 1];
        const qint64 extent = axisX ? shape.bounds.width() : shape.bounds.height();
        const qreal threshold = qMax<qreal>(params.ambiguityMinPx, params.ambiguityRatio * extent);

        const qint64 extremeCoord = takeMax ? coords.last().first.first : coords.first().first.first;
        QVector<int> extremeGroup;
        for (const auto& c : coords)
        {
            if (qAbs(c.first.first - extremeCoord) <= threshold)
                extremeGroup.append(c.second);
        }
        if (extremeGroup.size() == coords.size())
            continue; // 方向无信息，交给空间邻近继承与兜底

        for (int idx : extremeGroup)
            state[idx] = IsShadow;
        for (const auto& c : coords)
        {
            if (!extremeGroup.contains(c.second))
                state[c.second] = IsNotShadow;
        }
    }

    // ④ 空间邻近继承：歧义子区域继承同帧质心最近已判定子区域的状态。
    //    继承目标只限"被分割的图形"的子区域——没画分割线的图形（含
    //    开放背景）永不参与阴影，也不能从邻居继承状态。
    QSet<qint32> dividedShapes;
    for (auto it = shapeChildren.constBegin(); it != shapeChildren.constEnd(); ++it)
    {
        if (it.value().size() >= 2)
            dividedShapes.insert(it.key());
    }

    QVector<int> determined;
    for (int r = 0; r < nSub; ++r)
    {
        if (state[r] != Unknown)
            determined.append(r);
    }

    QSet<qint32> unresolvedShapes;
    if (determined.isEmpty())
    {
        for (int r = 0; r < nSub; ++r)
        {
            if (state[r] == Unknown && dividedShapes.contains(parentOf[r]))
                unresolvedShapes.insert(parentOf[r]);
        }
    }
    else
    {
        for (int r = 0; r < nSub; ++r)
        {
            if (state[r] != Unknown || !dividedShapes.contains(parentOf[r]))
                continue;
            int best = -1;
            qint64 bestD = std::numeric_limits<qint64>::max();
            const QPoint c = subSeg.regions[r].centroid;
            for (int d : determined)
            {
                const QPoint cd = subSeg.regions[d].centroid;
                const qint64 dx = c.x() - cd.x();
                const qint64 dy = c.y() - cd.y();
                const qint64 distSq = dx * dx + dy * dy;
                if (distSq < bestD)
                {
                    bestD = distSq;
                    best = d;
                }
            }
            if (best >= 0)
                state[r] = state[best];
            else
                unresolvedShapes.insert(parentOf[r]);
        }
    }

    // ④b 兜底：方向无信息且同帧无可借鉴（邻近继承未覆盖）→ 按所选方向
    //    取极端、并列按次轴字典序确定性选边——被切割的图形必有填充，
    //    警告仍会提示用户核对
    for (auto it = shapeChildren.constBegin(); it != shapeChildren.constEnd(); ++it)
    {
        const QVector<int>& children = it.value();
        if (children.size() < 2 || !dividedShapes.contains(it.key()))
            continue;
        bool anyUnknown = false;
        for (int idx : children)
        {
            if (state[idx] == Unknown)
            {
                anyUnknown = true;
                break;
            }
        }
        if (!anyUnknown)
            continue;

        const auto coords = buildCoords(children);
        const int pickIdx = takeMax ? coords.last().second : coords.first().second;
        state[pickIdx] = IsShadow;
        for (const auto& c : coords)
        {
            if (c.second != pickIdx && state[c.second] == Unknown)
                state[c.second] = IsNotShadow;
        }
    }

    // ⑤ 警告：每条分割线按其主体所在图形归因——主体所在图形未被切开
    //    才报"未切开"；歧义整组继承不到任何已判定状态时报"无法判定"
    QSet<qint32> warnedShapes;
    for (auto it = compShapeTally.constBegin(); it != compShapeTally.constEnd(); ++it)
    {
        const QHash<qint32, qint64>& tally = it.value();
        qint32 dominant = 0;
        qint64 bestCount = 0;
        for (auto sit = tally.constBegin(); sit != tally.constEnd(); ++sit)
        {
            if (sit.value() > bestCount)
            {
                bestCount = sit.value();
                dominant = sit.key();
            }
        }
        if (dominant == 0)
            continue;
        const auto childIt = shapeChildren.constFind(dominant);
        if (childIt != shapeChildren.constEnd() && childIt.value().size() >= 2)
            continue; // 主体图形已被切开，越界尾巴不告警
        if (warnedShapes.contains(dominant))
            continue;
        warnedShapes.insert(dominant);
        Warning wv;
        wv.kind = Warning::UnclosedDivider;
        wv.area = shapeSeg.regions[dominant - 1].bounds;
        result.warnings.append(wv);
    }
    for (qint32 label : unresolvedShapes)
    {
        Warning wv;
        wv.kind = Warning::UnresolvedAmbiguous;
        wv.area = shapeSeg.regions[label - 1].bounds;
        result.warnings.append(wv);
    }

    // ⑥ 涂色：阴影子区域整体填充
    bool anyShadow = false;
    for (int r = 0; r < nSub && !anyShadow; ++r)
        anyShadow = (state[r] == IsShadow);
    if (!anyShadow)
        return result;

    QImage fill(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    fill.fill(0);
    const QRgb shadowPx = qPremultiply(params.fillColor);
    for (int y = 0; y < bounds.height(); ++y)
    {
        const qint32* lLine = subSeg.labelOf.constData() + static_cast<size_t>(y * w);
        QRgb* fLine = reinterpret_cast<QRgb*>(fill.scanLine(y));
        for (int x = 0; x < w; ++x)
        {
            const qint32 l = lLine[x];
            if (l > 0 && state[l - 1] == IsShadow)
                fLine[x] = shadowPx;
        }
    }

    // 接缝：分割线像素从贴着阴影侧的一圈向线内有限扩散涂色，
    // 消除明暗交界处的透明细缝；距离上限防止越线稿蔓延到图形外
    const int seamReach = qBound(2, closeR + 2, 8);
    QVector<qint32> seamDist(bounds.width() * bounds.height(), 0);
    QVector<QPoint> queue;
    for (int y = 0; y < bounds.height(); ++y)
    {
        const uchar* dLine = divider.constScanLine(y);
        for (int x = 0; x < w; ++x)
        {
            if (dLine[x] == 0)
                continue;
            bool nearShadow = false;
            for (int dy = -1; dy <= 1 && !nearShadow; ++dy)
            {
                for (int dx = -1; dx <= 1 && !nearShadow; ++dx)
                {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= bounds.height())
                        continue;
                    const qint32 l = subSeg.labelOf[static_cast<size_t>(ny * w + nx)];
                    if (l > 0 && state[l - 1] == IsShadow)
                        nearShadow = true;
                }
            }
            if (nearShadow)
            {
                seamDist[y * w + x] = 1;
                queue.append(QPoint(x, y));
            }
        }
    }
    for (int head = 0; head < queue.size(); ++head)
    {
        const QPoint pt = queue[head];
        const int d = seamDist[pt.y() * w + pt.x()];
        if (d >= seamReach)
            continue;
        static const int kDx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
        static const int kDy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
        for (int k = 0; k < 8; ++k)
        {
            const int nx = pt.x() + kDx[k], ny = pt.y() + kDy[k];
            if (nx < 0 || ny < 0 || nx >= w || ny >= bounds.height())
                continue;
            if (divider.constScanLine(ny)[nx] == 0)
                continue;
            if (seamDist[ny * w + nx] != 0)
                continue;
            seamDist[ny * w + nx] = d + 1;
            queue.append(QPoint(nx, ny));
        }
    }
    for (int y = 0; y < bounds.height(); ++y)
    {
        QRgb* fLine = reinterpret_cast<QRgb*>(fill.scanLine(y));
        const uchar* dLine = divider.constScanLine(y);
        for (int x = 0; x < w; ++x)
        {
            if (dLine[x] > 0 && seamDist[y * w + x] > 0)
                fLine[x] = shadowPx;
        }
    }

    result.fill = fill;
    return result;
}

} // namespace ShadowFill
