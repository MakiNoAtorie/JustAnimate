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

#include <functional>

#include <QAction>
#include <QFile>
#include <QIcon>
#include <QTemporaryDir>

#include "beziercurve.h"
#include "bitmapimage.h"
#include "editor.h"
#include "filemanager.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layermanager.h"
#include "object.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "vectorimage.h"

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

TEST_CASE("Editor: each edit counts once toward autosave")
{
    Object* object = new Object;
    object->init();
    object->addNewCameraLayer();
    Layer* layer = object->addNewBitmapLayer();

    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    editor->init();
    editor->layers()->setCurrentLayer(layer);

    int autosaves = 0;
    QObject::connect(editor, &Editor::needSave, [&] { autosaves++; });

    auto removeFrame = [&](int frame)
    {
        REQUIRE(layer->addNewKeyFrameAt(frame));
        editor->scrubTo(frame);
        editor->removeKey();
        REQUIRE_FALSE(layer->keyExists(frame));
    };

    auto checkEditsCountOnce = [&]
    {
        editor->undoRedo()->init();
        editor->preference()->set(SETTING::AUTO_SAVE, true);
        editor->preference()->set(SETTING::AUTO_SAVE_NUMBER, 2);
        editor->resetAutoSaveCounter();

        removeFrame(5);
        REQUIRE(autosaves == 0);
        removeFrame(6);
        REQUIRE(autosaves == 1);
    };

    SECTION("Legacy undo system")
    {
        editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, false);
        checkEditsCountOnce();
    }

    SECTION("New undo system")
    {
        editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, true);
        checkEditsCountOnce();
    }

    editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, false);
    delete editor;
    delete scribbleArea;
}

TEST_CASE("UndoRedoManager: undo/redo of selection edits")
{
    Object* object = new Object;
    object->init();
    object->addNewCameraLayer();
    LayerBitmap* bitmapLayer = object->addNewBitmapLayer();
    LayerVector* vectorLayer = object->addNewVectorLayer();

    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    scribbleArea->setEditor(editor);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    editor->init();

    editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, true);
    editor->undoRedo()->init();

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    // Runs `edit`, then checks that `state` reads `before` after undo and `after` after redo.
    auto checkUndoRedo = [&](const std::function<void()>& edit, const std::function<int()>& state, int before, int after)
    {
        REQUIRE(state() == before);
        edit();
        REQUIRE(state() == after);
        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE(state() == before);
        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(state() == after);
    };
    // An undo or redo step applies a pending flip and deselects; a flip shows either way.
    auto flipPending = [&] { return editor->select()->somethingSelected() && editor->select()->myScaleX() < 0; };

    SECTION("Bitmap layer")
    {
        editor->layers()->setCurrentLayer(bitmapLayer);
        editor->scrubTo(1);
        const QRgb black = qRgba(0, 0, 0, 255);
        bitmapLayer->getBitmapImageAtFrame(1)->setPixel(10, 10, black);
        bitmapLayer->getBitmapImageAtFrame(1)->setPixel(50, 50, black);

        auto opaquePixels = [&]
        {
            const QImage* img = bitmapLayer->getBitmapImageAtFrame(1)->image();
            int count = 0;
            for (int y = 0; y < img->height(); ++y)
                for (int x = 0; x < img->width(); ++x)
                    if (qAlpha(img->pixel(x, y)) != 0) { ++count; }
            return count;
        };

        checkUndoRedo([&] { editor->clearCurrentFrame(); }, opaquePixels, 2, 0);
        undoAction->trigger();

        editor->select()->setSelection(QRectF(5, 5, 10, 10));
        checkUndoRedo([&] { scribbleArea->deleteSelection(); }, opaquePixels, 2, 1);
        undoAction->trigger();

        editor->select()->setSelection(QRectF(5, 5, 10, 10));
        editor->copy();
        editor->select()->setSelection(QRectF(20, 20, 10, 10));
        checkUndoRedo([&] { editor->paste(); }, opaquePixels, 2, 3);

        // (10, 10) mirrors to the right half of the selection.
        auto flipped = [&] { return flipPending() || qAlpha(bitmapLayer->getBitmapImageAtFrame(1)->pixel(10, 10)) == 0 ? 1 : 0; };
        editor->select()->setSelection(QRectF(0, 0, 40, 40));
        checkUndoRedo([&] { editor->flipSelection(false); }, flipped, 0, 1);
    }

    SECTION("Vector layer")
    {
        editor->layers()->setCurrentLayer(vectorLayer);
        editor->scrubTo(1);
        BezierCurve curve({ QPointF(50, 50), QPointF(100, 100) });
        vectorLayer->getVectorImageAtFrame(1)->addCurve(curve, 1.0);

        auto hasCurves = [&] { return vectorLayer->getVectorImageAtFrame(1)->isEmpty() ? 0 : 1; };

        checkUndoRedo([&] { editor->clearCurrentFrame(); }, hasCurves, 1, 0);
        undoAction->trigger();

        vectorLayer->getVectorImageAtFrame(1)->selectAll();
        editor->select()->setSelection(QRectF(0, 0, 200, 200));
        checkUndoRedo([&] { scribbleArea->deleteSelection(); }, hasCurves, 1, 0);
        undoAction->trigger();

        editor->copy();
        vectorLayer->getVectorImageAtFrame(1)->clear();
        checkUndoRedo([&] { editor->paste(); }, hasCurves, 0, 1);

        // The curve starts at x = 50, which mirrors to x = 150.
        auto flipped = [&] { return flipPending() || vectorLayer->getVectorImageAtFrame(1)->getVertex(0, -1).x() > 100 ? 1 : 0; };
        vectorLayer->getVectorImageAtFrame(1)->selectAll();
        editor->select()->setSelection(QRectF(0, 0, 200, 200));
        checkUndoRedo([&] { editor->flipSelection(false); }, flipped, 0, 1);
    }

    editor->preference()->set(SETTING::NEW_UNDO_REDO_SYSTEM_ON, false);
    delete editor;
    delete scribbleArea;
}
