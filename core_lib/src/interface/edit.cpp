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
#include "edit.h"

#include "undoredomanager.h"

Edit::Edit(UndoRedoManager* manager, const QString& description, Layer* layer, int frame) : mManager(manager)
{
    if (mManager) { mStateId = mManager->beginEdit(description, layer, frame); }
}

Edit::Edit(Edit&& other) noexcept : mManager(other.mManager), mStateId(other.mStateId)
{
    other.mManager = nullptr;
}

Edit::~Edit()
{
    end(false);
}

void Edit::commit()
{
    end(true);
}

void Edit::end(bool commit)
{
    if (mManager == nullptr) { return; }
    UndoRedoManager* manager = mManager;
    mManager = nullptr;
    manager->endEdit(mStateId, commit);
}
