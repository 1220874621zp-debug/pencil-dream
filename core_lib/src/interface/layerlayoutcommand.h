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
#ifndef LAYERLAYOUTCOMMAND_H
#define LAYERLAYOUTCOMMAND_H

#include "undoredocommand.h"
#include "layer.h"

/** Full-layout undo/redo for TVP-style timeline batch operations
 *  (hold-N redistribution, batch delete, frame paste, block move, trim ripple).
 *
 *  The command stores, per touched layer, the complete keyframe layout
 *  (pointer identity + position + length + explicit flag) captured before
 *  and after the operation. Keys that are absent from the layer while a
 *  layout is applied (deleted by the operation, or created by it and removed
 *  again on undo) are kept alive inside the command, so a single command can
 *  cycle undo/redo indefinitely without cloning pixel data.
 */
class LayerLayoutCommand : public UndoRedoCommand
{
public:
    struct LayerLayouts
    {
        int layerId = 0;
        QList<KeyFrameLayoutEntry> undoLayout;
        QList<KeyFrameLayoutEntry> redoLayout;
    };

    /** `ownedKeys` are keyframes outside their layer right now (removed by the
     *  operation); the command keeps them alive and reinserts them on undo. */
    LayerLayoutCommand(Editor* editor,
                       const QList<LayerLayouts>& layouts,
                       const QList<KeyFrame*>& ownedKeys,
                       const QString& description,
                       QUndoCommand* parent = nullptr);
    ~LayerLayoutCommand() override;

    void undo() override;
    void redo() override;

private:
    void apply(const QList<KeyFrameLayoutEntry>& layout, int layerId);

    QList<LayerLayouts> mLayouts;
    QList<KeyFrame*> mOwnedKeys; // not currently hosted by any layer
};

/** Undo/redo for layer reordering (insert-style drag, single step).
 *  可选携带图层分组状态快照：组操作（建/入/离/散/整组移动/组开关）与重排
 *  合并为同一条撤销命令。 */
class LayerOrderCommand : public UndoRedoCommand
{
public:
    struct GroupSnapshot
    {
        GroupSnapshot(); // 默认实参在类内引用本类临时量，GCC 要求默认构造在类外定义

        bool valid = false;
        QHash<int, int> layerGroupId; // layerId -> groupId（-1 = 无组）
        QList<LayerGroupInfo> groups;
        int nextGroupId = 1;
    };

    /** 捕获当前对象的完整分组状态（时间轴拖拽成组等入口共用） */
    static GroupSnapshot captureGroups(class Object* obj);

    LayerOrderCommand(Editor* editor,
                      const QList<int>& undoOrder,
                      const QList<int>& redoOrder,
                      const QString& description,
                      QUndoCommand* parent = nullptr,
                      const GroupSnapshot& undoGroups = GroupSnapshot(),
                      const GroupSnapshot& redoGroups = GroupSnapshot());

    void undo() override;
    void redo() override;

private:
    void apply(const QList<int>& order, const GroupSnapshot& groups);

    QList<int> mUndoOrder;
    QList<int> mRedoOrder;
    GroupSnapshot mUndoGroups;
    GroupSnapshot mRedoGroups;
};

/** 拆分图层颜色：一次性新建多个图层（含各自关键帧）的单步撤销命令。
 *  调用方先建好层、完成分组并插入 object（redo 态），命令 undo 时摘层
 *  （接管所有权）并还原拆分前的分组快照，redo 时按记录的原索引挂回并
 *  重放拆分后的分组快照；析构时删除仍被摘下的层（文档切换防泄漏）。
 *  hideOriginal 时同步快照源层可见性。 */
class SplitLayerCommand : public UndoRedoCommand
{
public:
    SplitLayerCommand(Editor* editor,
                      const QList<Layer*>& createdLayers,
                      int sourceLayerId,
                      bool hideOriginal,
                      const LayerOrderCommand::GroupSnapshot& undoGroups,
                      const QString& description,
                      QUndoCommand* parent = nullptr);
    ~SplitLayerCommand() override;

    void undo() override;
    void redo() override;

private:
    void detachLayers();
    void attachLayers();
    void applyGroups(const LayerOrderCommand::GroupSnapshot& groups);
    void refreshUi(int currentLayerId);

    QList<Layer*> mCreatedLayers;
    QList<int> mCreatedLayerIds;
    QList<int> mAttachIndices;   // 构造时各新层在 object 中的索引（redo 挂回位置）
    int mSourceLayerId = -1;
    bool mHideOriginal = false;
    bool mLayersAttached = true; // 摘下态归命令所有
    LayerOrderCommand::GroupSnapshot mUndoGroups; // 拆分前分组状态（调用方拆分前捕获传入）
    LayerOrderCommand::GroupSnapshot mRedoGroups; // 拆分后分组状态（构造时捕获）
};

#endif // LAYERLAYOUTCOMMAND_H
