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
    mActive = hit != FrameHit::Empty && layer >= 0 && layer < layers->count();
    mHit = hit;
    mModifiers = modifiers;
    mLayer = layer;
    mStartFrame = mLastFrame = frame;
    mCanMove = mMoving = mCanBoxSelect = mSelected = false;
    if (!mActive) { return; }
    if (hit != FrameHit::Cell)
    {
        layers->setCurrentLayer(layer);
        return;
    }

    Layer* pressed = layers->getLayer(layer);
    // Switching layers deselects all frames: keep a box selection to drag it.
    if (layers->currentLayerIndex() != layer && !pressed->isFrameSelected(frame))
    {
        layers->setCurrentLayer(layer);
        emit mEditor->selectedFramesChanged();
    }

    if (modifiers == Qt::AltModifier)
    {
        LayerManager::selectAllFramesAfter(pressed, frame);
        mSelected = mCanMove = true;
    }
    else if (!pressed->isFrameSelected(frame))
    {
        if (modifiers == Qt::ControlModifier) { LayerManager::toggleFrameSelected(pressed, frame, true); }
        else if (modifiers == Qt::ShiftModifier) { LayerManager::extendSelectionTo(pressed, frame); }
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
    if (mHit != FrameHit::Cell) { return; }
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
    if (mHit == FrameHit::ExposureEdge)
    {
        layers->changeExposure(mStartFrame, mLastFrame - mStartFrame);
    }
    else if (mHit == FrameHit::Link)
    {
        layers->linkDrawing(mStartFrame, qMax(1, mLastFrame));
    }
    else if (mMoving)
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
        LayerManager::setFrameSelected(pressed, mLastFrame, !wasSelected);
        emit mEditor->selectedFramesChanged();
    }
}
