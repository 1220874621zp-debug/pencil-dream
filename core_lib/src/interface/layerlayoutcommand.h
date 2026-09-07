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

/** Undo/redo for layer reordering (insert-style drag, single step). */
class LayerOrderCommand : public UndoRedoCommand
{
public:
    LayerOrderCommand(Editor* editor,
                      const QList<int>& undoOrder,
                      const QList<int>& redoOrder,
                      const QString& description,
                      QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(const QList<int>& order);

    QList<int> mUndoOrder;
    QList<int> mRedoOrder;
};

#endif // LAYERLAYOUTCOMMAND_H
