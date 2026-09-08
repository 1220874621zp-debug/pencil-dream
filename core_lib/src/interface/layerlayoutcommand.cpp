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
#include "layerlayoutcommand.h"

#include "editor.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"

LayerLayoutCommand::LayerLayoutCommand(Editor* editor,
                                       const QList<LayerLayouts>& layouts,
                                       const QList<KeyFrame*>& ownedKeys,
                                       const QString& description,
                                       QUndoCommand* parent)
    : UndoRedoCommand(editor, parent)
    , mLayouts(layouts)
    , mOwnedKeys(ownedKeys)
{
    setText(description);
}

LayerLayoutCommand::~LayerLayoutCommand()
{
    // keys that never went back into a layer belong to the command
    for (KeyFrame* key : mOwnedKeys)
    {
        delete key;
    }
}

void LayerLayoutCommand::apply(const QList<KeyFrameLayoutEntry>& layout, int layerId)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr)
    {
        setObsolete(true);
        return;
    }

    QList<KeyFrame*> extracted;
    layer->applyKeyFrameLayout(layout, extracted);

    // extracted keys stay alive under the command's custody; the layout
    // rebuild inserted the listed-but-previously-extracted ones back into
    // the layer, so custody is simply "not hosted by any touched layer"
    mOwnedKeys.append(extracted);
    QList<KeyFrame*> stillOwned;
    for (KeyFrame* key : mOwnedKeys)
    {
        bool hosted = false;
        for (const LayerLayouts& l : mLayouts)
        {
            Layer* host = editor()->layers()->findLayerById(l.layerId);
            if (host != nullptr && host->containsKeyFramePointer(key)) { hosted = true; break; }
        }
        if (!hosted && key != nullptr) { stillOwned.append(key); }
    }
    mOwnedKeys = stillOwned;
}

void LayerLayoutCommand::undo()
{
    UndoRedoCommand::undo();

    for (const LayerLayouts& layouts : mLayouts)
    {
        apply(layouts.undoLayout, layouts.layerId);
    }

    emit editor()->framesModified();
    editor()->layers()->notifyAnimationLengthChanged();
}

void LayerLayoutCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore the automatic redo issued when the command joins the undo stack:
    // the operation itself has already been applied by the caller.
    if (isFirstRedo()) { setFirstRedo(false); return; }

    for (const LayerLayouts& layouts : mLayouts)
    {
        apply(layouts.redoLayout, layouts.layerId);
    }

    emit editor()->framesModified();
    editor()->layers()->notifyAnimationLengthChanged();
}

LayerOrderCommand::GroupSnapshot LayerOrderCommand::captureGroups(Object* obj)
{
    GroupSnapshot snap;
    snap.valid = true;
    snap.groups = obj->layerGroups();
    snap.nextGroupId = obj->nextLayerGroupId();
    for (int i = 0; i < obj->getLayerCount(); ++i)
    {
        snap.layerGroupId.insert(obj->getLayer(i)->id(), obj->getLayer(i)->groupId());
    }
    return snap;
}

LayerOrderCommand::LayerOrderCommand(Editor* editor,
                                     const QList<int>& undoOrder,
                                     const QList<int>& redoOrder,
                                     const QString& description,
                                     QUndoCommand* parent,
                                     const GroupSnapshot& undoGroups,
                                     const GroupSnapshot& redoGroups)
    : UndoRedoCommand(editor, parent)
    , mUndoOrder(undoOrder)
    , mRedoOrder(redoOrder)
    , mUndoGroups(undoGroups)
    , mRedoGroups(redoGroups)
{
    setText(description);
}

void LayerOrderCommand::apply(const QList<int>& order, const GroupSnapshot& groups)
{
    editor()->object()->applyLayerOrder(order);
    if (groups.valid)
    {
        editor()->object()->applyLayerGroupState(groups.layerGroupId, groups.groups, groups.nextGroupId);
    }
    else
    {
        // 纯重排也可能打散组：兜底修复连续性
        editor()->object()->repairLayerGroupContiguity();
    }
    editor()->scrubTo(editor()->currentFrame()); // refresh canvas state
    emit editor()->updateTimeLine();
    editor()->getScribbleArea()->onLayerChanged();
}

void LayerOrderCommand::undo()
{
    UndoRedoCommand::undo();
    apply(mUndoOrder, mUndoGroups);
}

void LayerOrderCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore the automatic redo issued when the command joins the undo stack:
    // the reorder has already been applied by the caller.
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(mRedoOrder, mRedoGroups);
}
