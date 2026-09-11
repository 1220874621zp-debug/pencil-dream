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

#include "layermanager.h"

#include <algorithm>

#include <QPainter>

#include "object.h"
#include "editor.h"
#include "scribblearea.h"
#include "layerlayoutcommand.h"
#include "undoredomanager.h"

#include "layersound.h"
#include "layervideo.h"
#include "layerbitmap.h"
#include "layercamera.h"

LayerManager::LayerManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

LayerManager::~LayerManager()
{
}

bool LayerManager::init()
{
    return true;
}

Status LayerManager::load(Object*)
{
    mLastCameraLayerIdx = 0;
    // Do not emit layerCountChanged here because the editor has not updated to this object yet
    // Leave that to the caller of this function
    return Status::OK;
}

Status LayerManager::save(Object* o)
{
    o->data()->setCurrentLayer(editor()->currentLayerIndex());
    return Status::OK;
}

LayerCamera* LayerManager::getCameraLayerBelow(int layerIndex) const
{
    return static_cast<LayerCamera*>(object()->getLayerBelow(layerIndex, Layer::CAMERA));
}

Layer* LayerManager::getLastCameraLayer()
{
    Layer* layer = object()->getLayer(mLastCameraLayerIdx);
    if (layer->type() == Layer::CAMERA)
    {
        return layer;
    }

    // it's not a camera layer
    std::vector<LayerCamera*> camLayers = object()->getLayersByType<LayerCamera>();
    if (camLayers.size() > 0)
    {
        return camLayers[0];
    }
    return nullptr;
}

Layer* LayerManager::currentLayer()
{
    Layer* layer = currentLayer(0);
    Q_ASSERT(layer != nullptr);
    return layer;
}

Layer* LayerManager::currentLayer(int incr)
{
    Q_ASSERT(object() != nullptr);
    return object()->getLayer(editor()->currentLayerIndex() + incr);
}

Layer* LayerManager::getLayer(int index)
{
    Q_ASSERT(object() != nullptr);
    return object()->getLayer(index);
}

Layer* LayerManager::findLayerByName(QString sName, Layer::LAYER_TYPE type)
{
    return object()->findLayerByName(sName, type);
}

Layer* LayerManager::findLayerById(int layerId) const
{
    return object()->findLayerById(layerId);
}

int LayerManager::currentLayerIndex()
{
    return editor()->currentLayerIndex();
}

void LayerManager::setCurrentLayer(int layerIndex)
{
    Q_ASSERT(layerIndex >= 0);
    Q_ASSERT(layerIndex < object()->getLayerCount());

    // Deselect frames of previous layer.
    Layer* previousLayer = currentLayer();
    if (previousLayer != object()->getLayer(layerIndex)) {
        previousLayer->deselectAll();
    }

    emit currentLayerWillChange(layerIndex);

    // Do not check if layer index has changed
    // because the current layer may have changed either way
    editor()->setCurrentLayerIndex(layerIndex);
    emit currentLayerChanged(layerIndex);

    if (object()->getLayer(layerIndex)->type() == Layer::CAMERA)
    {
        mLastCameraLayerIdx = layerIndex;
    }
}

void LayerManager::setCurrentLayer(Layer* layer)
{
    setCurrentLayer(getIndex(layer));
}

void LayerManager::gotoNextLayer()
{
    if (editor()->currentLayerIndex() < object()->getLayerCount() - 1)
    {
        currentLayer()->deselectAll();
        editor()->setCurrentLayerIndex(editor()->currentLayerIndex() + 1);
        emit currentLayerChanged(editor()->currentLayerIndex());
    }
}

void LayerManager::gotoPreviouslayer()
{
    if (editor()->currentLayerIndex() > 0)
    {
        currentLayer()->deselectAll();
        editor()->setCurrentLayerIndex(editor()->currentLayerIndex() - 1);
        emit currentLayerChanged(editor()->currentLayerIndex());
    }
}

QString LayerManager::nameSuggestLayer(const QString& name)
{
    // if no layers: return name
    if (count() == 0)
    {
        return name;
    }
    QVector<QString> sLayers;
    // fill Vector with layer names
    for (int i = 0; i < count(); i++)
    {
        sLayers.append(getLayer(i)->name());
    }
    // if name is not in list, return name
    if (!sLayers.contains(name))
    {
        return name;
    }
    int newIndex = 2;
    QString newName = name;
    do {
        newName = QStringLiteral("%1 %2")
            .arg(name).arg(QString::number(newIndex++));
    } while (sLayers.contains(newName));
    return newName;
}

Layer* LayerManager::createLayer(Layer::LAYER_TYPE type, const QString& strLayerName)
{
    Layer* layer = nullptr;
    switch (type) {
    case Layer::BITMAP:
        layer = object()->addNewBitmapLayer();
        break;
    case Layer::COLORIZE:
        layer = object()->addNewColorizeLayer();
        break;
    case Layer::SOUND:
        layer = object()->addNewSoundLayer();
        break;
    case Layer::MOVIE:
        layer = object()->addNewVideoLayer();
        break;
    case Layer::CAMERA:
        layer = object()->addNewCameraLayer();
        break;
    default:
        Q_ASSERT(true);
        return nullptr;
    }

    layer->setName(strLayerName);
    emit layerCountChanged(count());
    setCurrentLayer(getLastLayerIndex());

    return layer;
}

LayerBitmap* LayerManager::createBitmapLayer(const QString& strLayerName)
{
    LayerBitmap* layer = object()->addNewBitmapLayer();
    layer->setName(strLayerName);

    emit layerCountChanged(count());
    setCurrentLayer(getLastLayerIndex());

    return layer;
}

LayerBitmap* LayerManager::createColorizeLayer(const QString& strLayerName)
{
    LayerBitmap* layer = object()->addNewColorizeLayer();
    layer->setName(strLayerName);

    // 插到当前层正下方：若当前层是位图图层，新填色层立即获得线稿源
    const int currentIndex = currentLayerIndex();
    if (currentIndex >= 0 && currentIndex < count() - 1)
    {
        object()->moveLayer(count() - 1, currentIndex + 1);
    }

    emit layerCountChanged(count());
    setCurrentLayer(currentIndex + 1);

    return layer;
}

LayerCamera* LayerManager::createCameraLayer(const QString& strLayerName)
{
    LayerCamera* layer = object()->addNewCameraLayer();
    layer->setName(strLayerName);

    emit layerCountChanged(count());
    setCurrentLayer(getLastLayerIndex());

    return layer;
}

LayerVideo* LayerManager::createVideoLayer(const QString& strLayerName)
{
    LayerVideo* layer = object()->addNewVideoLayer();
    layer->setName(strLayerName);

    emit layerCountChanged(count());
    setCurrentLayer(getLastLayerIndex());

    return layer;
}

LayerSound* LayerManager::createSoundLayer(const QString& strLayerName)
{
    LayerSound* layer = object()->addNewSoundLayer();
    layer->setName(strLayerName);

    emit layerCountChanged(count());
    setCurrentLayer(getLastLayerIndex());

    return layer;
}

int LayerManager::lastFrameAtFrame(int frameIndex)
{
    Object* o = object();
    for (int i = frameIndex; i >= 0; i -= 1)
    {
        for (int layerIndex = 0; layerIndex < o->getLayerCount(); ++layerIndex)
        {
            auto pLayer = o->getLayer(layerIndex);
            if (pLayer->keyExists(i))
            {
                return i;
            }
        }
    }
    return -1;
}

int LayerManager::firstKeyFrameIndex()
{
    int minPosition = INT_MAX;

    Object* o = object();
    for (int i = 0; i < o->getLayerCount(); ++i)
    {
        Layer* pLayer = o->getLayer(i);

        int position = pLayer->firstKeyFramePosition();
        if (position < minPosition)
        {
            minPosition = position;
        }
    }
    return minPosition;
}

int LayerManager::lastKeyFrameIndex()
{
    int maxPosition = 0;

    for (int i = 0; i < object()->getLayerCount(); ++i)
    {
        Layer* pLayer = object()->getLayer(i);

        int position = pLayer->getMaxKeyFramePosition();
        if (position > maxPosition)
        {
            maxPosition = position;
        }
    }
    return maxPosition;
}

int LayerManager::count()
{
    return object()->getLayerCount();
}

bool LayerManager::canDeleteLayer(int index) const
{
    return object()->canDeleteLayer(index);
}

Status LayerManager::mergeBitmapLayerDown(int upperIndex)
{
    Object* obj = object();
    if (upperIndex <= 0 || upperIndex >= obj->getLayerCount())
    {
        return Status::FAIL;
    }
    Layer* upperLayer = obj->getLayer(upperIndex);
    Layer* lowerLayer = obj->getLayer(upperIndex - 1);
    if (upperLayer == nullptr || lowerLayer == nullptr
        || upperLayer->type() != Layer::BITMAP || lowerLayer->type() != Layer::BITMAP)
    {
        return Status::FAIL;
    }
    auto* upper = static_cast<LayerBitmap*>(upperLayer);
    auto* lower = static_cast<LayerBitmap*>(lowerLayer);

    // 上层曝光分段（key 起/止），升序；开放尾块按单格计
    struct Seg { int pos; int end; BitmapImage* img; };
    QList<Seg> segs;
    upper->foreachKeyFrame([&](KeyFrame* key)
    {
        int end = upper->getBlockEnd(key);
        if (end < 0) { end = key->pos() + 1; }
        segs.append({ key->pos(), end, static_cast<BitmapImage*>(key) });
    });
    std::sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b) { return a.pos < b.pos; });

    // 在上层分段边界处拆分下层：新 key 克隆该帧下层的显示内容（无显示=空白帧），
    // 使下层在两层的所有显示边界上都有显式 key，逐段内容恒定
    auto ensureLowerKeyAt = [lower](int frame) -> void
    {
        if (lower->keyExists(frame)) { return; }
        BitmapImage* displayed = lower->getLastBitmapImageAtFrame(frame);
        BitmapImage* fresh = nullptr;
        if (displayed != nullptr)
        {
            fresh = displayed->clone();
        }
        else
        {
            QImage blank(1, 1, QImage::Format_ARGB32_Premultiplied);
            blank.fill(Qt::transparent);
            fresh = new BitmapImage(QPoint(0, 0), blank);
        }
        lower->addKeyFrame(frame, fresh);
    };
    for (const Seg& seg : segs)
    {
        ensureLowerKeyAt(seg.pos);
        ensureLowerKeyAt(seg.end);
    }

    // 上层内容（含层不透明度）贴到下层对应 key
    const qreal upperOpacity = upper->opacity();
    for (const Seg& seg : segs)
    {
        BitmapImage* target = static_cast<BitmapImage*>(lower->getKeyFrameAt(seg.pos));
        if (target == nullptr) { continue; }
        if (upperOpacity >= 1.0)
        {
            target->paste(seg.img);
        }
        else
        {
            // 半透明层：以层不透明度重绘到透明底再贴，保持位置
            const QImage src = *seg.img->image();
            QImage overlay(src.size(), QImage::Format_ARGB32_Premultiplied);
            overlay.fill(Qt::transparent);
            QPainter overlayPainter(&overlay);
            overlayPainter.setOpacity(upperOpacity);
            overlayPainter.drawImage(0, 0, src);
            overlayPainter.end();
            BitmapImage temp(seg.img->bounds().topLeft(), overlay);
            target->paste(&temp);
        }
    }

    // 删除上层，选中合并后的下层
    const int lowerIndex = upperIndex - 1;
    const Status st = deleteLayer(upperIndex);
    if (!st.ok()) { return st; }
    setCurrentLayer(lowerIndex);
    return Status::OK;
}

Status LayerManager::deleteLayer(int index)
{
    Layer* layer = object()->getLayer(index);
    if (layer->type() == Layer::CAMERA)
    {
        std::vector<LayerCamera*> camLayers = object()->getLayersByType<LayerCamera>();
        if (camLayers.size() == 1)
            return Status::ERROR_NEED_AT_LEAST_ONE_CAMERA_LAYER;
    }
    Q_ASSERT(object()->getLayerCount() >= 2);

    // current layer is the last layer && we are deleting it
    if (index == object()->getLayerCount() - 1 &&
        index == currentLayerIndex())
    {
        setCurrentLayer(currentLayerIndex() - 1);
    }
    object()->deleteLayer(layer);
    object()->repairLayerGroupContiguity(); // 删成员后组可能空/断开
    if (index >= currentLayerIndex())
    {
        // current layer has changed, so trigger updates
        setCurrentLayer(currentLayerIndex());
    }

    emit layerDeleted(index);
    emit layerCountChanged(count());

    return Status::OK;
}

Status LayerManager::renameLayer(Layer* layer, const QString& newName)
{
    if (newName.isEmpty()) return Status::FAIL;

    layer->setName(newName);
    emit currentLayerChanged(getIndex(layer));
    return Status::OK;
}

void LayerManager::notifyLayerChanged(Layer* layer)
{
    emit currentLayerChanged(getIndex(layer));
}

/**
 * @brief Get the length of current project
 * @return int: the position of the last key frame in the timeline + its length
 */
int LayerManager::animationLength(bool includeSounds)
{
    int maxFrame = -1;

    Object* o = object();
    for (int i = 0; i < o->getLayerCount(); i++)
    {
        if (o->getLayer(i)->type() == Layer::SOUND)
        {
            if (!includeSounds)
                continue;

            Layer* soundLayer = o->getLayer(i);
            soundLayer->foreachKeyFrame([&maxFrame](KeyFrame* keyFrame)
            {
                int endPosition = keyFrame->pos() + (keyFrame->length() - 1);
                if (endPosition > maxFrame)
                {
                    maxFrame = endPosition;
                }
            });
        }
        else
        {
            int lastFramePos = o->getLayer(i)->getMaxKeyFramePosition();
            if (lastFramePos > maxFrame)
            {
                maxFrame = lastFramePos;
            }
        }
    }
    return maxFrame;
}

void LayerManager::notifyAnimationLengthChanged()
{
    emit animationLengthChanged(animationLength(true));
}

int LayerManager::getIndex(Layer* layer) const
{
    const Object* o = object();
    for (int i = 0; i < o->getLayerCount(); ++i)
    {
        if (layer == o->getLayer(i))
            return i;
    }
    return -1;
}

// ---- 图层多选 ----

bool LayerManager::isLayerSelected(const Layer* layer) const
{
    return layer != nullptr && mSelectedLayerIds.contains(layer->id());
}

void LayerManager::selectSingleLayer(int index)
{
    Layer* layer = object()->getLayer(index);
    if (layer == nullptr) { return; }
    mSelectedLayerIds = QList<int>{ layer->id() };
    mSelectionAnchorId = layer->id();
    emit layerSelectionChanged();
}

void LayerManager::selectLayerRange(int clickedIndex)
{
    Layer* clicked = object()->getLayer(clickedIndex);
    if (clicked == nullptr) { return; }
    if (mSelectionAnchorId < 0 || mSelectedLayerIds.isEmpty())
    {
        mSelectionAnchorId = clicked->id();
    }
    Layer* anchor = findLayerById(mSelectionAnchorId);
    if (anchor == nullptr)
    {
        mSelectionAnchorId = clicked->id();
        selectSingleLayer(clickedIndex);
        return;
    }
    const int a = getIndex(anchor);
    const int b = clickedIndex;
    mSelectedLayerIds.clear();
    for (int i = qMin(a, b); i <= qMax(a, b); ++i)
    {
        mSelectedLayerIds.append(object()->getLayer(i)->id());
    }
    emit layerSelectionChanged();
}

int LayerManager::groupSelectedLayers()
{
    if (mSelectedLayerIds.size() < 2) { return -1; }

    // 栈序收集选中层（保持相对顺序），并要求全部可入组
    QList<Layer*> picked;
    for (int i = 0; i < object()->getLayerCount(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (mSelectedLayerIds.contains(layer->id()))
        {
            if (!layer->isGroupable()) { return -1; }
            picked.append(layer);
        }
    }
    if (picked.size() < 2) { return -1; }

    const auto before = LayerOrderCommand::captureGroups(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    const int gid = object()->createLayerGroup(tr("组 %1").arg(object()->layerGroups().size() + 1));

    // 非连续：整批收拢到首个（最上）选中层的位置，保持相对顺序
    const int targetIdx = object()->getIndex(picked.first());
    int insertAt = targetIdx;
    for (Layer* layer : picked)
    {
        const int cur = object()->getIndex(layer);
        if (cur < insertAt)
        {
            object()->moveLayer(cur, insertAt); // 后移插入，目标自增补偿
        }
        else
        {
            object()->moveLayer(cur, insertAt);
        }
        layer->setGroupId(gid);
        ++insertAt;
    }
    object()->repairLayerGroupContiguity();

    editor()->undoRedo()->pushUndoCommand(new LayerOrderCommand(
        editor(), orderBefore, object()->layerIdOrder(), tr("选中图层成组"),
        nullptr, before, LayerOrderCommand::captureGroups(object())));

    // 成组后收敛选择为该组
    mSelectedLayerIds.clear();
    for (Layer* layer : picked) { mSelectedLayerIds.append(layer->id()); }
    emit layerSelectionChanged();
    emit editor()->updateTimeLine();
    editor()->getScribbleArea()->onLayerChanged();
    return gid;
}

// ---- 图层分组操作 ----

namespace
{
LayerOrderCommand::GroupSnapshot captureGroupSnapshot(Object* obj)
{
    LayerOrderCommand::GroupSnapshot snap;
    snap.valid = true;
    snap.groups = obj->layerGroups();
    snap.nextGroupId = obj->nextLayerGroupId();
    for (int i = 0; i < obj->getLayerCount(); ++i)
    {
        snap.layerGroupId.insert(obj->getLayer(i)->id(), obj->getLayer(i)->groupId());
    }
    return snap;
}
}

/** 组操作公共收尾：修复连续性→推一条合并撤销→通知 UI 刷新 */
static void finishGroupOp(LayerManager* /*self*/, Editor* editor,
                          const QList<int>& orderBefore,
                          const LayerOrderCommand::GroupSnapshot& before,
                          const QString& desc)
{
    editor->object()->repairLayerGroupContiguity();
    editor->undoRedo()->pushUndoCommand(new LayerOrderCommand(
        editor, orderBefore, editor->object()->layerIdOrder(), desc,
        nullptr, before, captureGroupSnapshot(editor->object())));
    emit editor->updateTimeLine();
    editor->getScribbleArea()->onLayerChanged();
}

int LayerManager::createGroupWithLayer(int layerIndex)
{
    Layer* layer = object()->getLayer(layerIndex);
    if (layer == nullptr || !layer->isGroupable() || layer->groupId() >= 0)
    {
        return -1;
    }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    const int gid = object()->createLayerGroup(tr("组 %1").arg(object()->layerGroups().size() + 1));
    layer->setGroupId(gid);

    finishGroupOp(this, editor(), orderBefore, before, tr("图层成组"));
    return gid;
}

bool LayerManager::addLayerToGroup(int layerIndex, int groupId)
{
    Layer* layer = object()->getLayer(layerIndex);
    if (layer == nullptr || !layer->isGroupable()) { return false; }
    if (object()->layerGroupInfo(groupId) == nullptr) { return false; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    layer->setGroupId(groupId);

    finishGroupOp(this, editor(), orderBefore, before, tr("加入图层组"));
    return true;
}

bool LayerManager::removeLayerFromGroup(int layerIndex)
{
    Layer* layer = object()->getLayer(layerIndex);
    if (layer == nullptr || layer->groupId() < 0) { return false; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    layer->setGroupId(-1);

    finishGroupOp(this, editor(), orderBefore, before, tr("移出图层组"));
    return true;
}

bool LayerManager::dissolveGroup(int groupId)
{
    if (object()->layerGroupInfo(groupId) == nullptr) { return false; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    const QList<int> members = object()->layerGroupMemberIndices(groupId);
    for (int index : members)
    {
        object()->getLayer(index)->setGroupId(-1);
    }
    object()->removeLayerGroupEntry(groupId);

    finishGroupOp(this, editor(), orderBefore, before, tr("解散图层组"));
    return true;
}

void LayerManager::renameGroup(int groupId, const QString& name)
{
    if (name.isEmpty()) { return; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    object()->renameLayerGroup(groupId, name);

    finishGroupOp(this, editor(), orderBefore, before, tr("重命名图层组"));
}

void LayerManager::toggleGroupCollapsed(int groupId)
{
    const LayerGroupInfo* info = object()->layerGroupInfo(groupId);
    if (info == nullptr) { return; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    object()->setLayerGroupCollapsed(groupId, !info->collapsed);

    // 折叠时当前层若在组内：默认切到组内最上层（展开后红框落在最上层）
    if (!info->collapsed) // 即将变为收起
    {
        Layer* cur = currentLayer();
        if (cur != nullptr && cur->groupId() == groupId)
        {
            const QList<int> members = object()->layerGroupMemberIndices(groupId);
            if (!members.isEmpty())
            {
                setCurrentLayer(members.last());
            }
        }
    }

    finishGroupOp(this, editor(), orderBefore, before, tr("展开/收起图层组"));
}

void LayerManager::setGroupVisible(int groupId, bool visible)
{
    if (object()->layerGroupInfo(groupId) == nullptr) { return; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    object()->setLayerGroupVisible(groupId, visible);

    finishGroupOp(this, editor(), orderBefore, before, tr("图层组可见性"));
}

void LayerManager::setGroupLocked(int groupId, bool locked)
{
    if (object()->layerGroupInfo(groupId) == nullptr) { return; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    object()->setLayerGroupLocked(groupId, locked);

    finishGroupOp(this, editor(), orderBefore, before, tr("图层组锁定"));
}

bool LayerManager::moveLayerGroup(int groupId, int toIndex)
{
    const QList<int> members = object()->layerGroupMemberIndices(groupId);
    if (members.isEmpty()) { return false; }

    const auto before = captureGroupSnapshot(object());
    const QList<int> orderBefore = object()->layerIdOrder();

    const bool ok = object()->moveLayerRange(members.first(), members.size(), toIndex);

    finishGroupOp(this, editor(), orderBefore, before, tr("移动图层组"));
    return ok;
}
