/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

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

#include "bitmapimage.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "pegbaraligner.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "undoredomanager.h"

TEST_CASE("PegBarAligner: align from a held frame, then undo and redo")
{
    TestSession session;
    Object* object = session.object;
    LayerBitmap* layer = object->addNewBitmapLayer();
    layer->setName("Drawing");

    // Key 1 holds the reference peg mark at (10, 10); key 5 has it at (13, 14).
    const QRgb black = qRgba(0, 0, 0, 255);
    layer->getBitmapImageAtFrame(1)->setPixel(10, 10, black);
    layer->addNewKeyFrameAt(5);
    layer->getBitmapImageAtFrame(5)->setPixel(13, 14, black);

    session.start();
    Editor* editor = session.editor.get();

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(3); // a held frame: key 1 governs it

    Status st = PegBarAligner(editor, QRect(0, 0, 30, 30)).align({ "Drawing" });
    REQUIRE(st.ok());
    REQUIRE(qAlpha(layer->getBitmapImageAtFrame(1)->pixel(10, 10)) == 255);
    REQUIRE(qAlpha(layer->getBitmapImageAtFrame(5)->pixel(10, 10)) == 255);

    REQUIRE(undoAction->isEnabled());
    undoAction->trigger();
    REQUIRE(qAlpha(layer->getBitmapImageAtFrame(5)->pixel(13, 14)) == 255);
    REQUIRE(qAlpha(layer->getBitmapImageAtFrame(5)->pixel(10, 10)) == 0);

    REQUIRE(redoAction->isEnabled());
    redoAction->trigger();
    REQUIRE(qAlpha(layer->getBitmapImageAtFrame(5)->pixel(10, 10)) == 255);
    REQUIRE(qAlpha(layer->getBitmapImageAtFrame(5)->pixel(13, 14)) == 0);
}

TEST_CASE("PegBarAligner moves every Linked Drawing and one undo restores them all")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->setName("Drawing");
    const QRgb black = qRgba(0, 0, 0, 255);
    layer->getBitmapImageAtFrame(1)->setPixel(10, 10, black);
    layer->addNewKeyFrameAt(5);
    layer->getBitmapImageAtFrame(5)->setPixel(13, 14, black);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    editor->layers()->setCurrentLayer(layer);
    REQUIRE(editor->layers()->linkDrawing(5, 7));
    editor->scrubTo(1);

    REQUIRE(PegBarAligner(editor, QRect(0, 0, 30, 30)).align({ "Drawing" }).ok());
    for (int frame : { 5, 7 })
    {
        INFO(frame);
        CHECK(qAlpha(layer->getBitmapImageAtFrame(frame)->pixel(10, 10)) == 255);
        CHECK(qAlpha(layer->getBitmapImageAtFrame(frame)->pixel(13, 14)) == 0);
    }

    undoAction->trigger();
    for (int frame : { 5, 7 })
    {
        INFO(frame);
        CHECK(qAlpha(layer->getBitmapImageAtFrame(frame)->pixel(13, 14)) == 255);
        CHECK(qAlpha(layer->getBitmapImageAtFrame(frame)->pixel(10, 10)) == 0);
    }
}

TEST_CASE("PegBarAligner changes no layer when a later layer is locked")
{
    TestSession session;
    const QRgb black = qRgba(0, 0, 0, 255);
    LayerBitmap* first = session.object->addNewBitmapLayer();
    first->setName("First");
    first->getBitmapImageAtFrame(1)->setPixel(10, 10, black);
    first->addNewKeyFrameAt(5);
    first->getBitmapImageAtFrame(5)->setPixel(13, 14, black);
    LayerBitmap* locked = session.object->addNewBitmapLayer();
    locked->setName("Locked");
    locked->getBitmapImageAtFrame(1)->setPixel(12, 12, black);
    locked->setLockAll(true);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(first);
    editor->scrubTo(1);

    PegBarAligner(editor, QRect(0, 0, 30, 30)).align({ "First", "Locked" });
    CHECK(qAlpha(first->getBitmapImageAtFrame(5)->pixel(13, 14)) == 255);
    CHECK(qAlpha(locked->getBitmapImageAtFrame(1)->pixel(12, 12)) == 255);
}
