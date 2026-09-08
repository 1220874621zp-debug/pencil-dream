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
    return static_cast<ColorizeImage*>(getLastKeyFrameAtPosition(frameNumber));
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

    LayerBitmap::loadDomElement(element, dataDirPath, progressStep);
}

bool LayerColorize::updateColoringAtFrame(int frameNumber, LayerBitmap* sourceLayer)
{
    ColorizeJobData data;
    if (!buildColorizeJob(this, frameNumber, sourceLayer, data))
    {
        // 无可计算内容：清空缓存视为完成
        if (auto* frame = getLastColorizeImageAtFrame(frameNumber))
            frame->setColoringResult(QImage(), QRect());
        return true;
    }

    QImage result = Colorize::colorize(data.lineImg, data.strokeImg, data.lineImg.rect(), data.options);
    if (auto* frame = getColorizeImageAtFrame(data.keyPos))
        frame->setColoringResult(result, data.bounds);
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

    return true;
}
