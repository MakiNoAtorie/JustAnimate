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

void Edit::willChange(const QRect& canvasRect)
{
    if (mManager) { mManager->willChange(mStateId, canvasRect); }
}

void Edit::recordLayerAdd(int layerId)
{
    if (mManager) { mManager->recordLayerAdd(layerId, QString()); }
}

void Edit::recordLayerRemove(Layer* layer, int index)
{
    if (mManager) { mManager->recordLayerRemove(layer, index, QString()); }
    else { delete layer; }
}

void Edit::recordBitmapMoves(const QList<BitmapOffset>& offsets)
{
    if (mManager) { mManager->recordBitmapMoves(offsets, QString()); }
}

void Edit::recordAttribute(AttributeId id, int layerId, int frame, const QVariant& before, const QVariant& after)
{
    if (mManager) { mManager->recordAttribute(id, layerId, frame, before, after); }
}

void Edit::recordMarkers(int layerId, const QMap<int, FrameMarker>& before)
{
    if (mManager) { mManager->recordMarkers(layerId, before, QString()); }
}

void Edit::recordCameraSize(int layerId, QRect before, QRect after)
{
    if (mManager) { mManager->recordCameraSize(layerId, before, after, QString()); }
}

void Edit::recordKeyFrameOpacities(int layerId, const QMap<int, qreal>& before, const QMap<int, qreal>& after)
{
    if (mManager) { mManager->recordKeyFrameOpacities(layerId, before, after, QString()); }
}

void Edit::recordLayerArrangement(const LayerArrangement& before)
{
    if (mManager) { mManager->recordLayerArrangement(before, QString()); }
}

void Edit::end(bool commit)
{
    if (mManager == nullptr) { return; }
    UndoRedoManager* manager = mManager;
    mManager = nullptr;
    manager->endEdit(mStateId, commit);
}
