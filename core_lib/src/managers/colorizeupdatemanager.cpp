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
#include "colorizeupdatemanager.h"

#include <QPointer>
#include <QThreadPool>

#include "editor.h"
#include "object.h"
#include "scribblearea.h"
#include "layercolorize.h"
#include "colorizeimage.h"
#include "graphics/bitmap/colorizeengine.h"

/** 工作线程任务：只碰自己的数据副本，结果队列回主线程 */
class ColorizeUpdateRunnable : public QRunnable
{
public:
    ColorizeUpdateRunnable(ColorizeUpdateManager* owner, ColorizeJobData data)
        : mOwner(owner), mData(std::move(data)) {}

    void run() override
    {
        QImage result = Colorize::colorize(mData.lineImg, mData.strokeImg,
                                           mData.lineImg.rect(), mData.options);

        QPointer<ColorizeUpdateManager> owner = mOwner;
        const int layerId = mData.layerId;
        const int keyPos = mData.keyPos;
        const QRect bounds = mData.bounds;

        // 归属管理器可能已销毁（退出）；QPointer 判活
        if (owner)
        {
            QMetaObject::invokeMethod(owner, "applyResult", Qt::QueuedConnection,
                                      Q_ARG(int, layerId), Q_ARG(int, keyPos),
                                      Q_ARG(QImage, result), Q_ARG(QRect, bounds));
        }
    }

private:
    ColorizeUpdateManager* mOwner = nullptr;
    ColorizeJobData mData;
};

ColorizeUpdateManager::ColorizeUpdateManager(Editor* editor)
    : BaseManager(editor, "ColorizeUpdateManager")
{
}

ColorizeUpdateManager::~ColorizeUpdateManager()
{
    mShutdown = true;
}

bool ColorizeUpdateManager::init()
{
    return true;
}

Status ColorizeUpdateManager::load(Object* object)
{
    Q_UNUSED(object)
    return Status::SAFE;
}

Status ColorizeUpdateManager::save(Object* object)
{
    Q_UNUSED(object)
    return Status::SAFE;
}

void ColorizeUpdateManager::requestVisibleUpdates()
{
    if (mShutdown) { return; }

    Object* object = this->object();
    const int frame = editor()->currentFrame();

    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        Layer* layer = object->getLayer(i);
        if (layer == nullptr || !layer->visible() || layer->type() != Layer::COLORIZE)
            continue;

        auto colorizeLayer = static_cast<LayerColorize*>(layer);
        ColorizeImage* frameImage = colorizeLayer->getLastColorizeImageAtFrame(frame);
        if (frameImage == nullptr)
            continue;

        if (frameImage->needsUpdate() ||
            frameImage->computedStructureGeneration() != object->layerStructureGeneration())
        {
            enqueueJob(colorizeLayer, frame);
        }
    }
}

void ColorizeUpdateManager::requestUpdate(LayerColorize* layer, int frameNumber)
{
    if (mShutdown || layer == nullptr) { return; }
    enqueueJob(layer, frameNumber);
}

void ColorizeUpdateManager::enqueueJob(LayerColorize* layer, int frameNumber)
{
    Object* object = this->object();
    const int index = object->getIndex(layer);
    if (index < 0) { return; }

    LayerBitmap* source = object->getBitmapLayerAbove(index);

    ColorizeJobData data;
    if (!LayerColorize::buildColorizeJob(layer, frameNumber, source, data))
        return;

    const QPair<int, int> record(data.layerId, data.keyPos);
    if (mInFlight.contains(record))
        return;
    mInFlight.insert(record);

    QThreadPool::globalInstance()->start(new ColorizeUpdateRunnable(this, data));
}

void ColorizeUpdateManager::applyResult(int layerId, int keyPos, const QImage& result, const QRect& bounds)
{
    if (mShutdown) { return; }
    if (!takeJobRecord(layerId, keyPos)) { return; }

    Object* object = this->object();
    Layer* layer = object->findLayerById(layerId);
    if (layer == nullptr || layer->type() != Layer::COLORIZE) { return; }

    auto colorizeLayer = static_cast<LayerColorize*>(layer);
    ColorizeImage* frameImage = colorizeLayer->getColorizeImageAtFrame(keyPos);
    if (frameImage == nullptr) { return; }

    frameImage->setColoringResult(result, bounds, object->layerStructureGeneration());
    Q_EMIT frameUpdated(layerId, keyPos);

    // 画布帧缓存整清后重绘（着色结果覆盖整块曝光，逐帧失效不可靠）
    if (editor()->getScribbleArea() != nullptr)
    {
        editor()->getScribbleArea()->invalidateCanvasCache();
    }
}

bool ColorizeUpdateManager::takeJobRecord(int layerId, int keyPos)
{
    return mInFlight.remove(QPair<int, int>(layerId, keyPos)) > 0;
}
