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
#include "undoredomanager.h"

TEST_CASE("PegBarAligner: align from a held frame, then undo and redo")
{
    Object* object = new Object;
    object->init();
    LayerBitmap* layer = object->addNewBitmapLayer();
    layer->setName("Drawing");

    // Key 1 holds the reference peg mark at (10, 10); key 5 has it at (13, 14).
    const QRgb black = qRgba(0, 0, 0, 255);
    layer->getBitmapImageAtFrame(1)->setPixel(10, 10, black);
    layer->addNewKeyFrameAt(5);
    layer->getBitmapImageAtFrame(5)->setPixel(13, 14, black);

    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    editor->init();

    editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, true);
    editor->undoRedo()->init();

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

    delete editor;
    delete scribbleArea;
}
