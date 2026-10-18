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

#ifndef UNDOREDOCOMMAND_H
#define UNDOREDOCOMMAND_H

#include <QUndoCommand>
#include <QRectF>

#include "bitmapimage.h"
#include "vectorimage.h"
#include "soundclip.h"
#include "camera.h"
#include "layer.h"
#include "object.h"
#include "undoredomanager.h"

class Editor;
class UndoRedoManager;
class PreferenceManager;
class SoundClip;
class Camera;
class KeyFrame;
class TransformCommand;

class UndoRedoCommand : public QUndoCommand
{
public:
    explicit UndoRedoCommand(Editor* editor, QUndoCommand* parent = nullptr);
    ~UndoRedoCommand() = default;

protected:
    Editor* editor() const { return mEditor; }

    bool isFirstRedo() const { return mIsFirstRedo; }
    void setFirstRedo(const bool state) { mIsFirstRedo = state; }

private:
    Editor* mEditor = nullptr;
    bool mIsFirstRedo = true;
};

class KeyFrameRemoveCommand : public UndoRedoCommand
{
public:
    KeyFrameRemoveCommand(const KeyFrame* undoKeyFrame,
                        int undoLayerId,
                        const QString& description,
                        Editor* editor,
                        QUndoCommand* parent = nullptr
                                               );
    ~KeyFrameRemoveCommand() override;

    void undo() override;
    void redo() override;

private:

    int undoLayerId = 0;
    int redoLayerId = 0;

    KeyFrame* undoKeyFrame = nullptr;
    int redoPosition = 0;
};

class KeyFrameAddCommand : public UndoRedoCommand
{
public:
    KeyFrameAddCommand(int undoPosition,
                        int undoLayerId,
                        const QString& description,
                        Editor* editor,
                        QUndoCommand* parent = nullptr);
    ~KeyFrameAddCommand() = default;

    void undo() override;
    void redo() override;

private:

    int undoLayerId = 0;
    int redoLayerId = 0;

    int undoPosition = 0;
    int redoPosition = 0;
};

class KeyFramePasteCommand : public UndoRedoCommand
{
public:
    KeyFramePasteCommand(const KeyFrame* pastedKeyFrame,
                         int layerId,
                         const QString& description,
                         Editor* editor,
                         QUndoCommand* parent = nullptr);
    ~KeyFramePasteCommand() override;

    void undo() override;
    void redo() override;

private:
    KeyFrame* pastedKeyFrame = nullptr;
    int layerId = 0;
};

class MoveKeyFramesCommand : public UndoRedoCommand
{
public:
    /// `moves` holds one (position before, position after) pair for each keyframe that changed position
    MoveKeyFramesCommand(const QList<QPair<int, int>>& moves,
                         int layerId,
                         const QString& description,
                         Editor* editor,
                         QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(const QList<QPair<int, int>>& fromTo);

    QList<QPair<int, int>> moves;
    int layerId = 0;
};

class MoveBitmapsCommand : public UndoRedoCommand
{
public:
    MoveBitmapsCommand(const QList<BitmapOffset>& offsets,
                       const QString& description,
                       Editor* editor,
                       QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(int direction);

    QList<BitmapOffset> offsets;
};

class BitmapReplaceCommand : public UndoRedoCommand
{

public:
    BitmapReplaceCommand(const BitmapImage* backupBitmap,
                  const int undoLayerId,
                  const QString& description,
                  Editor* editor,
                  QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    int undoLayerId = 0;
    int redoLayerId = 0;

    BitmapImage undoBitmap;
    BitmapImage redoBitmap;
};

class VectorReplaceCommand : public UndoRedoCommand
{
public:
    VectorReplaceCommand(const VectorImage* undoVector,
                     const int undoLayerId,
                     const QString& description,
                     Editor* editor,
                     QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    int undoLayerId = 0;
    int redoLayerId = 0;

    VectorImage undoVector;
    VectorImage redoVector;
};

class TransformCommand : public UndoRedoCommand

{
public:
    TransformCommand(const QRectF& undoSelectionRect,
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
                     QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(const QRectF& selectionRect,
               const QPointF& translation,
               const qreal rotationAngle,
               const qreal scaleX,
               const qreal scaleY,
               const qreal skewX,
               const qreal skewY,
               const QPointF& selectionAnchor,
               const bool roundPixels);

    QRectF undoSelectionRect;
    QRectF redoSelectionRect;

    QPointF undoAnchor;
    QPointF redoAnchor;

    QPointF undoTranslation;
    QPointF redoTranslation;

    qreal undoScaleX;
    qreal undoScaleY;

    qreal redoScaleX;
    qreal redoScaleY;

    qreal undoSkewX;
    qreal undoSkewY;
    qreal redoSkewX;
    qreal redoSkewY;

    qreal undoRotationAngle;
    qreal redoRotationAngle;

    bool roundPixels;
};

/// Undo and redo of a layer add or a layer delete. The command owns the layer while it is out of the object.
class LayerAddRemoveCommand : public UndoRedoCommand
{
public:
    /// @param removedLayer The layer that was taken out of the object at removedIndex, or nullptr if the layer was added
    LayerAddRemoveCommand(int layerId,
                          Layer* removedLayer,
                          int removedIndex,
                          const QString& description,
                          Editor* editor,
                          QUndoCommand* parent = nullptr);
    ~LayerAddRemoveCommand() override;

    void undo() override;
    void redo() override;

private:
    void attach();
    void detach();

    const bool isAdd;
    int layerId = 0;
    int layerIndex = 0;
    Layer* detachedLayer = nullptr;
};

/// One attribute table edit: the row, where it applies, and the value before and after.
class AttributeCommand : public UndoRedoCommand
{
public:
    AttributeCommand(AttributeId id, int layerId, int frame, const QVariant& before, const QVariant& after, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(const QVariant& value);

    AttributeId id;
    int layerId = 0;
    int frame = 0;
    QVariant before;
    QVariant after;
};

class LayerBlendModeCommand : public UndoRedoCommand
{
public:
    LayerBlendModeCommand(int layerId, BlendMode oldMode, BlendMode newMode, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(BlendMode mode);

    int layerId = 0;
    BlendMode oldMode = BlendMode::Normal;
    BlendMode newMode = BlendMode::Normal;
};

class LayerRoughCommand : public UndoRedoCommand
{
public:
    LayerRoughCommand(int layerId, bool rough, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(bool value);

    int layerId = 0;
    bool rough = false;
};

class SceneLengthCommand : public UndoRedoCommand
{
public:
    SceneLengthCommand(int oldLength, int newLength, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(int length);

    int oldLength = 0;
    int newLength = 0;
};

class LayerTimesheetCommand : public UndoRedoCommand
{
public:
    LayerTimesheetCommand(int layerId, TimesheetColumn oldColumn, TimesheetColumn newColumn, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(TimesheetColumn column);

    int layerId = 0;
    TimesheetColumn oldColumn = TimesheetColumn::Default;
    TimesheetColumn newColumn = TimesheetColumn::Default;
};

class LayerStoryboardCommand : public UndoRedoCommand
{
public:
    LayerStoryboardCommand(int layerId, bool storyboard, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(bool value);

    int layerId = 0;
    bool storyboard = false;
};

/// Undo and redo of a layer reorder or a layer group edit.
class LayerArrangementCommand : public UndoRedoCommand
{
public:
    LayerArrangementCommand(const LayerArrangement& before, const LayerArrangement& after, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(const LayerArrangement& arrangement);

    LayerArrangement before;
    LayerArrangement after;
};

/// Undo and redo of a Paper template edit.
class PaperTemplateCommand : public UndoRedoCommand
{
public:
    PaperTemplateCommand(const PaperTemplate& before, const PaperTemplate& after, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(const PaperTemplate& paper);

    PaperTemplate before;
    PaperTemplate after;
};

/// Undo and redo of a change to the markers of one layer id (0: the global markers).
class MarkersCommand : public UndoRedoCommand
{
public:
    MarkersCommand(int layerId, const QMap<int, FrameMarker>& before, const QMap<int, FrameMarker>& after,
                   const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(const QMap<int, FrameMarker>& markers);

    int layerId = 0;
    QMap<int, FrameMarker> before;
    QMap<int, FrameMarker> after;
};

/// Undo and redo of a camera layer size change.
class CameraSizeCommand : public UndoRedoCommand
{
public:
    CameraSizeCommand(int layerId, QRect before, QRect after, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(QRect viewRect);

    int layerId = 0;
    QRect before;
    QRect after;
};

/// Undo and redo of a Drawing opacity change, by key position.
class KeyFrameOpacityCommand : public UndoRedoCommand
{
public:
    KeyFrameOpacityCommand(int layerId, const QMap<int, qreal>& before, const QMap<int, qreal>& after,
                           const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(const QMap<int, qreal>& opacities);

    int layerId = 0;
    QMap<int, qreal> before;
    QMap<int, qreal> after;
};

/// Undo and redo of a change to the camera keys of a camera layer, by key position.
class CameraModifyCommand : public UndoRedoCommand
{
public:
    CameraModifyCommand(const std::vector<std::unique_ptr<KeyFrame>>& before,
                        const std::vector<std::unique_ptr<Camera>>& after,
                        int layerId, const QString& description, Editor* editor);

    void undo() override;
    void redo() override;

private:
    void apply(const std::vector<std::unique_ptr<Camera>>& cameras);

    std::vector<std::unique_ptr<Camera>> before;
    std::vector<std::unique_ptr<Camera>> after;
    int layerId = 0;
};

#endif // UNDOREDOCOMMAND_H
