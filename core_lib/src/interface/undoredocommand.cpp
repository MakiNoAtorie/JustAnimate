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

#include "layermanager.h"
#include "selectionmanager.h"
#include "viewmanager.h"

#include "layersound.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layer.h"

#include "editor.h"
#include "scribblearea.h"
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
    // Not the current frame: one step can remove several keyframes.
    this->redoPosition = undoKeyFrame->pos();

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

    // The layer keeps its last keyframe on removal, so the position can be occupied.
    KeyFrame* restoredKeyFrame = undoKeyFrame->clone();
    if (!layer->addKeyFrame(undoKeyFrame->pos(), restoredKeyFrame)) {
        delete restoredKeyFrame;
    }

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

MoveKeyFramesCommand::MoveKeyFramesCommand(const QList<QPair<int, int>>& moves,
                                         int layerId,
                                         const QString& description,
                                         Editor* editor,
                                         QUndoCommand *parent)
    : UndoRedoCommand(editor, parent), moves(moves), layerId(layerId)
{
    setText(description);
}

void MoveKeyFramesCommand::undo()
{
    UndoRedoCommand::undo();

    QList<QPair<int, int>> movesBack;
    for (const QPair<int, int>& move : qAsConst(moves)) {
        movesBack.append(qMakePair(move.second, move.first));
    }
    apply(movesBack);
}

void MoveKeyFramesCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(moves);
}

void MoveKeyFramesCommand::apply(const QList<QPair<int, int>>& fromTo)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);

    // The frames are not where this step left them: the step cannot be applied.
    if (!layer || !layer->moveKeyFrames(fromTo)) {
        return setObsolete(true);
    }

    if (layer->type() == Layer::CAMERA) {
        editor()->view()->forceUpdateViewTransform();
    }
    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->framesModified();
}
MoveBitmapsCommand::MoveBitmapsCommand(const QList<BitmapOffset>& offsets,
                                       const QString& description,
                                       Editor* editor,
                                       QUndoCommand* parent)
    : UndoRedoCommand(editor, parent), offsets(offsets)
{
    setText(description);
}

void MoveBitmapsCommand::undo()
{
    UndoRedoCommand::undo();
    apply(-1);
}

void MoveBitmapsCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(1);
}

void MoveBitmapsCommand::apply(int direction)
{
    for (const BitmapOffset& move : qAsConst(offsets))
    {
        Layer* layer = editor()->layers()->findLayerById(move.layerId);
        if (layer == nullptr || layer->type() != Layer::BITMAP) { continue; }

        BitmapImage* img = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(move.frame);
        if (img == nullptr) { continue; }

        img->moveTopLeft(img->topLeft() + direction * move.offset);
        emit editor()->frameModified(move.frame);
    }
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

VectorReplaceCommand::VectorReplaceCommand(const VectorImage* undoVector,
                                   const int undoLayerId,
                                   const QString& description,
                                   Editor* editor,
                                   QUndoCommand* parent) : UndoRedoCommand(editor, parent)
{

    this->undoVector = *undoVector;
    this->undoLayerId = undoLayerId;
    Layer* layer = editor->layers()->currentLayer();
    redoLayerId = layer->id();
    redoVector = *static_cast<LayerVector*>(layer)->
            getLastVectorImageAtFrame(editor->currentFrame());

    setText(description);
}

void VectorReplaceCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(undoLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::undo();

    static_cast<LayerVector*>(layer)->replaceKeyFrame(&undoVector);

    editor()->scrubTo(undoVector.pos());
}

void VectorReplaceCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(redoLayerId);
    if (!layer) {
        return setObsolete(true);
    }

    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    static_cast<LayerVector*>(layer)->replaceKeyFrame(&redoVector);

    editor()->scrubTo(redoVector.pos());
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

// A new layer can get the id of a layer that is out of the object, and a save then
// overwrites its frame files. Keep the frames in memory and make the next save write them.
// shortcut: the whole layer stays in memory while it is in the undo stack, copy its files aside when that costs too much.
static void keepFramesInMemory(const Layer* layer)
{
    layer->foreachKeyFrame([](KeyFrame* key)
    {
        key->loadFile();
        key->setModified(true);
    });
}

LayerAddRemoveCommand::LayerAddRemoveCommand(int layerId,
                                             Layer* removedLayer,
                                             int removedIndex,
                                             const QString& description,
                                             Editor* editor,
                                             QUndoCommand* parent)
    : UndoRedoCommand(editor, parent), isAdd(removedLayer == nullptr)
{
    this->layerId = layerId;
    this->layerIndex = removedIndex;
    this->detachedLayer = removedLayer;
    if (removedLayer) { keepFramesInMemory(removedLayer); }

    setText(description);
}

LayerAddRemoveCommand::~LayerAddRemoveCommand()
{
    delete detachedLayer;
}

void LayerAddRemoveCommand::undo()
{
    UndoRedoCommand::undo();

    if (isAdd) { detach(); } else { attach(); }
}

void LayerAddRemoveCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    if (isAdd) { attach(); } else { detach(); }
}

void LayerAddRemoveCommand::attach()
{
    // An edit that is not in the undo stack (e.g. import layers) can have given the id to another layer
    if (detachedLayer == nullptr || editor()->layers()->findLayerById(layerId) != nullptr) {
        return setObsolete(true);
    }

    editor()->layers()->insertLayer(layerIndex, detachedLayer);
    detachedLayer = nullptr;

    editor()->layers()->notifyAnimationLengthChanged();
    editor()->getScribbleArea()->onLayerChanged();
    emit editor()->updateTimeLine();
}

void LayerAddRemoveCommand::detach()
{
    LayerManager* layers = editor()->layers();
    Layer* layer = layers->findLayerById(layerId);
    // The object keeps at least one layer
    if (layer == nullptr || layers->count() < 2) {
        return setObsolete(true);
    }

    layerIndex = 0;
    while (layers->getLayer(layerIndex) != layer) { ++layerIndex; }
    keepFramesInMemory(layer);
    detachedLayer = layers->takeLayer(layerIndex);

    layers->notifyAnimationLengthChanged();
    editor()->getScribbleArea()->onLayerChanged();
    emit editor()->updateTimeLine();
}
