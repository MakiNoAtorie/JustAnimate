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
#include "frameeditgesture.h"

#include "editor.h"
#include "layer.h"
#include "layermanager.h"

void FrameEditGesture::press(int layer, int frame, FrameHit hit, Qt::KeyboardModifiers modifiers)
{
    LayerManager* layers = mEditor->layers();
    mActive = hit == FrameHit::Cell && layer >= 0 && layer < layers->count();
    mModifiers = modifiers;
    mLayer = layer;
    mStartFrame = mLastFrame = frame;
    mCanMove = mMoving = mCanBoxSelect = mSelected = false;
    if (!mActive) { return; }

    Layer* pressed = layers->getLayer(layer);
    // Switching layers deselects all frames: keep a box selection to drag it.
    if (layers->currentLayerIndex() != layer && !pressed->isFrameSelected(frame))
    {
        layers->setCurrentLayer(layer);
        emit mEditor->selectedFramesChanged();
    }

    if (modifiers == Qt::AltModifier)
    {
        pressed->selectAllFramesAfter(frame);
        mSelected = mCanMove = true;
    }
    else if (!pressed->isFrameSelected(frame))
    {
        if (modifiers == Qt::ControlModifier) { pressed->toggleFrameSelected(frame, true); }
        else if (modifiers == Qt::ShiftModifier) { pressed->extendSelectionTo(frame); }
        else { layers->selectFrames(layer, layer, frame, frame); }
        mSelected = mCanBoxSelect = true;
    }
    else
    {
        mCanMove = true;
        return;
    }
    emit mEditor->selectedFramesChanged();
}

void FrameEditGesture::drag(int layer, int frame)
{
    if (!mActive) { return; }
    mLastFrame = frame;
    if (mCanMove)
    {
        mMoving = true;
    }
    else if (mCanBoxSelect)
    {
        mEditor->layers()->selectFrames(mLayer, layer, mStartFrame, frame);
        emit mEditor->selectedFramesChanged();
    }
}

void FrameEditGesture::release()
{
    if (!mActive) { return; }
    mActive = false;
    LayerManager* layers = mEditor->layers();
    if (mMoving)
    {
        mMoving = false;
        layers->moveSelectedFrames(mLastFrame - mStartFrame);
    }
    else if (!mSelected)
    {
        // A click on a selected frame clears the selection; with Ctrl it deselects only that frame.
        Layer* pressed = layers->getLayer(mLayer);
        const bool wasSelected = pressed->isFrameSelected(mLastFrame);
        if (mModifiers != Qt::ControlModifier) { layers->deselectAllFrames(); }
        pressed->setFrameSelected(mLastFrame, !wasSelected);
        emit mEditor->selectedFramesChanged();
    }
}
