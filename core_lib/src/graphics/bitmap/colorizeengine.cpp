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
#include <QPainter>
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

// ===================== 跨帧色点搬运 =====================

// 线稿 alpha 提取为灰度图（bounds 内，无线稿 = 0）
QImage extractAlphaMap(const QImage& lineArt, const QRect& bounds)
{
    QImage alphaImg(lineArt.size(), QImage::Format_Grayscale8);
    alphaImg.fill(0);
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(lineArt.constScanLine(y));
        uchar* dstLine = alphaImg.scanLine(y);
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            dstLine[x] = static_cast<uchar>(qAlpha(srcLine[x]));
    }
    return alphaImg;
}

int countLinePixels(const QImage& alphaMap, const QRect& rc, int threshold = 128)
{
    int count = 0;
    for (int y = rc.top(); y <= rc.bottom(); ++y)
    {
        const uchar* line = alphaMap.constScanLine(y);
        for (int x = rc.left(); x <= rc.right(); ++x)
            if (line[x] >= threshold)
                ++count;
    }
    return count;
}

// bounds 内非透明内容的包围盒（无内容返回空矩形）
QRect nonEmptyBounds(const QImage& img, const QRect& bounds)
{
    int minX = img.width(), minY = img.height(), maxX = -1, maxY = -1;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            if (qAlpha(line[x]) > 0)
            {
                minX = qMin(minX, x); maxX = qMax(maxX, x);
                minY = qMin(minY, y); maxY = qMax(maxY, y);
            }
    }
    return maxX < 0 ? QRect() : QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}

struct AnchorMatch
{
    bool valid = false;
    QPoint offset;
};

// 块匹配：以 center 为锚，在 A/B 两张线稿 alpha 图间找最佳平移量。
// 块半径自适应增长到能"看见"线稿（minRadius 起）；锚点附近无线稿
// （纯平区无法定位）返回 invalid。
// 打分 = ε-并列带内选离 reference 最近：先取纯 SSD 最小值，再在
// minSSD + 10%*reference处SSD 的带内候选里选 |d - reference|² 最小。
// 平坦区（SSD 处处相等）带内即全体 → 回退 reference；特征区纯 SSD
// 决胜且不偏好零位移（固定惩罚项与线密度相关的 SSD 量级失配，会
// 把真实大位移压成 0）。
AnchorMatch matchAnchor(const QImage& alphaA, const QImage& alphaB, const QPoint& center,
                        const QRect& bounds, const TransportOptions& options,
                        const QPoint& reference, int windowRadius, int minRadius)
{
    // 自适应块半径：增长到块内能"看见"线稿为止
    int radius = qBound(1, minRadius, options.patchRadiusMax);
    QRect patch;
    int linePixels = 0;
    while (true)
    {
        patch = QRect(center - QPoint(radius, radius), QSize(2 * radius + 1, 2 * radius + 1)).intersected(bounds);
        if (patch.isEmpty())
            return AnchorMatch();
        linePixels = countLinePixels(alphaA, patch);
        if (linePixels >= 8 || radius >= options.patchRadiusMax)
            break;
        radius = qMin(options.patchRadiusMax, radius * 2);
    }
    if (linePixels < 8)
        return AnchorMatch();

    const int n = patch.width() * patch.height();

    double minSSD = std::numeric_limits<double>::max();
    double ssdAtReference = std::numeric_limits<double>::max();
    bool hasReference = false;

    // 候选偏移 → 平均 SSD（两遍中的第一遍数据直接缓存）
    struct Candidate { double meanSSD; int dx; int dy; };
    std::vector<Candidate> candidates;
    candidates.reserve((2 * windowRadius + 1) * (2 * windowRadius + 1));

    for (int dy = reference.y() - windowRadius; dy <= reference.y() + windowRadius; ++dy)
    {
        for (int dx = reference.x() - windowRadius; dx <= reference.x() + windowRadius; ++dx)
        {
            double ssd = 0.0;
            int valid = 0;
            for (int py = 0; py < patch.height(); ++py)
            {
                const int ay = patch.top() + py;
                const int by = ay + dy;
                if (by < bounds.top() || by > bounds.bottom())
                    continue;
                const uchar* aLine = alphaA.constScanLine(ay);
                const uchar* bLine = alphaB.constScanLine(by);
                for (int px = 0; px < patch.width(); ++px)
                {
                    const int ax = patch.left() + px;
                    const int bx = ax + dx;
                    if (bx < bounds.left() || bx > bounds.right())
                        continue;
                    const int diff = int(aLine[ax]) - int(bLine[bx]);
                    ssd += diff * diff;
                    ++valid;
                }
            }
            // 候选块大半落到界外：不可信，跳过
            if (valid * 2 < n)
                continue;
            const double meanSSD = ssd / valid;
            candidates.push_back(Candidate{ meanSSD, dx, dy });
            minSSD = qMin(minSSD, meanSSD);
            if (dx == reference.x() && dy == reference.y())
            {
                ssdAtReference = meanSSD;
                hasReference = true;
            }
        }
    }
    if (candidates.empty())
        return AnchorMatch();

    // ε 带宽度取参考点 SSD 的 10%（平坦区趋 0 → 精确并列 → 回退 reference）
    const double eps = 0.10 * (hasReference ? ssdAtReference : minSSD) + 1e-9;

    AnchorMatch best;
    double bestDist = std::numeric_limits<double>::max();
    for (const Candidate& c : candidates)
    {
        if (c.meanSSD > minSSD + eps)
            continue;
        const double rx = c.dx - reference.x();
        const double ry = c.dy - reference.y();
        const double dist = rx * rx + ry * ry;
        if (dist < bestDist)
        {
            bestDist = dist;
            best.valid = true;
            best.offset = QPoint(c.dx, c.dy);
        }
    }
    return best;
}

// 位移列表每轴取中位数（多锚投票的鲁棒聚合）
QPoint medianOffset(const QVector<QPoint>& offsets)
{
    Q_ASSERT(!offsets.isEmpty());
    QVector<int> xs, ys;
    xs.reserve(offsets.size());
    ys.reserve(offsets.size());
    for (const QPoint& p : offsets)
    {
        xs.append(p.x());
        ys.append(p.y());
    }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    return QPoint(xs[xs.size() / 2], ys[ys.size() / 2]);
}

// 全局位移估计：在线稿像素上均匀采样锚点（大窗口，覆盖多结构），
// 各锚独立匹配后取中位数。相邻动画帧以整体运动为主，该层先吸收
// 全部位移，组级细化只需处理残差形变。
// 注意：大分辨率下应换金字塔/下采样实现（锚数×窗口²×块² 的暴力积）。
QPoint globalOffsetEstimate(const QImage& alphaA, const QImage& alphaB,
                            const QRect& bounds, const TransportOptions& options)
{
    QVector<QPoint> anchors;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const uchar* line = alphaA.constScanLine(y);
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            if (line[x] >= 128)
                anchors.append(QPoint(x, y));
    }
    if (anchors.isEmpty())
        return QPoint(0, 0);

    const int step = qMax(1, anchors.size() / 9);
    QVector<QPoint> votes;
    for (int i = step / 2; i < anchors.size(); i += step)
    {
        const AnchorMatch m = matchAnchor(alphaA, alphaB, anchors[i], bounds, options,
                                          QPoint(0, 0), options.searchRadius,
                                          options.patchRadiusMax);
        if (m.valid)
            votes.append(m.offset);
    }
    if (votes.size() < 3)
        return QPoint(0, 0);
    return medianOffset(votes);
}

} // namespace

// ===================== 公开 API =====================

QImage buildHeightMap(const QImage& lineArt, const QRect& bounds, const FilteringOptions& options)
{
    Q_ASSERT(lineArt.format() == QImage::Format_ARGB32_Premultiplied ||
             lineArt.format() == QImage::Format_ARGB32);

    // 1) alpha 提取（QImage 构造不保证清零，extractAlphaMap 整体填 0 = 无线稿）
    QImage alphaImg = extractAlphaMap(lineArt, bounds);

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

namespace {
/* 灰度蒙版 Chebyshev 膨胀（每遍 3x3 取大扩 1px） */
QImage dilateMask(const QImage& mask, int radius)
{
    QImage cur = mask;
    for (int pass = 0; pass < radius; ++pass)
    {
        QImage next = cur;
        for (int y = 0; y < cur.height(); ++y)
        {
            for (int x = 0; x < cur.width(); ++x)
            {
                uchar m = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    const int yy = y + dy;
                    if (yy < 0 || yy >= cur.height())
                        continue;
                    const uchar* line = cur.constScanLine(yy);
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int xx = x + dx;
                        if (xx < 0 || xx >= cur.width())
                            continue;
                        if (line[xx] > m)
                            m = line[xx];
                    }
                }
                next.scanLine(y)[x] = m;
            }
        }
        cur = next;
    }
    return cur;
}
} // namespace

QVector<int> classifyStrokeMasters(QVector<KeyStroke>& strokes,
                                   QRgb transparentColor, bool hasTransparent,
                                   bool mergeVariants)
{
    const int n = strokes.size();
    QVector<int> master(n);
    for (int i = 0; i < n; ++i)
        master[i] = i;
    if (n <= 1)
        return master;

    const auto isTransp = [&](int i) {
        return hasTransparent && strokes[i].color == transparentColor;
    };

    // pass 1: 色相近似并入首个相似主色（面积降序 → 首个 = 最大）。
    // 透明组自身不折叠（恒为主色），但可作折叠目标（其色相变体并入）。
    // mergeVariants=false 时跳过（显示路径"以画布为准"：同色相的
    // 实心色如暗红/纯红是用户分别涂的，独立显示；软边渐变无 3x3
    // 同色核已被上游滤除，无需靠色相合并收拾）
    if (mergeVariants)
    {
        for (int i = 0; i < n; ++i)
        {
            if (isTransp(i))
                continue;
            for (int j = 0; j < i; ++j)
            {
                if (master[j] != j)
                    continue;
                if (similarColors(strokes[j].color, strokes[i].color))
                {
                    master[i] = j;
                    break;
                }
            }
        }
    }

    // pass 2: 空间混合带。对未被 pass 1 折叠的组建立膨胀邻接；
    // 与 ≥2 个其它组相邻 = 叠色混合带，并入面积最大相邻组
    // （下标最小 = 面积降序最大）。
    QVector<int> cand;
    for (int i = 0; i < n; ++i)
        if (master[i] == i && !strokes[i].mask.isNull() && !isTransp(i))
            cand.append(i);
    if (cand.size() >= 3)
    {
        // 各组覆盖包围盒（稀疏蒙版全图尺寸，扫一次）
        QVector<QRect> bbox(n);
        for (int i : cand)
        {
            QRect b;
            const QImage& m = strokes[i].mask;
            for (int y = 0; y < m.height(); ++y)
            {
                const uchar* line = m.constScanLine(y);
                for (int x = 0; x < m.width(); ++x)
                    if (line[x] > 0)
                        b = b.isNull() ? QRect(x, y, 1, 1) : b.united(QRect(x, y, 1, 1));
            }
            bbox[i] = b;
        }
        // 膨胀邻接：j 的覆盖像素落在 dilate(i,2) 内即相邻
        QVector<QImage> dil(n);
        for (int i : cand)
            dil[i] = dilateMask(strokes[i].mask, 2);
        const auto adjacent = [&](int i, int j) {
            if (bbox[j].isNull() || bbox[i].isNull())
                return false;
            for (int y = bbox[j].top(); y <= bbox[j].bottom(); ++y)
            {
                const uchar* line = strokes[j].mask.constScanLine(y);
                const uchar* di = dil[i].constScanLine(y);
                for (int x = bbox[j].left(); x <= bbox[j].right(); ++x)
                    if (line[x] > 0 && di[x] > 0)
                        return true;
            }
            return false;
        };
        for (int ii = 0; ii < cand.size(); ++ii)
        {
            const int i = cand[ii];
            int bestNeighbor = -1;
            int neighborCount = 0;
            for (int jj = 0; jj < cand.size(); ++jj)
            {
                if (jj == ii)
                    continue;
                const int j = cand[jj];
                if (adjacent(i, j))
                {
                    ++neighborCount;
                    if (bestNeighbor < 0 || j < bestNeighbor)
                        bestNeighbor = j;
                }
            }
            if (neighborCount >= 2)
                master[i] = bestNeighbor;
        }
    }

    // 链式跟随：混合带并入的相邻组若也是混合带，顺藤到最终主色
    for (int i = 0; i < n; ++i)
    {
        int m = master[i];
        int guard = 0;
        while (master[m] != m && guard++ < n)
            m = master[m];
        master[i] = m;
    }

    // 主色代表值提升：每族取明度(V)最高、并列取饱和度(S)更高的精确值
    // ——笔尖混合/涂抹会把大面积像素画脏（混入透明底的黑，明度降低），
    // 面积最大的代码常是脏色而非用户所选色；笔色是族内最纯最亮的那个。
    // 透明组精确色不动（isTransparent 按精确 == 匹配）。
    for (int m = 0; m < n; ++m)
    {
        if (master[m] != m)
            continue;
        if (hasTransparent && strokes[m].color == transparentColor)
            continue;
        int best = m;
        int bestV = -1;
        int bestS = -1;
        for (int g = m; g < n; ++g) // 面积降序：并列时保持更早（面积大）的
        {
            if (master[g] != m)
                continue;
            int h = 0, s = 0, v = 0;
            QColor(strokes[g].color).getHsv(&h, &s, &v);
            if (v > bestV || (v == bestV && s > bestS))
            {
                bestV = v;
                bestS = s;
                best = g;
            }
        }
        if (best != m)
            strokes[m].color = strokes[best].color;
    }

    return master;
}

void claimIntentColors(QVector<KeyStroke>& strokes, const QVector<int>& master,
                       QRgb transparentColor, bool hasTransparent,
                       const QVector<QRgb>& intentColors)
{
    if (intentColors.isEmpty())
        return;
    const int n = strokes.size();
    for (int m = 0; m < n; ++m)
    {
        if (master[m] != m)
            continue;
        if (hasTransparent && strokes[m].color == transparentColor)
            continue;
        // 同族可能有多个相似候选（历史选色/色板），取离族内实际像素
        // （已提升为最亮代表值）RGB 距离最近者——涂纯红时即使登记表
        // 里残留暗红，认领结果也是纯红
        int bestDistSq = std::numeric_limits<int>::max();
        QRgb best = strokes[m].color;
        for (const QRgb intent : intentColors)
        {
            if (similarColors(intent, strokes[m].color))
            {
                const int d = colorDistanceSq(intent, strokes[m].color);
                if (d < bestDistSq)
                {
                    bestDistSq = d;
                    best = intent;
                }
            }
        }
        strokes[m].color = best;
    }
}

QImage normalizeStrokeColors(const QImage& strokesImage, const QRect& bounds,
                             QRgb transparentColor, bool hasTransparent,
                             const QVector<QRgb>& intentColors)
{
    QImage result(strokesImage.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    if (bounds.isEmpty() || strokesImage.isNull())
        return result;

    QVector<KeyStroke> groups = splitKeyStrokesByColor(strokesImage, bounds);
    if (groups.isEmpty())
        return result;
    const QVector<int> master = classifyStrokeMasters(groups, transparentColor,
                                                      hasTransparent);
    claimIntentColors(groups, master, transparentColor, hasTransparent, intentColors);
    // 族内像素精确重涂为代表代码（含意图色）：α 取原像素透明度
    for (int g = 0; g < groups.size(); ++g)
    {
        const QRgb rep = groups[master[g]].color;
        const QImage& mask = groups[g].mask;
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
        {
            const uchar* mLine = mask.constScanLine(y);
            QRgb* dst = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = bounds.left(); x <= bounds.right(); ++x)
            {
                if (mLine[x] == 0)
                    continue;
                const int a = mLine[x];
                dst[x] = qPremultiply(qRgba(qRed(rep), qGreen(rep), qBlue(rep), a));
            }
        }
    }
    return result;
}

void mergeVariantStrokes(QVector<KeyStroke>& strokes, const FilteringOptions& options)
{
    if (strokes.size() <= 1)
        return;

    // 透明组钉首位：精确色保持不变（后续 isTransparent 按精确 == 标记仍命中）
    if (options.hasTransparentColor)
    {
        for (int i = 0; i < strokes.size(); ++i)
        {
            if (strokes[i].color == options.transparentColor)
            {
                if (i != 0)
                    strokes.move(i, 0);
                break;
            }
        }
    }

    // 主色分类：变体（色相近似）与叠色混合带（空间贴 ≥2 组）并入主色
    const QVector<int> master = classifyStrokeMasters(strokes, options.transparentColor, options.hasTransparentColor);

    QVector<KeyStroke> merged;
    for (int g = 0; g < strokes.size(); ++g)
    {
        const int mg = master[g];
        if (mg == g)
        {
            merged.append(strokes[g]);
            continue;
        }
        // 找 merged 中主色组 mg（mg < g，已在 merged 内；主色色值唯一）
        KeyStroke* dst = nullptr;
        for (KeyStroke& m : merged)
        {
            if (m.color == strokes[mg].color)
            {
                dst = &m;
                break;
            }
        }
        if (dst == nullptr)
            continue; // 主色组未入列（蒙版为空等退化）：丢弃该组

        const bool variant = similarColors(dst->color, strokes[g].color);
        if (variant)
        {
            // 变体只在其主色覆盖邻域（膨胀2px）内并入（抗锯齿边/叠色
            // 交界补强主色种子）；远离主色的孤立变体岛（如落在另一色
            // 域中的混色条纹）整岛丢弃——否则以主色名义扩散成杂色区
            const QImage nearMaster = dilateMask(dst->mask, 2);
            for (int y = 0; y < dst->mask.height(); ++y)
            {
                uchar* d = dst->mask.scanLine(y);
                const uchar* src = strokes[g].mask.constScanLine(y);
                const uchar* nearLine = nearMaster.constScanLine(y);
                for (int x = 0; x < dst->mask.width(); ++x)
                    if (nearLine[x] > 0 && src[x] > d[x])
                        d[x] = src[x];
            }
        }
        else
        {
            // 叠色混合带：空间上必然贴着母色，整组并入（覆盖度取大）
            for (int y = 0; y < dst->mask.height(); ++y)
            {
                uchar* d = dst->mask.scanLine(y);
                const uchar* src = strokes[g].mask.constScanLine(y);
                for (int x = 0; x < dst->mask.width(); ++x)
                    if (src[x] > d[x])
                        d[x] = src[x];
            }
        }
    }
    strokes = merged;
}

namespace {
/*
 * 每封闭区域只保留一组有色种子：区域内种子覆盖最大者胜出，其余
 * 有色种子在该区域内清零。同区域多色点是分水岭多色斑的直接来源
 * （用户规则：一个封闭区域只标记一个颜色点）。
 * 仅作用于封闭区域（touchesEdge 豁免）——开放/半开放区域（如缺口
 * 的连通域）颜色靠分水岭距离竞争分治是既有正确行为，清种子反而丢色。
 * 透明组豁免——透明是保护标记（可在有色区域内开洞），不参与竞争。
 */
void enforceOneSeedPerRegion(QVector<KeyStroke>& strokes,
                             const RegionSegmentation& seg)
{
    const int regionCount = seg.regions.size();
    const int groupCount = strokes.size();
    if (regionCount == 0 || groupCount == 0)
        return;

    // 区域×组 覆盖计数（labelOf 为 bounds 行优先；mask 为全图尺寸）
    QVector<qint64> counts(regionCount * groupCount, 0);
    for (int y = 0; y < seg.bounds.height(); ++y)
    {
        const qint32* labelLine = seg.labelOf.constData() + y * seg.bounds.width();
        for (int x = 0; x < seg.bounds.width(); ++x)
        {
            const qint32 label = labelLine[x];
            if (label <= 0)
                continue;
            const int absY = y + seg.bounds.top();
            const int absX = x + seg.bounds.left();
            for (int g = 0; g < groupCount; ++g)
            {
                const QImage& mask = strokes[g].mask;
                if (!mask.isNull() && mask.constScanLine(absY)[absX] > 0)
                    ++counts[(label - 1) * groupCount + g];
            }
        }
    }

    const int bw = seg.bounds.width();
    const int left = seg.bounds.left();
    for (int r = 0; r < regionCount; ++r)
    {
        if (seg.regions[r].touchesEdge)
            continue; // 开放区域：颜色竞争分治，不清种子
        int winner = -1;
        qint64 best = 0;
        for (int g = 0; g < groupCount; ++g)
        {
            if (strokes[g].isTransparent)
                continue;
            const qint64 c = counts[r * groupCount + g];
            if (c > best)
            {
                best = c;
                winner = g;
            }
        }
        if (winner < 0)
            continue;
        for (int g = 0; g < groupCount; ++g)
        {
            if (g == winner || strokes[g].isTransparent || counts[r * groupCount + g] == 0)
                continue;
            QImage& mask = strokes[g].mask;
            if (mask.isNull())
                continue;
            for (int y = 0; y < seg.bounds.height(); ++y)
            {
                const qint32* labelLine = seg.labelOf.constData() + y * bw;
                uchar* maskLine = mask.scanLine(y + seg.bounds.top());
                for (int x = 0; x < bw; ++x)
                    if (labelLine[x] == r + 1)
                        maskLine[x + left] = 0;
            }
        }
    }
}
} // namespace

QImage colorize(const QImage& lineArt,
                const QImage& strokesImage,
                const QRect& bounds,
                const FilteringOptions& options,
                const std::function<bool(int)>& progress)
{
    const QImage heightMap = buildHeightMap(lineArt, bounds, options);
    QVector<KeyStroke> strokes = splitKeyStrokesByColor(strokesImage, bounds);
    // 变体组归并：混色变体不再作为独立颜色源扩散（防杂色区）
    mergeVariantStrokes(strokes, options);

    // 透明颜色标记（Krita transparentIndex：该颜色区域保持不填）
    if (options.hasTransparentColor)
    {
        for (auto& stroke : strokes)
        {
            if (stroke.color == options.transparentColor)
                stroke.isTransparent = true;
        }
    }

    // 一区一点：每个封闭区域只保留覆盖最大的有色种子
    enforceOneSeedPerRegion(strokes, segmentRegions(lineArt, bounds, options));

    return runWatershed(heightMap, strokes, bounds, options.cleanUpAmount, progress);
}

QImage transportStrokes(const QImage& lineArtA,
                        const QImage& strokesA,
                        const QImage& lineArtB,
                        const QRect& bounds,
                        const TransportOptions& options)
{
    Q_ASSERT(lineArtA.size() == strokesA.size());
    Q_ASSERT(lineArtB.size() == strokesA.size());

    QImage result(strokesA.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    if (bounds.isEmpty())
        return result;

    const QImage alphaA = extractAlphaMap(lineArtA, bounds);
    const QImage alphaB = extractAlphaMap(lineArtB, bounds);

    // 第一层：全图锚点投票估计全局位移（整体运动）
    const QPoint globalOffset = globalOffsetEstimate(alphaA, alphaB, bounds, options);

    QPainter painter(&result);

    // 第二层：每个颜色组在全局位移邻域内细化（残差形变），
    // 锚点 = 组质心，整组按包围盒抠出平移贴回。
    // 组间不同位移若造成重叠，后贴者（面积较小色组）覆盖先贴者
    const QVector<KeyStroke> strokes = splitKeyStrokesByColor(strokesA, bounds);
    for (const KeyStroke& stroke : strokes)
    {
        int minX = bounds.right(), minY = bounds.bottom();
        int maxX = bounds.left(), maxY = bounds.top();
        qint64 sumX = 0, sumY = 0, count = 0;
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
        {
            const uchar* line = stroke.mask.constScanLine(y);
            for (int x = bounds.left(); x <= bounds.right(); ++x)
            {
                if (line[x] > 0)
                {
                    minX = qMin(minX, x);
                    maxX = qMax(maxX, x);
                    minY = qMin(minY, y);
                    maxY = qMax(maxY, y);
                    sumX += x;
                    sumY += y;
                    ++count;
                }
            }
        }
        if (count == 0)
            continue;

        const QPoint centroid(qRound(sumX / double(count)), qRound(sumY / double(count)));
        const AnchorMatch m = matchAnchor(alphaA, alphaB, centroid, bounds, options,
                                           globalOffset, options.refineRadius,
                                           options.patchRadiusMin);
        // 纯平锚点无法定位：跟随全局位移
        const QPoint offset = m.valid ? m.offset : globalOffset;

        const QRect box(QPoint(minX, minY), QPoint(maxX, maxY));
        QImage colored(box.size(), QImage::Format_ARGB32_Premultiplied);
        colored.fill(Qt::transparent);
        for (int y = 0; y < box.height(); ++y)
        {
            const uchar* mLine = stroke.mask.constScanLine(box.top() + y);
            QRgb* cLine = reinterpret_cast<QRgb*>(colored.scanLine(y));
            for (int x = 0; x < box.width(); ++x)
            {
                const uchar a = mLine[box.left() + x];
                if (a > 0)
                    cLine[x] = qPremultiply(qRgba(qRed(stroke.color), qGreen(stroke.color), qBlue(stroke.color), a));
            }
        }
        painter.drawImage(box.topLeft() + offset, colored);
    }

    painter.end();
    return result;
}

QImage transportStrokesByBounds(const QImage& lineArtA,
                                const QImage& strokesA,
                                const QImage& lineArtB,
                                const QRect& bounds)
{
    Q_ASSERT(lineArtA.size() == strokesA.size());
    Q_ASSERT(lineArtB.size() == strokesA.size());

    QImage result(strokesA.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    if (bounds.isEmpty())
        return result;

    const QRect boxA = nonEmptyBounds(lineArtA, bounds);
    const QRect boxB = nonEmptyBounds(lineArtB, bounds);
    if (boxA.isEmpty() || boxB.isEmpty() || boxA.width() < 1 || boxA.height() < 1)
        return result;

    // 每组重画固定大小实心标记点（不搬像素，无链式退化）；落点容差 ≈ dot/2
    const int dot = 9;

    QPainter painter(&result);
    painter.setPen(Qt::NoPen);

    const QVector<KeyStroke> strokes = splitKeyStrokesByColor(strokesA, bounds);
    for (const KeyStroke& stroke : strokes)
    {
        // 组质心 → 帧A包围盒内相对位置（越界的组钳到边缘附近）
        qint64 sumX = 0, sumY = 0, count = 0;
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
        {
            const uchar* line = stroke.mask.constScanLine(y);
            for (int x = bounds.left(); x <= bounds.right(); ++x)
                if (line[x] > 0) { sumX += x; sumY += y; ++count; }
        }
        if (count == 0)
            continue;

        const qreal u = qBound(0.02, (sumX / double(count) - boxA.left()) / boxA.width(), 0.98);
        const qreal v = qBound(0.02, (sumY / double(count) - boxA.top()) / boxA.height(), 0.98);
        const QPoint target(qRound(boxB.left() + u * (boxB.width() - dot)),
                            qRound(boxB.top() + v * (boxB.height() - dot)));

        painter.setBrush(QColor(stroke.color));
        painter.drawRect(target.x(), target.y(), dot, dot);
    }

    painter.end();
    return result;
}

QImage makeBackgroundWrap(const QImage& lineArt, const QRect& bounds, QRgb color)
{
    QImage result(lineArt.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    if (bounds.isEmpty())
        return result;

    const QRect box = nonEmptyBounds(lineArt, bounds);
    if (box.isEmpty())
        return result;

    // 描边形式（用户指定）：沿线稿包围盒边缘一圈保护色。
    // 连通背景接触描边即归透明组；描边厚 3px 保证闭缝模糊下仍是有效种子
    QPainter painter(&result);
    QPen pen(QColor(color), 3);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(box.adjusted(-1, -1, 1, 1));
    painter.end();
    return result;
}

RegionSegmentation segmentRegions(const QImage& lineArt, const QRect& bounds, const FilteringOptions& options)
{
    RegionSegmentation seg;
    seg.bounds = bounds;
    if (bounds.isEmpty())
        return seg;

    // 屏障 = 滤波后的高度图（含闭缝/边缘检测）；任何非零高度都是墙
    // （分割须比分水岭的梯度分离更细而非更粗：模糊缝/软重叠墙处
    // 分水岭能分开两色，若连通域判成一区会连累"一区一种子"误清色）
    const QImage height = buildHeightMap(lineArt, bounds, options);
    seg.labelOf.fill(0, bounds.width() * bounds.height());

    const int w = bounds.width();
    const auto labelAt = [&](int x, int y) { return seg.labelOf[(y - bounds.top()) * w + (x - bounds.left())]; };
    const auto setLabel = [&](int x, int y, qint32 l) { seg.labelOf[(y - bounds.top()) * w + (x - bounds.left())] = l; };

    QStack<QPoint> stack;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const uchar* hLine = height.constScanLine(y);
        for (int x = bounds.left(); x <= bounds.right(); ++x)
        {
            if (hLine[x] > 0 || labelAt(x, y) != 0)
                continue;

            const qint32 label = static_cast<qint32>(seg.regions.size()) + 1;
            RegionLabel r;
            r.bounds = QRect(x, y, 1, 1);
            qint64 sumX = 0, sumY = 0;
            bool touches = false;

            stack.push(QPoint(x, y));
            setLabel(x, y, label);
            while (!stack.isEmpty())
            {
                const QPoint pt = stack.pop();
                ++r.area;
                sumX += pt.x();
                sumY += pt.y();
                r.bounds = r.bounds.united(QRect(pt, QSize(1, 1)));
                if (pt.x() == bounds.left() || pt.x() == bounds.right() ||
                    pt.y() == bounds.top() || pt.y() == bounds.bottom())
                    touches = true;

                const QPoint neighbours[4] = { pt + QPoint(-1, 0), pt + QPoint(1, 0),
                                               pt + QPoint(0, -1), pt + QPoint(0, 1) };
                for (const QPoint& n : neighbours)
                {
                    if (!bounds.contains(n))
                        continue;
                    if (height.constScanLine(n.y())[n.x()] > 0)
                        continue;
                    if (labelAt(n.x(), n.y()) != 0)
                        continue;
                    setLabel(n.x(), n.y(), label);
                    stack.push(n);
                }
            }

            r.centroid = QPoint(qRound(sumX / double(r.area)), qRound(sumY / double(r.area)));
            r.touchesEdge = touches;
            // 锚点 = 区域内离质心最近的像素（扫区域包围盒）
            qint64 bestDist = std::numeric_limits<qint64>::max();
            for (int yy = r.bounds.top(); yy <= r.bounds.bottom(); ++yy)
                for (int xx = r.bounds.left(); xx <= r.bounds.right(); ++xx)
                    if (labelAt(xx, yy) == label)
                    {
                        const qint64 dx = xx - r.centroid.x();
                        const qint64 dy = yy - r.centroid.y();
                        const qint64 d = dx * dx + dy * dy;
                        if (d < bestDist)
                        {
                            bestDist = d;
                            r.anchor = QPoint(xx, yy);
                        }
                    }
            seg.regions.append(r);
        }
    }
    return seg;
}

QImage transportStrokesByRegions(const QImage& lineArtA,
                                 const QImage& coloringA,
                                 const QImage& lineArtB,
                                 const QRect& bounds,
                                 const FilteringOptions& options,
                                 QRgb transparentColor,
                                 bool hasTransparent)
{
    Q_ASSERT(lineArtA.size() == coloringA.size());
    Q_ASSERT(lineArtB.size() == coloringA.size());

    QImage result(coloringA.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    if (bounds.isEmpty())
        return result;

    // 落点 = 目标帧各分割区域锚点（保证标记落进封闭区域）；
    // 颜色 = 锚点经包围盒相对映射回源帧、采样源帧着色颜色场——
    // 不做"源区域单色"假设（软边/缺口的半连通区域里分水岭本就两色分治）
    const RegionSegmentation segB = segmentRegions(lineArtB, bounds, options);
    if (segB.regions.isEmpty())
        return result;

    const QRect boxA = nonEmptyBounds(lineArtA, bounds);
    const QRect boxB = nonEmptyBounds(lineArtB, bounds);
    if (boxA.isEmpty() || boxB.isEmpty() || boxA.width() < 1 || boxA.height() < 1)
        return result;

    const int dot = 9;

    // 源帧颜色统计 → 主色表：相近色并入面积最大者（画笔软边/流量中间色
    // 不产生独立标记，防止杂色在帧间滚雪球）
    struct ColorStat
    {
        qint64 count = 0;
        qint64 sumX = 0;
        qint64 sumY = 0;
    };
    QHash<QRgb, ColorStat> exact;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(coloringA.constScanLine(y));
        for (int x = bounds.left(); x <= bounds.right(); ++x)
        {
            const QRgb px = line[x];
            if (qAlpha(px) == 0)
                continue;
            ColorStat& s = exact[qUnpremultiply(px)];
            ++s.count;
            s.sumX += x;
            s.sumY += y;
        }
    }
    QVector<QPair<qint64, QRgb>> order;
    for (auto it = exact.begin(); it != exact.end(); ++it)
        order.append(qMakePair(it.value().count, it.key()));
    std::sort(order.begin(), order.end(),
              [](const QPair<qint64, QRgb>& a, const QPair<qint64, QRgb>& b) { return a.first > b.first; });
    struct MajorColor
    {
        QRgb color = 0;
        ColorStat stat;
    };
    QVector<MajorColor> majors;
    for (const auto& item : order)
    {
        const ColorStat& s = exact[item.second];
        bool merged = false;
        for (MajorColor& m : majors)
        {
            if (similarColors(m.color, item.second))
            {
                m.stat.count += s.count;
                m.stat.sumX += s.sumX;
                m.stat.sumY += s.sumY;
                merged = true;
                break;
            }
        }
        if (!merged)
            majors.append(MajorColor{ item.second, s });
    }
    if (majors.isEmpty())
        return result;

    // 归一到主色表：majors 按面积降序，取第一个相似主色（=面积最大者），
    // 无相似则原样返回（着色结果源自 colorize，混合带已在源头归并）
    const auto normalizeColor = [&majors](QRgb c) {
        for (const MajorColor& m : majors)
        {
            if (similarColors(m.color, c))
                return m.color;
        }
        return c;
    };

    QPainter painter(&result);
    painter.setPen(Qt::NoPen);

    // 区域标记状态：0=未标记 1=颜色标记 2=透明标记（一区一点不变量）
    QVector<int> regionState(segB.regions.size(), 0);
    QVector<QRgb> regionColor(segB.regions.size(), 0);

    const auto sampleSource = [&](const QPoint& targetPoint, QRgb& outColor, bool& outTransparent) {
        const qreal u = qBound(0.0, (targetPoint.x() - boxB.left()) / double(boxB.width()), 1.0);
        const qreal v = qBound(0.0, (targetPoint.y() - boxB.top()) / double(boxB.height()), 1.0);
        const QPoint src(qRound(boxA.left() + u * boxA.width()),
                         qRound(boxA.top() + v * boxA.height()));
        if (!bounds.contains(src))
        {
            outTransparent = true;
            outColor = 0;
            return;
        }
        const QRgb px = coloringA.pixel(src);
        if (qAlpha(px) > 0)
        {
            outColor = normalizeColor(qUnpremultiply(px));
            outTransparent = false;
        }
        else
        {
            outColor = 0;
            outTransparent = true;
        }
    };

    for (int ri = 0; ri < segB.regions.size(); ++ri)
    {
        const RegionLabel& r = segB.regions[ri];
        QRgb color = 0;
        bool transparent = false;
        sampleSource(r.anchor, color, transparent);
        if (transparent)
        {
            if (!hasTransparent)
                continue; // 未标记：稍后按邻近原则继承
            painter.setBrush(QColor(transparentColor));
            regionState[ri] = 2;
        }
        else
        {
            painter.setBrush(QColor(color));
            regionState[ri] = 1;
            regionColor[ri] = color;
        }
        painter.drawRect(r.anchor.x(), r.anchor.y(), dot, dot);
    }

    // 映射点 → 所在区域（出界/落屏障返回 -1）
    const auto regionAt = [&](const QPoint& pt) {
        const int rx = pt.x() - segB.bounds.left();
        const int ry = pt.y() - segB.bounds.top();
        if (rx < 0 || ry < 0 || rx >= segB.bounds.width() || ry >= segB.bounds.height())
            return -1;
        return int(segB.labelOf[ry * segB.bounds.width() + rx]) - 1;
    };
    // 最近区域（按质心距离，可限定状态过滤）
    const auto nearestRegion = [&](const QPoint& pt, int wantedState) {
        int best = -1;
        qint64 bestD = std::numeric_limits<qint64>::max();
        for (int i = 0; i < segB.regions.size(); ++i)
        {
            if (wantedState >= 0 && regionState[i] != wantedState)
                continue;
            const QPoint d = segB.regions[i].centroid - pt;
            const qint64 dist = qint64(d.x()) * d.x() + qint64(d.y()) * d.y();
            if (dist < bestD)
            {
                bestD = dist;
                best = i;
            }
        }
        return best;
    };

    // 颜色补漏：主色表每种颜色（面积≥64px）都应出现在目标标记中——
    // 缺失时把映射点所在区域的标记改成该颜色（替换而非叠加，
    // 维持一区一点），保证任何颜色不丢
    for (const MajorColor& m : majors)
    {
        if (m.stat.count < 64)
            continue;
        bool present = false;
        for (int i = 0; i < regionColor.size() && !present; ++i)
            if (regionState[i] == 1 && regionColor[i] == m.color)
                present = true;
        if (present)
            continue;
        const qreal u = qBound(0.0, (m.stat.sumX / double(m.stat.count) - boxA.left()) / double(boxA.width()), 1.0);
        const qreal v = qBound(0.0, (m.stat.sumY / double(m.stat.count) - boxA.top()) / double(boxA.height()), 1.0);
        const QPoint target(qRound(boxB.left() + u * boxB.width()),
                            qRound(boxB.top() + v * boxB.height()));
        int ri = regionAt(target);
        if (ri < 0)
            ri = nearestRegion(target, -1);
        if (ri >= 0 && regionState[ri] == 2)
            ri = nearestRegion(target, 0); // 透明标记区不改写（保护语义优先）
        if (ri < 0)
            continue;
        regionState[ri] = 1;
        regionColor[ri] = m.color;
        painter.setBrush(QColor(m.color));
        painter.drawRect(segB.regions[ri].anchor.x(), segB.regions[ri].anchor.y(), dot, dot);
    }

    // 邻近继承：仍未标记的封闭区域（采空且无透明语义）继承质心最近的
    // 已标区域颜色——始终保证每个封闭区域有一个颜色点
    for (int ri = 0; ri < segB.regions.size(); ++ri)
    {
        if (regionState[ri] != 0 || segB.regions[ri].touchesEdge)
            continue;
        const int src = nearestRegion(segB.regions[ri].centroid, 1);
        if (src < 0)
            continue;
        regionState[ri] = 1;
        regionColor[ri] = regionColor[src];
        painter.setBrush(QColor(regionColor[ri]));
        painter.drawRect(segB.regions[ri].anchor.x(), segB.regions[ri].anchor.y(), dot, dot);
    }

    painter.end();
    return result;
}

} // namespace Colorize
