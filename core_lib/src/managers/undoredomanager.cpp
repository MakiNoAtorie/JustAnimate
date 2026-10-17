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

    mUndoStack.setUndoLimit(editor()->preference()->getInt(SETTING::UNDO_REDO_MAX_STEPS));

    return true;
}

void UndoRedoManager::onSettingChanged(SETTING setting)
{
    if (setting == SETTING::UNDO_REDO_MAX_STEPS) {
        // The stack needs to be cleared in order to change the undo redo limit
        clearStack();
        qDebug() << "updated undo stack limit";
        mUndoStack.setUndoLimit(editor()->preference()->getInt(SETTING::UNDO_REDO_MAX_STEPS));
    }
}

Status UndoRedoManager::load(Object* /*o*/)
{
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

void UndoRedoManager::record(SAVESTATE_ID saveStateId, const QString& description)
{
    if (!mSaveStates.contains(saveStateId)) {
        return;
    }

    UndoSaveState* saveState = mSaveStates.take(saveStateId);
    if (!saveState) { return; }

    switch (saveState->recordType)
    {
        case UndoRedoRecordType::KEYFRAME_MODIFY: {
            replaceKeyFrame(*saveState, description);
            break;
        }
        case UndoRedoRecordType::KEYFRAME_REMOVE: {
            removeKeyFrame(*saveState, description);
            break;
        }
        case UndoRedoRecordType::KEYFRAME_ADD: {
            addKeyFrame(*saveState, description);
            break;
        }
        case UndoRedoRecordType::KEYFRAME_MOVE: {
            moveKeyFrames(*saveState, description);
            break;
        }
        case UndoRedoRecordType::BITMAPS_MOVE: {
            moveBitmaps(*saveState, description);
            break;
        }
        case UndoRedoRecordType::KEYFRAMES_REMOVE: {
            removeKeyFrames(*saveState, description);
            break;
        }
        case UndoRedoRecordType::KEYFRAMES_PASTE: {
            pasteKeyFrames(*saveState, description);
            break;
        }
        case UndoRedoRecordType::CAMERA_MODIFY: {
            modifyCameras(*saveState, description);
            break;
        }
        default: {
            QString reason("Unhandled case for: ");
            reason.append(description);
            Q_ASSERT_X(false, "UndoRedoManager::record", qPrintable(reason));
            break;
        }
    }

    if (saveState->endsMacro) { endMacro(); }
    // The save state has now been used and should be invalidated so we can't use it again.
    clearState(saveState);
}

void UndoRedoManager::discardState(SAVESTATE_ID saveStateId)
{
    UndoSaveState* saveState = mSaveStates.take(saveStateId);
    if (saveState && saveState->endsMacro) { endMacro(); }
    delete saveState;
}

void UndoRedoManager::clearState(UndoSaveState*& state)
{
    if (state) {
        delete state;
        state = nullptr;
    }
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
    if (mMacroDepth > 0)
    {
        if (!mMacroOpen)
        {
            mUndoStack.beginMacro(mMacroDescription);
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

void UndoRedoManager::endMacroWith(SAVESTATE_ID saveStateId)
{
    UndoSaveState* saveState = mSaveStates.value(saveStateId);
    if (saveState) { saveState->endsMacro = true; }
    else { endMacro(); }
}

void UndoRedoManager::endMacro()
{
    if (--mMacroDepth > 0 || !mMacroOpen) { return; }
    mMacroOpen = false;
    mUndoStack.endMacro();

    emit didUpdateUndoStack();
    emit didRecordEdit();
}

void UndoRedoManager::removeKeyFrame(const UndoSaveState& undoState, const QString& description)
{
    KeyFrameRemoveCommand* element = new KeyFrameRemoveCommand(undoState.keyframe.get(),
                                                           undoState.layerId,
                                                           description,
                                                           editor());
    pushCommand(element);
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

void UndoRedoManager::moveBitmaps(const UndoSaveState& undoState, const QString& description)
{
    pushCommand(new MoveBitmapsCommand(undoState.userState.bitmapOffsets, description, editor()));
}

void UndoRedoManager::removeKeyFrames(const UndoSaveState& undoState, const QString& description)
{
    if (undoState.selectedKeyframes.empty()) { return; }

    QUndoCommand* element = new QUndoCommand(description);
    for (const std::unique_ptr<KeyFrame>& keyframe : undoState.selectedKeyframes) {
        new KeyFrameRemoveCommand(keyframe.get(), undoState.layerId, description, editor(), element);
    }
    pushCommand(element);
}

void UndoRedoManager::pasteKeyFrames(const UndoSaveState& undoState, const QString& description)
{
    const Layer* layer = editor()->layers()->findLayerById(undoState.layerId);
    if (layer == nullptr) { return; }

    QList<QPair<int, int>> moves;
    QList<const KeyFrame*> pasted;
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        auto it = undoState.keyframePositions.constFind(key);
        if (it == undoState.keyframePositions.constEnd()) {
            pasted.append(key);
        } else if (it.value() != key->pos()) {
            moves.append(qMakePair(it.value(), key->pos()));
        }
    });
    if (pasted.isEmpty()) { return; }

    // Children redo in order and undo in reverse: make room first, then insert.
    QUndoCommand* element = new QUndoCommand(description);
    if (!moves.isEmpty()) {
        new MoveKeyFramesCommand(moves, undoState.layerId, description, editor(), element);
    }
    for (const KeyFrame* key : pasted) {
        new KeyFramePasteCommand(key, undoState.layerId, description, editor(), element);
    }
    pushCommand(element);
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

SAVESTATE_ID UndoRedoManager::createState(UndoRedoRecordType recordType, const Layer* layer)
{
    int saveStateId = mSaveStateId;
    UndoSaveState* state = new UndoSaveState();
    state->recordType = recordType;
    initCommonKeyFrameState(state, layer ? layer : editor()->layers()->currentLayer());

    mSaveStates[saveStateId] = state;
    mSaveStateId += 1;

    return saveStateId;
}

void UndoRedoManager::addUserState(SAVESTATE_ID saveStateId, const UserSaveState& userState)
{
    if (!mSaveStates.contains(saveStateId)) { return; }
    mSaveStates[saveStateId]->userState = userState;
}

void UndoRedoManager::recordLayerAdd(int layerId, const QString& description)
{
    pushCommand(new LayerAddRemoveCommand(layerId, nullptr, 0, description, editor()));
}

void UndoRedoManager::recordLayerRemove(Layer* layer, int index, const QString& description)
{
    pushCommand(new LayerAddRemoveCommand(layer->id(), layer, index, description, editor()));
}

void UndoRedoManager::recordLayerBlendMode(int layerId, BlendMode oldMode, BlendMode newMode, const QString& description)
{
    pushCommand(new LayerBlendModeCommand(layerId, oldMode, newMode, description, editor()));
}

void UndoRedoManager::recordLayerRough(int layerId, bool rough, const QString& description)
{
    pushCommand(new LayerRoughCommand(layerId, rough, description, editor()));
}

void UndoRedoManager::recordPaperTemplate(const PaperTemplate& before, const PaperTemplate& after, const QString& description)
{
    pushCommand(new PaperTemplateCommand(before, after, description, editor()));
}

void UndoRedoManager::recordSceneLength(int oldLength, int newLength, const QString& description)
{
    pushCommand(new SceneLengthCommand(oldLength, newLength, description, editor()));
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

void UndoRedoManager::recordLayerTimesheet(int layerId, TimesheetColumn oldColumn, TimesheetColumn newColumn, const QString& description)
{
    pushCommand(new LayerTimesheetCommand(layerId, oldColumn, newColumn, description, editor()));
}

void UndoRedoManager::recordLayerStoryboard(int layerId, bool storyboard, const QString& description)
{
    pushCommand(new LayerStoryboardCommand(layerId, storyboard, description, editor()));
}

void UndoRedoManager::recordPanelFields(int layerId, int frame, const PanelFields& before, const PanelFields& after,
                                        const QString& description)
{
    pushCommand(new PanelFieldsCommand(layerId, frame, before, after, description, editor()));
}

void UndoRedoManager::recordLayerArrangement(const LayerArrangement& before, const QString& description)
{
    const LayerArrangement after = object()->layerArrangement();
    if (after == before) { return; }
    pushCommand(new LayerArrangementCommand(before, after, description, editor()));
}

void UndoRedoManager::initCommonKeyFrameState(UndoSaveState* undoSaveState, const Layer* layer) const
{
    undoSaveState->layerType = layer->type();
    undoSaveState->layerId = layer->id();
    undoSaveState->currentFrameIndex = editor()->currentFrame();

    if (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR) {
        auto selectMan = editor()->select();
        undoSaveState->selectionState = SelectionSaveState(
            selectMan->mySelectionRect(),
            selectMan->myRotation(),
            selectMan->myScaleX(),
            selectMan->myScaleY(),
            selectMan->mySkewX(),
            selectMan->mySkewY(),
            selectMan->myTranslation(),
            selectMan->currentTransformAnchor());
    }

    const int frameIndex = editor()->currentFrame();
    auto keyframe = layer->getKeyFrameWhichCovers(frameIndex);
    if (keyframe == nullptr)
    {
        keyframe = layer->getLastKeyFrameAtPosition(frameIndex);
    }

    if (keyframe != nullptr) {
        undoSaveState->keyframe = std::unique_ptr<KeyFrame>(keyframe->clone());
    }

    if (undoSaveState->recordType == UndoRedoRecordType::KEYFRAME_MOVE ||
        undoSaveState->recordType == UndoRedoRecordType::KEYFRAMES_PASTE) {
        layer->foreachKeyFrame([undoSaveState](KeyFrame* key) {
            undoSaveState->keyframePositions.insert(key, key->pos());
        });
    } else if (undoSaveState->recordType == UndoRedoRecordType::CAMERA_MODIFY && layer->type() == Layer::CAMERA) {
        layer->foreachKeyFrame([undoSaveState](KeyFrame* key) {
            undoSaveState->selectedKeyframes.emplace_back(key->clone());
        });
    } else if (undoSaveState->recordType == UndoRedoRecordType::KEYFRAMES_REMOVE) {
        const QList<int> positions = layer->selectedKeyFramesPositions();
        for (int position : positions) {
            const KeyFrame* selected = layer->getKeyFrameAt(position);
            if (selected != nullptr) {
                undoSaveState->selectedKeyframes.emplace_back(selected->clone());
            }
        }
    }
}

QAction* UndoRedoManager::createUndoAction(QObject* parent, const QIcon& icon)
{
    QAction* undoAction = mUndoStack.createUndoAction(parent);
    undoAction->setIcon(icon);
    return undoAction;
}

QAction* UndoRedoManager::createRedoAction(QObject* parent, const QIcon& icon)
{
    QAction* redoAction = mUndoStack.createRedoAction(parent);
    redoAction->setIcon(icon);
    return redoAction;
}

void UndoRedoManager::clearStack()
{
    mUndoStack.clear();
}
