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
#include <QSettings>
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
#include "pencildef.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "testfixture.h"
#include "undoredomanager.h"
#include "vectorimage.h"

TEST_CASE("UndoRedoManager: undo/redo of Remove frame")
{
    TestSession session;
    Object* object = session.object;
    // A camera layer first, so that layer index and layer id differ.
    object->addNewCameraLayer();
    Layer* bitmapLayer = object->addNewBitmapLayer();
    Layer* vectorLayer = object->addNewVectorLayer();

    session.start();
    Editor* editor = session.editor.get();

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

        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE(layer->keyExists(5));
        REQUIRE(layer->keyExists(1));

        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE_FALSE(layer->keyExists(5));
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
}

TEST_CASE("UndoRedoManager: unsaved changes survive a failed save")
{
    TestSession session;
    Object* object = session.object;
    object->addNewBitmapLayer();

    session.start();
    Editor* editor = session.editor.get();

    UndoRedoManager* undoRedo = editor->undoRedo();
    undoRedo->record(undoRedo->createState(UndoRedoRecordType::KEYFRAME_MODIFY), "Draw");
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
}

TEST_CASE("UndoRedoManager: frame and layer edits flag the project as unsaved")
{
    TestSession session;
    Object* object = session.object;
    object->addNewBitmapLayer();

    session.start();
    Editor* editor = session.editor.get();
    UndoRedoManager* undoRedo = editor->undoRedo();

    REQUIRE_FALSE(undoRedo->hasUnsavedChanges());

    SECTION("Frames modified")
    {
        emit editor->framesModified();
        REQUIRE(undoRedo->hasUnsavedChanges());
        undoRedo->markSaved();
        REQUIRE_FALSE(undoRedo->hasUnsavedChanges());
    }

    SECTION("Layer add, rename and delete")
    {
        Layer* layer = editor->layers()->createBitmapLayer("New");
        REQUIRE(undoRedo->hasUnsavedChanges());
        undoRedo->markSaved();

        REQUIRE(editor->layers()->renameLayer(layer, "Renamed").ok());
        REQUIRE(undoRedo->hasUnsavedChanges());
        undoRedo->markSaved();

        editor->layers()->deleteLayer(editor->layers()->currentLayerIndex());
        REQUIRE(undoRedo->hasUnsavedChanges());
    }
}

TEST_CASE("Editor: each edit counts once toward autosave")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    Layer* layer = object->addNewBitmapLayer();

    session.start();
    Editor* editor = session.editor.get();
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

    const SettingsGuard guard({ SETTING_AUTO_SAVE, SETTING_AUTO_SAVE_NUMBER });
    editor->preference()->set(SETTING::AUTO_SAVE, true);
    editor->preference()->set(SETTING::AUTO_SAVE_NUMBER, 2);
    editor->resetAutoSaveCounter();

    removeFrame(5);
    REQUIRE(autosaves == 0);
    removeFrame(6);
    REQUIRE(autosaves == 1);
}

TEST_CASE("UndoRedoManager: undo/redo of selection edits")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    LayerBitmap* bitmapLayer = object->addNewBitmapLayer();
    LayerVector* vectorLayer = object->addNewVectorLayer();

    session.start();
    Editor* editor = session.editor.get();
    ScribbleArea* scribbleArea = session.area.get();


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
}

TEST_CASE("UndoRedoManager: undo/redo of image import")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    LayerBitmap* bitmapLayer = object->addNewBitmapLayer();
    LayerVector* vectorLayer = object->addNewVectorLayer();

    session.start();
    Editor* editor = session.editor.get();


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

    auto opaquePixels = [&]
    {
        const QImage* img = bitmapLayer->getBitmapImageAtFrame(1)->image();
        int count = 0;
        for (int y = 0; y < img->height(); ++y)
            for (int x = 0; x < img->width(); ++x)
                if (qAlpha(img->pixel(x, y)) != 0) { ++count; }
        return count;
    };

    ImportImageConfig config;
    config.positionType = ImportImageConfig::None;

    SECTION("Bitmap image")
    {
        const QString path = dir.filePath("image.png");
        QImage source(4, 4, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::black);
        REQUIRE(source.save(path));

        editor->layers()->setCurrentLayer(bitmapLayer);
        editor->scrubTo(1);
        bitmapLayer->getBitmapImageAtFrame(1)->setPixel(50, 50, qRgba(0, 0, 0, 255));

        checkUndoRedo([&] { REQUIRE(editor->importImage(path, config).ok()); }, opaquePixels, 1, 17);
    }

    SECTION("Animated image")
    {
        // A single-frame 1x1 GIF with one opaque black pixel.
        const QString path = dir.filePath("anim.gif");
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(QByteArray::fromHex(
            "47494638396101000100800000000000ffffff21f90400000000002c00000000010001000002024401003b"));
        file.close();

        editor->layers()->setCurrentLayer(bitmapLayer);
        editor->scrubTo(1);
        bitmapLayer->getBitmapImageAtFrame(1)->setPixel(50, 50, qRgba(0, 0, 0, 255));

        checkUndoRedo([&] { REQUIRE(editor->importAnimatedImage(path, 1, [](int) {}, [] { return false; }).ok()); },
                      opaquePixels, 1, 2);
    }

    SECTION("Vector image")
    {
        const QString path = dir.filePath("image.vec");
        VectorImage source;
        BezierCurve curve({ QPointF(50, 50), QPointF(100, 100) });
        source.addCurve(curve, 1.0);
        REQUIRE(source.write(path, "VEC").ok());

        editor->layers()->setCurrentLayer(vectorLayer);
        editor->scrubTo(1);
        auto hasCurves = [&] { return vectorLayer->getVectorImageAtFrame(1)->isEmpty() ? 0 : 1; };

        checkUndoRedo([&] { REQUIRE(editor->importImage(path, config).ok()); }, hasCurves, 0, 1);
    }
}

TEST_CASE("UndoRedoManager: undo/redo of frame operations")
{
    TestSession session;
    Object* object = session.object;
    // A camera layer first, so that layer index and layer id differ.
    object->addNewCameraLayer();
    LayerBitmap* layer = object->addNewBitmapLayer();
    REQUIRE(object->getLayer(1) == layer);
    REQUIRE(layer->id() != 1);

    session.start();
    Editor* editor = session.editor.get();

    // Frames at 1, 3, 5 and 6. Each has one opaque pixel at (its first position, 0).
    editor->layers()->setCurrentLayer(layer);
    for (int pos : { 1, 3, 5, 6 })
    {
        layer->addNewKeyFrameAt(pos);
        layer->getBitmapImageAtFrame(pos)->setPixel(pos, 0, qRgba(0, 0, 0, 255));
    }

    // Lists "position=mark" from the last frame to the first. The mark is 0 after the pixel moved away.
    auto frames = [&]
    {
        QString list;
        layer->foreachKeyFrame([&](KeyFrame* key)
        {
            int mark = 0;
            for (int x = 1; x <= 6; ++x)
            {
                if (qAlpha(static_cast<BitmapImage*>(key)->pixel(x, 0)) != 0) { mark = x; }
            }
            list += QString("%1=%2 ").arg(key->pos()).arg(mark);
        });
        return list.trimmed().toStdString();
    };
    auto select = [&](std::initializer_list<int> positions)
    {
        layer->deselectAll();
        for (int pos : positions) { layer->setFrameSelected(pos, true); }
    };

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    // Runs `edit`, then checks that the frames read as at the start after undo and `after` after redo.
    auto checkUndoRedo = [&](const std::function<void()>& edit, const std::string& after)
    {
        const std::string before = "6=6 5=5 3=3 1=1";
        REQUIRE(frames() == before);
        edit();
        REQUIRE(frames() == after);
        // The user selects another frame before the undo.
        select({ 1 });
        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE(frames() == before);
        select({ 3 });
        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(frames() == after);
        undoAction->trigger();
        REQUIRE(frames() == before);
    };
    // Records `edit` as one step of the given type, as the timeline and the frame actions do.
    auto recorded = [&](UndoRedoRecordType type, const std::function<void()>& edit)
    {
        return [&, type, edit]
        {
            const SAVESTATE_ID saveStateId = editor->undoRedo()->createState(type);
            edit();
            editor->undoRedo()->record(saveStateId, "Frame operation");
        };
    };

    SECTION("Remove frame")
    {
        editor->scrubTo(5);
        checkUndoRedo([&] { editor->removeKey(); }, "6=6 3=3 1=1");
    }

    SECTION("Remove the selected frames")
    {
        select({ 3, 6 });
        checkUndoRedo(recorded(UndoRedoRecordType::KEYFRAMES_REMOVE, [&]
        {
            for (int pos : layer->selectedKeyFramesPositions()) { layer->removeKeyFrame(pos); }
        }), "5=5 1=1");
    }

    SECTION("Cut the selected frames")
    {
        select({ 3, 6 });
        checkUndoRedo([&] { editor->copyAndCut(); }, "5=5 1=1");
    }

    SECTION("Paste frames, which pushes the later frames back")
    {
        select({ 1, 3 });
        editor->copy();
        editor->scrubTo(5);
        checkUndoRedo([&] { editor->paste(); }, "8=6 7=3 6=5 5=1 3=3 1=1");
    }

    SECTION("Move frames")
    {
        select({ 5, 6 });
        checkUndoRedo(recorded(UndoRedoRecordType::KEYFRAME_MOVE, [&] { layer->moveSelectedFrames(2); }), "8=6 7=5 3=3 1=1");
    }

    SECTION("Move frames, shortened by the frame in the way at position 1")
    {
        select({ 3 });
        checkUndoRedo(recorded(UndoRedoRecordType::KEYFRAME_MOVE, [&] { layer->moveSelectedFrames(-5); }), "6=6 5=5 2=3 1=1");
    }

    SECTION("Reverse")
    {
        select({ 1, 3, 6 });
        checkUndoRedo(recorded(UndoRedoRecordType::KEYFRAME_MOVE, [&] { layer->reverseOrderOfSelection(); }), "6=1 5=5 3=3 1=6");
    }

    SECTION("Add exposure")
    {
        select({ 3 });
        checkUndoRedo(recorded(UndoRedoRecordType::KEYFRAME_MOVE, [&] { layer->setExposureForSelectedFrames(1); }), "7=6 6=5 3=3 1=1");
    }

    SECTION("Subtract exposure")
    {
        select({ 1 });
        checkUndoRedo(recorded(UndoRedoRecordType::KEYFRAME_MOVE, [&] { layer->setExposureForSelectedFrames(-1); }), "5=6 4=5 2=3 1=1");
    }

    SECTION("Reposition Frames")
    {
        checkUndoRedo([&] { editor->repositionImage(QPoint(0, 20), 3); }, "6=6 5=5 3=0 1=1");
    }
}

TEST_CASE("UndoRedoManager: undo/redo of layer add and delete")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    Layer* cameraLayer = object->getLayer(0);
    LayerBitmap* bitmapLayer = object->addNewBitmapLayer();
    LayerVector* vectorLayer = object->addNewVectorLayer();
    const int bitmapLayerId = bitmapLayer->id();
    const int vectorLayerId = vectorLayer->id();

    session.start();
    Editor* editor = session.editor.get();


    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    LayerManager* layers = editor->layers();

    SECTION("Add layer")
    {
        layers->setCurrentLayer(bitmapLayer);
        const int addedId = layers->createLayer(Layer::CAMERA, "Added")->id();
        REQUIRE(layers->count() == 4);

        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE(layers->count() == 3);
        REQUIRE(layers->findLayerById(addedId) == nullptr);
        REQUIRE(layers->currentLayer() == vectorLayer);
        REQUIRE(layers->getLastCameraLayer() == cameraLayer);

        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(layers->count() == 4);
        REQUIRE(layers->getLayer(3)->id() == addedId);
        REQUIRE(layers->getLayer(3)->name() == "Added");
        REQUIRE(layers->currentLayerIndex() == 3);
    }

    SECTION("Delete a layer below the current layer")
    {
        const QRgb black = qRgba(0, 0, 0, 255);
        REQUIRE(bitmapLayer->addNewKeyFrameAt(5));
        bitmapLayer->getBitmapImageAtFrame(5)->setPixel(10, 10, black);
        layers->setCurrentLayer(vectorLayer);

        REQUIRE(layers->deleteLayer(1) == Status::OK);
        REQUIRE(layers->count() == 2);
        REQUIRE(layers->findLayerById(bitmapLayerId) == nullptr);
        // The current layer stays the same layer when a layer below it goes away.
        REQUIRE(layers->currentLayer() == vectorLayer);

        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE(layers->count() == 3);
        Layer* restored = layers->getLayer(1);
        REQUIRE(restored->id() == bitmapLayerId);
        REQUIRE(restored->type() == Layer::BITMAP);
        REQUIRE(restored->keyFrameCount() == 2);
        REQUIRE(restored->keyExists(1));
        REQUIRE(restored->keyExists(5));
        REQUIRE(static_cast<LayerBitmap*>(restored)->getBitmapImageAtFrame(5)->pixel(10, 10) == black);
        REQUIRE(layers->getLayer(2)->id() == vectorLayerId);
        REQUIRE(layers->currentLayer() == restored);

        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(layers->count() == 2);
        REQUIRE(layers->findLayerById(bitmapLayerId) == nullptr);
        REQUIRE(layers->currentLayer()->id() == vectorLayerId);
    }

    SECTION("Delete the current top layer")
    {
        layers->setCurrentLayer(vectorLayer);
        REQUIRE(layers->deleteLayer(2) == Status::OK);
        REQUIRE(layers->currentLayer() == bitmapLayer);

        undoAction->trigger();
        REQUIRE(layers->count() == 3);
        REQUIRE(layers->getLayer(2)->id() == vectorLayerId);
        REQUIRE(layers->currentLayerIndex() == 2);
    }
}

TEST_CASE("UndoRedoManager: an undone layer delete keeps its frames across saves")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("project.pclx");
    const QRgb black = qRgba(0, 0, 0, 255);
    const QList<int> frames = { 1, 3, 5 };

    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    LayerBitmap* layer = object->addNewBitmapLayer();
    layer->setName("Kept");
    const int layerId = layer->id();

    session.start();
    Editor* editor = session.editor.get();


    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());

    for (int frame : frames)
    {
        if (!layer->keyExists(frame)) { REQUIRE(layer->addNewKeyFrameAt(frame)); }
        layer->getBitmapImageAtFrame(frame)->setPixel(frame, frame, black);
    }

    FileManager fm;
    auto save = [&]
    {
        editor->prepareSave();
        REQUIRE(fm.save(object, path).ok());
    };
    save();

    // A frame that is not in memory exists only as a file in the working folder.
    for (int frame : frames)
    {
        BitmapImage* image = layer->getBitmapImageAtFrame(frame);
        image->unloadFile();
        REQUIRE_FALSE(image->isLoaded());
    }

    REQUIRE(editor->layers()->deleteLayer(1) == Status::OK);
    save();

    // A new layer gets the id of the deleted layer, so its frame files get the same names.
    LayerBitmap* other = editor->layers()->createBitmapLayer("Other");
    REQUIRE(other->id() == layerId);
    // Two pixels wide: a frame of the deleted layer that reads this file gets a second pixel.
    other->getBitmapImageAtFrame(1)->setPixel(40, 40, black);
    other->getBitmapImageAtFrame(1)->setPixel(41, 40, black);
    save();

    undoAction->trigger(); // Add Layer
    undoAction->trigger(); // Delete Layer
    REQUIRE(object->getLayerCount() == 2);
    save();

    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->getLayerCount() == 2);
    Layer* restored = loaded->findLayerByName("Kept");
    REQUIRE(restored != nullptr);
    REQUIRE(restored->type() == Layer::BITMAP);
    REQUIRE(restored->keyFrameCount() == 3);
    for (int frame : frames)
    {
        BitmapImage* image = static_cast<LayerBitmap*>(restored)->getBitmapImageAtFrame(frame);
        REQUIRE(image != nullptr);
        image->loadFile(); // A loaded project reads a frame, and its bounds, on demand
        INFO("frame " << frame);
        REQUIRE(image->pixel(frame, frame) == black);
        REQUIRE(qAlpha(image->pixel(frame + 1, frame)) == 0);
    }
}

TEST_CASE("UndoRedoManager: an old undo system preference is ignored")
{
    // A settings file written before the legacy undo system was removed can hold this key.
    const char* oldKey = "NewUndoRedoOn";
    const SettingsGuard guard({ oldKey });
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(oldKey, false);
    settings.sync();

    TestSession session;
    Object* object = session.object;
    Layer* layer = object->addNewBitmapLayer();

    REQUIRE(session.start());
    Editor* editor = session.editor.get();

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());

    editor->layers()->setCurrentLayer(layer);
    layer->addNewKeyFrameAt(5);
    editor->scrubTo(5);
    editor->removeKey();
    REQUIRE_FALSE(layer->keyExists(5));

    REQUIRE(undoAction->isEnabled());
    undoAction->trigger();
    REQUIRE(layer->keyExists(5));
}

TEST_CASE("UndoRedoManager: one user action is one undo step")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    TestSession s;
    s.object->addNewCameraLayer();
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    s.start();
    Editor* editor = s.editor.get();
    s.area->init(); // reads the empty-frame preference
    editor->layers()->setCurrentLayer(layer);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    int edits = 0;
    QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [&] { edits++; });

    // One undo takes the new frame away again; one redo brings it back.
    auto checkOneStep = [&](int frame, const std::function<void()>& edit)
    {
        editor->scrubTo(frame);
        REQUIRE_FALSE(layer->keyExists(frame));
        edit();
        REQUIRE(layer->keyExists(frame));
        REQUIRE(edits == 1);
        undoAction->trigger();
        REQUIRE_FALSE(layer->keyExists(frame));
        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(layer->keyExists(frame));
    };

    SECTION("Import an image onto an empty frame")
    {
        const QString path = dir.filePath("image.png");
        QImage source(4, 4, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::black);
        REQUIRE(source.save(path));
        ImportImageConfig config;
        config.positionType = ImportImageConfig::None;

        checkOneStep(5, [&] { REQUIRE(editor->importImage(path, config).ok()); });
        REQUIRE(qAlpha(layer->getBitmapImageAtFrame(5)->pixel(0, 0)) == 255);
    }

    SECTION("Paste onto an empty frame")
    {
        const SettingsGuard guard({ SETTING_DRAW_ON_EMPTY_FRAME_ACTION });
        editor->preference()->set(SETTING::DRAW_ON_EMPTY_FRAME_ACTION, CREATE_NEW_KEY);

        layer->getBitmapImageAtFrame(1)->setPixel(10, 10, qRgba(0, 0, 0, 255));
        editor->scrubTo(1);
        editor->select()->setSelection(QRectF(5, 5, 10, 10));
        editor->copy();
        editor->deselectAll();

        checkOneStep(5, [&] { editor->paste(); });
        REQUIRE_FALSE(layer->getBitmapImageAtFrame(5)->bounds().isEmpty());
    }

    SECTION("Add exposure, then a frame, as the Add Frame action does")
    {
        REQUIRE(layer->addNewKeyFrameAt(2));
        editor->scrubTo(1);
        {
            const UndoMacro macro(editor->undoRedo(), "Add frame");
            const SAVESTATE_ID saveStateId = editor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MOVE);
            REQUIRE(layer->insertExposureAt(1));
            editor->undoRedo()->record(saveStateId, "Add exposure");
            REQUIRE(editor->addNewKey() != nullptr);
        }
        REQUIRE(layer->keyFrameCount() == 3);
        REQUIRE(layer->keyExists(3));
        REQUIRE(edits == 1);
        undoAction->trigger();
        REQUIRE(layer->keyFrameCount() == 2);
        REQUIRE_FALSE(layer->keyExists(3));
        redoAction->trigger();
        REQUIRE(layer->keyFrameCount() == 3);
        REQUIRE(layer->keyExists(3));
    }

    SECTION("A macro with no record makes no undo step")
    {
        { const UndoMacro macro(editor->undoRedo(), "Nothing"); }
        REQUIRE(edits == 0);
        REQUIRE_FALSE(undoAction->isEnabled());
    }
}
