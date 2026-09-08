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

#ifndef LAYER_MANAGER_H
#define LAYER_MANAGER_H

#include "basemanager.h"
#include "layer.h"

class LayerBitmap;
class LayerCamera;
class LayerSound;


class LayerManager : public BaseManager
{
    Q_OBJECT

public:
    explicit LayerManager(Editor* editor);
    ~LayerManager() override;
    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    // Layer Management
    Layer* currentLayer();
    Layer* currentLayer(int offset);
    Layer* getLayer(int index);
    LayerCamera* getCameraLayerBelow(int layerIndex) const;
    Layer* findLayerByName(QString sName, Layer::LAYER_TYPE type = Layer::UNDEFINED);
    Layer* findLayerById(int layerId) const;
    Layer* getLastCameraLayer();
    int    currentLayerIndex();
    void   setCurrentLayer(int nIndex);
    void   setCurrentLayer(Layer* layer);
    int    count();

    bool canDeleteLayer(int index) const;
    Status deleteLayer(int index);
    Status renameLayer(Layer*, const QString& newName);
    void notifyLayerChanged(Layer*);

    void gotoNextLayer();
    void gotoPreviouslayer();

    /** Returns a new Layer with the given LAYER_TYPE */
    Layer* createLayer(Layer::LAYER_TYPE type, const QString& strLayerName);
    LayerBitmap* createBitmapLayer(const QString& strLayerName);
    LayerBitmap* createColorizeLayer(const QString& strLayerName);
    LayerCamera* createCameraLayer(const QString& strLayerName);
    LayerSound*  createSoundLayer(const QString& strLayerName);

    // KeyFrame Management
    int lastFrameAtFrame(int frameIndex);
    int firstKeyFrameIndex();
    int lastKeyFrameIndex();

    int animationLength(bool includeSounds = true);

    /** This should be emitted whenever the animation length frames, eg. adding, removing, duplicating */
    void notifyAnimationLengthChanged();

    QString nameSuggestLayer(const QString& name);
    int getLastLayerIndex() { return count() - 1; }

    // ---- 图层分组操作（每步一条撤销；入口保证声音/相机层不进组） ----
    /** 右键入口：把 layerIndex 处的图层收进一个新建组，返回组 id（失败 -1） */
    int createGroupWithLayer(int layerIndex);
    /** 拖拽入组：把 layerIndex 图层设为 groupId 成员（调用方保证已与组相邻） */
    bool addLayerToGroup(int layerIndex, int groupId);
    /** Alt 拖出：把 layerIndex 图层移出所在组 */
    bool removeLayerFromGroup(int layerIndex);
    /** 解散组：全部成员出组并删表条目 */
    bool dissolveGroup(int groupId);
    void renameGroup(int groupId, const QString& name);
    void toggleGroupCollapsed(int groupId);
    void setGroupVisible(int groupId, bool visible);
    void setGroupLocked(int groupId, bool locked);
    /** 整组块移动到 toIndex（insert 语义） */
    bool moveLayerGroup(int groupId, int toIndex);

signals:
    void currentLayerWillChange(int index);
    void currentLayerChanged(int index);
    void layerCountChanged(int count);
    void animationLengthChanged(int length);
    void layerDeleted(int index);

private:
    int getIndex(Layer*) const;

    int mLastCameraLayerIdx = 0;
};

#endif
