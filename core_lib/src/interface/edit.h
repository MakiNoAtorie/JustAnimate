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

class Layer;
class QString;
class UndoRedoManager;

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

private:
    friend class Editor;
    /** layer nullptr: a step with no snapshot of its own. frame 0: no Drawing snapshot. */
    Edit(UndoRedoManager* manager, const QString& description, Layer* layer, int frame);
    void end(bool commit);

    UndoRedoManager* mManager = nullptr; // nullptr once ended or moved from
    int mStateId = 0;
};

#endif // EDIT_H
