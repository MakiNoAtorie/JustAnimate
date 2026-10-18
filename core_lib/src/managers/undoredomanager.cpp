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

#include "object.h"
#include "editor.h"

#include <QAction>
#include <QDebug>


#include "preferencemanager.h"
#include "layermanager.h"
#include "undoredomanager.h"
#include "selectionmanager.h"

#include "undoredocommand.h"

#include "layerbitmap.h"
#include "layervector.h"


#include "bitmapimage.h"
#include "camera.h"
#include "vectorimage.h"

struct SelectionSaveState {
    QRectF  bounds;
    qreal   rotationAngle = 0.0;
    qreal   scaleX = 0.0;
    qreal   scaleY = 0.0;
    qreal   skewX = 0.0;
    qreal   skewY = 0.0;
    QPointF translation;
    QPointF anchor;
};

/// What an open Edit snapshots at its start.
struct UndoSaveState {
    int layerId = 0;
    int currentFrameIndex = 0;
    Layer::LAYER_TYPE layerType = Layer::UNDEFINED;
    std::unique_ptr<KeyFrame> keyframe;
    SelectionSaveState selectionState = {};

    // The position of each key of the layer at the start. The keys can dangle: compare them, do not dereference them.
    QHash<const KeyFrame*, int> keyframePositions;
    // Camera layer: copies of every camera key.
    std::vector<std::unique_ptr<KeyFrame>> selectedKeyframes;

    // The object's unrecorded-change flag when the state was taken.
    bool unrecordedChange = false;

    // Its step text, the push count when it began, and its layer (nullptr: no snapshot).
    QString description;
    int pushCount = 0;
    Layer* layer = nullptr;
    // Keys the layer removed during the Edit, kept alive so that a record can copy them
    // and so that no new key reuses their address.
    std::vector<std::unique_ptr<KeyFrame>> removedKeys;
    std::vector<std::unique_ptr<KeyFrame>>* outerRemovedKeys = nullptr;
};

UndoRedoManager::UndoRedoManager(Editor* editor) : BaseManager(editor, "UndoRedoManager")
{
    // Undo and redo only move the stack index; the window listens to this signal.
    connect(&mUndoStack, &QUndoStack::indexChanged, this, &UndoRedoManager::didUpdateUndoStack);
    connect(&mUndoStack, &QUndoStack::cleanChanged, this, &UndoRedoManager::didUpdateUndoStack);
    qDebug() << "UndoRedoManager: created";
}

UndoRedoManager::~UndoRedoManager()
{
    // ~QUndoStack clears the stack and signals; the window that listens may already be gone.
    mUndoStack.disconnect(this);
    clearSaveStates();
    qDebug() << "UndoRedoManager: destroyed";
}

bool UndoRedoManager::init()
{
    qDebug() << "UndoRedoManager: init";

    mUndoStack.setUndoLimit(editor()->preference()->get<SETTING::UNDO_REDO_MAX_STEPS>());

    return true;
}

void UndoRedoManager::onSettingChanged(SETTING setting)
{
    if (setting == SETTING::UNDO_REDO_MAX_STEPS) {
        // The stack needs to be cleared in order to change the undo redo limit
        clearStack();
        qDebug() << "updated undo stack limit";
        mUndoStack.setUndoLimit(editor()->preference()->get<SETTING::UNDO_REDO_MAX_STEPS>());
    }
}

Status UndoRedoManager::load(Object* /*o*/)
{
    // A gesture's open Edit points at the old object's layers: drop it with them.
    clearSaveStates();
    mMacroDepth = 0;
    mMacroOpen = false;
    clearStack();
    return Status::OK;
}

Status UndoRedoManager::save(Object* /*o*/)
{
    // The file has not been written yet; see markSaved().
    return Status::OK;
}

void UndoRedoManager::markSaved()
{
    editor()->object()->setModified(false);
    mUndoStack.setClean();
}

void UndoRedoManager::clearSaveStates()
{
    for (UndoSaveState* saveState : mSaveStates) {
        if (saveState) {
            delete saveState;
            saveState = nullptr;
        }
    }
    mSaveStates.clear();
}

bool UndoRedoManager::hasUnsavedChanges() const
{
    if (editor()->object()->isModified()) {
        return true;
    }
    return !mUndoStack.isClean();
}

void UndoRedoManager::pushCommand(QUndoCommand* command)
{
    mPushCount++;
    if (mMacroDepth > 0)
    {
        // Commands skip their first redo, so the push can wait for the step's end.
        if (!mMacroOpen && !mPendingCommand)
        {
            mPendingCommand.reset(command);
            return;
        }
        if (!mMacroOpen)
        {
            mUndoStack.beginMacro(mMacroDescription);
            mUndoStack.push(mPendingCommand.release());
            mMacroOpen = true;
        }
        mUndoStack.push(command);
        return; // endMacro() reports the one edit
    }
    mUndoStack.push(command);

    emit didUpdateUndoStack();
    emit didRecordEdit();
}

void UndoRedoManager::beginMacro(const QString& description)
{
    if (mMacroDepth++ == 0) { mMacroDescription = description; }
}

void UndoRedoManager::endMacro()
{
    if (--mMacroDepth > 0) { return; }
    if (mPendingCommand)
    {
        mPendingCommand->setText(mMacroDescription);
        mUndoStack.push(mPendingCommand.release());
    }
    else if (mMacroOpen)
    {
        mMacroOpen = false;
        mUndoStack.endMacro();
    }
    else { return; }

    emit didUpdateUndoStack();
    emit didRecordEdit();
}

void UndoRedoManager::addKeyFrame(const UndoSaveState& undoState, const QString& description)
{
    KeyFrameAddCommand* element = new KeyFrameAddCommand(undoState.currentFrameIndex,
                                                           undoState.layerId,
                                                           description,
                                                           editor());
    pushCommand(element);
}

void UndoRedoManager::replaceKeyFrame(const UndoSaveState& undoState, const QString& description)
{
    if (undoState.layerType == Layer::BITMAP) {
        replaceBitmap(undoState, description);
    } else if (undoState.layerType == Layer::VECTOR) {
        replaceVector(undoState, description);
    } else {
        // Implement other cases
    }
    // One record covers every Linked Drawing: undo and redo copy the image again.
    editor()->layers()->syncLinkedDrawings(editor()->layers()->findLayerById(undoState.layerId), undoState.currentFrameIndex);
}

void UndoRedoManager::moveKeyFrames(const UndoSaveState& undoState, const QString& description)
{
    const Layer* layer = editor()->layers()->findLayerById(undoState.layerId);
    if (layer == nullptr) { return; }

    QList<QPair<int, int>> moves;
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        auto it = undoState.keyframePositions.constFind(key);
        if (it != undoState.keyframePositions.constEnd() && it.value() != key->pos()) {
            moves.append(qMakePair(it.value(), key->pos()));
        }
    });
    if (moves.isEmpty()) { return; }

    pushCommand(new MoveKeyFramesCommand(moves, undoState.layerId, description, editor()));
}

void UndoRedoManager::recordBitmapMoves(const QList<BitmapOffset>& offsets, const QString& description)
{
    pushCommand(new MoveBitmapsCommand(offsets, description, editor()));
}

void UndoRedoManager::modifyCameras(const UndoSaveState& undoState, const QString& description)
{
    const Layer* layer = editor()->layers()->findLayerById(undoState.layerId);
    if (layer == nullptr || layer->type() != Layer::CAMERA) { return; }

    std::vector<std::unique_ptr<Camera>> after;
    layer->foreachKeyFrame([&after](KeyFrame* key) { after.emplace_back(static_cast<Camera*>(key->clone())); });
    const std::vector<std::unique_ptr<KeyFrame>>& before = undoState.selectedKeyframes;
    bool changed = before.size() != after.size();
    for (size_t i = 0; !changed && i < after.size(); ++i)
    {
        changed = !(*static_cast<const Camera*>(before[i].get()) == *after[i]);
    }
    if (!changed) { return; }

    pushCommand(new CameraModifyCommand(before, after, undoState.layerId, description, editor()));
}

void UndoRedoManager::replaceBitmap(const UndoSaveState& undoState, const QString& description)
{
    if (undoState.keyframe == nullptr || undoState.layerType != Layer::BITMAP) { return; }
    BitmapReplaceCommand* element = new BitmapReplaceCommand(static_cast<BitmapImage*>(undoState.keyframe.get()),
                                               undoState.layerId,
                                               description,
                                               editor());

    const SelectionSaveState& selectionState = undoState.selectionState;
    new TransformCommand(selectionState.bounds,
                         selectionState.translation,
                         selectionState.rotationAngle,
                         selectionState.scaleX,
                         selectionState.scaleY,
                         selectionState.skewX,
                         selectionState.skewY,
                         selectionState.anchor,
                         true, // roundPixels
                         description,
                         editor(), element);

    pushCommand(element);
}

void UndoRedoManager::replaceVector(const UndoSaveState& undoState, const QString& description)
{
    if (undoState.keyframe == nullptr || undoState.layerType != Layer::VECTOR) { return; }
    VectorReplaceCommand* element = new VectorReplaceCommand(static_cast<VectorImage*>(undoState.keyframe.get()),
                                                 undoState.layerId,
                                                 description,
                                                 editor());

    const SelectionSaveState& selectionState = undoState.selectionState;
    new TransformCommand(selectionState.bounds,
                         selectionState.translation,
                         selectionState.rotationAngle,
                         selectionState.scaleX,
                         selectionState.scaleY,
                         selectionState.skewX,
                         selectionState.skewY,
                         selectionState.anchor,
                         false, // Round pixels
                         description,
                         editor(), element);
    pushCommand(element);
}

void UndoRedoManager::recordLayerAdd(int layerId, const QString& description)
{
    pushCommand(new LayerAddRemoveCommand(layerId, nullptr, 0, description, editor()));
}

void UndoRedoManager::recordLayerRemove(Layer* layer, int index, const QString& description)
{
    pushCommand(new LayerAddRemoveCommand(layer->id(), layer, index, description, editor()));
}

void UndoRedoManager::recordAttribute(AttributeId id, int layerId, int frame, const QVariant& before, const QVariant& after)
{
    pushCommand(new AttributeCommand(id, layerId, frame, before, after, editor()));
}

void UndoRedoManager::recordMarkers(int layerId, const QMap<int, FrameMarker>& before, const QString& description)
{
    pushCommand(new MarkersCommand(layerId, before, object()->data()->markers(layerId), description, editor()));
}

void UndoRedoManager::recordCameraSize(int layerId, QRect before, QRect after, const QString& description)
{
    pushCommand(new CameraSizeCommand(layerId, before, after, description, editor()));
}

void UndoRedoManager::recordKeyFrameOpacities(int layerId, const QMap<int, qreal>& before, const QMap<int, qreal>& after, const QString& description)
{
    pushCommand(new KeyFrameOpacityCommand(layerId, before, after, description, editor()));
}

void UndoRedoManager::recordLayerArrangement(const LayerArrangement& before, const QString& description)
{
    const LayerArrangement after = object()->layerArrangement();
    if (after == before) { return; }
    pushCommand(new LayerArrangementCommand(before, after, description, editor()));
}

void UndoRedoManager::snapshotDrawing(UndoSaveState* undoSaveState, const Layer* layer, int frame) const
{
    if (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR) {
        auto selectMan = editor()->select();
        undoSaveState->selectionState = { selectMan->mySelectionRect(), selectMan->myRotation(),
                                          selectMan->myScaleX(), selectMan->myScaleY(),
                                          selectMan->mySkewX(), selectMan->mySkewY(),
                                          selectMan->myTranslation(), selectMan->currentTransformAnchor() };
    }

    auto keyframe = layer->getKeyFrameWhichCovers(frame);
    if (keyframe == nullptr)
    {
        keyframe = layer->getLastKeyFrameAtPosition(frame);
    }

    if (keyframe != nullptr) {
        undoSaveState->keyframe = std::unique_ptr<KeyFrame>(keyframe->clone());
    }
}

SAVESTATE_ID UndoRedoManager::beginEdit(const QString& description, Layer* layer, int frame)
{
    UndoSaveState* state = new UndoSaveState();
    state->description = description;
    state->pushCount = mPushCount;
    state->unrecordedChange = object()->isModified();
    if (layer != nullptr)
    {
        state->layer = layer;
        state->layerId = layer->id();
        state->layerType = layer->type();
        state->currentFrameIndex = frame;
        layer->foreachKeyFrame([state](KeyFrame* key) { state->keyframePositions.insert(key, key->pos()); });
        if (layer->type() == Layer::CAMERA)
        {
            layer->foreachKeyFrame([state](KeyFrame* key) { state->selectedKeyframes.emplace_back(key->clone()); });
        }
        else if (frame > 0 && layer->isPaintable())
        {
            snapshotDrawing(state, layer, frame);
        }
        state->outerRemovedKeys = layer->removedKeySink();
        layer->setRemovedKeySink(&state->removedKeys);
    }
    beginMacro(description);

    const SAVESTATE_ID saveStateId = mSaveStateId++;
    mSaveStates[saveStateId] = state;
    return saveStateId;
}

void UndoRedoManager::endEdit(SAVESTATE_ID saveStateId, bool commit)
{
    std::unique_ptr<UndoSaveState> state(mSaveStates.take(saveStateId));
    if (state == nullptr) { return; }

    // A step recorded inside this Edit already holds its change.
    const bool records = commit && state->layer != nullptr && mPushCount == state->pushCount;
    if (state->layer != nullptr)
    {
        state->layer->setRemovedKeySink(state->outerRemovedKeys);
        if (state->outerRemovedKeys != nullptr && !records)
        {
            for (std::unique_ptr<KeyFrame>& key : state->removedKeys) { state->outerRemovedKeys->push_back(std::move(key)); }
            state->removedKeys.clear();
        }
    }
    endMacro();
    if (records) { recordEdit(*state); }

    // The edit's signals flagged the object; the undo stack now holds the edit.
    if (mPushCount != state->pushCount) { object()->setModified(state->unrecordedChange); }
}

void UndoRedoManager::recordEdit(const UndoSaveState& state)
{
    const Layer* layer = state.layer;
    const QString& description = state.description;

    QList<QPair<int, int>> moves;
    QList<const KeyFrame*> added;
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        auto it = state.keyframePositions.constFind(key);
        if (it == state.keyframePositions.constEnd()) { added.append(key); }
        else if (it.value() != key->pos()) { moves.append(qMakePair(it.value(), key->pos())); }
    });
    QList<const KeyFrame*> removed;
    for (const std::unique_ptr<KeyFrame>& key : state.removedKeys)
    {
        if (state.keyframePositions.contains(key.get())) { removed.append(key.get()); }
    }

    if (removed.isEmpty() && added.isEmpty())
    {
        if (!moves.isEmpty()) { moveKeyFrames(state, description); }
        else if (layer->type() == Layer::CAMERA) { modifyCameras(state, description); }
        else if (state.currentFrameIndex > 0) { replaceKeyFrame(state, description); }
        return;
    }
    if (removed.isEmpty() && moves.isEmpty() && added.size() == 1 && added.first()->pos() == state.currentFrameIndex)
    {
        addKeyFrame(state, description);
        return;
    }
    if (removed.size() == 1 && moves.isEmpty() && added.isEmpty())
    {
        pushCommand(new KeyFrameRemoveCommand(removed.first(), state.layerId, description, editor()));
        return;
    }

    // Children redo in order and undo in reverse: remove, move, then insert.
    QUndoCommand* step = new QUndoCommand(description);
    for (const KeyFrame* key : removed) { new KeyFrameRemoveCommand(key, state.layerId, description, editor(), step); }
    if (!moves.isEmpty()) { new MoveKeyFramesCommand(moves, state.layerId, description, editor(), step); }
    for (const KeyFrame* key : added) { new KeyFramePasteCommand(key, state.layerId, description, editor(), step); }
    pushCommand(step);
}

QAction* UndoRedoManager::createUndoAction(QObject* parent, const QIcon& icon)
{
    QAction* undoAction = mUndoStack.createUndoAction(parent);
    undoAction->setIcon(icon);
    QObject::disconnect(undoAction, &QAction::triggered, &mUndoStack, &QUndoStack::undo);
    connect(undoAction, &QAction::triggered, this, [this] { replay(&QUndoStack::undo); });
    return undoAction;
}

QAction* UndoRedoManager::createRedoAction(QObject* parent, const QIcon& icon)
{
    QAction* redoAction = mUndoStack.createRedoAction(parent);
    redoAction->setIcon(icon);
    QObject::disconnect(redoAction, &QAction::triggered, &mUndoStack, &QUndoStack::redo);
    connect(redoAction, &QAction::triggered, this, [this] { replay(&QUndoStack::redo); });
    return redoAction;
}

void UndoRedoManager::replay(void (QUndoStack::*step)())
{
    // A replayed edit is on the stack: its frame signals must not flag an unrecorded change.
    const bool unrecordedChange = editor()->object()->isModified();
    (mUndoStack.*step)();
    editor()->object()->setModified(unrecordedChange);
    emit didUpdateUndoStack();
}

void UndoRedoManager::clearStack()
{
    mPendingCommand.reset();
    mUndoStack.clear();
}
