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

Algorithm derived from Krita's lazybrush code (GPL-2.0-or-later):
KisWatershedWorker / KisLazyFillTools / KisGaussianKernel
by Dmitry Kazakov <dimula73@gmail.com>, 2016-2017.

*/
#include "colorizeengine.h"

#include <QHash>
#include <QDebug>
#include <QMap>
#include <QStack>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <set>

namespace Colorize
{

namespace
{

// 供 std::multiset<QPoint> 排序（照抄 Krita CompareQPoints）
struct CompareQPoints
{
    bool operator()(const QPoint& p1, const QPoint& p2) const
    {
        return p1.y() < p2.y() || (p1.y() == p2.y() && p1.x() < p2.x());
    }
};

inline int pow2i(int v) { return v * v; }

// ===================== 卷积基础 =====================

// sigma = 0.3*radius + 0.3（照抄 KisGaussianKernel::sigmaFromRadius）
qreal gaussianSigma(qreal radius)
{
    return 0.3 * radius + 0.3;
}

// 一维高斯核（归一化，sum = 1）
QVector<qreal> makeGaussianKernel(qreal radius)
{
    const qreal sigma = gaussianSigma(radius);
    const int kernelSize = 6 * qCeil(sigma) + 1;
    const qreal multiplicand = 1.0 / std::sqrt(2.0 * M_PI * sigma * sigma);
    const qreal exponentMultiplicand = 1.0 / (2.0 * sigma * sigma);

    QVector<qreal> kernel(kernelSize);
    const int center = kernelSize / 2;
    qreal sum = 0.0;
    for (int x = 0; x < kernelSize; ++x)
    {
        const qreal xDistance = center - x;
        const qreal value = multiplicand * std::exp(-xDistance * xDistance * exponentMultiplicand);
        kernel[x] = value;
        sum += value;
    }
    for (auto& v : kernel)
        v /= sum;
    return kernel;
}

// 灰度图可分离高斯模糊（边界 = 重复钳位，对应 BORDER_REPEAT）
void convolveGaussianGray8(QImage& img, const QRect& rc, qreal radius)
{
    if (rc.isEmpty() || radius <= 0.0)
        return;

    const QVector<qreal> kernel = makeGaussianKernel(radius);
    const int kCenter = kernel.size() / 2;

    QImage horizontal(img.size(), QImage::Format_Grayscale8);
    horizontal.fill(0);

    // 水平
    for (int y = rc.top(); y <= rc.bottom(); ++y)
    {
        const uchar* srcLine = img.constScanLine(y);
        uchar* dstLine = horizontal.scanLine(y);
        for (int x = rc.left(); x <= rc.right(); ++x)
        {
            qreal sum = 0.0;
            for (int k = 0; k < kernel.size(); ++k)
            {
                int sx = x + k - kCenter;
                sx = qBound(rc.left(), sx, rc.right());
                sum += kernel[k] * srcLine[sx];
            }
            dstLine[x] = static_cast<uchar>(qRound(qBound(0.0, sum, 255.0)));
        }
    }

    // 垂直
    for (int y = rc.top(); y <= rc.bottom(); ++y)
    {
        uchar* dstLine = img.scanLine(y);
        for (int x = rc.left(); x <= rc.right(); ++x)
        {
            qreal sum = 0.0;
            for (int k = 0; k < kernel.size(); ++k)
            {
                int sy = y + k - kCenter;
                sy = qBound(rc.top(), sy, rc.bottom());
                sum += kernel[k] * horizontal.constScanLine(sy)[x];
            }
            dstLine[x] = static_cast<uchar>(qRound(qBound(0.0, sum, 255.0)));
        }
    }
}

// LoG 二维核（照抄 KisGaussianKernel::createLoGMatrix，zeroCentered=false, includeWrappedArea=true）
QVector<qreal> makeLoGKernel(qreal radius, qreal coeff)
{
    const int kernelSize = 4 * static_cast<int>(std::ceil(radius)) + 1;
    const qreal sigma = radius;
    const qreal multiplicand = -1.0 / (M_PI * sigma * sigma * sigma * sigma);
    const qreal exponentMultiplicand = 1.0 / (2.0 * sigma * sigma);

    QVector<qreal> matrix(kernelSize * kernelSize);
    const int center = kernelSize / 2;

    for (int y = 0; y < kernelSize; ++y)
    {
        const qreal yDistance = center - y;
        for (int x = 0; x < kernelSize; ++x)
        {
            const qreal xDistance = center - x;
            const qreal distance = xDistance * xDistance + yDistance * yDistance;
            const qreal normalizedDistance = exponentMultiplicand * distance;
            matrix[y * kernelSize + x] = multiplicand * (1.0 - normalizedDistance) * std::exp(-normalizedDistance);
        }
    }

    // 中心值 = -侧翼和，使总和为 0
    qreal lateralSum = 0.0;
    for (int i = 0; i < matrix.size(); ++i)
        if (i != center * kernelSize + center)
            lateralSum += matrix[i];
    matrix[center * kernelSize + center] = -lateralSum;

    // offset 分支（zeroCentered=false）保持为 0，照抄原实现

    qreal positiveSum = 0.0;
    for (const qreal v : matrix)
        if (v > 0.0)
            positiveSum += v;

    const qreal scale = coeff * 2.0 / positiveSum;
    for (auto& v : matrix)
        v *= scale;
    return matrix;
}

// LoG 卷积（无归一化，输出钳位 0..255，边界 = 重复钳位）
void convolveLoGGray8(QImage& img, const QRect& rc, qreal radius, qreal coeff)
{
    if (rc.isEmpty() || radius <= 0.0)
        return;

    const QVector<qreal> matrix = makeLoGKernel(radius, coeff);
    const int kernelSize = static_cast<int>(std::round(std::sqrt(matrix.size())));
    const int kCenter = kernelSize / 2;

    const QImage src = img.copy();

    for (int y = rc.top(); y <= rc.bottom(); ++y)
    {
        uchar* dstLine = img.scanLine(y);
        for (int x = rc.left(); x <= rc.right(); ++x)
        {
            qreal sum = 0.0;
            for (int ky = 0; ky < kernelSize; ++ky)
            {
                int sy = y + ky - kCenter;
                sy = qBound(rc.top(), sy, rc.bottom());
                const uchar* srcLine = src.constScanLine(sy);
                for (int kx = 0; kx < kernelSize; ++kx)
                {
                    int sx = x + kx - kCenter;
                    sx = qBound(rc.left(), sx, rc.right());
                    sum += matrix[ky * kernelSize + kx] * srcLine[sx];
                }
            }
            dstLine[x] = static_cast<uchar>(qRound(qBound(0.0, sum, 255.0)));
        }
    }
}

// 线性拉伸 min..max → 0..255（照抄 normalizeAlpha8Device）
void normalizeGray8(QImage& img, const QRect& rc)
{
    uchar maxPixel = 0;
    uchar minPixel = 255;
    for (int y = rc.top(); y <= rc.bottom(); ++y)
    {
        const uchar* line = img.constScanLine(y);
        for (int x = rc.left(); x <= rc.right(); ++x)
        {
            maxPixel = qMax(maxPixel, line[x]);
            minPixel = qMin(minPixel, line[x]);
        }
    }
    if (maxPixel <= minPixel)
    {
        // 全平图（无线稿）：无屏障
        for (int y = rc.top(); y <= rc.bottom(); ++y)
        {
            uchar* line = img.scanLine(y);
            for (int x = rc.left(); x <= rc.right(); ++x)
                line[x] = 0;
        }
        return;
    }

    const qreal scale = 255.0 / (maxPixel - minPixel);
    for (int y = rc.top(); y <= rc.bottom(); ++y)
    {
        uchar* line = img.scanLine(y);
        for (int x = rc.left(); x <= rc.right(); ++x)
            line[x] = static_cast<uchar>((line[x] - minPixel) * scale);
    }
}

// ===================== 分水岭 =====================

enum PrevDirections
{
    FROM_NOWHERE = 0,
    FROM_RIGHT,
    FROM_LEFT,
    FROM_TOP,
    FROM_BOTTOM
};

struct NeighbourStaticOffset
{
    const quint8 from;
    const bool statsOnly;
    const QPoint offset;
};

// 照抄 KisWatershedWorker 的静态偏移表：来自方向的那一侧只统计不扩散
const NeighbourStaticOffset staticOffsets[5][4] =
{
    { // FROM_NOWHERE
        { FROM_RIGHT,  false, QPoint(-1,  0) },
        { FROM_LEFT,   false, QPoint( 1,  0) },
        { FROM_BOTTOM, false, QPoint( 0, -1) },
        { FROM_TOP,    false, QPoint( 0,  1) },
    },
    { // FROM_RIGHT
        { FROM_RIGHT,  false, QPoint(-1,  0) },
        { FROM_LEFT,   true,  QPoint( 1,  0) },
        { FROM_BOTTOM, false, QPoint( 0, -1) },
        { FROM_TOP,    false, QPoint( 0,  1) },
    },
    { // FROM_LEFT
        { FROM_RIGHT,  true,  QPoint(-1,  0) },
        { FROM_LEFT,   false, QPoint( 1,  0) },
        { FROM_BOTTOM, false, QPoint( 0, -1) },
        { FROM_TOP,    false, QPoint( 0,  1) },
    },
    { // FROM_TOP
        { FROM_RIGHT,  false, QPoint(-1,  0) },
        { FROM_LEFT,   false, QPoint( 1,  0) },
        { FROM_BOTTOM, true,  QPoint( 0, -1) },
        { FROM_TOP,    false, QPoint( 0,  1) },
    },
    { // FROM_BOTTOM
        { FROM_RIGHT,  false, QPoint(-1,  0) },
        { FROM_LEFT,   false, QPoint( 1,  0) },
        { FROM_BOTTOM, false, QPoint( 0, -1) },
        { FROM_TOP,    true,  QPoint( 0,  1) },
    }
};

struct TaskPoint
{
    int x = 0;
    int y = 0;
    int distance = 0;
    qint32 group = 0;
    quint8 prevDirection = FROM_NOWHERE;
    quint8 level = 0;
};

struct CompareTaskPoints
{
    // priority_queue 取「最小」的 (level, distance)
    bool operator()(const TaskPoint& pt1, const TaskPoint& pt2) const
    {
        return pt1.level > pt2.level ||
               (pt1.level == pt2.level && pt1.distance > pt2.distance);
    }
};

using PointsPriorityQueue = std::priority_queue<TaskPoint, std::vector<TaskPoint>, CompareTaskPoints>;

struct FillGroup
{
    FillGroup() {}
    explicit FillGroup(int _colorIndex) : colorIndex(_colorIndex) {}

    /** 计算域边缘的自动背景组：清理pass将其视为合法邻居，不作为移除候选 */
    bool isBackground = false;

    int colorIndex = -1;

    struct LevelData
    {
        int positiveEdgeSize = 0;
        int negativeEdgeSize = 0;
        int foreignEdgeSize = 0;
        int allyEdgeSize = 0;
        int numFilledPixels = 0;

        bool narrowRegion = false;

        int totalEdgeSize() const
        {
            return positiveEdgeSize + negativeEdgeSize + foreignEdgeSize + allyEdgeSize;
        }

        QMap<qint32, std::multiset<QPoint, CompareQPoints>> conflictWithGroup;
    };

    QMap<int, LevelData> levels;
};

using GroupLevelPair = QPair<qint32, quint8>;

class WatershedWorker
{
public:
    WatershedWorker(const QImage& heightMap, const QRect& boundingRect,
                    const std::function<bool(int)>& progress)
        : mHeightMap(heightMap), mBoundingRect(boundingRect), mProgress(progress)
    {
        // 组图：与输入同尺寸的平铺 qint32 数组（0 = 未认领）
        mGroupMap = QVector<qint32>(heightMap.width() * heightMap.height(), 0);
    }

    void addKeyStroke(const QImage& strokeMask, QRgb color, bool isTransparent)
    {
        mKeyStrokeColors << color;
        mKeyStrokeTransparent << isTransparent;
        mKeyStrokeIsBorder << false;
        // 本地副本：笔画在解析时会被消费
        mKeyStrokes << strokeMask.convertToFormat(QImage::Format_Grayscale8);

        // 与已有笔画重叠处，后加的笔画优先（照抄 addKeyStroke 的清除规则）
        QImage& lastDev = mKeyStrokes.back();
        for (int i = 0; i < mKeyStrokes.size() - 1; ++i)
        {
            QImage& dev = mKeyStrokes[i];
            for (int y = mBoundingRect.top(); y <= mBoundingRect.bottom(); ++y)
            {
                const uchar* lastLine = lastDev.constScanLine(y);
                uchar* devLine = dev.scanLine(y);
                for (int x = mBoundingRect.left(); x <= mBoundingRect.right(); ++x)
                {
                    if (devLine[x] > 0 && lastLine[x] > 0)
                        devLine[x] = 0;
                }
            }
        }
    }

    // 返回 false = 被取消
    bool run(qreal cleanUpAmount)
    {
        // 开跑即汇报一次（小图不会命中周期性回调）
        if (mProgress && !mProgress(0))
            return false;

        mGroups << FillGroup(-1);

        for (int i = 0; i < mKeyStrokes.size(); ++i)
            parseColorIntoGroups(mKeyStrokes[i], i);

        // 自动背景组：计算域边缘一圈作为「透明笔画」参与竞争（与普通笔画
        // 同优先级——种子合并高度后同为 level>=1，靠距离公平竞争）。
        // Krita 原版靠用户手画透明笔画保护背景，这里默认提供：
        // 无竞争的单一颜色止于线稿范围，线稿外保持透明；
        // 封闭且无笔画的区域仍归就近颜色（Krita 行为）。
        addBorderBackgroundStroke();

        const QRect initRect = mGroupMapArea & mBoundingRect;
        initializeQueueFromGroupMap(initRect);
        if (!processQueue(0))
            return false;

        if (cleanUpAmount > 0.0)
            cleanupForeignEdgeGroups(cleanUpAmount);

        writeColoring();
        return true;
    }

    QImage takeResult() { return mResult; }

private:
    inline const uchar* heightLine(int y) const { return mHeightMap.constScanLine(y); }
    inline qint32 groupAt(int x, int y) const { return mGroupMap[y * mHeightMap.width() + x]; }
    inline void setGroupAt(int x, int y, qint32 g) { mGroupMap[y * mHeightMap.width() + x] = g; }

    // 连续域填充（照抄 fillContiguousGroup：消费笔画并写组号）。
    // 连续性判定 = 与种子值「精确等值」（阈值 0 的 HardSelection 语义），
    // 配合 mergeHeightmapOntoStroke 后即「同高度的连续笔画区域」为一组。
    void fillContiguousGroup(QImage& stroke, const QPoint& seed, qint32 groupIndex)
    {
        const uchar referenceValue = stroke.scanLine(seed.y())[seed.x()];
        QStack<QPoint> stack;
        stack.push(seed);

        while (!stack.isEmpty())
        {
            const QPoint pt = stack.pop();
            uchar* line = stroke.scanLine(pt.y());
            if (line[pt.x()] != referenceValue)
                continue;
            line[pt.x()] = 0;
            setGroupAt(pt.x(), pt.y(), groupIndex);
            mGroupMapArea |= QRect(pt, QSize(1, 1));

            const QPoint neighbours[4] = { pt + QPoint(-1, 0), pt + QPoint(1, 0),
                                           pt + QPoint(0, -1), pt + QPoint(0, 1) };
            for (const QPoint& n : neighbours)
            {
                if (!mBoundingRect.contains(n))
                    continue;
                if (stroke.constScanLine(n.y())[n.x()] == referenceValue)
                    stack.push(n);
            }
        }
    }

    void parseColorIntoGroups(QImage& stroke, int colorIndex)
    {
        // 笔画值 := max(1, 高度)（照抄 mergeHeightmapOntoStroke：分组按等值域进行）
        for (int y = mBoundingRect.top(); y <= mBoundingRect.bottom(); ++y)
        {
            const uchar* hLine = heightLine(y);
            uchar* line = stroke.scanLine(y);
            for (int x = mBoundingRect.left(); x <= mBoundingRect.right(); ++x)
            {
                if (line[x] > 0)
                    line[x] = qMax<uchar>(1, hLine[x]);
            }
        }

        for (int y = mBoundingRect.top(); y <= mBoundingRect.bottom(); ++y)
        {
            uchar* line = stroke.scanLine(y);
            for (int x = mBoundingRect.left(); x <= mBoundingRect.right(); ++x)
            {
                if (line[x] > 0)
                {
                    FillGroup group(colorIndex);
                    group.isBackground = mKeyStrokeIsBorder[colorIndex];
                    mGroups << group;
                    fillContiguousGroup(stroke, QPoint(x, y), mGroups.size() - 1);
                }
            }
        }
    }

    void initializeQueueFromGroupMap(const QRect& rc)
    {
        for (int y = rc.top(); y <= rc.bottom(); ++y)
        {
            const uchar* hLine = heightLine(y);
            for (int x = rc.left(); x <= rc.right(); ++x)
            {
                const qint32 g = groupAt(x, y);
                if (g > 0)
                {
                    TaskPoint pt;
                    pt.x = x;
                    pt.y = y;
                    pt.group = g;
                    pt.level = hLine[x];

                    mPointsQueue.push(pt);
                    // 清零以保证 foreign 统计正确（照抄）
                    setGroupAt(x, y, 0);
                }
            }
        }
    }

    // 计算域四边一圈合成「透明笔画」：与用户笔画同路径解析（合并高度、
    // 等值成组），保证优先级对等；不与用户笔画抢像素（addKeyStroke 后加优先）
    void addBorderBackgroundStroke()
    {
        QImage ring(mHeightMap.size(), QImage::Format_Grayscale8);
        ring.fill(0);
        const QRect& rc = mBoundingRect;
        for (int x = rc.left(); x <= rc.right(); ++x)
        {
            ring.scanLine(rc.top())[x] = 255;
            ring.scanLine(rc.bottom())[x] = 255;
        }
        for (int y = rc.top(); y <= rc.bottom(); ++y)
        {
            ring.scanLine(y)[rc.left()] = 255;
            ring.scanLine(y)[rc.right()] = 255;
        }

        addKeyStroke(ring, 0, true);
        mKeyStrokeIsBorder.back() = true;
        parseColorIntoGroups(mKeyStrokes.back(), mKeyStrokeColors.size() - 1);
    }

    void addForeignAlly(qint32 currGroupId, qint32 prevGroupId,
                        FillGroup& currGroup, FillGroup& prevGroup,
                        FillGroup::LevelData& currLevelData, FillGroup::LevelData& prevLevelData,
                        const QPoint& currPt, const QPoint& prevPt, bool sameLevel)
    {
        // 背景组与颜色的邻接是合法邻居关系：不记 foreign/ally/冲突点，
        // 否则清理pass的污染度量会把贴边的颜色组整体误删
        if (currGroup.isBackground || prevGroup.isBackground)
            return;

        if (currGroup.colorIndex != prevGroup.colorIndex || !sameLevel)
        {
            prevLevelData.foreignEdgeSize++;
            currLevelData.foreignEdgeSize++;

            if (sameLevel)
            {
                currLevelData.conflictWithGroup[prevGroupId].insert(currPt);
                prevLevelData.conflictWithGroup[currGroupId].insert(prevPt);
            }
        }
        else
        {
            prevLevelData.allyEdgeSize++;
            currLevelData.allyEdgeSize++;
        }
    }

    void removeForeignAlly(qint32 currGroupId, qint32 prevGroupId,
                           FillGroup& currGroup, FillGroup& prevGroup,
                           FillGroup::LevelData& currLevelData, FillGroup::LevelData& prevLevelData,
                           const QPoint& currPt, const QPoint& prevPt, bool sameLevel)
    {
        if (currGroup.isBackground || prevGroup.isBackground)
            return; // 与 addForeignAlly 对称：背景对从未记账

        if (currGroup.colorIndex != prevGroup.colorIndex || !sameLevel)
        {
            prevLevelData.foreignEdgeSize--;
            currLevelData.foreignEdgeSize--;

            if (sameLevel)
            {
                std::multiset<QPoint, CompareQPoints>& currSet = currLevelData.conflictWithGroup[prevGroupId];
                auto currIt = currSet.find(currPt);
                if (currIt != currSet.end())
                    currSet.erase(currIt);
                else
                    qDebug("[colorize] removeForeignAlly: curr point 缺失 g=%d lvl=%d vs=%d", currGroupId, sameLevel, prevGroupId);

                std::multiset<QPoint, CompareQPoints>& prevSet = prevLevelData.conflictWithGroup[currGroupId];
                auto prevIt = prevSet.find(prevPt);
                if (prevIt != prevSet.end())
                    prevSet.erase(prevIt);
                else
                    qDebug("[colorize] removeForeignAlly: prev point 缺失 g=%d lvl=%d vs=%d", prevGroupId, sameLevel, currGroupId);
            }
        }
        else
        {
            prevLevelData.allyEdgeSize--;
            currLevelData.allyEdgeSize--;
        }
    }

    void incrementLevelEdge(FillGroup::LevelData& currLevelData, FillGroup::LevelData& prevLevelData,
                            quint8 currLevel, quint8 prevLevel)
    {
        if (currLevel > prevLevel)
        {
            currLevelData.negativeEdgeSize++;
            prevLevelData.positiveEdgeSize++;
        }
        else
        {
            currLevelData.positiveEdgeSize++;
            prevLevelData.negativeEdgeSize++;
        }
    }

    void decrementLevelEdge(FillGroup::LevelData& currLevelData, FillGroup::LevelData& prevLevelData,
                            quint8 currLevel, quint8 prevLevel)
    {
        if (currLevel > prevLevel)
        {
            currLevelData.negativeEdgeSize--;
            prevLevelData.positiveEdgeSize--;
        }
        else
        {
            currLevelData.positiveEdgeSize--;
            prevLevelData.negativeEdgeSize--;
        }
    }

    void visitNeighbour(const QPoint& currPt, const QPoint& prevPt,
                        quint8 fromDirection, int prevDistance, quint8 prevLevel,
                        qint32 prevGroupId, FillGroup& prevGroup, FillGroup::LevelData& prevLevelData,
                        qint32 prevPrevGroupId, FillGroup& prevPrevGroup,
                        bool statsOnly = false)
    {
        if (!mBoundingRect.contains(currPt))
        {
            prevLevelData.positiveEdgeSize++;

            if (prevPrevGroupId > 0)
            {
                FillGroup::LevelData& prevPrevLevelData = prevPrevGroup.levels[prevLevel];
                prevPrevLevelData.positiveEdgeSize--;
            }
            return;
        }

        if (prevGroupId == mBackgroundGroupId)
            return;

        const qint32 currGroupId = groupAt(currPt.x(), currPt.y());
        const quint8 newLevel = heightLine(currPt.y())[currPt.x()];

        FillGroup& currGroup = mGroups[currGroupId];
        FillGroup::LevelData& currLevelData = currGroup.levels[newLevel];

        const bool needsAddTaskPoint =
            !currGroupId ||
            (mRecolorMode &&
             ((newLevel == prevLevel && currGroupId == mBackgroundGroupId) ||
              (newLevel >= prevLevel &&
               currGroup.colorIndex == mBackgroundGroupColor &&
               currLevelData.narrowRegion)));

        if (needsAddTaskPoint && !statsOnly)
        {
            TaskPoint pt;
            pt.x = currPt.x();
            pt.y = currPt.y();
            pt.group = prevGroupId;
            pt.level = newLevel;
            pt.distance = newLevel == prevLevel ? prevDistance + 1 : 0;
            pt.prevDirection = fromDirection;

            mPointsQueue.push(pt);
        }

        // 像素永远不会清零
        if (prevGroupId <= 0)
            return;
        if (prevGroupId == prevPrevGroupId)
            return;

        if (currGroupId)
        {
            const bool isSameLevel = prevLevel == newLevel;

            if ((!prevPrevGroupId || prevPrevGroupId == currGroupId) &&
                prevGroupId != currGroupId)
            {
                // 新增 foreign/ally 邻接
                addForeignAlly(currGroupId, prevGroupId,
                               currGroup, prevGroup,
                               currLevelData, prevLevelData,
                               currPt, prevPt, isSameLevel);
            }
            else if (prevPrevGroupId &&
                     prevPrevGroupId != currGroupId &&
                     prevGroupId == currGroupId)
            {
                // 移除 foreign/ally 邻接（擦除的是像素旧主的记录）
                FillGroup::LevelData& prevPrevLevelData = prevPrevGroup.levels[prevLevel];
                removeForeignAlly(currGroupId, prevPrevGroupId,
                                  currGroup, prevPrevGroup,
                                  currLevelData, prevPrevLevelData,
                                  currPt, prevPt, isSameLevel);
            }
            else if (prevPrevGroupId &&
                     prevPrevGroupId != currGroupId &&
                     prevGroupId != currGroupId)
            {
                // 该像素变成了另一组的 foreign/ally 像素
                FillGroup::LevelData& prevPrevLevelData = prevPrevGroup.levels[prevLevel];

                removeForeignAlly(currGroupId, prevPrevGroupId,
                                  currGroup, prevPrevGroup,
                                  currLevelData, prevPrevLevelData,
                                  currPt, prevPt, isSameLevel);

                addForeignAlly(currGroupId, prevGroupId,
                               currGroup, prevGroup,
                               currLevelData, prevLevelData,
                               currPt, prevPt, isSameLevel);
            }

            if (!isSameLevel)
            {
                if (prevGroupId == currGroupId)
                {
                    // 与自身不相连区域汇合
                    FillGroup::LevelData& sameGroupLevelData = currGroup.levels[newLevel];
                    incrementLevelEdge(sameGroupLevelData, prevLevelData, newLevel, prevLevel);
                }

                if (prevPrevGroupId == currGroupId)
                {
                    // 边界像素移除（现在登记为 foreign/ally 像素）
                    FillGroup::LevelData& sameGroupLevelData = currGroup.levels[newLevel];
                    FillGroup::LevelData& sameGroupPrevLevelData = currGroup.levels[prevLevel];
                    decrementLevelEdge(sameGroupLevelData, sameGroupPrevLevelData, newLevel, prevLevel);
                }
            }
        }
    }

    bool processQueue(qint32 backgroundGroupId)
    {
        mBackgroundGroupId = backgroundGroupId;
        mBackgroundGroupColor = mGroups[backgroundGroupId].colorIndex;
        mRecolorMode = backgroundGroupId > 1;

        mTotalPixelsToFill = static_cast<qint64>(mBoundingRect.width()) * mBoundingRect.height();
        mNumFilledPixels = 0;
        const quint64 progressReportingMask = (1ULL << 18) - 1; // 每 512x512 补丁汇报一次

        if (mRecolorMode)
            updateNarrowRegionMetrics();

        while (!mPointsQueue.empty())
        {
            const TaskPoint pt = mPointsQueue.top();
            mPointsQueue.pop();

            const qint32 prevGroupId = groupAt(pt.x, pt.y);

            if (prevGroupId == mBackgroundGroupId ||
                (mRecolorMode && mGroups[prevGroupId].colorIndex == mBackgroundGroupColor))
            {
                FillGroup& currGroup = mGroups[pt.group];
                FillGroup::LevelData& currLevelData = currGroup.levels[pt.level];
                currLevelData.numFilledPixels++;

                if (prevGroupId > 0)
                {
                    FillGroup::LevelData& prevLevelData = mGroups[prevGroupId].levels[pt.level];
                    prevLevelData.numFilledPixels--;
                }
                else
                {
                    mNumFilledPixels++;
                }

                const NeighbourStaticOffset* offsets = staticOffsets[pt.prevDirection];
                const QPoint currPt(pt.x, pt.y);

                for (int i = 0; i < 4; ++i)
                {
                    const QPoint nextPt = currPt + offsets[i].offset;
                    visitNeighbour(nextPt, currPt,
                                   offsets[i].from, pt.distance, pt.level,
                                   pt.group, currGroup, currLevelData,
                                   prevGroupId, mGroups[prevGroupId],
                                   offsets[i].statsOnly);
                }

                setGroupAt(pt.x, pt.y, pt.group);

                if (!(mNumFilledPixels & progressReportingMask))
                {
                    const int progressPercent =
                        qBound(0, static_cast<int>(100.0 * mNumFilledPixels / mTotalPixelsToFill), 100);
                    if (mProgress && !mProgress(progressPercent))
                        return false;
                }
            }
            else
            {
                // 无事可做（照抄：已被认领的出队点直接丢弃）
            }
        }

        mBackgroundGroupId = 0;
        mBackgroundGroupColor = -1;
        mRecolorMode = false;
        return true;
    }

    void writeColoring()
    {
        mResult = QImage(mBoundingRect.size(), QImage::Format_ARGB32_Premultiplied);
        mResult.fill(Qt::transparent);

        for (int y = mBoundingRect.top(); y <= mBoundingRect.bottom(); ++y)
        {
            QRgb* dstLine = reinterpret_cast<QRgb*>(mResult.scanLine(y - mBoundingRect.top()));
            for (int x = mBoundingRect.left(); x <= mBoundingRect.right(); ++x)
            {
                const qint32 g = groupAt(x, y);
                const int colorIndex = mGroups[g].colorIndex;
                if (colorIndex >= 0 && !mKeyStrokeTransparent[colorIndex])
                    dstLine[x - mBoundingRect.left()] = qPremultiply(mKeyStrokeColors[colorIndex]);
            }
        }
    }

    QVector<TaskPoint> tryRemoveConflictingPlane(qint32 group, quint8 level)
    {
        QVector<TaskPoint> result;

        FillGroup& g = mGroups[group];
        FillGroup::LevelData& l = g.levels[level];

        for (auto conflictIt = l.conflictWithGroup.begin(); conflictIt != l.conflictWithGroup.end(); ++conflictIt)
        {
            std::vector<QPoint> uniquePoints;
            std::unique_copy(conflictIt->begin(), conflictIt->end(), std::back_inserter(uniquePoints));

            for (const QPoint& point : uniquePoints)
            {
                TaskPoint pt;
                pt.x = point.x();
                pt.y = point.y();
                pt.group = conflictIt.key();
                pt.level = level;

                result.append(pt);
                // 不写组图（照抄）
            }
        }

        return result;
    }

    void updateNarrowRegionMetrics()
    {
        for (qint32 i = 0; i < mGroups.size(); ++i)
        {
            FillGroup& group = mGroups[i];
            for (auto levelIt = group.levels.begin(); levelIt != group.levels.end(); ++levelIt)
            {
                FillGroup::LevelData& l = levelIt.value();
                const qreal areaToPerimeterRatio = qreal(l.numFilledPixels) / l.totalEdgeSize();
                l.narrowRegion = areaToPerimeterRatio < 2.0;
            }
        }
    }

    QVector<GroupLevelPair> calculateConflictingPairs()
    {
        QVector<GroupLevelPair> result;

        for (qint32 i = 0; i < mGroups.size(); ++i)
        {
            FillGroup& group = mGroups[i];
            if (group.isBackground)
                continue; // 背景组不作为移除候选
            for (auto levelIt = group.levels.begin(); levelIt != group.levels.end(); ++levelIt)
            {
                FillGroup::LevelData& l = levelIt.value();
                for (auto conflictIt = l.conflictWithGroup.begin(); conflictIt != l.conflictWithGroup.end(); ++conflictIt)
                {
                    if (!conflictIt->empty() && !mGroups[conflictIt.key()].isBackground)
                    {
                        // 与背景组的边界是合法邻居关系，不算漏色污染
                        result.append(GroupLevelPair(i, levelIt.key()));
                        break;
                    }
                }
            }
        }

        return result;
    }

    void cleanupForeignEdgeGroups(qreal cleanUpAmount)
    {
        // 阈值范围 [0.05...0.5]（照抄）
        const qreal foreignEdgePortionThreshold = 0.05 + 0.45 * (1.0 - qBound(0.0, cleanUpAmount, 1.0));

        QVector<GroupLevelPair> conflicts = calculateConflictingPairs();

        // 按总边长排序
        QMap<qreal, GroupLevelPair> sortedPairs;
        for (const GroupLevelPair& pair : conflicts)
        {
            FillGroup::LevelData& level = mGroups[pair.first].levels[pair.second];
            sortedPairs.insert(level.totalEdgeSize(), pair);
        }

        // 从最小到最大依次移除
        for (auto pairIt = sortedPairs.begin(); pairIt != sortedPairs.end(); ++pairIt)
        {
            const qint32 groupIndex = pairIt->first;
            const quint8 levelIndex = pairIt->second;
            FillGroup::LevelData& level = mGroups[groupIndex].levels[levelIndex];

            const int thisLength = pairIt.key();
            const qreal thisForeignPortion = qreal(level.foreignEdgeSize) / thisLength;

            // 手工统计（替代 boost::accumulators 的 count/mean/min）
            int count = 0;
            qreal sum = 0.0;
            int minVal = std::numeric_limits<int>::max();
            for (auto it = level.conflictWithGroup.begin(); it != level.conflictWithGroup.end(); ++it)
            {
                const int otherLength = mGroups[it.key()].levels[levelIndex].totalEdgeSize();
                ++count;
                sum += otherLength;
                minVal = qMin(minVal, otherLength);
            }
            if (count == 0)
                break;

            const qreal minMetric = minVal / qreal(thisLength);
            const qreal meanMetric = sum / count / thisLength;

            if (!(thisForeignPortion > foreignEdgePortionThreshold))
                continue;

            if (minMetric > 1.0 && meanMetric > 1.2)
            {
                QVector<TaskPoint> taskPoints = tryRemoveConflictingPlane(groupIndex, levelIndex);

                if (!taskPoints.isEmpty())
                {
                    for (const TaskPoint& pt : taskPoints)
                        mPointsQueue.push(pt);
                    processQueue(groupIndex);
                }
            }
        }
    }

private:
    QImage mHeightMap;
    QRect mBoundingRect;
    std::function<bool(int)> mProgress;

    QVector<QImage> mKeyStrokes;
    QVector<QRgb> mKeyStrokeColors;
    QVector<bool> mKeyStrokeTransparent;
    QVector<bool> mKeyStrokeIsBorder;

    QVector<FillGroup> mGroups;
    QVector<qint32> mGroupMap;
    QRect mGroupMapArea;

    PointsPriorityQueue mPointsQueue;

    qint32 mBackgroundGroupId = 0;
    int mBackgroundGroupColor = -1;
    bool mRecolorMode = false;

    qint64 mTotalPixelsToFill = 0;
    quint64 mNumFilledPixels = 0;

    QImage mResult;
};

} // namespace

// ===================== 公开 API =====================

QImage buildHeightMap(const QImage& lineArt, const QRect& bounds, const FilteringOptions& options)
{
    Q_ASSERT(lineArt.format() == QImage::Format_ARGB32_Premultiplied ||
             lineArt.format() == QImage::Format_ARGB32);

    // 1) alpha 提取（QImage 构造不保证清零，先整体填 0 = 无线稿）
    QImage alphaImg(lineArt.size(), QImage::Format_Grayscale8);
    alphaImg.fill(0);
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(lineArt.constScanLine(y));
        uchar* dstLine = alphaImg.scanLine(y);
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            dstLine[x] = static_cast<uchar>(qAlpha(srcLine[x]));
    }

    // 2) 可选 LoG 边缘检测：LoG(0.5*size) → 线性归一化 → 高斯(size)
    if (options.useEdgeDetection && options.edgeDetectionSize > 0.0)
    {
        convolveLoGGray8(alphaImg, bounds, 0.5 * options.edgeDetectionSize, -1.0);
        normalizeGray8(alphaImg, bounds);
        convolveGaussianGray8(alphaImg, bounds, options.edgeDetectionSize);
    }

    // 3) 可选模糊闭缝：备份 → 模糊 → 原图以 source-over 盖回（缺口被模糊值填充）
    if (options.fuzzyRadius > 0.0)
    {
        const QImage saved = alphaImg.copy();
        convolveGaussianGray8(alphaImg, bounds, options.fuzzyRadius);
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
        {
            const uchar* savedLine = saved.constScanLine(y);
            uchar* line = alphaImg.scanLine(y);
            for (int x = bounds.left(); x <= bounds.right(); ++x)
            {
                const qreal src = savedLine[x];
                const qreal dst = line[x];
                // alpha source-over: src + dst*(1-src/255)
                line[x] = static_cast<uchar>(qRound(src + dst * (1.0 - src / 255.0)));
            }
        }
    }

    // 4) 线性归一化 + 平方曲线。
    //    Krita 原链为 normalizeAndInvert(v) = (255-v')²/255 之后再取 255- 反转给 worker，
    //    两次取反抵消后等效于 v'²/255（255 = 线稿屏障）。
    normalizeGray8(alphaImg, bounds);
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        uchar* line = alphaImg.scanLine(y);
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            line[x] = static_cast<uchar>(pow2i(line[x]) / 255);
    }

    return alphaImg;
}

QVector<KeyStroke> splitKeyStrokesByColor(const QImage& strokesImage, const QRect& bounds)
{
    QHash<QRgb, QImage> masks;
    QHash<QRgb, qint64> areas;

    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(strokesImage.constScanLine(y));
        for (int x = bounds.left(); x <= bounds.right(); ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;

            // 反预乘后按精确 RGB 聚类；抗锯齿边缘像素颜色不变，自然归组
            const int r = qBound(0, qRound(qRed(px) * 255.0 / a), 255);
            const int g = qBound(0, qRound(qGreen(px) * 255.0 / a), 255);
            const int b = qBound(0, qRound(qBlue(px) * 255.0 / a), 255);
            const QRgb key = qRgb(r, g, b);

            auto it = masks.find(key);
            if (it == masks.end())
            {
                QImage mask(strokesImage.size(), QImage::Format_Grayscale8);
                mask.fill(0);
                it = masks.insert(key, mask);
            }
            it.value().scanLine(y)[x] = static_cast<uchar>(a);
            areas[key] += a;
        }
    }

    // 面积降序（大面积笔画优先，行为与 Krita 的启发式一致）
    QVector<QPair<qint64, QRgb>> order;
    for (auto it = areas.begin(); it != areas.end(); ++it)
        order.append(qMakePair(it.value(), it.key()));
    std::sort(order.begin(), order.end(),
              [](const QPair<qint64, QRgb>& a, const QPair<qint64, QRgb>& b) {
                  return a.first > b.first;
              });

    QVector<KeyStroke> strokes;
    for (const auto& item : order)
        strokes.append(KeyStroke{ masks.take(item.second), item.second, false });

    return strokes;
}

QImage runWatershed(const QImage& heightMap,
                    QVector<KeyStroke> strokes,
                    const QRect& bounds,
                    qreal cleanUpAmount,
                    const std::function<bool(int)>& progress)
{
    WatershedWorker worker(heightMap, bounds, progress);
    for (const KeyStroke& stroke : strokes)
    {
        if (stroke.mask.isNull())
            continue;
        worker.addKeyStroke(stroke.mask, stroke.color, stroke.isTransparent);
    }

    if (!worker.run(cleanUpAmount))
        return QImage();

    return worker.takeResult();
}

QImage colorize(const QImage& lineArt,
                const QImage& strokesImage,
                const QRect& bounds,
                const FilteringOptions& options,
                const std::function<bool(int)>& progress)
{
    const QImage heightMap = buildHeightMap(lineArt, bounds, options);
    QVector<KeyStroke> strokes = splitKeyStrokesByColor(strokesImage, bounds);
    return runWatershed(heightMap, strokes, bounds, options.cleanUpAmount, progress);
}

} // namespace Colorize
