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
#include "object.h"

#include <QDomDocument>
#include <QTextStream>
#include <QProgressDialog>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QImageWriter>
#include <QRegularExpression>
#include <QHash>
#include <QSet>

#include "layer.h"
#include "layerbitmap.h"
#include "layercolorize.h"
#include "colorizeimage.h"
#include "layersound.h"
#include "layercamera.h"

#include "util.h"
#include "bitmapimage.h"
#include "fileformat.h"
#include "activeframepool.h"


Object::Object()
{
    mActiveFramePool.reset(new ActiveFramePool);
}

Object::~Object()
{
    mActiveFramePool->clear();

    for (Layer* layer : mLayers)
        delete layer;
    mLayers.clear();

    deleteWorkingDir();
}

void Object::init()
{
    createWorkingDir();

    // default palette
    loadDefaultPalette();
}

QDomElement Object::saveXML(QDomDocument& doc) const
{
    QDomElement objectTag = doc.createElement("object");

    for (Layer* layer : mLayers)
    {
        QDomElement layerTag = layer->createDomElement(doc);
        objectTag.appendChild(layerTag);
    }

    if (!mLayerGroups.isEmpty())
    {
        QDomElement groupsTag = doc.createElement("layerGroups");
        for (const LayerGroupInfo& info : mLayerGroups)
        {
            QDomElement g = doc.createElement("group");
            g.setAttribute("id", info.id);
            g.setAttribute("name", info.name);
            g.setAttribute("collapsed", info.collapsed ? 1 : 0);
            g.setAttribute("visible", info.visible ? 1 : 0);
            g.setAttribute("locked", info.locked ? 1 : 0);
            groupsTag.appendChild(g);
        }
        objectTag.appendChild(groupsTag);
    }
    return objectTag;
}

bool Object::loadXML(const QDomElement& docElem, ProgressCallback progressForward)
{
    if (docElem.isNull())
    {
        return false;
    }

    const QString dataDirPath = mDataDirPath;

    for (QDomNode node = docElem.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement(); // try to convert the node to an element.
        if (element.tagName() != "layer")
        {
            continue;
        }

        Layer* newLayer;
        switch (element.attribute("type").toInt())
        {
        case Layer::BITMAP:
            newLayer = new LayerBitmap(getUniqueLayerID());
            break;
        case Layer::VECTOR:
            qWarning() << "Vector layers are not supported in this build, skipping layer:" << element.tagName();
            continue;
        case Layer::SOUND:
            newLayer = new LayerSound(getUniqueLayerID());
            break;
        case Layer::CAMERA:
            newLayer = new LayerCamera(getUniqueLayerID());
            break;
        case Layer::COLORIZE:
            newLayer = new LayerColorize(getUniqueLayerID());
            break;
        default:
            Q_UNREACHABLE();
        }
        mLayers.append(newLayer);
        newLayer->loadDomElement(element, dataDirPath, progressForward);
    }

    for (QDomNode node = docElem.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement();
        if (element.tagName() != "layerGroups")
        {
            continue;
        }
        for (QDomNode g = element.firstChild(); !g.isNull(); g = g.nextSibling())
        {
            QDomElement ge = g.toElement();
            if (ge.tagName() != "group") { continue; }
            LayerGroupInfo info;
            info.id = ge.attribute("id").toInt();
            info.name = ge.attribute("name", tr("组"));
            info.collapsed = ge.attribute("collapsed", "0").toInt() == 1;
            info.visible = ge.attribute("visible", "1").toInt() == 1;
            info.locked = ge.attribute("locked", "0").toInt() == 1;
            if (info.id > 0)
            {
                mLayerGroups.append(info);
                mNextLayerGroupId = qMax(mNextLayerGroupId, info.id + 1);
            }
        }
    }
    // 防御：手工改坏的工程文件组信息不连续时自动修复
    repairLayerGroupContiguity();
    return true;
}

LayerBitmap* Object::addNewBitmapLayer()
{
    LayerBitmap* layerBitmap = new LayerBitmap(getUniqueLayerID());
    mLayers.append(layerBitmap);

    layerBitmap->addNewKeyFrameAt(1);

    ++mLayerStructureGeneration;

    return layerBitmap;
}

LayerBitmap* Object::addNewColorizeLayer()
{
    auto layerColorize = new LayerColorize(getUniqueLayerID());
    mLayers.append(layerColorize);

    layerColorize->addNewKeyFrameAt(1);

    ++mLayerStructureGeneration;

    return layerColorize;
}

LayerBitmap* Object::getBitmapLayerAbove(int i) const
{
    // 图层索引 0 = 栈顶；向上 = 索引递减
    for (int index = i - 1; index >= 0; --index)
    {
        Layer* layer = mLayers.at(index);
        if (layer->type() == Layer::BITMAP)
            return static_cast<LayerBitmap*>(layer);
    }
    return nullptr;
}

LayerBitmap* Object::getColorizeSourceLayer(int colorizeIndex, int frameNumber) const
{
    auto layerUsable = [frameNumber](Layer* layer) -> bool {
        if (layer->type() != Layer::BITMAP)
            return false;
        auto bitmapLayer = static_cast<LayerBitmap*>(layer);
        BitmapImage* frame = bitmapLayer->getLastBitmapImageAtFrame(frameNumber);
        return frame != nullptr && !frame->bounds().isEmpty();
    };

    // 先向上（栈顶方向），再向下
    for (int index = colorizeIndex - 1; index >= 0; --index)
    {
        if (layerUsable(mLayers.at(index)))
            return static_cast<LayerBitmap*>(mLayers.at(index));
    }
    for (int index = colorizeIndex + 1; index < mLayers.size(); ++index)
    {
        if (layerUsable(mLayers.at(index)))
            return static_cast<LayerBitmap*>(mLayers.at(index));
    }
    return nullptr;
}

LayerSound* Object::addNewSoundLayer()
{
    LayerSound* layerSound = new LayerSound(getUniqueLayerID());
    mLayers.append(layerSound);

    ++mLayerStructureGeneration;

    // No default keyFrame at position 1 for Sound layer.

    return layerSound;
}

LayerCamera* Object::addNewCameraLayer()
{
    LayerCamera* layerCamera = new LayerCamera(getUniqueLayerID());
    mLayers.append(layerCamera);

    layerCamera->addNewKeyFrameAt(1);

    ++mLayerStructureGeneration;

    return layerCamera;
}

void Object::createWorkingDir()
{
    QString projectName;
    if (mFilePath.isEmpty())
    {
        projectName = "Default";
    }
    else
    {
        QFileInfo fileInfo(mFilePath);
        projectName = fileInfo.completeBaseName();
    }
    QDir dir(QDir::tempPath());

    QString strWorkingDir;
    do
    {
        strWorkingDir = QString("%1/Pencil2D/%2_%3_%4/").arg(QDir::tempPath(),
                                                             projectName,
                                                             PFF_TMP_DECOMPRESS_EXT,
                                                             uniqueString(8));
    }
    while(dir.exists(strWorkingDir));

    dir.mkpath(strWorkingDir);
    mWorkingDirPath = strWorkingDir;

    QDir dataDir(strWorkingDir + PFF_DATA_DIR);
    dataDir.mkpath(".");

    mDataDirPath = dataDir.absolutePath();
}

void Object::deleteWorkingDir() const
{
    if (!mWorkingDirPath.isEmpty())
    {
        QDir dir(mWorkingDirPath);
        bool ok = dir.removeRecursively();
        Q_ASSERT(ok);
    }
}

void Object::setWorkingDir(const QString& path)
{
    QDir dir(path);
    Q_ASSERT(dir.exists());
    mWorkingDirPath = path;
}

int Object::getMaxLayerID()
{
    int maxId = 0;
    for (Layer* iLayer : mLayers)
    {
        if (iLayer->id() > maxId)
        {
            maxId = iLayer->id();
        }
    }
    return maxId;
}

int Object::getUniqueLayerID()
{
    return 1 + getMaxLayerID();
}

Layer* Object::getLayer(int i) const
{
    if (i < 0 || i >= getLayerCount())
    {
        return nullptr;
    }

    return mLayers.at(i);
}

Layer* Object::getLayerBelow(int i, Layer::LAYER_TYPE type) const
{
    for (; i >= 0; --i)
    {
        Layer* layerCheck = getLayer(i);
        Q_ASSERT(layerCheck);
        if (layerCheck->type() == type)
        {
            return layerCheck;
        }
    }

    return nullptr;
}

Layer* Object::findLayerById(int layerId) const
{
    for(Layer* layer : mLayers)
    {
        if (layer->id() == layerId)
        {
            return layer;
        }
    }
    return nullptr;
}

Layer* Object::findLayerByName(const QString& strName, Layer::LAYER_TYPE type) const
{
    bool bCheckType = (type != Layer::UNDEFINED);
    for (Layer* layer : mLayers)
    {
        bool isTypeMatch = (bCheckType) ? (type == layer->type()) : true;
        if (isTypeMatch && layer->name() == strName)
        {
            return layer;
        }
    }
    return nullptr;
}

int Object::getIndex(Layer* layer) const
{
    return mLayers.indexOf(layer);
}

void Object::invalidateColorizeBelow(int layerIndex, int frameNumber)
{
    // 从被编辑的位图层向下走：遇到下一个位图层即止（它挡住了线稿源关系），
    // 途经的填色层都以被编辑层为线稿源
    for (int i = layerIndex + 1; i < mLayers.size(); ++i)
    {
        Layer* layer = mLayers.at(i);
        if (layer->type() == Layer::BITMAP)
            break;
        if (layer->type() == Layer::COLORIZE)
        {
            auto colorizeLayer = static_cast<LayerColorize*>(layer);
            if (frameNumber < 0)
            {
                colorizeLayer->foreachKeyFrame([](KeyFrame* key)
                {
                    if (auto* frame = static_cast<ColorizeImage*>(key))
                        frame->setNeedsUpdate(true);
                });
            }
            else
            {
                if (auto* frame = colorizeLayer->getLastColorizeImageAtFrame(frameNumber))
                    frame->setNeedsUpdate(true);
            }
        }
    }
}

Layer* Object::takeLayer(int layerId)
{
    // Removes the layer from this Object and returns it
    // The ownership of this layer has been transfer to the caller
    int index = -1;
    for (int i = 0; i< mLayers.length(); ++i)
    {
        Layer* layer = mLayers[i];
        if (layer->id() == layerId)
        {
            index = i;
            break;
        }
    }

    if (index == -1) { return nullptr; }

    Layer* layer = mLayers.takeAt(index);
    ++mLayerStructureGeneration;
    return layer;
}

bool Object::swapLayers(int i, int j)
{
    bool canSwap = canSwapLayers(i, j);
    if (!canSwap) { return false; }

    if (i != j)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
        mLayers.swapItemsAt(i, j);
#else
        mLayers.swap(i, j);
#endif
    }
    ++mLayerStructureGeneration;
    return true;
}

bool Object::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= mLayers.size()) { return false; }
    if (toIndex < 0 || toIndex >= mLayers.size()) { return false; }
    if (fromIndex == toIndex) { return true; }

    Layer* layer = mLayers.takeAt(fromIndex);
    mLayers.insert(toIndex, layer);
    ++mLayerStructureGeneration;
    return true;
}

QList<int> Object::layerIdOrder() const
{
    QList<int> order;
    for (Layer* layer : mLayers)
    {
        order.append(layer->id());
    }
    return order;
}

// ---- 图层分组 ----

int Object::createLayerGroup(const QString& name)
{
    LayerGroupInfo info;
    info.id = mNextLayerGroupId++;
    info.name = name;
    mLayerGroups.append(info);
    ++mLayerGroupGeneration;
    return info.id;
}

void Object::renameLayerGroup(int groupId, const QString& name)
{
    if (LayerGroupInfo* info = layerGroupInfo(groupId))
    {
        info->name = name;
        ++mLayerGroupGeneration;
    }
}

LayerGroupInfo* Object::layerGroupInfo(int groupId)
{
    for (LayerGroupInfo& info : mLayerGroups)
    {
        if (info.id == groupId) { return &info; }
    }
    return nullptr;
}

const LayerGroupInfo* Object::layerGroupInfo(int groupId) const
{
    for (const LayerGroupInfo& info : mLayerGroups)
    {
        if (info.id == groupId) { return &info; }
    }
    return nullptr;
}

void Object::removeLayerGroupEntry(int groupId)
{
    for (int i = 0; i < mLayerGroups.size(); ++i)
    {
        if (mLayerGroups[i].id == groupId)
        {
            mLayerGroups.removeAt(i);
            ++mLayerGroupGeneration;
            return;
        }
    }
}

void Object::setLayerGroupCollapsed(int groupId, bool collapsed)
{
    if (LayerGroupInfo* info = layerGroupInfo(groupId))
    {
        info->collapsed = collapsed;
        ++mLayerGroupGeneration;
    }
}

void Object::setLayerGroupVisible(int groupId, bool visible)
{
    if (LayerGroupInfo* info = layerGroupInfo(groupId))
    {
        info->visible = visible;
        ++mLayerGroupGeneration;
    }
}

void Object::setLayerGroupLocked(int groupId, bool locked)
{
    if (LayerGroupInfo* info = layerGroupInfo(groupId))
    {
        info->locked = locked;
        ++mLayerGroupGeneration;
    }
}

bool Object::isLayerGroupVisible(int groupId) const
{
    const LayerGroupInfo* info = layerGroupInfo(groupId);
    return (info == nullptr) ? true : info->visible;
}

bool Object::isLayerGroupLocked(int groupId) const
{
    const LayerGroupInfo* info = layerGroupInfo(groupId);
    return (info == nullptr) ? false : info->locked;
}

bool Object::isLayerRenderable(const Layer* layer) const
{
    if (layer == nullptr) { return false; }
    return layer->visible() && isLayerGroupVisible(layer->groupId());
}

bool Object::isLayerEditable(const Layer* layer) const
{
    if (layer == nullptr) { return false; }
    return !layer->locked() && !isLayerGroupLocked(layer->groupId());
}

QList<int> Object::layerGroupMemberIndices(int groupId) const
{
    QList<int> indices;
    int start = -1;
    for (int i = 0; i < mLayers.size(); ++i)
    {
        if (mLayers[i]->groupId() == groupId)
        {
            start = i;
            break;
        }
    }
    if (start < 0) { return indices; }
    for (int i = start; i < mLayers.size() && mLayers[i]->groupId() == groupId; ++i)
    {
        indices.append(i);
    }
    return indices;
}

void Object::repairLayerGroupContiguity()
{
    bool changed = false;
    // 记录每个组首个（最上）成员索引，之后出现的同组成员若与主体断开则剔出
    QHash<int, int> firstMember; // groupId -> index of first member
    for (int i = 0; i < mLayers.size(); ++i)
    {
        const int gid = mLayers[i]->groupId();
        if (gid < 0) { continue; }
        if (!firstMember.contains(gid))
        {
            firstMember.insert(gid, i);
        }
    }
    for (int i = 0; i < mLayers.size(); ++i)
    {
        const int gid = mLayers[i]->groupId();
        if (gid < 0) { continue; }
        // 从该组首个成员起做连续段扫描；i 不在段内 = 断开成员
        const int start = firstMember.value(gid, -1);
        int end = start;
        while (end + 1 < mLayers.size() && mLayers[end + 1]->groupId() == gid) { ++end; }
        if (i < start || i > end)
        {
            mLayers[i]->setGroupId(-1);
            changed = true;
        }
    }
    // 清掉没有成员的空组
    for (int g = mLayerGroups.size() - 1; g >= 0; --g)
    {
        if (!firstMember.contains(mLayerGroups[g].id))
        {
            mLayerGroups.removeAt(g);
            changed = true;
        }
    }
    if (changed) { ++mLayerGroupGeneration; }
}

QList<Object::TimelineRowRef> Object::buildTimelineRows() const
{
    // 行序列表按栈序（0..n-1）生成；本时间轴视觉上是倒着画的（索引大 = 屏幕上方），
    // 因此组头行要排在栈序最大的成员之后（视觉上正好在组块顶端）
    QList<TimelineRowRef> rows;
    for (int i = 0; i < mLayers.size(); ++i)
    {
        Layer* layer = mLayers[i];
        const int gid = layer->groupId();
        const LayerGroupInfo* info = (gid >= 0) ? layerGroupInfo(gid) : nullptr;

        if (info == nullptr || !info->collapsed)
        {
            TimelineRowRef row;
            row.layer = layer;
            row.groupId = gid;
            rows.append(row);
        }
        if (gid >= 0)
        {
            const bool isTopOfGroup = (i + 1 >= mLayers.size() || mLayers[i + 1]->groupId() != gid);
            if (isTopOfGroup)
            {
                TimelineRowRef header;
                header.isHeader = true;
                header.groupId = gid;
                rows.append(header);
            }
        }
    }
    return rows;
}

bool Object::moveLayerRange(int fromIndex, int count, int toIndex)
{
    if (count <= 0) { return true; }
    if (fromIndex < 0 || fromIndex + count > mLayers.size()) { return false; }
    if (toIndex < 0 || toIndex > mLayers.size()) { return false; }
    if (toIndex >= fromIndex && toIndex <= fromIndex + count) { return true; } // 原地

    QList<Layer*> block;
    for (int i = 0; i < count; ++i)
    {
        block.append(mLayers.takeAt(fromIndex));
    }
    // take 之后插入目标的换算：目标是原坐标系的槽位
    int insertAt = (toIndex > fromIndex) ? toIndex - count : toIndex;
    insertAt = qBound(0, insertAt, mLayers.size());
    for (int i = 0; i < count; ++i)
    {
        mLayers.insert(insertAt + i, block[i]);
    }
    ++mLayerStructureGeneration;
    return true;
}

void Object::applyLayerGroupState(const QHash<int, int>& layerGroupIdMap,
                                  const QList<LayerGroupInfo>& groups,
                                  int nextGroupId)
{
    mLayerGroups = groups;
    mNextLayerGroupId = qMax(mNextLayerGroupId, nextGroupId);
    for (Layer* layer : mLayers)
    {
        const auto it = layerGroupIdMap.constFind(layer->id());
        layer->setGroupId(it != layerGroupIdMap.constEnd() ? it.value() : -1);
    }
    repairLayerGroupContiguity();
    ++mLayerGroupGeneration;
}

void Object::applyLayerOrder(const QList<int>& orderedIds)
{
    QHash<int, Layer*> byId;
    for (Layer* layer : mLayers)
    {
        byId.insert(layer->id(), layer);
    }

    QList<Layer*> reordered;
    for (int id : orderedIds)
    {
        Layer* layer = byId.take(id);
        if (layer != nullptr)
        {
            reordered.append(layer);
        }
    }
    // anything not listed (should not happen) keeps its relative order at the end
    for (Layer* layer : mLayers)
    {
        if (byId.contains(layer->id()) && byId.value(layer->id()) == layer)
        {
            reordered.append(layer);
            byId.remove(layer->id());
        }
    }
    mLayers = reordered;
    ++mLayerStructureGeneration;
}

bool Object::canSwapLayers(int layerIndexLeft, int layerIndexRight) const
{
    if (layerIndexLeft < 0 || layerIndexLeft >= mLayers.size())
    {
        return false;
    }

    if (layerIndexRight < 0 || layerIndexRight >= mLayers.size())
    {
        return false;
    }

    Layer* firstLayer = mLayers.first();
    Layer* leftLayer = mLayers.at(layerIndexLeft);
    Layer* rightLayer = mLayers.at(layerIndexRight);

    // The bottom layer can't be swapped!
    if ((leftLayer->type() == Layer::CAMERA ||
         rightLayer->type() == Layer::CAMERA) &&
         (firstLayer == leftLayer || firstLayer == rightLayer)) {
        return false;
    }
    return true;
}

bool Object::canDeleteLayer(int index) const
{
    // We expect the first camera layer to be at the bottom and this layer must not be deleted!
    if (index == 0) {
        return false;
    }

    if (mLayers.at(index) == nullptr)
    {
        return false;
    }

    return true;
}

void Object::deleteLayer(int i)
{
    if (i > -1 && i < mLayers.size())
    {
        delete mLayers.takeAt(i);
        // 行缓存(TimeLineCells.mRows)按结构代数失效；删层不 bump 会让
        // 后续重绘拿着悬空 Layer* 画时间轴（UAF/堆损坏）
        ++mLayerStructureGeneration;
    }
}

void Object::deleteLayer(Layer* layer)
{
    auto it = std::find(mLayers.begin(), mLayers.end(), layer);

    if (it != mLayers.end())
    {
        delete layer;
        mLayers.erase(it);
        ++mLayerStructureGeneration;
    }
}

bool Object::addLayer(Layer* layer)
{
    if (layer == nullptr || mLayers.contains(layer))
    {
        return false;
    }
    layer->setId(getUniqueLayerID());
    mLayers.append(layer);
    ++mLayerStructureGeneration;
    return true;
}

ColorRef Object::getColor(int index) const
{
    ColorRef result(Qt::white, tr("error"));
    if (index > -1 && index < mPalette.size())
    {
        result = mPalette.at(index);
    }
    return result;
}

void Object::setColor(int index, const QColor& newColor)
{
    Q_ASSERT(index >= 0);

    mPalette[index].color = newColor;
}

void Object::setColorRef(int index, const ColorRef& newColorRef)
{
    mPalette[index] = newColorRef;
}

void Object::movePaletteColor(int start, int end)
{
    mPalette.move(start, end);
}

void Object::addColorAtIndex(int index, const ColorRef& newColor)
{
    mPalette.insert(index, newColor);
}

void Object::removeColor(int index)
{
    mPalette.removeAt(index);
}

void Object::renameColor(int i, const QString& text)
{
    mPalette[i].name = text;
}

QString Object::savePalette(const QString& dataFolder) const
{
    QString fullPath = QDir(dataFolder).filePath(PFF_PALETTE_FILE);
    bool ok = exportPalette(fullPath);
    if (ok)
        return fullPath;
    return "";
}

void Object::exportPaletteGPL(QFile& file) const
{
    QString fileName = QFileInfo(file).baseName();
    QTextStream out(&file);

    out << "GIMP Palette" << "\n";
    out << "Name: " << fileName << "\n";
    out << "#" << "\n";

    for (const ColorRef& ref : mPalette)
    {
        QColor toRgb = ref.color.toRgb();
        out << QString("%1 %2 %3").arg(toRgb.red()).arg(toRgb.green()).arg(toRgb.blue());
        out << " " << ref.name << "\n";
    }
}

void Object::exportPalettePencil(QFile& file) const
{
    QTextStream out(&file);

    // 序列化视图：当前 mPalette 实时状态覆盖回激活槽位（const 方法不落地改动）
    // 注意：局部变量不能叫 slots——那是 Qt 的关键字宏
    QList<PaletteSlot> slotList = mPaletteSlots;
    if (mCurrentPaletteIndex >= 0 && mCurrentPaletteIndex < slotList.size())
    {
        slotList[mCurrentPaletteIndex].colors = mPalette;
    }
    if (slotList.isEmpty())
    {
        slotList.append({ tr("默认色卡"), mPalette });
    }

    QDomDocument doc("PencilPalette");
    QDomElement root = doc.createElement("palette");
    root.setAttribute("active", mCurrentPaletteIndex);
    doc.appendChild(root);
    for (const PaletteSlot& slot : slotList)
    {
        QDomElement entryTag = doc.createElement("paletteentry");
        entryTag.setAttribute("name", slot.name);
        root.appendChild(entryTag);
        for (const ColorRef& ref : slot.colors)
        {
            QDomElement tag = doc.createElement("Color");
            tag.setAttribute("name", ref.name);
            tag.setAttribute("red", ref.color.red());
            tag.setAttribute("green", ref.color.green());
            tag.setAttribute("blue", ref.color.blue());
            tag.setAttribute("alpha", ref.color.alpha());
            entryTag.appendChild(tag);
        }
    }
    int indentSize = 2;
    doc.save(out, indentSize);
}

bool Object::exportPalette(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        qDebug("Error: cannot export palette");
        return false;
    }

    if (file.fileName().endsWith(".gpl", Qt::CaseInsensitive))
        exportPaletteGPL(file);
    else
        exportPalettePencil(file);

    return true;
}

/* Import the .gpl GIMP palette format.
 *
 * This functions supports importing both the old and new .gpl formats.
 * This should load colors the same as GIMP, with the following intentional exceptions:
 * - Whitespace before and after a name does not appear in the name
 * - The last line is processed, even if there is not a trailing newline
 * - Colors without a name will use our automatic naming system rather than "Untitled"
 */
void Object::importPaletteGPL(QFile& file)
{
    QTextStream in(&file);
    QString line;

    // The first line must start with "GIMP Palette"
    // Displaying an error here would be nice
    in.readLineInto(&line);
    if (!line.startsWith("GIMP Palette")) return;

    in.readLineInto(&line);

    // There are two GPL formats, the new one must start with "Name: " on the second line
    if (line.startsWith("Name: "))
    {
        in.readLineInto(&line);
        // The new format contains an optional third line starting with "Columns: "
        if (line.startsWith("Columns: "))
        {
            // Skip to the next line
            in.readLineInto(&line);
        }
    }

    // Colors inherit the value from the previous color for missing channels
    // Some palettes may rely on this behavior, so we should try to replicate it
    QColor prevColor(Qt::black);

    do
    {
        // Ignore comments and empty lines
        if (line.isEmpty() || line.startsWith("#")) continue;

        int red = 0;
        int green = 0;
        int blue = 0;

        int countInLine = 0;
        QString name = "";
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        for(const QString& snip : line.split(QRegularExpression("\\s|\\t"), Qt::SkipEmptyParts))
#else
        for(const QString& snip : line.split(QRegularExpression("\\s|\\t"), QString::SkipEmptyParts))
#endif
        {
            switch (countInLine)
            {
            case 0:
                red = snip.toInt();
                break;
            case 1:
                green = snip.toInt();
                break;
            case 2:
                blue = snip.toInt();
                break;
            default:
                name += snip + " ";
            }
            countInLine++;
        }

        // trim additional spaces
        name = name.trimmed();

        // Get values from previous color if necessary
        if (countInLine < 2) green = prevColor.green();
        if (countInLine < 3) blue = prevColor.blue();

        // GIMP assigns colors the name "Untitled" by default now,
        // so in addition to missing names, we also use automatic
        // naming for this
        if (name.isEmpty() || name == "Untitled") name = QString();

        QColor color(red, green, blue);
        if (color.isValid())
        {
            mPalette.append(ColorRef(color, name));
            prevColor = color;
        }
    } while (in.readLineInto(&line));
}

void Object::importPalettePencil(QFile& file)
{
    QDomDocument doc;
    doc.setContent(&file);

    QDomElement docElem = doc.documentElement();
    QDomNode tag = docElem.firstChild();
    while (!tag.isNull())
    {
        QDomElement e = tag.toElement(); // try to convert the node to an element.
        if (!e.isNull() && e.tagName() == "Color")
        {
            QString name = e.attribute("name");
            int r = e.attribute("red").toInt();
            int g = e.attribute("green").toInt();
            int b = e.attribute("blue").toInt();
            int a = e.attribute("alpha", "255").toInt();
            mPalette.append(ColorRef(QColor(r, g, b, a), name));
        }
        tag = tag.nextSibling();
    }
}

void Object::openPalette(const QString& filePath)
{
    if (!QFile::exists(filePath))
    {
        return;
    }

    mPalette.clear();
    importPalette(filePath);
}

/*
 * Imports palette, e.g. appends to palette
*/
bool Object::importPalette(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::ReadOnly))
    {
        return false;
    }

    if (file.fileName().endsWith(".gpl", Qt::CaseInsensitive))
    {
        importPaletteGPL(file);
    } else {
        importPalettePencil(file);
    }
    return true;
}


void Object::loadDefaultPalette()
{
    mPalette.clear();
    addColor(ColorRef(QColor(Qt::black), tr("Black")));
    addColor(ColorRef(QColor(Qt::red), tr("Red")));
    addColor(ColorRef(QColor(Qt::darkRed), tr("Dark Red")));
    addColor(ColorRef(QColor(255, 128, 0), tr("Orange")));
    addColor(ColorRef(QColor(128, 64, 0), tr("Dark Orange")));
    addColor(ColorRef(QColor(Qt::yellow), tr("Yellow")));
    addColor(ColorRef(QColor(Qt::darkYellow), tr("Dark Yellow")));
    addColor(ColorRef(QColor(Qt::green), tr("Green")));
    addColor(ColorRef(QColor(Qt::darkGreen), tr("Dark Green")));
    addColor(ColorRef(QColor(Qt::cyan), tr("Cyan")));
    addColor(ColorRef(QColor(Qt::darkCyan), tr("Dark Cyan")));
    addColor(ColorRef(QColor(Qt::blue), tr("Blue")));
    addColor(ColorRef(QColor(Qt::darkBlue), tr("Dark Blue")));
    addColor(ColorRef(QColor(255, 255, 255), tr("White")));
    addColor(ColorRef(QColor(220, 220, 229), tr("Very Light Grey")));
    addColor(ColorRef(QColor(Qt::lightGray), tr("Light Grey")));
    addColor(ColorRef(QColor(Qt::gray), tr("Grey")));
    addColor(ColorRef(QColor(Qt::darkGray), tr("Dark Grey")));
    addColor(ColorRef(QColor(255, 227, 187), tr("Pale Orange Yellow")));
    addColor(ColorRef(QColor(221, 196, 161), tr("Pale Grayish Orange Yellow")));
    addColor(ColorRef(QColor(255, 214, 156), tr("Orange Yellow ")));
    addColor(ColorRef(QColor(207, 174, 127), tr("Grayish Orange Yellow")));
    addColor(ColorRef(QColor(255, 198, 116), tr("Light Orange Yellow")));
    addColor(ColorRef(QColor(227, 177, 105), tr("Light Grayish Orange Yellow")));

    // 新文档：重置为单一默认色卡
    mPaletteSlots.clear();
    mPaletteSlots.append({ tr("默认色卡"), mPalette });
    mCurrentPaletteIndex = 0;
}

QString Object::paletteName(int index) const
{
    if (index < 0 || index >= mPaletteSlots.size())
    {
        return QString();
    }
    return mPaletteSlots.at(index).name;
}

void Object::syncActivePaletteToSlot()
{
    if (mCurrentPaletteIndex >= 0 && mCurrentPaletteIndex < mPaletteSlots.size())
    {
        mPaletteSlots[mCurrentPaletteIndex].colors = mPalette;
    }
}

void Object::switchToPalette(int index)
{
    if (index < 0 || index >= mPaletteSlots.size() || index == mCurrentPaletteIndex)
    {
        return;
    }
    syncActivePaletteToSlot();
    mCurrentPaletteIndex = index;
    mPalette = mPaletteSlots.at(index).colors;
}

int Object::addPalette(const QString& name)
{
    syncActivePaletteToSlot();
    mPaletteSlots.append({ name, QList<ColorRef>() });
    mCurrentPaletteIndex = mPaletteSlots.size() - 1;
    mPalette.clear();
    return mCurrentPaletteIndex;
}

void Object::renamePalette(int index, const QString& name)
{
    if (index < 0 || index >= mPaletteSlots.size() || name.isEmpty())
    {
        return;
    }
    mPaletteSlots[index].name = name;
}

void Object::removePalette(int index)
{
    if (mPaletteSlots.size() <= 1)
    {
        return; // 至少保留一张色卡
    }
    if (index < 0 || index >= mPaletteSlots.size())
    {
        return;
    }
    mPaletteSlots.removeAt(index);
    if (mCurrentPaletteIndex == index)
    {
        mCurrentPaletteIndex = qMin(index, mPaletteSlots.size() - 1);
        mPalette = mPaletteSlots.at(mCurrentPaletteIndex).colors;
    }
    else if (mCurrentPaletteIndex > index)
    {
        mCurrentPaletteIndex--;
    }
}

void Object::ensurePaletteSlots()
{
    if (mPaletteSlots.isEmpty())
    {
        mPaletteSlots.append({ tr("默认色卡"), mPalette });
        mCurrentPaletteIndex = 0;
    }
}

bool Object::loadProjectPalette(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        return false;
    }

    QDomDocument doc;
    if (!doc.setContent(&file))
    {
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.isNull() || root.tagName() != "palette")
    {
        return false;
    }

    // 检测新容器格式（paletteentry 子节点）；旧格式是扁平的 Color 列表
    bool multiFormat = false;
    for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling())
    {
        if (n.toElement().tagName() == "paletteentry")
        {
            multiFormat = true;
            break;
        }
    }

    if (!multiFormat)
    {
        // 旧工程：颜色追加进当前缓冲，再补一张默认卡槽
        for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling())
        {
            QDomElement e = n.toElement();
            if (e.tagName() != "Color") { continue; }
            mPalette.append(ColorRef(QColor(e.attribute("red").toInt(),
                                            e.attribute("green").toInt(),
                                            e.attribute("blue").toInt(),
                                            e.attribute("alpha", "255").toInt()),
                                      e.attribute("name")));
        }
        ensurePaletteSlots();
        return true;
    }

    mPaletteSlots.clear();
    for (QDomNode entryNode = root.firstChild(); !entryNode.isNull(); entryNode = entryNode.nextSibling())
    {
        QDomElement entry = entryNode.toElement();
        if (entry.tagName() != "paletteentry") { continue; }

        PaletteSlot slot;
        slot.name = entry.attribute("name", tr("色卡"));
        for (QDomNode colorNode = entry.firstChild(); !colorNode.isNull(); colorNode = colorNode.nextSibling())
        {
            QDomElement e = colorNode.toElement();
            if (e.tagName() != "Color") { continue; }
            slot.colors.append(ColorRef(QColor(e.attribute("red").toInt(),
                                               e.attribute("green").toInt(),
                                               e.attribute("blue").toInt(),
                                               e.attribute("alpha", "255").toInt()),
                                         e.attribute("name")));
        }
        mPaletteSlots.append(slot);
    }
    mCurrentPaletteIndex = qBound(0, root.attribute("active", "0").toInt(), qMax(0, mPaletteSlots.size() - 1));
    mPalette = mPaletteSlots.value(mCurrentPaletteIndex).colors;
    return true;
}

void Object::paintImage(QPainter& painter,int frameNumber,
                        bool background,
                        bool antialiasing) const
{
    updateActiveFrames(frameNumber);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // paints the background
    if (background)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.setWorldMatrixEnabled(false);
        painter.drawRect(QRect(0, 0, painter.device()->width(), painter.device()->height()));
        painter.setWorldMatrixEnabled(true);
    }

    bool anyClipMask = false;
    for (Layer* layer : mLayers)
    {
        if (layer->type() == Layer::BITMAP && layer->clipMask())
        {
            anyClipMask = true;
            break;
        }
    }

    if (!anyClipMask)
    {
        const int layerCount = mLayers.size();
        for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        {
            Layer* layer = mLayers.at(layerIndex);
            if (!isLayerRenderable(layer))
            {
                continue;
            }

            painter.setOpacity(1.0);

            if (layer->type() == Layer::BITMAP)
            {

                LayerBitmap* layerBitmap = static_cast<LayerBitmap*>(layer);
                // 循环层按显示帧取内容（开放尾块区域回绕；cover 本身保持字面语义）
                BitmapImage* bitmap = static_cast<BitmapImage*>(layerBitmap->getKeyFrameWhichCovers(
                    layerBitmap->displayFrameFor(frameNumber)));
                if (bitmap)
                {
                    // same layer-opacity blend as CanvasPainter::paintCurrentBitmapFrame
                    painter.setOpacity(bitmap->getOpacity() - (1.0 - layer->opacity()));
                    bitmap->paintImage(painter);
                }

            }
            else if (layer->type() == Layer::COLORIZE)
            {
                // 导出/渲染为最终观感：只画着色结果（受 Show output 控制），
                // 笔画不进导出；离线路径上过期帧同步兜底重算
                auto layerColorize = static_cast<LayerColorize*>(layer);
                ColorizeImage* frame = static_cast<ColorizeImage*>(layerColorize->getKeyFrameWhichCovers(
                    layerColorize->displayFrameFor(frameNumber)));
                if (frame)
                {
                    frame->loadFile();
                    if (frame->needsUpdate() ||
                        frame->computedStructureGeneration() != layerStructureGeneration())
                    {
                        layerColorize->updateColoringAtFrame(frameNumber,
                                                             getColorizeSourceLayer(layerIndex, frameNumber),
                                                             layerStructureGeneration());
                    }
                    if (layerColorize->showColoring() && !frame->coloringImage().isNull())
                    {
                        painter.setOpacity(frame->getOpacity() - (1.0 - layer->opacity()));
                        painter.drawImage(frame->coloringBounds().topLeft(), frame->coloringImage());
                    }
                }
            }
        }
        return;
    }

    // Clipping-mask compositing: each layer is rasterized through the caller's
    // combined transform into a device-space buffer, masked by the alpha of
    // the accumulated layers below (CanvasPainter semantics), then blitted.
    const QTransform deviceTransform = painter.combinedTransform();
    const QRect deviceRect(0, 0, painter.device()->width(), painter.device()->height());
    QImage accum(deviceRect.size(), QImage::Format_ARGB32_Premultiplied);
    accum.fill(Qt::transparent);
    QImage groupMask;
    bool groupValid = false;

    for (Layer* layer : mLayers)
    {
        if (!isLayerRenderable(layer) || layer->type() != Layer::BITMAP)
        {
            continue;
        }

        // a non-clipped layer terminates the clipped run above it
        if (!layer->clipMask())
        {
            groupValid = false;
        }

        LayerBitmap* layerBitmap = static_cast<LayerBitmap*>(layer);
        BitmapImage* bitmap = static_cast<BitmapImage*>(layerBitmap->getKeyFrameWhichCovers(frameNumber));
        if (bitmap == nullptr)
        {
            continue;
        }

        QImage layerBuffer(deviceRect.size(), QImage::Format_ARGB32_Premultiplied);
        layerBuffer.fill(Qt::transparent);
        {
            QPainter bufferPainter(&layerBuffer);
            bufferPainter.setRenderHint(QPainter::Antialiasing, true);
            bufferPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            bufferPainter.setTransform(deviceTransform);
            // same layer-opacity blend as CanvasPainter::paintCurrentBitmapFrame
            bufferPainter.setOpacity(qBound(0.0, bitmap->getOpacity() - (1.0 - layer->opacity()), 1.0));
            bitmap->paintImage(bufferPainter);
        }

        if (layer->clipMask())
        {
            if (!groupValid)
            {
                groupMask = accum.copy();
                groupValid = true;
            }
            QPainter maskPainter(&layerBuffer);
            maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            maskPainter.drawImage(QPoint(0, 0), groupMask);
        }

        painter.save();
        painter.setWorldMatrixEnabled(false);
        painter.setOpacity(1.0);
        painter.drawImage(painter.viewport(), layerBuffer, deviceRect);
        painter.restore();

        QPainter accumPainter(&accum);
        accumPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        accumPainter.drawImage(QPoint(0, 0), layerBuffer);
    }
}

QString Object::copyFileToDataFolder(const QString& strFilePath)
{
    if (!QFile::exists(strFilePath))
    {
        qDebug() << "[Object] sound file doesn't exist: " << strFilePath;
        return "";
    }

    QString sNewFileName = "sound_";
    sNewFileName += QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz.");
    sNewFileName += QFileInfo(strFilePath).suffix();

    QString destFile = QDir(mDataDirPath).filePath(sNewFileName);

    if (QFile::exists(destFile))
    {
        QFile::remove(destFile);
    }

    bool bCopyOK = QFile::copy(strFilePath, destFile);
    if (!bCopyOK)
    {
        qDebug() << "[Object] couldn't copy sound file to data folder: " << strFilePath;
        return "";
    }

    return destFile;
}

Status Object::exportFrames(int frameStart, int frameEnd,
                          const LayerCamera* cameraLayer,
                          QSize exportSize,
                          QString filePath,
                          QString format,
                          bool transparency,
                          bool exportKeyframesOnly,
                          const QString& layerName,
                          bool antialiasing,
                          QProgressDialog* progress = nullptr,
                          int progressMax = 50) const
{
    Q_ASSERT(cameraLayer);

    QString extension = "";
    QString formatStr = format;
    if (formatStr == "PNG" || formatStr == "png")
    {
        format = "PNG";
        extension = ".png";
    }
    if (formatStr == "JPG" || formatStr == "jpg" || formatStr == "JPEG" || formatStr == "jpeg")
    {
        format = "JPG";
        extension = ".jpg";
        transparency = false; // JPG doesn't support transparency, so we have to include the background
    }
    if (formatStr == "TIFF" || formatStr == "tiff" || formatStr == "TIF" || formatStr == "tif")
    {
        format = "TIFF";
        extension = ".tiff";
    }
    if (formatStr == "BMP" || formatStr == "bmp")
    {
        format = "BMP";
        extension = ".bmp";
        transparency = false;
    }
    if (formatStr == "WEBP" || formatStr == "webp") {
        format = "WEBP";
        extension = ".webp";
    }
    if (filePath.endsWith(extension, Qt::CaseInsensitive))
    {
        filePath.chop(extension.size());
    }

    qDebug() << "Exporting frames from "
        << frameStart << "to"
        << frameEnd
        << "at size " << exportSize;

    DebugDetails dd;
    dd << "\n[Export frames diagnostics]\n";
    bool ok = true;

    for (int currentFrame = frameStart; currentFrame <= frameEnd; currentFrame++)
    {
        if (progress != nullptr)
        {
            int totalFramesToExport = (frameEnd - frameStart) + 1;
            if (totalFramesToExport != 0) // Avoid dividing by zero.
            {
                progress->setValue((currentFrame - frameStart + 1) * progressMax / totalFramesToExport);
                QApplication::processEvents(); // Required to make progress bar update on-screen.
            }

            if (progress->wasCanceled())
            {
                break;
            }
        }

        QTransform view = cameraLayer->getViewAtFrame(currentFrame);
        QSize camSize = cameraLayer->getViewSize();

        QString frameNumberString = QString::number(currentFrame);
        while (frameNumberString.length() < 4)
        {
            frameNumberString.prepend("0");
        }
        QString sFileName = filePath + frameNumberString + extension;
        Layer* layer = findLayerByName(layerName);
        Status st = Status::SAFE;
        if (exportKeyframesOnly)
        {
            if (layer->keyExists(currentFrame))
            {
                st = exportIm(currentFrame, view, camSize, exportSize, sFileName, format, antialiasing, transparency);
            }
        }
        else
        {
            st = exportIm(currentFrame, view, camSize, exportSize, sFileName, format, antialiasing, transparency);
        }

        if (!st.ok())
        {
            ok = false;
            dd.collect(st.details());
        }
    }

    if (!ok)
    {
        dd << "\nError: Failed to export one or more frames";
        return Status(Status::FAIL, dd);
    }

    return Status::OK;
}

Status Object::exportIm(int frame, const QTransform& view, QSize cameraSize, QSize exportSize, const QString& filePath, const QString& format, bool antialiasing, bool transparency) const
{
    QImage imageToExport(exportSize, QImage::Format_ARGB32_Premultiplied);

    QColor bgColor = Qt::white;
    if (transparency)
        bgColor.setAlpha(0);
    imageToExport.fill(bgColor);

    QTransform centralizeCamera;
    centralizeCamera.translate(cameraSize.width() / 2, cameraSize.height() / 2);

    QPainter painter(&imageToExport);
    painter.setWorldTransform(view * centralizeCamera);
    painter.setWindow(QRect(0, 0, cameraSize.width(), cameraSize.height()));

    paintImage(painter, frame, false, antialiasing);

    QImageWriter writer(filePath, format.toStdString().c_str());
    bool b = writer.write(imageToExport);
    if (b) {
        return Status::OK;
    } else {
        DebugDetails dd;
        dd << "Object::exportIm";
        dd << QString("&nbsp;&nbsp;filePath: ").append(filePath);
        dd << QString("&nbsp;&nbsp;Error: %1 (code %2)").arg(writer.errorString()).arg(static_cast<int>(writer.error()));
        return Status(Status::FAIL, dd);
    }
}

int Object::getLayerCount() const
{
    return mLayers.size();
}

void Object::setData(const ObjectData& d)
{
    mData = d;
}

int Object::totalKeyFrameCount() const
{
    int sum = 0;
    for (const Layer* layer : mLayers)
    {
        sum += layer->keyFrameCount();
    }
    return sum;
}

void Object::updateActiveFrames(int frame) const
{
    const int beginFrame = std::max(frame - 3, 1);
    const int endFrame = frame + 4;

    const int minFrameCount = getLayerCount() * (endFrame - beginFrame);
    mActiveFramePool->setMinFrameCount(minFrameCount);

    for (Layer* layer : mLayers)
    {
        if (isLayerRenderable(layer))
        {
            // 循环层按显示帧预载（回绕到周期内的帧号），否则循环区永远不预热
            const int center = layer->displayFrameFor(frame);
            const int layerBegin = std::max(center - 3, 1);
            const int layerEnd = center + 4;
            for (int k = layerBegin; k < layerEnd; ++k)
            {
                KeyFrame* key = layer->getKeyFrameAt(k);
                mActiveFramePool->put(key);
            }
        }
    }
}

void Object::setActiveFramePoolSize(int sizeInMB)
{
    // convert MB to Byte
    mActiveFramePool->resize(qint64(sizeInMB) * 1024 * 1024);
}
