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
#include <QFile>
#include <QIcon>
#include <QTemporaryDir>

#include "editor.h"
#include "filemanager.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layermanager.h"
#include "object.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "undoredomanager.h"

TEST_CASE("UndoRedoManager legacy: undo/redo of Remove frame")
{
    Object* object = new Object;
    object->init();
    // A camera layer first, so that layer index and layer id differ.
    object->addNewCameraLayer();
    Layer* bitmapLayer = object->addNewBitmapLayer();
    Layer* vectorLayer = object->addNewVectorLayer();

    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    editor->init();

    // Use the legacy undo system, which is the default.
    editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, false);
    editor->undoRedo()->init();

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    auto removeFrameThenUndoRedo = [&](Layer* layer, int layerIndex)
    {
        REQUIRE(object->getLayer(layerIndex) == layer);
        REQUIRE(layerIndex != layer->id());
        editor->layers()->setCurrentLayer(layer);
        layer->addNewKeyFrameAt(5);
        editor->scrubTo(5);

        editor->removeKey();
        REQUIRE_FALSE(layer->keyExists(5));

        editor->undoRedo()->updateUndoAction(undoAction);
        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE(layer->keyExists(5));
        REQUIRE(layer->keyExists(1));

        editor->undoRedo()->updateRedoAction(redoAction);
        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(layer->keyExists(1));
    };

    SECTION("Bitmap layer")
    {
        removeFrameThenUndoRedo(bitmapLayer, 1);
    }

    SECTION("Vector layer")
    {
        removeFrameThenUndoRedo(vectorLayer, 2);
    }

    delete editor;
    delete scribbleArea;
}

TEST_CASE("UndoRedoManager: unsaved changes survive a failed save")
{
    Object* object = new Object;
    object->init();
    object->addNewBitmapLayer();

    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    editor->init();

    // Use the legacy undo system, which is the default.
    editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, false);
    editor->undoRedo()->init();

    UndoRedoManager* undoRedo = editor->undoRedo();
    undoRedo->legacyBackup(0, 1, "Draw");
    REQUIRE(undoRedo->hasUnsavedChanges());

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SECTION("A failed save keeps the project marked as modified")
    {
        // A path whose parent is a regular file can never be written, even as root.
        QFile blocker(dir.filePath("not-a-folder"));
        REQUIRE(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        editor->prepareSave();
        FileManager fm;
        Status st = fm.save(object, dir.filePath("not-a-folder/project.pclx"));
        REQUIRE_FALSE(st.ok());

        REQUIRE(undoRedo->hasUnsavedChanges());
    }

    SECTION("A successful save marks the project as saved")
    {
        editor->prepareSave();
        FileManager fm;
        Status st = fm.save(object, dir.filePath("project.pclx"));
        REQUIRE(st.ok());
        undoRedo->markSaved();

        REQUIRE_FALSE(undoRedo->hasUnsavedChanges());
    }

    delete editor;
    delete scribbleArea;
}
