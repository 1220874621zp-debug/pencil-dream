/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2008-2009 Mj Mendoza IV
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include <QDebug>
#include <algorithm>

#include "layermanager.h"
#include "selectionmanager.h"

#include "layersound.h"
#include "layerbitmap.h"
#include "layer.h"
#include "object.h"
#include "scribblearea.h"

#include "editor.h"
#include "undoredocommand.h"

UndoRedoCommand::UndoRedoCommand(Editor* editor, QUndoCommand* parent) : QUndoCommand(parent)
{
    qDebug() << "backupElement created";
    mEditor = editor;
}

KeyFrameRemoveCommand::KeyFrameRemoveCommand(const KeyFrame* undoKeyFrame,
                                         int undoLayerId,
                                         const QString &description,
                                         Editor *editor,
                                         QUndoCommand *parent) : UndoRedoCommand(editor, parent)
{
    this->undoKeyFrame = undoKeyFrame->clone();
    this->undoLayerId = undoLayerId;

    this->redoLayerId = editor->layers()->currentLayer()->id();
    this->redoPosition = editor->currentFrame();

    setText(description);
}

KeyFrameRemoveCommand::~KeyFrameRemoveCommand()
{
    delete undoKeyFrame;
}

void KeyFrameRemoveCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(undoLayerId);
    if (layer == nullptr) {
        // Until we support layer deletion recovery, we mark the command as
        // obsolete as soon as it's been
        return setObsolete(true);
    }

    UndoRedoCommand::undo();

    layer->addKeyFrame(undoKeyFrame->pos(), undoKeyFrame->clone());

    emit editor()->frameModified(undoKeyFrame->pos());
    editor()->layers()->notifyAnimationLengthChanged();
    editor()->scrubTo(undoKeyFrame->pos());
}

void KeyFrameRemoveCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(redoLayerId);
    if (layer == nullptr) {
        // Until we support layer deletion recovery, we mark the command as
        // obsolete as soon as it's been
        return setObsolete(true);
    }

    UndoRedoCommand::redo();

    if (isFirstRedo()) { setFirstRedo(false); return; }

    layer->removeKeyFrame(redoPosition);

    emit editor()->frameModified(redoPosition);
    editor()->layers()->notifyAnimationLengthChanged();
    editor()->scrubTo(redoPosition);
}

KeyFrameAddCommand::KeyFrameAddCommand(int undoPosition,
                                       int undoLayerId,
                                       const QString &description,
                                       Editor *editor,
                                       QUndoCommand *parent)
    : UndoRedoCommand(editor, parent)
{
    this->undoPosition = undoPosition;
    this->undoLayerId = undoLayerId;

    this->redoLayerId = editor->layers()->currentLayer()->id();
    this->redoPosition = editor->currentFrame();

    setText(description);
}

void KeyFrameAddCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(undoLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::undo();

    layer->removeKeyFrame(undoPosition);

    emit editor()->frameModified(undoPosition);
    editor()->layers()->notifyAnimationLengthChanged();
    editor()->layers()->setCurrentLayer(layer);
    editor()->scrubTo(undoPosition);
}

void KeyFrameAddCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(redoLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    layer->addNewKeyFrameAt(redoPosition);

    emit editor()->frameModified(redoPosition);
    editor()->layers()->notifyAnimationLengthChanged();
    editor()->layers()->setCurrentLayer(layer);
    editor()->scrubTo(redoPosition);
}

MoveKeyFramesCommand::MoveKeyFramesCommand(int offset,
                                         QList<int> listOfPositions,
                                         int undoLayerId,
                                         const QString& description,
                                         Editor* editor,
                                         QUndoCommand *parent)
    : UndoRedoCommand(editor, parent)
{
    this->frameOffset = offset;
    this->positions = listOfPositions;

    this->undoLayerId = undoLayerId;
    this->redoLayerId = editor->layers()->currentLayer()->id();

    setText(description);
}

void MoveKeyFramesCommand::undo()
{
    Layer* undoLayer = editor()->layers()->findLayerById(undoLayerId);

    if (!undoLayer) {
        return setObsolete(true);
    }

    UndoRedoCommand::undo();

    for (int position : qAsConst(positions)) {
        undoLayer->setFrameSelected(position, true);
    }
    undoLayer->moveSelectedFrames(-frameOffset);

    emit editor()->framesModified();
}

void MoveKeyFramesCommand::redo()
{
    Layer* redoLayer = editor()->layers()->findLayerById(redoLayerId);

    if (!redoLayer) {
        return setObsolete(true);
    }

    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    QList<int> newPositions = positions;


    for (int position : qAsConst(newPositions)) {
        redoLayer->setFrameSelected(position, true);
    }

    redoLayer->moveSelectedFrames(frameOffset);

    emit editor()->framesModified();
}
BitmapReplaceCommand::BitmapReplaceCommand(const BitmapImage* undoBitmap,
                             const int undoLayerId,
                             const QString& description,
                             Editor *editor,
                             QUndoCommand *parent) : UndoRedoCommand(editor, parent)
{

    this->undoBitmap = *undoBitmap;
    this->undoLayerId = undoLayerId;

    Layer* layer = editor->layers()->currentLayer();
    redoLayerId = layer->id();
    redoBitmap = *static_cast<LayerBitmap*>(layer)->
            getLastBitmapImageAtFrame(editor->currentFrame());

    setText(description);
}

BitmapReplaceCommand::BitmapReplaceCommand(const BitmapImage* undoBitmap,
                                           const BitmapImage* redoBitmap,
                                           const int layerId,
                                           const QString& description,
                                           Editor* editor,
                                           QUndoCommand* parent) : UndoRedoCommand(editor, parent)
{
    Q_ASSERT(undoBitmap != nullptr && redoBitmap != nullptr);

    this->undoBitmap = *undoBitmap;
    this->redoBitmap = *redoBitmap;
    this->undoLayerId = layerId;
    this->redoLayerId = layerId;

    setText(description);
}

void BitmapReplaceCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(undoLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::undo();

    static_cast<LayerBitmap*>(layer)->replaceKeyFrame(&undoBitmap);

    editor()->scrubTo(undoBitmap.pos());
}

void BitmapReplaceCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(redoLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    static_cast<LayerBitmap*>(layer)->replaceKeyFrame(&redoBitmap);

    editor()->scrubTo(redoBitmap.pos());
}

TransformCommand::TransformCommand(const QRectF& undoSelectionRect,
                                   const QPointF& undoTranslation,
                                   const qreal undoRotationAngle,
                                   const qreal undoScaleX,
                                   const qreal undoScaleY,
                                   const QPointF& undoTransformAnchor,
                                   const bool roundPixels,
                                   const QString& description,
                                   Editor* editor,
                                   QUndoCommand *parent) : UndoRedoCommand(editor, parent)
{
    this->roundPixels = roundPixels;

    this->undoSelectionRect = undoSelectionRect;
    this->undoAnchor = undoTransformAnchor;
    this->undoTranslation = undoTranslation;
    this->undoRotationAngle = undoRotationAngle;
    this->undoScaleX = undoScaleX;
    this->undoScaleY = undoScaleY;

    auto selectMan = editor->select();
    redoSelectionRect = selectMan->mySelectionRect();
    redoAnchor = selectMan->currentTransformAnchor();
    redoTranslation = selectMan->myTranslation();
    redoRotationAngle = selectMan->myRotation();
    redoScaleX = selectMan->myScaleX();
    redoScaleY = selectMan->myScaleY();

    setText(description);
}

void TransformCommand::undo()
{
    UndoRedoCommand::undo();
    apply(undoSelectionRect,
          undoTranslation,
          undoRotationAngle,
          undoScaleX,
          undoScaleY,
          undoAnchor,
          roundPixels);
}

void TransformCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(redoSelectionRect,
          redoTranslation,
          redoRotationAngle,
          redoScaleX,
          redoScaleY,
          redoAnchor,
          roundPixels);
}

void TransformCommand::apply(const QRectF& selectionRect,
                             const QPointF& translation,
                             const qreal rotationAngle,
                             const qreal scaleX,
                             const qreal scaleY,
                             const QPointF& selectionAnchor,
                             const bool roundPixels)
{
    auto selectMan = editor()->select();
    selectMan->setSelection(selectionRect, roundPixels);
    selectMan->setTransformAnchor(selectionAnchor);
    selectMan->setTranslation(translation);
    selectMan->setRotation(rotationAngle);
    selectMan->setScale(scaleX, scaleY);

    selectMan->calculateSelectionTransformation();
}

ConvertLayerCommand::ConvertLayerCommand(Editor* editor,
                                         Layer* newLayer,
                                         Layer* oldLayer,
                                         int index,
                                         const QString& description,
                                         QUndoCommand* parent)
    : UndoRedoCommand(editor, parent)
    , mNewLayer(newLayer)
    , mOldLayer(oldLayer)
    , mIndex(index)
{
    setText(description);
}

ConvertLayerCommand::~ConvertLayerCommand()
{
    // 摘下态的那层归命令所有（挂靠态归 object；文档切换 clearStack 防泄漏）
    if (!mNewAttached)
    {
        delete mNewLayer;
    }
    else
    {
        delete mOldLayer;
    }
}

void ConvertLayerCommand::refreshUi()
{
    Layer* current = editor()->layers()->findLayerById(mNewLayer->id());
    if (current != nullptr)
    {
        editor()->layers()->setCurrentLayer(current);
    }
    editor()->scrubTo(editor()->currentFrame());
    emit editor()->updateTimeLine();
    editor()->getScribbleArea()->onLayerChanged();
}

void ConvertLayerCommand::undo()
{
    UndoRedoCommand::undo();

    // 摘新挂旧（同 id，任何时刻只有一层在册）
    editor()->object()->takeLayer(mNewLayer->id());
    editor()->object()->insertLayer(mIndex, mOldLayer);
    mNewAttached = false;
    refreshUi();
}

void ConvertLayerCommand::redo()
{
    UndoRedoCommand::redo();

    // 命令入栈时的自动 redo：换壳结果已由调用方应用
    if (isFirstRedo()) { setFirstRedo(false); return; }

    editor()->object()->takeLayer(mOldLayer->id());
    editor()->object()->insertLayer(mIndex, mNewLayer);
    mNewAttached = true;
    refreshUi();
}

BreakInstanceCommand::BreakInstanceCommand(const int layerId,
                                           const int framePos,
                                           const int siblingAnchorPos,
                                           const QString& description,
                                           Editor* editor,
                                           QUndoCommand* parent)
    : UndoRedoCommand(editor, parent)
    , mLayerId(layerId)
    , mFramePos(framePos)
    , mSiblingAnchorPos(siblingAnchorPos)
{
    setText(description);
}

void BreakInstanceCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::undo();

    BitmapImage* frame = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(mFramePos);
    BitmapImage* anchor = (mSiblingAnchorPos >= 0)
            ? static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(mSiblingAnchorPos)
            : nullptr;
    if (frame != nullptr && anchor != nullptr && anchor != frame)
    {
        frame->shareDataFrom(anchor);
    }
    // 找不到锚点（异常状态）：保持独立帧，像素本就一致，最多损失链关系

    editor()->scrubTo(mFramePos);
}

void BreakInstanceCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::redo();

    // 忽略入栈时的自动首次 redo（断链已在命令构造前由调用方完成）
    if (isFirstRedo()) { setFirstRedo(false); return; }

    BitmapImage* frame = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(mFramePos);
    if (frame != nullptr)
    {
        frame->breakInstance();
    }

    editor()->scrubTo(mFramePos);
}
