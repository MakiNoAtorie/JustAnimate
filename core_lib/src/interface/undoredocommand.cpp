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
#include "soundclip.h"
#include "soundmanager.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layercamera.h"
#include "layer.h"

#include "editor.h"
#include "scribblearea.h"
#include "undoredocommand.h"

// A copied SoundClip has no player: give a restored clip its own.
static void attachSoundPlayer(Editor* editor, Layer* layer, KeyFrame* key)
{
    if (layer->type() == Layer::SOUND) { editor->sound()->processSound(static_cast<SoundClip*>(key)); }
}

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
    if (layer->addKeyFrame(undoKeyFrame->pos(), restoredKeyFrame)) {
        attachSoundPlayer(editor(), layer, restoredKeyFrame);
    } else {
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

KeyFramePasteCommand::KeyFramePasteCommand(const KeyFrame* pastedKeyFrame,
                                           int layerId,
                                           const QString& description,
                                           Editor* editor,
                                           QUndoCommand* parent)
    : UndoRedoCommand(editor, parent), pastedKeyFrame(pastedKeyFrame->clone()), layerId(layerId)
{
    setText(description);
}

KeyFramePasteCommand::~KeyFramePasteCommand()
{
    delete pastedKeyFrame;
}

void KeyFramePasteCommand::undo()
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return setObsolete(true); }

    UndoRedoCommand::undo();

    layer->removeKeyFrame(pastedKeyFrame->pos());

    emit editor()->frameModified(pastedKeyFrame->pos());
    editor()->layers()->notifyAnimationLengthChanged();
}

void KeyFramePasteCommand::redo()
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return setObsolete(true); }

    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    KeyFrame* key = pastedKeyFrame->clone();
    if (layer->addKeyFrame(pastedKeyFrame->pos(), key)) {
        attachSoundPlayer(editor(), layer, key);
    } else {
        delete key;
    }

    emit editor()->frameModified(pastedKeyFrame->pos());
    editor()->layers()->notifyAnimationLengthChanged();
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
    editor()->layers()->moveSoundMarkers(layer, fromTo);

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
    editor()->layers()->syncLinkedDrawings(layer, undoBitmap.pos());

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
    editor()->layers()->syncLinkedDrawings(layer, redoBitmap.pos());

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
    editor()->layers()->syncLinkedDrawings(layer, undoVector.pos());

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
    editor()->layers()->syncLinkedDrawings(layer, redoVector.pos());

    editor()->scrubTo(redoVector.pos());
}

TransformCommand::TransformCommand(const QRectF& undoSelectionRect,
                                   const QPointF& undoTranslation,
                                   const qreal undoRotationAngle,
                                   const qreal undoScaleX,
                                   const qreal undoScaleY,
                                   const qreal undoSkewX,
                                   const qreal undoSkewY,
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
    this->undoSkewX = undoSkewX;
    this->undoSkewY = undoSkewY;

    auto selectMan = editor->select();
    redoSelectionRect = selectMan->mySelectionRect();
    redoAnchor = selectMan->currentTransformAnchor();
    redoTranslation = selectMan->myTranslation();
    redoRotationAngle = selectMan->myRotation();
    redoScaleX = selectMan->myScaleX();
    redoScaleY = selectMan->myScaleY();
    redoSkewX = selectMan->mySkewX();
    redoSkewY = selectMan->mySkewY();

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
          undoSkewX,
          undoSkewY,
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
          redoSkewX,
          redoSkewY,
          redoAnchor,
          roundPixels);
}

void TransformCommand::apply(const QRectF& selectionRect,
                             const QPointF& translation,
                             const qreal rotationAngle,
                             const qreal scaleX,
                             const qreal scaleY,
                             const qreal skewX,
                             const qreal skewY,
                             const QPointF& selectionAnchor,
                             const bool roundPixels)
{
    auto selectMan = editor()->select();
    selectMan->setSelection(selectionRect, roundPixels);
    selectMan->setTransformAnchor(selectionAnchor);
    selectMan->setTranslation(translation);
    selectMan->setRotation(rotationAngle);
    selectMan->setScale(scaleX, scaleY);
    selectMan->setSkew(skewX, skewY);

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

LayerBlendModeCommand::LayerBlendModeCommand(int layerId, BlendMode oldMode, BlendMode newMode,
                                             const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), oldMode(oldMode), newMode(newMode)
{
    setText(description);
}

void LayerBlendModeCommand::undo()
{
    UndoRedoCommand::undo();
    apply(oldMode);
}

void LayerBlendModeCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(newMode);
}

void LayerBlendModeCommand::apply(BlendMode mode)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return; }
    layer->setBlendMode(mode);
    editor()->layers()->notifyLayerChanged(layer);
}

SceneLengthCommand::SceneLengthCommand(int oldLength, int newLength, const QString& description, Editor* editor)
    : UndoRedoCommand(editor), oldLength(oldLength), newLength(newLength)
{
    setText(description);
}

void SceneLengthCommand::undo()
{
    UndoRedoCommand::undo();
    apply(oldLength);
}

void SceneLengthCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(newLength);
}

void SceneLengthCommand::apply(int length)
{
    editor()->object()->setSceneLength(length);
    editor()->object()->modification();
    editor()->layers()->notifyAnimationLengthChanged();
    emit editor()->updateTimeLine();
}

LayerTimesheetCommand::LayerTimesheetCommand(int layerId, TimesheetColumn oldColumn, TimesheetColumn newColumn,
                                             const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), oldColumn(oldColumn), newColumn(newColumn)
{
    setText(description);
}

void LayerTimesheetCommand::undo()
{
    UndoRedoCommand::undo();
    apply(oldColumn);
}

void LayerTimesheetCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(newColumn);
}

void LayerTimesheetCommand::apply(TimesheetColumn column)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return; }
    layer->setTimesheetColumn(column);
    editor()->layers()->notifyLayerChanged(layer);
}

LayerRoughCommand::LayerRoughCommand(int layerId, bool rough, const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), rough(rough)
{
    setText(description);
}

void LayerRoughCommand::undo()
{
    UndoRedoCommand::undo();
    apply(!rough);
}

void LayerRoughCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(rough);
}

void LayerRoughCommand::apply(bool value)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return; }
    layer->setRough(value);
    editor()->layers()->notifyLayerChanged(layer);
}

LayerStoryboardCommand::LayerStoryboardCommand(int layerId, bool storyboard, const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), storyboard(storyboard)
{
    setText(description);
}

void LayerStoryboardCommand::undo()
{
    UndoRedoCommand::undo();
    apply(!storyboard);
}

void LayerStoryboardCommand::redo()
{
    UndoRedoCommand::redo();
    if (isFirstRedo()) { setFirstRedo(false); return; }
    apply(storyboard);
}

void LayerStoryboardCommand::apply(bool value)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return; }
    layer->setStoryboard(value);
    editor()->layers()->notifyLayerChanged(layer);
}

PanelFieldsCommand::PanelFieldsCommand(int layerId, int frame, const PanelFields& before, const PanelFields& after,
                                       const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), frame(frame), before(before), after(after)
{
    setText(description);
}

void PanelFieldsCommand::undo()
{
    UndoRedoCommand::undo();
    apply(before);
}

void PanelFieldsCommand::redo()
{
    UndoRedoCommand::redo();
    if (isFirstRedo()) { setFirstRedo(false); return; }
    apply(after);
}

void PanelFieldsCommand::apply(const PanelFields& fields)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    KeyFrame* key = layer ? layer->getKeyFrameAt(frame) : nullptr;
    if (key == nullptr) { return; }
    key->setPanelFields(fields);
    editor()->object()->modification();
    emit editor()->frameModified(frame);
}

LayerArrangementCommand::LayerArrangementCommand(const LayerArrangement& before, const LayerArrangement& after,
                                                 const QString& description, Editor* editor)
    : UndoRedoCommand(editor), before(before), after(after)
{
    setText(description);
}

void LayerArrangementCommand::undo()
{
    UndoRedoCommand::undo();
    apply(before);
}

void LayerArrangementCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(after);
}

void LayerArrangementCommand::apply(const LayerArrangement& arrangement)
{
    editor()->layers()->applyLayerArrangement(arrangement);
}

PaperTemplateCommand::PaperTemplateCommand(const PaperTemplate& before, const PaperTemplate& after,
                                           const QString& description, Editor* editor)
    : UndoRedoCommand(editor), before(before), after(after)
{
    setText(description);
}

void PaperTemplateCommand::undo()
{
    UndoRedoCommand::undo();
    apply(before);
}

void PaperTemplateCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(after);
}

void PaperTemplateCommand::apply(const PaperTemplate& paper)
{
    // The Timesheet values are export memory, not undoable edits: keep the current ones.
    const PaperTemplate& current = editor()->object()->data()->paperTemplate();
    PaperTemplate next = paper;
    next.sheetScene = current.sheetScene;
    next.sheetShot = current.sheetShot;
    next.sheetAnimator = current.sheetAnimator;
    editor()->object()->data()->setPaperTemplate(next);
    emit editor()->framesModified();
}

CameraSizeCommand::CameraSizeCommand(int layerId, QRect before, QRect after, const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), before(before), after(after)
{
    setText(description);
}

void CameraSizeCommand::undo()
{
    UndoRedoCommand::undo();
    apply(before);
}

void CameraSizeCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(after);
}

void CameraSizeCommand::apply(QRect viewRect)
{
    auto layer = static_cast<LayerCamera*>(editor()->layers()->findLayerById(layerId));
    if (layer == nullptr) { return; }
    layer->setViewRect(viewRect);
    editor()->object()->modification();
    editor()->view()->forceUpdateViewTransform();
}

KeyFrameOpacityCommand::KeyFrameOpacityCommand(int layerId, const QMap<int, qreal>& before, const QMap<int, qreal>& after,
                                               const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), before(before), after(after)
{
    setText(description);
}

void KeyFrameOpacityCommand::undo()
{
    UndoRedoCommand::undo();
    apply(before);
}

void KeyFrameOpacityCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(after);
}

void KeyFrameOpacityCommand::apply(const QMap<int, qreal>& opacities)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return; }
    for (auto it = opacities.cbegin(); it != opacities.cend(); ++it)
    {
        KeyFrame* key = layer->getKeyFrameAt(it.key());
        if (key == nullptr) { continue; }
        key->setOpacity(it.value());
        layer->markFrameAsDirty(it.key());
    }
    emit editor()->framesModified();
}

MarkersCommand::MarkersCommand(int layerId, const QMap<int, FrameMarker>& before, const QMap<int, FrameMarker>& after,
                               const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId), before(before), after(after)
{
    setText(description);
}

void MarkersCommand::undo()
{
    UndoRedoCommand::undo();
    apply(before);
}

void MarkersCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(after);
}

void MarkersCommand::apply(const QMap<int, FrameMarker>& markers)
{
    editor()->object()->data()->setMarkers(layerId, markers);
    editor()->object()->modification();
    emit editor()->updateTimeLine();
}

CameraModifyCommand::CameraModifyCommand(const std::vector<std::unique_ptr<KeyFrame>>& before,
                                         const std::vector<std::unique_ptr<Camera>>& after,
                                         int layerId, const QString& description, Editor* editor)
    : UndoRedoCommand(editor), layerId(layerId)
{
    for (const auto& key : before) { this->before.emplace_back(static_cast<Camera*>(key->clone())); }
    for (const auto& cam : after) { this->after.emplace_back(cam->clone()); }
    setText(description);
}

void CameraModifyCommand::undo()
{
    UndoRedoCommand::undo();
    apply(before);
}

void CameraModifyCommand::redo()
{
    UndoRedoCommand::redo();

    // Ignore automatic redo when added to undo stack
    if (isFirstRedo()) { setFirstRedo(false); return; }

    apply(after);
}

void CameraModifyCommand::apply(const std::vector<std::unique_ptr<Camera>>& cameras)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    if (layer == nullptr) { return setObsolete(true); }

    for (const auto& saved : cameras)
    {
        Camera* cam = static_cast<Camera*>(layer->getKeyFrameAt(saved->pos()));
        if (cam == nullptr) { continue; }
        cam->assign(*saved);
    }
    editor()->view()->forceUpdateViewTransform();
    emit editor()->frameModified(editor()->currentFrame());
}
