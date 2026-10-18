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
#include <QVariant>

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
struct LayerArrangement;
enum class AttributeId;
struct FrameMarker;
struct UndoSaveState;

using SAVESTATE_ID = int;

/// Moves the bitmap keyframe at `frame` on layer `layerId` by `offset` canvas pixels
struct BitmapOffset {
    int layerId = 0;
    int frame = 0;
    QPoint offset;
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

    /** Checks whether there are unsaved changes.
     *  @return true if there are unsaved changes, otherwise false */
    bool hasUnsavedChanges() const;

    /** Marks the current state as saved. Call only after the file was written successfully. */
    void markSaved();

    /** Number of Edits open now. */
    int pendingStateCount() const { return static_cast<int>(mSaveStates.size()); }

    /** Records that the layer with the given id was added to the object. */
    void recordLayerAdd(int layerId, const QString& description);

    /** Records that a layer was taken out of the object at index.
     *  Takes ownership of the layer, so that undo can put the same layer back. */
    void recordLayerRemove(Layer* layer, int index, const QString& description);

    /** Records that each bitmap key in `offsets` moved by its offset on the canvas. */
    void recordBitmapMoves(const QList<BitmapOffset>& offsets, const QString& description);
    /** Records one attribute table edit. `frame` is the key position for a Drawing row, else 0. */
    void recordAttribute(AttributeId id, int layerId, int frame, const QVariant& before, const QVariant& after);
    /** Records that the markers of `layerId` (0: global) changed from `before` to the current set */
    void recordMarkers(int layerId, const QMap<int, FrameMarker>& before, const QString& description);
    /** Records that the view rect of camera layer `layerId` changed from `before` to `after` */
    void recordCameraSize(int layerId, QRect before, QRect after, const QString& description);
    /** Records that the Drawings of `layerId` changed opacity from `before` to `after`, keyed by position */
    void recordKeyFrameOpacities(int layerId, const QMap<int, qreal>& before, const QMap<int, qreal>& after, const QString& description);
    /** Records a change of layer order or groups from `before` to the current arrangement */
    void recordLayerArrangement(const LayerArrangement& before, const QString& description);

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
    friend class Edit;
    SAVESTATE_ID beginEdit(const QString& description, Layer* layer, int frame);
    void endEdit(SAVESTATE_ID saveStateId, bool commit);
    void recordEdit(const UndoSaveState& state);
    /** Makes the records between beginMacro and endMacro one undo step.
     *  No step is made when nothing is recorded. */
    void beginMacro(const QString& description);
    void endMacro();
    void snapshotDrawing(UndoSaveState* state, const Layer* layer, int frame) const;

    void replaceKeyFrame(const UndoSaveState& undoState, const QString& description);
    void replaceBitmap(const UndoSaveState& undoState, const QString& description);
    void replaceVector(const UndoSaveState& undoState, const QString& description);

    void addKeyFrame(const UndoSaveState& undoState, const QString& description);
    void moveKeyFrames(const UndoSaveState& undoState, const QString& description);
    void modifyCameras(const UndoSaveState& undoState, const QString& description);

    void pushCommand(QUndoCommand* command);
    void replay(void (QUndoStack::*step)());

    void clearSaveStates();

    QUndoStack mUndoStack;
    int mMacroDepth = 0;
    bool mMacroOpen = false; // the stack macro starts at the first record
    QString mMacroDescription;
    // The first command of an open step: a step of one command stays that command.
    std::unique_ptr<QUndoCommand> mPendingCommand;
    int mPushCount = 0;

    QMap<SAVESTATE_ID, UndoSaveState*> mSaveStates;

    SAVESTATE_ID mSaveStateId = 1;
};

#endif // UNDOREDOMANAGER_H
