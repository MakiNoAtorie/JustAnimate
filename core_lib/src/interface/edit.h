/*

JustAnimate - Traditional Animation Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef EDIT_H
#define EDIT_H

#include <QList>
#include <QMap>
#include <QRect>

class Layer;
class QString;
class QVariant;
class UndoRedoManager;
enum class AttributeId;
struct BitmapOffset;
struct FrameMarker;
struct LayerArrangement;

/** One undo step. Get it from Editor::edit before the change, then commit().
 *  It snapshots on creation and records what changed on commit; it chooses the record type.
 *  Leaving scope without commit records nothing of its own.
 *  Edits nest: an Edit made inside another joins its step, so a multi-layer change is
 *  one outer Edit with one Edit per layer. An Edit whose life saw a recorded step records
 *  only those steps: open an inner Edit after the steps it follows. */
class [[nodiscard]] Edit
{
public:
    Edit(Edit&& other) noexcept;
    Edit& operator=(Edit&&) = delete;
    ~Edit();

    void commit();

    // Records the Edit cannot infer from its snapshot. They join this Edit's step and take its description.
    void recordLayerAdd(int layerId);
    /** Takes ownership of layer, so undo can put the same layer back; deletes it when nothing records. */
    void recordLayerRemove(Layer* layer, int index);
    void recordBitmapMoves(const QList<BitmapOffset>& offsets);
    /** `frame` is the key position for a Drawing row, else 0. */
    void recordAttribute(AttributeId id, int layerId, int frame, const QVariant& before, const QVariant& after);
    /** The markers of `layerId` (0: global) changed from `before` to the current set. */
    void recordMarkers(int layerId, const QMap<int, FrameMarker>& before);
    void recordCameraSize(int layerId, QRect before, QRect after);
    void recordKeyFrameOpacities(int layerId, const QMap<int, qreal>& before, const QMap<int, qreal>& after);
    /** Layer order or groups changed from `before` to the current arrangement. */
    void recordLayerArrangement(const LayerArrangement& before);

private:
    friend class Editor;
    /** layer nullptr: a step with no snapshot of its own. frame 0: no Drawing snapshot. */
    Edit(UndoRedoManager* manager, const QString& description, Layer* layer, int frame);
    void end(bool commit);

    UndoRedoManager* mManager = nullptr; // nullptr once ended or moved from
    int mStateId = 0;
};

#endif // EDIT_H
