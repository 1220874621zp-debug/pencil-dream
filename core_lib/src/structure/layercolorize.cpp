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
            const int r = qBound(0, qRound(qRed(px) * 255.0 / a), 255);
            const int g = qBound(0, qRound(qGreen(px) * 255.0 / a), 255);
            const int b = qBound(0, qRound(qBlue(px) * 255.0 / a), 255);
            if (qRgb(r, g, b) == color)
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

    QHash<QRgb, qint64> areas;
    for (int y = 0; y < image->height(); ++y)
    {
        const QRgb* line = reinterpret_cast<const QRgb*>(image->constScanLine(y));
        for (int x = 0; x < image->width(); ++x)
        {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0)
                continue;
            const int r = qBound(0, qRound(qRed(px) * 255.0 / a), 255);
            const int g = qBound(0, qRound(qGreen(px) * 255.0 / a), 255);
            const int b = qBound(0, qRound(qBlue(px) * 255.0 / a), 255);
            areas[qRgb(r, g, b)] += a;
        }
    }

    QVector<QPair<qint64, QRgb>> order;
    for (auto it = areas.begin(); it != areas.end(); ++it)
        order.append(qMakePair(it.value(), it.key()));
    std::sort(order.begin(), order.end(),
              [](const QPair<qint64, QRgb>& a, const QPair<qint64, QRgb>& b) { return a.first > b.first; });
    for (const auto& item : order)
        colors.append(item.second);
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

    QImage result = Colorize::colorize(data.lineImg, data.strokeImg, data.lineImg.rect(), data.options);
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
