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
#ifndef FRAMEEDITGESTURE_H
#define FRAMEEDITGESTURE_H

#include <Qt>

class Editor;

/// What the press hit: no track (Empty), a cell of a track, a Drawing's Exposure edge or a Drawing to link.
enum class FrameHit { Empty, Cell, ExposureEdge, Link };

/** One mouse gesture on the frame cells: click select, box select, frame move, Exposure edge drag or link drag.
 *  Views map pixels to (layer index, frame, hit) and paint; an edit ends in one LayerManager call, one undo step.
 *  ExposureEdge: frame is the last frame of the Exposure. Link: frame holds the Drawing to link. */
class FrameEditGesture
{
public:
    explicit FrameEditGesture(Editor* editor) : mEditor(editor) {}

    void press(int layer, int frame, FrameHit hit, Qt::KeyboardModifiers modifiers);
    void drag(int layer, int frame);
    void release();

    bool isMoving() const { return mMoving; }
    /// The hit of the gesture in progress, Empty when none.
    FrameHit hit() const { return mActive ? mHit : FrameHit::Empty; }
    int startFrame() const { return mStartFrame; }
    int lastFrame() const { return mLastFrame; }

private:
    Editor* mEditor;
    Qt::KeyboardModifiers mModifiers;
    bool mActive = false;
    FrameHit mHit = FrameHit::Empty;
    int mLayer = 0;
    int mStartFrame = 0;
    int mLastFrame = 0;
    bool mCanMove = false;
    bool mMoving = false;
    bool mCanBoxSelect = false;
    bool mSelected = false; // the press or a drag changed the selection
};

#endif // FRAMEEDITGESTURE_H
