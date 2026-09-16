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
#include "layercolorize.h"

#include <QPainter>
#include <QDir>

#include "colorizeimage.h"
#include "graphics/bitmap/colorizeengine.h"

LayerColorize::LayerColorize(int id)
    : LayerBitmap(id, Layer::COLORIZE)
{
    setName(tr("Colorize Layer"));
}

LayerColorize::~LayerColorize()
{
}

ColorizeImage* LayerColorize::getColorizeImageAtFrame(int frameNumber)
{
    Q_ASSERT(frameNumber >= 1);
    return static_cast<ColorizeImage*>(getKeyFrameAt(frameNumber));
}

ColorizeImage* LayerColorize::getLastColorizeImageAtFrame(int frameNumber)
{
    Q_ASSERT(frameNumber >= 1);
    return static_cast<ColorizeImage*>(getLastKeyFrameAtPosition(displayFrameFor(frameNumber)));
}

void LayerColorize::replaceKeyFrame(const KeyFrame* keyframe)
{
    auto colorizeFrame = getColorizeImageAtFrame(keyframe->pos());
    *static_cast<BitmapImage*>(colorizeFrame) = *static_cast<const BitmapImage*>(keyframe);
    colorizeFrame->setNeedsUpdate(true);
}

KeyFrame* LayerColorize::createKeyFrame(int position)
{
    auto frame = new ColorizeImage;
    frame->setPos(position);
    frame->enableAutoCrop(true);
    return frame;
}

void LayerColorize::loadImageAtFrame(QString strFilePath, QPoint topLeft, int frameNumber, qreal opacity)
{
    auto pKeyFrame = new ColorizeImage;
    *static_cast<BitmapImage*>(pKeyFrame) = BitmapImage(topLeft, strFilePath);
    pKeyFrame->enableAutoCrop(true);
    pKeyFrame->setPos(frameNumber);
    pKeyFrame->setOpacity(opacity);
    loadKey(pKeyFrame);
}

QDomElement LayerColorize::createDomElement(QDomDocument& doc) const
{
    QDomElement layerElem = LayerBitmap::createDomElement(doc);

    if (mUseEdgeDetection)
        layerElem.setAttribute("colorizeEdgeDetection", 1);
    if (mEdgeDetectionSize != 4.0)
        layerElem.setAttribute("colorizeEdgeSize", QString::number(mEdgeDetectionSize, 'f', 1));
    if (mFuzzyRadius != 0.0)
        layerElem.setAttribute("colorizeFuzzyRadius", QString::number(mFuzzyRadius, 'f', 1));
    if (mCleanUpAmount != 0.7)
        layerElem.setAttribute("colorizeCleanUp", QString::number(mCleanUpAmount, 'f', 2));
    if (!mEditKeyStrokes)
        layerElem.setAttribute("colorizeEditKeyStrokes", 0);
    if (!mShowColoring)
        layerElem.setAttribute("colorizeShowColoring", 0);
    if (mHasTransparentColor)
        layerElem.setAttribute("colorizeTransparentColor", static_cast<int>(mTransparentColor));
    if (!mIntentColors.isEmpty())
    {
        QStringList hex;
        for (QRgb c : mIntentColors)
            hex << QString::number(static_cast<uint>(c), 16);
        layerElem.setAttribute("colorizeIntentColors", hex.join(";"));
    }

    return layerElem;
}

void LayerColorize::loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep)
{
    if (element.attribute("colorizeEdgeDetection").toInt())
        mUseEdgeDetection = true;
    if (!element.attribute("colorizeEdgeSize").isEmpty())
        mEdgeDetectionSize = element.attribute("colorizeEdgeSize").toDouble();
    if (!element.attribute("colorizeFuzzyRadius").isEmpty())
        mFuzzyRadius = element.attribute("colorizeFuzzyRadius").toDouble();
    if (!element.attribute("colorizeCleanUp").isEmpty())
        mCleanUpAmount = element.attribute("colorizeCleanUp").toDouble();
    if (element.hasAttribute("colorizeEditKeyStrokes"))
        mEditKeyStrokes = element.attribute("colorizeEditKeyStrokes").toInt() != 0;
    if (element.hasAttribute("colorizeShowColoring"))
        mShowColoring = element.attribute("colorizeShowColoring").toInt() != 0;
    if (element.hasAttribute("colorizeTransparentColor"))
    {
        mTransparentColor = static_cast<QRgb>(element.attribute("colorizeTransparentColor").toInt());
        mHasTransparentColor = true;
    }
    if (element.hasAttribute("colorizeIntentColors"))
    {
        for (const QString& hex : element.attribute("colorizeIntentColors").split(';'))
        {
            bool ok = false;
            const uint c = hex.toUInt(&ok, 16);
            if (ok && c != 0)
                mIntentColors.insert(static_cast<QRgb>(c));
        }
    }

    LayerBitmap::loadDomElement(element, dataDirPath, progressStep);
}

void LayerColorize::setTransparentColor(QRgb color)
{
    mTransparentColor = color;
    mHasTransparentColor = true;
}

void LayerColorize::removeStrokeColor(int frameNumber, QRgb color)
{
    ColorizeImage* frame = getLastColorizeImageAtFrame(frameNumber);
    if (frame == nullptr)
        return;
    frame->loadFile();

    QImage* image = frame->image();
    if (image == nullptr || image->isNull())
        return;

    // 删除判定与列表同源（以画布为准）：显示色（实心主色）被删时，
    // 清除该精确色全部像素（含同色软边低 alpha）+ 折叠进它的混合带组
    const QVector<QRgb> shown = strokeColorsAtFrame(frameNumber);
    if (!shown.contains(color))
        return; // 非列表显示色：无对应删除按钮

    QVector<Colorize::KeyStroke> groups;
    QHash<QRgb, QImage*> maskOf;
    {
        QSet<QRgb> allColors;
        for (int y = 0; y < image->height(); ++y)
        {
            const QRgb* line = reinterpret_cast<const QRgb*>(image->constScanLine(y));
            for (int x = 0; x < image->width(); ++x)
            {
                const QRgb px = line[x];
                const int a = qAlpha(px);
                if (a == 0) continue;
                allColors.insert(qRgb(qBound(0, qRound(qRed(px) * 255.0 / a), 255),
                                      qBound(0, qRound(qGreen(px) * 255.0 / a), 255),
                                      qBound(0, qRound(qBlue(px) * 255.0 / a), 255)));
            }
        }
        for (const QRgb c : allColors)
        {
            Colorize::KeyStroke g;
            g.color = c;
            g.mask = QImage(image->size(), QImage::Format_Grayscale8);
            g.mask.fill(0);
            groups.append(g);
        }
        for (auto& g : groups)
            maskOf[g.color] = &g.mask;
        for (int y = 0; y < image->height(); ++y)
        {
            const QRgb* line = reinterpret_cast<const QRgb*>(image->constScanLine(y));
            for (int x = 0; x < image->width(); ++x)
            {
                const QRgb px = line[x];
                const int a = qAlpha(px);
                if (a == 0) continue;
                const QRgb key = qRgb(qBound(0, qRound(qRed(px) * 255.0 / a), 255),
                                      qBound(0, qRound(qGreen(px) * 255.0 / a), 255),
                                      qBound(0, qRound(qBlue(px) * 255.0 / a), 255));
                maskOf.value(key, nullptr)->scanLine(y)[x] = static_cast<uchar>(a);
            }
        }
    }

    QSet<QRgb> familyColors;
    familyColors.insert(color);
    const QVector<int> master = Colorize::classifyStrokeMasters(
        groups, mTransparentColor, mHasTransparentColor, false);
    for (int g = 0; g < groups.size(); ++g)
    {
        if (master[g] == g)
            continue;
        // 混合带（贴 ≥2 组的非相似折叠组）随任意删除一并清——母色去其一
        // 即失去归属，且删除后重分类会使其只剩单邻而冒充主色
        if (!Colorize::similarColors(groups[g].color, groups[master[g]].color))
            familyColors.insert(groups[g].color);
    }

    bool changed = false;
    for (int y = 0; y < image->height(); ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(image->scanLine(y));
        for (int x = 0; x < image->width(); ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;
            const QRgb key = qRgb(qBound(0, qRound(qRed(px) * 255.0 / a), 255),
                                  qBound(0, qRound(qGreen(px) * 255.0 / a), 255),
                                  qBound(0, qRound(qBlue(px) * 255.0 / a), 255));
            if (familyColors.contains(key))
            {
                line[x] = 0;
                changed = true;
            }
        }
    }

    if (changed)
        frame->setModified(true);

    // 若删除的是透明颜色本身，一并取消标记
    if (mHasTransparentColor && color == mTransparentColor)
        mHasTransparentColor = false;
}

void LayerColorize::setPaletteColors(const QVector<QRgb>& colors)
{
    mPaletteColors = colors;
}

QVector<QRgb> LayerColorize::intentCandidateColors() const
{
    QVector<QRgb> list = mIntentColors.values().toVector();
    std::sort(list.begin(), list.end());
    list += mPaletteColors;
    return list;
}

void LayerColorize::addIntentColor(QRgb color)
{
    if (mIntentColors.contains(color))
        return;
    mIntentColors.insert(color);
    // 无需主动失效：登记发生在涂色（endStroke）时，笔画修改本就触发
    // 着色重算；面板「更新全部」重算时新代表色生效
}

QVector<QRgb> LayerColorize::intentColors() const
{
    QVector<QRgb> list = mIntentColors.values().toVector();
    std::sort(list.begin(), list.end());
    return list;
}

QVector<QRgb> LayerColorize::strokeColorsAtFrame(int frameNumber)
{
    QVector<QRgb> colors;
    ColorizeImage* frame = getLastColorizeImageAtFrame(frameNumber);
    if (frame == nullptr)
        return colors;
    frame->loadFile();

    QImage* image = frame->image();
    if (image == nullptr || image->isNull())
        return colors;

    // 以画布为准（Krita keyStrokesColors 语义——显示全部实心笔画色，
    // 不做色相似并）：实心 = 存在 3x3 同色核（手涂色点/传播标记必有，
    // 笔刷软边与抗锯齿的 1-2px 渐变环没有）；叠色混合带（贴 ≥2 组的
    // 中间色）折叠进母色不显示（与填色治理同源）。
    QSet<QRgb> solidColors;
    const int w = image->width(), hgt = image->height();
    for (int y = 1; y < hgt - 1; ++y)
    {
        const QRgb* above = reinterpret_cast<const QRgb*>(image->constScanLine(y - 1));
        const QRgb* line = reinterpret_cast<const QRgb*>(image->constScanLine(y));
        const QRgb* below = reinterpret_cast<const QRgb*>(image->constScanLine(y + 1));
        for (int x = 1; x < w - 1; ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;
            const int r = qBound(0, qRound(qRed(px) * 255.0 / a), 255);
            const int g = qBound(0, qRound(qGreen(px) * 255.0 / a), 255);
            const int b = qBound(0, qRound(qBlue(px) * 255.0 / a), 255);
            const QRgb key = qRgb(r, g, b);
            if (solidColors.contains(key))
                continue;
            bool core = true;
            for (int dy = -1; dy <= 1 && core; ++dy)
            {
                const QRgb* nl = dy < 0 ? above : (dy > 0 ? below : line);
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const QRgb npx = nl[x + dx];
                    const int na = qAlpha(npx);
                    if (na == 0) { core = false; break; }
                    const int nr = qBound(0, qRound(qRed(npx) * 255.0 / na), 255);
                    const int ng = qBound(0, qRound(qGreen(npx) * 255.0 / na), 255);
                    const int nb = qBound(0, qRound(qBlue(npx) * 255.0 / na), 255);
                    if (qRgb(nr, ng, nb) != key) { core = false; break; }
                }
            }
            if (core)
                solidColors.insert(key);
        }
    }
    if (solidColors.isEmpty())
        return colors;

    // 面积统计（只计实心色）→ 面积降序组装 KeyStroke 组 → 空间混合带折叠
    QHash<QRgb, qint64> areas;
    for (const QRgb key : solidColors)
        areas.insert(key, 0);
    for (int y = 0; y < hgt; ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image->constScanLine(y));
        for (int x = 0; x < w; ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;
            const QRgb key = qRgb(qBound(0, qRound(qRed(px) * 255.0 / a), 255),
                                  qBound(0, qRound(qGreen(px) * 255.0 / a), 255),
                                  qBound(0, qRound(qBlue(px) * 255.0 / a), 255));
            if (areas.contains(key))
                areas[key] += a;
        }
    }
    QVector<QPair<qint64, QRgb>> order;
    for (auto it = areas.begin(); it != areas.end(); ++it)
        order.append(qMakePair(it.value(), it.key()));
    std::sort(order.begin(), order.end(),
              [](const QPair<qint64, QRgb>& a, const QPair<qint64, QRgb>& b) { return a.first > b.first; });

    // 每实心色一张覆盖蒙版（值 = alpha），供空间邻接判定
    QVector<Colorize::KeyStroke> groups;
    for (const auto& item : order)
    {
        Colorize::KeyStroke stroke;
        stroke.color = item.second;
        stroke.mask = QImage(image->size(), QImage::Format_Grayscale8);
        stroke.mask.fill(0);
        groups.append(stroke);
    }
    QHash<QRgb, QImage*> maskOf;
    for (auto& g : groups)
        maskOf[g.color] = &g.mask;
    for (int y = 0; y < hgt; ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image->constScanLine(y));
        for (int x = 0; x < w; ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;
            const QRgb key = qRgb(qBound(0, qRound(qRed(px) * 255.0 / a), 255),
                                  qBound(0, qRound(qGreen(px) * 255.0 / a), 255),
                                  qBound(0, qRound(qBlue(px) * 255.0 / a), 255));
            QImage* m = maskOf.value(key, nullptr);
            if (m != nullptr)
                m->scanLine(y)[x] = static_cast<uchar>(a);
        }
    }

    const QVector<int> master = Colorize::classifyStrokeMasters(
        groups, mTransparentColor, mHasTransparentColor, false /* 以画布为准：不并色相变体 */);
    for (int g = 0; g < groups.size(); ++g)
        if (master[g] == g)
            colors.append(groups[g].color);
    return colors;
}

bool LayerColorize::updateColoringAtFrame(int frameNumber, LayerBitmap* sourceLayer, quint32 structureGeneration)
{
    ColorizeJobData data;
    if (!buildColorizeJob(this, frameNumber, sourceLayer, data))
    {
        // 无可计算内容：清空缓存视为完成（同时记当代数，避免逐帧重算）
        if (auto* frame = getLastColorizeImageAtFrame(frameNumber))
            frame->setColoringResult(QImage(), QRect(), structureGeneration);
        return true;
    }

    // 笔画按主色代表值（优先意图色=用户所选颜色代码）重涂后喂引擎：
    // 填色输出即用户所选色，画布脏像素不参与取色
    // 以画布为准：填色用像素色本身（重染保证新笔画=纯色代码），
    // normalize 只做族代表统一与混合带归并，不再意图/色板认领
    const QImage normalized = Colorize::normalizeStrokeColors(
        data.strokeImg, data.strokeImg.rect(), mTransparentColor, mHasTransparentColor, QVector<QRgb>());
    QImage result = Colorize::colorize(data.lineImg, normalized, data.lineImg.rect(), data.options);
    if (auto* frame = getColorizeImageAtFrame(data.keyPos))
        frame->setColoringResult(result, data.bounds, structureGeneration);
    return true;
}

bool LayerColorize::buildColorizeJob(LayerColorize* layer, int frameNumber,
                                     LayerBitmap* sourceLayer, ColorizeJobData& out)
{
    if (layer == nullptr) { return false; }

    ColorizeImage* frame = layer->getLastColorizeImageAtFrame(frameNumber);
    if (frame == nullptr) { return false; }
    frame->loadFile();

    BitmapImage* lineArt = nullptr;
    if (sourceLayer != nullptr)
    {
        lineArt = sourceLayer->getLastBitmapImageAtFrame(frameNumber);
        if (lineArt != nullptr)
            lineArt->loadFile();
    }

    // 计算域 = 线稿内容包围盒 ∪ 笔画内容包围盒（Krita 蒙版范围语义）
    QRect bounds;
    if (lineArt != nullptr)
        bounds |= lineArt->bounds();
    bounds |= frame->bounds();

    if (bounds.isEmpty() || frame->image()->isNull())
        return false;

    QImage lineImg(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    lineImg.fill(Qt::transparent);
    if (lineArt != nullptr)
    {
        QPainter painter(&lineImg);
        painter.drawImage(lineArt->topLeft() - bounds.topLeft(), *lineArt->image());
        painter.end();
    }

    QImage strokeImg(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    strokeImg.fill(Qt::transparent);
    {
        QPainter painter(&strokeImg);
        painter.drawImage(frame->topLeft() - bounds.topLeft(), *frame->image());
        painter.end();
    }

    out.layerId = layer->id();
    out.keyPos = frame->pos();
    out.bounds = bounds;
    out.lineImg = lineImg;
    out.strokeImg = strokeImg;

    out.options.useEdgeDetection = layer->mUseEdgeDetection;
    out.options.edgeDetectionSize = layer->mEdgeDetectionSize;
    out.options.fuzzyRadius = layer->mFuzzyRadius;
    out.options.cleanUpAmount = layer->mCleanUpAmount;
    out.options.hasTransparentColor = layer->mHasTransparentColor;
    out.options.transparentColor = layer->mTransparentColor;

    // 透明标记自愈：标记色已不在笔画中（被移除/误标）则本次忽略
    if (out.options.hasTransparentColor)
    {
        const QImage& strokesImage = strokeImg;
        bool found = false;
        for (int y = 0; y < strokesImage.height() && !found; ++y)
        {
            const QRgb* line = reinterpret_cast<const QRgb*>(strokesImage.constScanLine(y));
            for (int x = 0; x < strokesImage.width(); ++x)
            {
                const QRgb px = line[x];
                const int alpha = qAlpha(px);
                if (alpha == 0) continue;
                const int r = qBound(0, qRound(qRed(px) * 255.0 / alpha), 255);
                const int g = qBound(0, qRound(qGreen(px) * 255.0 / alpha), 255);
                const int b = qBound(0, qRound(qBlue(px) * 255.0 / alpha), 255);
                if (qRgb(r, g, b) == out.options.transparentColor) { found = true; break; }
            }
        }
        if (!found)
            out.options.hasTransparentColor = false;
    }

#ifdef COLORIZE_JOB_DEBUG_DUMP
    {
        const QString dir = QDir::temp().absoluteFilePath("pencil-colorize-tests");
        QDir().mkpath(dir);
        lineImg.save(dir + "/job_line.png");
        strokeImg.save(dir + "/job_stroke.png");
    }
#endif

    return true;
}
