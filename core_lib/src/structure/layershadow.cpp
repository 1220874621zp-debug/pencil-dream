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
#include "layershadow.h"

#include <QPainter>
#include <QDir>

#include "shadowimage.h"
#include "graphics/bitmap/shadowengine.h"

LayerShadow::LayerShadow(int id)
    : LayerBitmap(id, Layer::SHADOW)
{
    setName(tr("Shadow Layer"));
}

LayerShadow::~LayerShadow()
{
}

ShadowImage* LayerShadow::getShadowImageAtFrame(int frameNumber)
{
    Q_ASSERT(frameNumber >= 1);
    return static_cast<ShadowImage*>(getKeyFrameAt(frameNumber));
}

ShadowImage* LayerShadow::getLastShadowImageAtFrame(int frameNumber)
{
    Q_ASSERT(frameNumber >= 1);
    return static_cast<ShadowImage*>(getLastKeyFrameAtPosition(displayFrameFor(frameNumber)));
}

void LayerShadow::replaceKeyFrame(const KeyFrame* keyframe)
{
    auto shadowFrame = getShadowImageAtFrame(keyframe->pos());
    *static_cast<BitmapImage*>(shadowFrame) = *static_cast<const BitmapImage*>(keyframe);
    shadowFrame->setNeedsUpdate(true);
}

KeyFrame* LayerShadow::createKeyFrame(int position)
{
    auto frame = new ShadowImage;
    frame->setPos(position);
    frame->enableAutoCrop(true);
    return frame;
}

void LayerShadow::loadImageAtFrame(QString strFilePath, QPoint topLeft, int frameNumber, qreal opacity)
{
    auto pKeyFrame = new ShadowImage;
    *static_cast<BitmapImage*>(pKeyFrame) = BitmapImage(topLeft, strFilePath);
    pKeyFrame->enableAutoCrop(true);
    pKeyFrame->setPos(frameNumber);
    pKeyFrame->setOpacity(opacity);
    loadKey(pKeyFrame);
}

QRgb LayerShadow::markerColor(int slot) const
{
    if (slot < 0 || slot >= kMarkerSlotCount)
        return 0;
    return mMarkerColors[slot];
}

void LayerShadow::setMarkerColor(int slot, QRgb color)
{
    if (slot < 0 || slot >= kMarkerSlotCount)
        return;
    mMarkerColors[slot] = color;
}

void LayerShadow::setMarkerTransparent(int slot, bool transparent)
{
    if (slot < 0 || slot >= kMarkerSlotCount)
        return;
    mMarkerTransparent[slot] = transparent;
}

QVector<QRgb> LayerShadow::disabledMarkerColors() const
{
    QVector<QRgb> colors;
    for (int i = 0; i < kMarkerSlotCount; ++i)
    {
        if (mMarkerTransparent[i])
            colors.append(mMarkerColors[i]);
    }
    return colors;
}

QDomElement LayerShadow::createDomElement(QDomDocument& doc) const
{
    QDomElement layerElem = LayerBitmap::createDomElement(doc);

    if (mDirection != ShadowFill::DirLeft)
        layerElem.setAttribute("shadowDirection", static_cast<int>(mDirection));
    if (mFillColor != qRgb(0, 0, 0))
        layerElem.setAttribute("shadowFillColor", static_cast<int>(mFillColor));
    if (mGapRadius != 4.0)
        layerElem.setAttribute("shadowGapRadius", QString::number(mGapRadius, 'f', 1));
    {
        QStringList hex;
        for (int i = 0; i < kMarkerSlotCount; ++i)
            hex << QString::number(static_cast<uint>(mMarkerColors[i]), 16);
        layerElem.setAttribute("shadowMarkerColors", hex.join(";"));
        QStringList trans;
        for (int i = 0; i < kMarkerSlotCount; ++i)
            trans << (mMarkerTransparent[i] ? "1" : "0");
        if (mMarkerTransparent[0] || mMarkerTransparent[1] || mMarkerTransparent[2])
            layerElem.setAttribute("shadowMarkerTransparent", trans.join(";"));
    }
    if (!mEditLines)
        layerElem.setAttribute("shadowEditLines", 0);
    if (!mShowFill)
        layerElem.setAttribute("shadowShowFill", 0);

    return layerElem;
}

void LayerShadow::loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep)
{
    if (element.hasAttribute("shadowDirection"))
        mDirection = static_cast<ShadowFill::Direction>(element.attribute("shadowDirection").toInt());
    if (element.hasAttribute("shadowFillColor"))
        mFillColor = static_cast<QRgb>(element.attribute("shadowFillColor").toInt());
    if (!element.attribute("shadowGapRadius").isEmpty())
        mGapRadius = element.attribute("shadowGapRadius").toDouble();
    if (element.hasAttribute("shadowMarkerColors"))
    {
        const QStringList hex = element.attribute("shadowMarkerColors").split(';');
        for (int i = 0; i < kMarkerSlotCount && i < hex.size(); ++i)
        {
            bool ok = false;
            const uint c = hex.at(i).toUInt(&ok, 16);
            if (ok && c != 0)
                mMarkerColors[i] = static_cast<QRgb>(c);
        }
    }
    if (element.hasAttribute("shadowMarkerTransparent"))
    {
        const QStringList flags = element.attribute("shadowMarkerTransparent").split(';');
        for (int i = 0; i < kMarkerSlotCount && i < flags.size(); ++i)
            mMarkerTransparent[i] = flags.at(i).toInt() != 0;
    }
    if (element.hasAttribute("shadowEditLines"))
        mEditLines = element.attribute("shadowEditLines").toInt() != 0;
    if (element.hasAttribute("shadowShowFill"))
        mShowFill = element.attribute("shadowShowFill").toInt() != 0;

    LayerBitmap::loadDomElement(element, dataDirPath, progressStep);
}

bool LayerShadow::updateShadowAtFrame(int frameNumber, LayerBitmap* sourceLayer, quint32 structureGeneration)
{
    ShadowImage* frame = getLastShadowImageAtFrame(frameNumber);
    if (frame == nullptr)
        return false;
    frame->loadFile();

    BitmapImage* lineArt = nullptr;
    if (sourceLayer != nullptr)
    {
        lineArt = sourceLayer->getLastBitmapImageAtFrame(frameNumber);
        if (lineArt != nullptr)
            lineArt->loadFile();
    }

    // 计算域 = 线稿内容包围盒 ∪ 分割线内容包围盒（同填色层）
    QRect bounds;
    if (lineArt != nullptr)
        bounds |= lineArt->bounds();
    bounds |= frame->bounds();

    QImage* strokeImage = frame->image();
    if (bounds.isEmpty() || strokeImage == nullptr || strokeImage->isNull())
    {
        // 无可计算内容：记空结果视为完成（记当代数，避免逐帧重算不停）
        frame->setShadowResult(QImage(), QRect(), structureGeneration);
        frame->setWarnings(QVector<ShadowFill::Warning>());
        return true;
    }

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
        painter.drawImage(frame->topLeft() - bounds.topLeft(), *strokeImage);
        painter.end();
    }

    ShadowFill::Params params;
    params.direction = mDirection;
    params.gapRadius = mGapRadius;
    params.fillColor = mFillColor;

    ShadowFill::Result result = ShadowFill::computeShadow(lineImg, strokeImg, bounds, params,
                                                          disabledMarkerColors());
    frame->setShadowResult(result.fill, bounds, structureGeneration);
    frame->setWarnings(result.warnings);
    return true;
}
