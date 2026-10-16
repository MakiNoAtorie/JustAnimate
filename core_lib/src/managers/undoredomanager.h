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
#ifndef UNDOREDOMANAGER_H
#define UNDOREDOMANAGER_H

#include "basemanager.h"
#include "layer.h"
#include "keyframe.h"

#include "preferencesdef.h"

#include <QUndoStack>
#include <QRectF>
#include <QMap>
#include <QHash>

#include <memory>
#include <vector>

class QAction;
class QUndoCommand;

class BitmapImage;
class VectorImage;
class Camera;
class SoundClip;
class KeyFrame;
class UndoRedoCommand;

using SAVESTATE_ID = int;

/// The undo/redo type which correspond to what is being recorded
enum class UndoRedoRecordType {
    KEYFRAME_MODIFY, // Any modification that involve a keyframe
    KEYFRAME_REMOVE, // Removing a keyframe
    KEYFRAME_ADD, // Adding a keyframe
    KEYFRAME_MOVE, // Any change of keyframe positions on the current layer: move, reverse, exposure
    BITMAPS_MOVE, // Moving bitmap keyframes on the canvas, e.g. peg bar alignment
    KEYFRAMES_REMOVE, // Removing the selected keyframes
    KEYFRAMES_PASTE, // Pasting keyframes, which can push later keyframes back
    // SCRUB_LAYER, // Scrubbing layer
    // SCRUB_KEYFRAME, // Scrubbing keyframe
    INVALID
};

struct SelectionSaveState {

    SelectionSaveState() = default;
    SelectionSaveState(const QRectF& rect,
                       const qreal rotationAngle,
                       const qreal scaleX,
                       const qreal scaleY,
                       const QPointF& translation,
                       const QPointF& anchor)
    {
        this->bounds = rect;
        this->rotationAngle = rotationAngle;
        this->scaleX = scaleX;
        this->scaleY = scaleY;
        this->translation = translation;
        this->anchor = anchor;
    }

    QRectF  bounds;
    qreal   rotationAngle = 0.0;
    qreal   scaleX = 0.0;
    qreal   scaleY = 0.0;
    QPointF translation;
    QPointF anchor;
};

/// Moves the bitmap keyframe at `frame` on layer `layerId` by `offset` canvas pixels
struct BitmapOffset {
    int layerId = 0;
    int frame = 0;
    QPoint offset;
};

/// Use this struct to store user related data that will later be added to the backup
/// This struct is meant to be safely shared and stored temporarily,
/// as such don't store ptrs here...
/// All data stored in here should be based on ZII (zero is initialization) principle
/// Only store what you need.
struct UserSaveState {
    QList<BitmapOffset> bitmapOffsets;
};

/// This is the main undo/redo state structure which is meant to populate
/// whatever states that needs to be stored temporarily.
struct UndoSaveState {
    // Common data
    UndoRedoRecordType recordType = UndoRedoRecordType::INVALID;
    int layerId = 0;
    int currentFrameIndex = 0;
    Layer::LAYER_TYPE layerType = Layer::UNDEFINED;
    std::unique_ptr<KeyFrame> keyframe;
    SelectionSaveState selectionState = {};

    // KEYFRAME_MOVE: the position of each keyframe of the layer before the change.
    // The keys can dangle: compare them, do not dereference them.
    QHash<const KeyFrame*, int> keyframePositions;
    // KEYFRAMES_REMOVE: copies of the selected keyframes
    std::vector<std::unique_ptr<KeyFrame>> selectedKeyframes;

    UserSaveState userState = {};
};

class UndoRedoManager : public BaseManager
{
    Q_OBJECT

public:
    explicit UndoRedoManager(Editor* editor);
    ~UndoRedoManager() override;

    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    /** Records the given save state.
     *  The input save state is cleaned up and set to nullptr after use.
    * @param SaveStateId The state that will be fetched and recorded based on the input SaveStateId.
    * @param description The description that will bound to the undo/redo action.
    */
    void record(SAVESTATE_ID SaveStateId, const QString& description);


    /** Checks whether there are unsaved changes.
     *  @return true if there are unsaved changes, otherwise false */
    bool hasUnsavedChanges() const;

    /** Marks the current state as saved. Call only after the file was written successfully. */
    void markSaved();

    /** Prepares and returns an save state with common data
     * @param layer The layer to record; nullptr records the current layer
     * @return A UndoSaveState struct with common keyframe data */
    SAVESTATE_ID createState(UndoRedoRecordType recordType, const Layer* layer = nullptr);

    /** Adds userState to the saveState found at SaveStateId
     *  If no record is found matching the id, nothing happens.
     *  @param SaveStateId The id used to fetch the saveState
     *  @param userState The data to be inserted onto on the saveState
     */
    void addUserState(SAVESTATE_ID SaveStateId, const UserSaveState& userState);

    /** Records that the layer with the given id was added to the object. */
    void recordLayerAdd(int layerId, const QString& description);

    /** Records that a layer was taken out of the object at index.
     *  Takes ownership of the layer, so that undo can put the same layer back. */
    void recordLayerRemove(Layer* layer, int index, const QString& description);

    /** Records that the blend mode of the layer with the given id changed from oldMode. */
    void recordLayerBlendMode(int layerId, BlendMode oldMode, BlendMode newMode, const QString& description);

    /** Makes the records between beginMacro and endMacro one undo step.
     *  Use UndoMacro. No step is made when nothing is recorded. */
    void beginMacro(const QString& description);
    void endMacro();

    QAction* createUndoAction(QObject* parent, const QIcon& icon);
    QAction* createRedoAction(QObject* parent, const QIcon& icon);

    /** Clears the undo stack */
    void clearStack();

    void onSettingChanged(SETTING setting);

signals:
    void didUpdateUndoStack();
    /** An undo step was recorded: one per user edit, unlike didUpdateUndoStack. */
    void didRecordEdit();

private:

    void replaceKeyFrame(const UndoSaveState& undoState, const QString& description);
    void replaceBitmap(const UndoSaveState& undoState, const QString& description);
    void replaceVector(const UndoSaveState& undoState, const QString& description);

    void addKeyFrame(const UndoSaveState& undoState, const QString& description);
    void removeKeyFrame(const UndoSaveState& undoState, const QString& description);
    void moveKeyFrames(const UndoSaveState& undoState, const QString& description);
    void moveBitmaps(const UndoSaveState& undoState, const QString& description);
    void removeKeyFrames(const UndoSaveState& undoState, const QString& description);
    void pasteKeyFrames(const UndoSaveState& undoState, const QString& description);

    void initCommonKeyFrameState(UndoSaveState* undoSaveState, const Layer* layer) const;

    void pushCommand(QUndoCommand* command);

    void clearState(UndoSaveState*& state);
    void clearSaveStates();

    QUndoStack mUndoStack;
    int mMacroDepth = 0;
    bool mMacroOpen = false; // the stack macro starts at the first record
    QString mMacroDescription;

    QMap<SAVESTATE_ID, UndoSaveState*> mSaveStates;

    SAVESTATE_ID mSaveStateId = 1;
};

/// One user action = one undo step: groups the records made during its life.
class UndoMacro
{
public:
    UndoMacro(UndoRedoManager* manager, const QString& description) : mManager(manager) { mManager->beginMacro(description); }
    ~UndoMacro() { mManager->endMacro(); }
    Q_DISABLE_COPY(UndoMacro)

private:
    UndoRedoManager* mManager;
};

#endif // UNDOREDOMANAGER_H
