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
#include "catch.hpp"

#include <QAction>
#include <QIcon>

#include "editor.h"
#include "frameeditgesture.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"

namespace
{

QList<int> keyPositions(const Layer* layer)
{
    QList<int> positions;
    layer->foreachKeyFrame([&](const KeyFrame* key) { positions.prepend(key->pos()); });
    return positions;
}

} // namespace

TEST_CASE("FrameEditGesture click selects and Ctrl-click adds", "[FrameEditGesture]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(3);
    layer->addNewKeyFrameAt(5);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    FrameEditGesture gesture(editor);

    gesture.press(0, 3, FrameHit::Cell, Qt::NoModifier);
    gesture.release();
    REQUIRE(layer->getSelectedFramesByPos() == QList<int>({ 3 }));

    gesture.press(0, 5, FrameHit::Cell, Qt::ControlModifier);
    gesture.release();
    REQUIRE(layer->getSelectedFramesByPos() == QList<int>({ 3, 5 }));

    SECTION("A click on a selected Drawing without a drag clears the selection")
    {
        gesture.press(0, 5, FrameHit::Cell, Qt::NoModifier);
        gesture.release();
        REQUIRE(layer->getSelectedFramesByPos().isEmpty());
    }
    CHECK_FALSE(undo->isEnabled());
}

TEST_CASE("FrameEditGesture box drag selects across layers", "[FrameEditGesture]")
{
    TestSession session;
    LayerBitmap* bottom = session.object->addNewBitmapLayer();
    LayerBitmap* top = session.object->addNewBitmapLayer();
    bottom->addNewKeyFrameAt(3);
    top->addNewKeyFrameAt(4);
    top->addNewKeyFrameAt(8);
    REQUIRE(session.start());
    FrameEditGesture gesture(session.editor.get());

    gesture.press(0, 2, FrameHit::Cell, Qt::NoModifier); // an empty cell
    gesture.drag(1, 4);
    gesture.drag(1, 6);
    gesture.release();
    REQUIRE(bottom->getSelectedFramesByPos() == QList<int>({ 3 }));
    REQUIRE(top->getSelectedFramesByPos() == QList<int>({ 4 }));
}

TEST_CASE("FrameEditGesture moves the selected Drawings in one undo step", "[FrameEditGesture]")
{
    TestSession session;
    LayerBitmap* bottom = session.object->addNewBitmapLayer();
    LayerBitmap* top = session.object->addNewBitmapLayer();
    bottom->addNewKeyFrameAt(3);
    top->addNewKeyFrameAt(3);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    FrameEditGesture gesture(editor);

    gesture.press(0, 3, FrameHit::Cell, Qt::NoModifier);
    gesture.drag(1, 3);
    gesture.release();
    REQUIRE(top->getSelectedFramesByPos() == QList<int>({ 3 }));

    gesture.press(1, 3, FrameHit::Cell, Qt::NoModifier);
    REQUIRE_FALSE(gesture.isMoving());
    gesture.drag(1, 4);
    REQUIRE(gesture.isMoving());
    gesture.drag(1, 6);
    gesture.release();
    REQUIRE_FALSE(gesture.isMoving());
    REQUIRE(keyPositions(bottom) == QList<int>({ 1, 6 }));
    REQUIRE(keyPositions(top) == QList<int>({ 1, 6 }));

    undo->trigger();
    REQUIRE(keyPositions(bottom) == QList<int>({ 1, 3 }));
    REQUIRE(keyPositions(top) == QList<int>({ 1, 3 }));
    REQUIRE_FALSE(undo->isEnabled());
}
