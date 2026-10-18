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
#include <QDoubleSpinBox>
#include <QSlider>
#include <QApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QIcon>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QSettings>
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QTimer>

#include "actioncommands.h"
#include "beziercurve.h"
#include "camera.h"
#include "cameracontextmenu.h"
#include "cameraeasingtype.h"
#include "cameratool.h"
#include "colormanager.h"
#include "bitmapimage.h"
#include "editor.h"
#include "filemanager.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layervector.h"
#include "layersound.h"
#include "layermanager.h"
#include "layeropacitydialog.h"
#include "object.h"
#include "pencildef.h"
#include "preferencemanager.h"
#include "repositionframesdialog.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "soundclip.h"
#include "soundmanager.h"
#include "soundplayer.h"
#include "projectsession.h"
#include "stroketool.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "undoredomanager.h"
#include "vectorimage.h"
#include "viewmanager.h"
#include "pencilsettings.h"

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
    editor->edit(editor->layers()->currentLayer(), editor->currentFrame(), "Draw").commit();
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

TEST_CASE("UndoRedoManager: undo and redo refresh the modified state")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    session.start();
    UndoRedoManager* undoRedo = session.editor->undoRedo();

    QObject parent;
    QAction* undoAction = undoRedo->createUndoAction(&parent, QIcon());
    QAction* redoAction = undoRedo->createRedoAction(&parent, QIcon());

    const auto draw = [&] { session.editor->edit(session.editor->layers()->currentLayer(), 1, "Draw").commit(); };
    draw();
    undoRedo->markSaved();
    draw();
    REQUIRE(undoRedo->hasUnsavedChanges());

    int updated = 0;
    int recorded = 0;
    QObject::connect(undoRedo, &UndoRedoManager::didUpdateUndoStack, &parent, [&] { updated++; });
    QObject::connect(undoRedo, &UndoRedoManager::didRecordEdit, &parent, [&] { recorded++; });

    undoAction->trigger();
    REQUIRE(updated > 0);
    REQUIRE_FALSE(undoRedo->hasUnsavedChanges());

    updated = 0;
    redoAction->trigger();
    REQUIRE(updated > 0);
    REQUIRE(undoRedo->hasUnsavedChanges());
    REQUIRE(recorded == 0);
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
    editor->preference()->set<SETTING::AUTO_SAVE>(true);
    editor->preference()->set<SETTING::AUTO_SAVE_NUMBER>(2);
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
        LayerManager::deselectFrames(layer);
        for (int pos : positions) { LayerManager::setFrameSelected(layer, pos, true); }
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
    // Records `edit` as one step, as the timeline and the frame actions do.
    auto recorded = [&](const std::function<void()>& edit)
    {
        return [&, edit]
        {
            Edit step = editor->edit(layer, "Frame operation");
            edit();
            step.commit();
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
        checkUndoRedo(recorded([&]
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
        checkUndoRedo(recorded([&] { LayerManager::moveSelectedFrames(layer, 2); }), "8=6 7=5 3=3 1=1");
    }

    SECTION("Move frames, shortened by the frame in the way at position 1")
    {
        select({ 3 });
        checkUndoRedo(recorded([&] { LayerManager::moveSelectedFrames(layer, -5); }), "6=6 5=5 2=3 1=1");
    }

    SECTION("Reverse")
    {
        select({ 1, 3, 6 });
        checkUndoRedo(recorded([&] { LayerManager::reverseOrderOfSelection(layer); }), "6=1 5=5 3=3 1=6");
    }

    SECTION("Add exposure")
    {
        select({ 3 });
        checkUndoRedo(recorded([&] { LayerManager::setExposureForSelectedFrames(layer, 1); }), "7=6 6=5 3=3 1=1");
    }

    SECTION("Subtract exposure")
    {
        select({ 1 });
        checkUndoRedo(recorded([&] { LayerManager::setExposureForSelectedFrames(layer, -1); }), "5=6 4=5 2=3 1=1");
    }

    SECTION("Reposition Frames")
    {
        checkUndoRedo([&] { editor->repositionImage(QPoint(0, 20), 3); }, "6=6 5=5 3=0 1=1");
    }

    SECTION("Reposition Frames on several frames is one step")
    {
        select({ 3, 5 });
        editor->scrubTo(3);
        checkUndoRedo([&]
        {
            RepositionFramesDialog dialog(nullptr);
            dialog.setCore(editor);
            dialog.initUI();
            editor->select()->translate(QPointF(0, 20));
            editor->select()->calculateSelectionTransformation();
            dialog.updateDialogText();
            dialog.repositionFrames();
        }, "6=6 5=0 3=0 1=1");
    }
}

TEST_CASE("UndoRedoManager: box-select across layers and move the frames as one step")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    LayerBitmap* bottom = object->addNewBitmapLayer();
    LayerBitmap* top = object->addNewBitmapLayer();
    LayerBitmap* outside = object->addNewBitmapLayer();
    REQUIRE(object->getLayer(1) == bottom);
    REQUIRE(object->getLayer(3) == outside);

    session.start();
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    layers->setCurrentLayer(bottom);

    // Each frame has one opaque pixel at (its first position, 0).
    for (LayerBitmap* layer : { bottom, top, outside })
    {
        for (int pos : { 1, 3, 4, 8 })
        {
            layer->addNewKeyFrameAt(pos);
            layer->getBitmapImageAtFrame(pos)->setPixel(pos, 0, qRgba(0, 0, 0, 255));
        }
    }

    // Lists "position=mark" per layer, layers bottom to top, frames last to first.
    auto frames = [&]
    {
        QString list;
        for (LayerBitmap* layer : { bottom, top, outside })
        {
            layer->foreachKeyFrame([&](KeyFrame* key)
            {
                int mark = 0;
                for (int x = 1; x <= 8; ++x)
                {
                    if (qAlpha(static_cast<BitmapImage*>(key)->pixel(x, 0)) != 0) { mark = x; }
                }
                list += QString("%1=%2 ").arg(key->pos()).arg(mark);
            });
            list += "| ";
        }
        return list.trimmed().toStdString();
    };
    const std::string before = "8=8 4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |";

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    // Drag a box from frame 2 on the top layer to frame 5 on the bottom layer.
    layers->selectFrames(2, 1, 5, 2);
    REQUIRE(bottom->selectedKeyFramesPositions() == QList<int>({ 3, 4 }));
    REQUIRE(top->selectedKeyFramesPositions() == QList<int>({ 3, 4 }));
    REQUIRE_FALSE(outside->hasAnySelectedFrames());

    SECTION("Move")
    {
        const std::string after = "8=8 6=4 5=3 1=1 | 8=8 6=4 5=3 1=1 | 8=8 4=4 3=3 1=1 |";
        REQUIRE(layers->moveSelectedFrames(2));
        REQUIRE(frames() == after);

        undoAction->trigger();
        REQUIRE(frames() == before);
        REQUIRE_FALSE(undoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(frames() == after);
        undoAction->trigger();
        REQUIRE(frames() == before);
    }

    SECTION("A new box replaces the selection on every layer")
    {
        layers->selectFrames(3, 3, 8, 8);
        REQUIRE_FALSE(bottom->hasAnySelectedFrames());
        REQUIRE_FALSE(top->hasAnySelectedFrames());
        REQUIRE(outside->selectedKeyFramesPositions() == QList<int>({ 8 }));
    }

    SECTION("A frame in the way on one layer moves nothing")
    {
        LayerManager::setFrameSelected(top, 8, true);
        REQUIRE_FALSE(layers->moveSelectedFrames(4));
        REQUIRE(frames() == before);
        REQUIRE_FALSE(undoAction->isEnabled());
    }
}

TEST_CASE("UndoRedoManager: ripple insert and delete shift the later frames of the layer")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    LayerBitmap* layer = object->addNewBitmapLayer();
    LayerBitmap* other = object->addNewBitmapLayer();

    session.start();
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    layers->setCurrentLayer(layer);

    // Each frame has one opaque pixel at (its first position, 0).
    for (LayerBitmap* l : { layer, other })
    {
        for (int pos : { 1, 3, 4, 8 })
        {
            l->addNewKeyFrameAt(pos);
            l->getBitmapImageAtFrame(pos)->setPixel(pos, 0, qRgba(0, 0, 0, 255));
        }
    }

    // Lists "position=mark" per layer, frames last to first; mark 0 is a blank frame.
    auto frames = [&]
    {
        QString list;
        for (LayerBitmap* l : { layer, other })
        {
            l->foreachKeyFrame([&](KeyFrame* key)
            {
                int mark = 0;
                for (int x = 1; x <= 8; ++x)
                {
                    if (qAlpha(static_cast<BitmapImage*>(key)->pixel(x, 0)) != 0) { mark = x; }
                }
                list += QString("%1=%2 ").arg(key->pos()).arg(mark);
            });
            list += "| ";
        }
        return list.trimmed().toStdString();
    };
    const std::string before = "8=8 4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |";

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    auto undoRedoUndo = [&](const std::string& after)
    {
        REQUIRE(frames() == after);
        undoAction->trigger();
        REQUIRE(frames() == before);
        REQUIRE_FALSE(undoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(frames() == after);
        undoAction->trigger();
        REQUIRE(frames() == before);
    };

    SECTION("Insert adds a blank frame after the current frame and pushes every later frame")
    {
        editor->scrubTo(3);
        REQUIRE(layers->rippleInsertFrame());
        undoRedoUndo("9=8 5=4 4=0 3=3 1=1 | 8=8 4=4 3=3 1=1 |");
    }

    SECTION("Delete inside a hold shortens the hold by one frame")
    {
        editor->scrubTo(5);
        REQUIRE(layers->rippleRemoveFrame());
        undoRedoUndo("7=8 4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |");
    }

    SECTION("Delete on a keyframe removes the drawing and pulls later frames by one")
    {
        editor->scrubTo(4); // frame 3 now holds 3 to 6
        REQUIRE(layers->rippleRemoveFrame());
        undoRedoUndo("7=8 3=3 1=1 | 8=8 4=4 3=3 1=1 |");
    }

    SECTION("Delete keeps the frame selection on the moved frames")
    {
        LayerManager::setFrameSelected(layers->currentLayer(), 8, true);
        editor->scrubTo(6);
        REQUIRE(layers->rippleRemoveFrame());
        CHECK(layers->currentLayer()->selectedKeyFramesPositions() == QList<int>({ 7 }));
        undoRedoUndo("7=8 4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |");
    }

    SECTION("Delete the last frame")
    {
        editor->scrubTo(8);
        REQUIRE(layers->rippleRemoveFrame());
        undoRedoUndo("4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |");
    }
}

TEST_CASE("UndoRedoManager: hold and clean exposure across layers as one step")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    LayerBitmap* bottom = object->addNewBitmapLayer();
    LayerBitmap* top = object->addNewBitmapLayer();
    LayerBitmap* outside = object->addNewBitmapLayer();

    session.start();
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    layers->setCurrentLayer(bottom);

    // Each frame has one opaque pixel at (its first position, 0); frame 3 of the bottom layer repeats frame 1.
    for (LayerBitmap* layer : { bottom, top, outside })
    {
        for (int pos : { 1, 3, 4, 8 })
        {
            layer->addNewKeyFrameAt(pos);
            const int x = (layer == bottom && pos == 3) ? 1 : pos;
            layer->getBitmapImageAtFrame(pos)->setPixel(x, 0, qRgba(0, 0, 0, 255));
        }
    }

    // Lists "position=mark" per layer, layers bottom to top, frames last to first.
    auto frames = [&]
    {
        QString list;
        for (LayerBitmap* layer : { bottom, top, outside })
        {
            layer->foreachKeyFrame([&](KeyFrame* key)
            {
                int mark = 0;
                for (int x = 1; x <= 8; ++x)
                {
                    if (qAlpha(static_cast<BitmapImage*>(key)->pixel(x, 0)) != 0) { mark = x; }
                }
                list += QString("%1=%2 ").arg(key->pos()).arg(mark);
            });
            list += "| ";
        }
        return list.trimmed().toStdString();
    };
    const std::string before = "8=8 4=4 3=1 1=1 | 8=8 4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |";

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    auto checkUndoRedo = [&](const std::string& after)
    {
        REQUIRE(frames() == after);
        undoAction->trigger();
        REQUIRE(frames() == before);
        REQUIRE_FALSE(undoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(frames() == after);
    };

    layers->selectFrames(1, 2, 1, 4);

    SECTION("2s gives every selected drawing a 2-frame hold")
    {
        layers->setExposureForSelectedFrames(2, true);
        checkUndoRedo("7=8 5=4 3=1 1=1 | 7=8 5=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |");
    }

    SECTION("Add exposure")
    {
        layers->setExposureForSelectedFrames(1, false);
        checkUndoRedo("11=8 6=4 4=1 1=1 | 11=8 6=4 4=3 1=1 | 8=8 4=4 3=3 1=1 |");
    }

    SECTION("Clean exposure removes a drawing that repeats the one before it")
    {
        layers->cleanExposureOfSelectedFrames();
        checkUndoRedo("8=8 4=4 1=1 | 8=8 4=4 3=3 1=1 | 8=8 4=4 3=3 1=1 |");
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
    QSettings& settings = pencilSettings();
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
        editor->preference()->set<SETTING::DRAW_ON_EMPTY_FRAME_ACTION>(CREATE_NEW_KEY);

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
            Edit addFrame = editor->edit("Add frame");
            {
                Edit exposure = editor->edit(layer, "Add exposure");
                REQUIRE(layer->insertExposureAt(1));
                exposure.commit();
            }
            REQUIRE(editor->addNewKey() != nullptr);
            addFrame.commit();
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

    SECTION("An Edit with no record makes no undo step")
    {
        editor->edit("Nothing").commit();
        REQUIRE(edits == 0);
        REQUIRE_FALSE(undoAction->isEnabled());
    }
}

TEST_CASE("UndoRedoManager - moving a sound clip moves the lip-sync markers inside it")
{
    TestSession session;
    LayerSound* sound = session.object->addNewSoundLayer();
    SoundClip* clip = new SoundClip;
    clip->setLength(3);
    REQUIRE(sound->addKeyFrame(2, clip));
    ObjectData* data = session.object->data();
    const int id = sound->id();
    for (int frame : { 1, 2, 4, 5 }) { data->setMarker(frame, { QString::number(frame), Qt::red }, id); }
    data->setMarker(3, { "global", Qt::red });
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(sound);

    // Lists "frame=name" for the sound layer's markers.
    auto markers = [&]
    {
        QString list;
        const auto m = data->markers(id);
        for (auto it = m.constBegin(); it != m.constEnd(); ++it) { list += QString("%1=%2 ").arg(it.key()).arg(it.value().name); }
        return list.trimmed().toStdString();
    };
    const std::string before = "1=1 2=2 4=4 5=5";
    const std::string after = "1=1 5=5 7=2 9=4";

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    LayerManager::setFrameSelected(sound, 2, true);
    REQUIRE(editor->layers()->moveSelectedFrames(5));
    CHECK(markers() == after);
    CHECK(data->markers().keys() == QList<int>{ 3 });
    undoAction->trigger();
    CHECK(markers() == before);
    redoAction->trigger();
    CHECK(markers() == after);
}

TEST_CASE("UndoRedoManager - adding moving and removing a marker are one undo step each")
{
    TestSession session;
    ObjectData* data = session.object->data();
    data->setMarker(2, { "keep", Qt::blue });
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();

    // Lists "frame=name/color" for the global markers.
    auto markers = [&]
    {
        QString list;
        const auto m = data->markers();
        for (auto it = m.constBegin(); it != m.constEnd(); ++it)
        {
            list += QString("%1=%2/%3 ").arg(it.key()).arg(it.value().name, it.value().color.name());
        }
        return list.trimmed().toStdString();
    };

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    const std::string start = "2=keep/#0000ff";
    const std::string added = "2=keep/#0000ff 5=A/#ff0000";
    const std::string moved = "2=keep/#0000ff 8=A/#ff0000";
    const std::string removed = "8=A/#ff0000";

    layers->setMarker(0, 5, { "A", Qt::red });
    CHECK(markers() == added);
    CHECK(undoAction->text().endsWith("Add Marker"));
    REQUIRE(layers->moveMarker(0, 5, 8));
    CHECK(markers() == moved);
    CHECK(undoAction->text().endsWith("Move Marker"));
    layers->removeMarker(0, 2);
    CHECK(markers() == removed);
    CHECK(undoAction->text().endsWith("Remove Marker"));
    CHECK_FALSE(layers->moveMarker(0, 2, 3));

    undoAction->trigger();
    CHECK(markers() == moved);
    undoAction->trigger();
    CHECK(markers() == added);
    undoAction->trigger();
    CHECK(markers() == start);
    redoAction->trigger();
    CHECK(markers() == added);
    redoAction->trigger();
    CHECK(markers() == moved);
    redoAction->trigger();
    CHECK(markers() == removed);
}

namespace
{

void sendTablet(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

void tabletClick(ScribbleArea* area, QPointF pos)
{
    sendTablet(area, QEvent::TabletPress, pos);
    sendTablet(area, QEvent::TabletRelease, pos);
}

} // namespace

TEST_CASE("UndoRedoManager: a bucket click that fills nothing leaves no pending save state")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(3);
    layer->removeKeyFrame(1); // frame 1 is before the first key: a click there fills nothing
    layer->getBitmapImageAtFrame(3)->drawRect(QRectF(0, 0, 100, 100), Qt::NoPen, QBrush(Qt::red),
                                              QPainter::CompositionMode_Source, false);

    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BUCKET);
    editor->color()->setFrontColor(Qt::blue);

    int edits = 0;
    QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [&edits] { ++edits; });
    const QPointF pos = editor->view()->mapCanvasToScreen(QPointF(50, 50));

    editor->scrubTo(1);
    for (int i = 0; i < 5; ++i)
        tabletClick(area, pos);
    CHECK(edits == 0);
    CHECK(editor->undoRedo()->pendingStateCount() == 0);

    editor->scrubTo(3);
    tabletClick(area, pos);
    CHECK(edits == 1);
    CHECK(editor->undoRedo()->pendingStateCount() == 0);
    CHECK(QColor(layer->getBitmapImageAtFrame(3)->pixel(50, 50)) == QColor(Qt::blue));

    QObject parent;
    editor->undoRedo()->createUndoAction(&parent, QIcon())->trigger();
    CHECK(QColor(layer->getBitmapImageAtFrame(3)->pixel(50, 50)) == QColor(Qt::red));
}

TEST_CASE("UndoRedoManager: a bucket drag that fills three regions is one undo step")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(3);
    layer->removeKeyFrame(1);
    BitmapImage* image = layer->getBitmapImageAtFrame(3);
    for (int x : { 10, 60, 110 })
        image->drawRect(QRectF(x, 10, 30, 30), Qt::NoPen, QBrush(Qt::red), QPainter::CompositionMode_Source, false);

    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BUCKET);
    editor->color()->setFrontColor(Qt::blue);

    int edits = 0;
    QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [&edits] { ++edits; });
    auto drag = [&] {
        const QPointF start = editor->view()->mapCanvasToScreen(QPointF(25, 25));
        sendTablet(area, QEvent::TabletPress, start);
        for (int x : { 25, 75, 125 })
            sendTablet(area, QEvent::TabletMove, editor->view()->mapCanvasToScreen(QPointF(x, 25)));
        sendTablet(area, QEvent::TabletRelease, editor->view()->mapCanvasToScreen(QPointF(125, 25)));
    };
    auto filled = [&](QColor color) {
        BitmapImage* img = layer->getBitmapImageAtFrame(3);
        return QColor(img->pixel(25, 25)) == color && QColor(img->pixel(75, 25)) == color
            && QColor(img->pixel(125, 25)) == color;
    };

    editor->scrubTo(1); // before the first key: the drag fills nothing
    drag();
    CHECK(edits == 0);

    editor->scrubTo(3);
    drag();
    CHECK(edits == 1);
    CHECK(filled(Qt::blue));
    CHECK(editor->undoRedo()->pendingStateCount() == 0);

    QObject parent;
    editor->undoRedo()->createUndoAction(&parent, QIcon())->trigger();
    CHECK(filled(Qt::red));
    editor->undoRedo()->createRedoAction(&parent, QIcon())->trigger();
    CHECK(filled(Qt::blue));
}

TEST_CASE("UndoRedoManager: a smudge click on a vector layer before its first key leaves no pending save state")
{
    TestSession session;
    LayerVector* layer = session.object->addNewVectorLayer();
    layer->addNewKeyFrameAt(3);
    layer->removeKeyFrame(1);

    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(SMUDGE);
    editor->scrubTo(1);

    tabletClick(area, editor->view()->mapCanvasToScreen(QPointF(50, 50)));
    CHECK(editor->undoRedo()->pendingStateCount() == 0);
}

TEST_CASE("UndoRedoManager: a stroke whose release came from another device leaves no pending save state")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();

    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL);
    editor->scrubTo(1);
    const QPointF pos = editor->view()->mapCanvasToScreen(QPointF(50, 50));

    // A tablet press, then a mouse release: the tool ignores the release.
    sendTablet(area, QEvent::TabletPress, pos);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, area->mapToGlobal(pos), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(area, &release);
    tabletClick(area, pos);
    CHECK(editor->undoRedo()->pendingStateCount() == 0);
}

TEST_CASE("UndoRedoManager: undo/redo of layer swap")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    object->addNewBitmapLayer();
    object->addNewVectorLayer();
    object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    auto order = [&]
    {
        QList<int> ids;
        for (int i = 0; i < object->getLayerCount(); ++i) { ids.append(object->getLayer(i)->id()); }
        return ids;
    };

    const QList<int> before = order();
    editor->swapLayers(1, 2);
    const QList<int> after = order();
    REQUIRE(after == QList<int>{ before[0], before[2], before[1], before[3] });

    SECTION("Undo and redo the swap")
    {
        undoAction->trigger();
        CHECK(order() == before);
        redoAction->trigger();
        CHECK(order() == after);
    }

    SECTION("Undo a layer delete after the swap, then the swap")
    {
        REQUIRE(editor->layers()->deleteLayer(1) == Status::OK);
        undoAction->trigger();
        CHECK(order() == after);
        undoAction->trigger();
        CHECK(order() == before);
    }
}

TEST_CASE("UndoRedoManager: a camera drag is one undo step")
{
    TestSession session;
    LayerCamera* layer = session.object->addNewCameraLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    editor->tools()->setCurrentTool(CAMERA);
    Camera* cam = layer->getCameraAtFrame(1);
    const QPointF before = cam->translation();

    int edits = 0;
    QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [&edits] { ++edits; });
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };
    // The hover picks the move mode; a press keeps it.
    QTabletEvent hover(QEvent::TabletMove, QPointingDevice::primaryPointingDevice(), screen(QPointF(0, 0)),
                       area->mapToGlobal(screen(QPointF(0, 0))), 0.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::NoButton, Qt::NoButton);
    QApplication::sendEvent(area, &hover);
    sendTablet(area, QEvent::TabletPress, screen(QPointF(0, 0)));
    for (int i = 1; i <= 4; ++i) { sendTablet(area, QEvent::TabletMove, screen(QPointF(i * 5, i * 2))); }
    sendTablet(area, QEvent::TabletRelease, screen(QPointF(20, 8)));

    const QPointF after = cam->translation();
    REQUIRE(after != before);
    CHECK(edits == 1);
    CHECK(editor->undoRedo()->pendingStateCount() == 0);

    QObject parent;
    editor->undoRedo()->createUndoAction(&parent, QIcon())->trigger();
    CHECK(layer->getCameraAtFrame(1)->translation() == before);
    editor->undoRedo()->createRedoAction(&parent, QIcon())->trigger();
    CHECK(layer->getCameraAtFrame(1)->translation() == after);

    SECTION("A click without a drag records nothing")
    {
        tabletClick(area, screen(QPointF(200, 200)));
        CHECK(edits == 1);
        CHECK(editor->undoRedo()->pendingStateCount() == 0);
    }
}

TEST_CASE("UndoRedoManager: undo/redo of camera reset actions")
{
    TestSession session;
    LayerCamera* layer = session.object->addNewCameraLayer();
    layer->addNewKeyFrameAt(5);
    layer->getCameraAtFrame(1)->scale(2.0);
    layer->getCameraAtFrame(5)->translate(QPointF(100, 0));
    layer->updatePathControlPointAtFrame(QPointF(50, 50), 1);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    CameraTool* tool = static_cast<CameraTool*>(editor->tools()->getTool(CAMERA));

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    Camera* cam = layer->getCameraAtFrame(1);

    SECTION("Reset scale")
    {
        tool->performAction(CameraTool::RESET_SCALING);
        REQUIRE(cam->scaling() == Approx(1.0));
        undoAction->trigger();
        CHECK(cam->scaling() == Approx(2.0));
        redoAction->trigger();
        CHECK(cam->scaling() == Approx(1.0));
    }

    SECTION("Reset path")
    {
        tool->performAction(CameraTool::RESET_PATH);
        REQUIRE_FALSE(cam->pathControlPointMoved());
        undoAction->trigger();
        CHECK(cam->pathControlPointMoved());
        CHECK(cam->getPathControlPoint() == QPointF(50, 50));
        redoAction->trigger();
        CHECK_FALSE(cam->pathControlPointMoved());
    }
}

TEST_CASE("UndoRedoManager: camera context menu edits are one undo step each")
{
    TestSession session;
    LayerCamera* layer = session.object->addNewCameraLayer();
    layer->addNewKeyFrameAt(5);
    Camera* cam = layer->getCameraAtFrame(1);
    cam->scale(2.0);
    cam->rotate(15);
    cam->translate(QPointF(30, 0));
    cam->setEasingType(CameraEasingType::INSINE);
    layer->getCameraAtFrame(5)->translate(QPointF(100, 40));
    layer->getCameraAtFrame(5)->scale(0.5);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);

    const QString text = GENERATE(as<QString>{}, "Linear", "Reset all", "Reset position", "Reset scale", "Reset rotation",
                                  "Align horizontally to frame 5", "Align vertically to frame 5", "Hold to frame 5");
    INFO(text.toStdString());
    CameraContextMenu menu(editor, 1, layer);
    QAction* action = nullptr;
    for (QAction* a : menu.findChildren<QAction*>())
        if (a->text() == text) { action = a; }
    REQUIRE(action != nullptr);

    int edits = 0;
    QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [&edits] { ++edits; });
    // Some edits change the next key: compare both.
    auto keysEqual = [layer](const std::vector<std::unique_ptr<Camera>>& saved) {
        return *layer->getCameraAtFrame(1) == *saved[0] && *layer->getCameraAtFrame(5) == *saved[1];
    };
    auto snapshot = [layer] {
        std::vector<std::unique_ptr<Camera>> keys;
        keys.emplace_back(layer->getCameraAtFrame(1)->clone());
        keys.emplace_back(layer->getCameraAtFrame(5)->clone());
        return keys;
    };
    const auto before = snapshot();
    action->trigger();
    const auto after = snapshot();
    REQUIRE_FALSE(keysEqual(before));
    CHECK(edits == 1);
    CHECK(editor->undoRedo()->pendingStateCount() == 0);

    QObject parent;
    editor->undoRedo()->createUndoAction(&parent, QIcon())->trigger();
    CHECK(keysEqual(before));
    editor->undoRedo()->createRedoAction(&parent, QIcon())->trigger();
    CHECK(keysEqual(after));
}

namespace
{
// A valid 16-bit mono PCM WAV of silence, so a media player can open it.
QString writeSilentWav(const QTemporaryDir& dir)
{
    const QString path = dir.filePath("silence.wav");
    const quint32 sampleRate = 8000;
    const QByteArray data(sampleRate * 2, '\0');
    QByteArray wav;
    QDataStream s(&wav, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s.writeRawData("RIFF", 4);
    s << quint32(36 + data.size());
    s.writeRawData("WAVEfmt ", 8);
    s << quint32(16) << quint16(1) << quint16(1) << sampleRate << quint32(sampleRate * 2) << quint16(2) << quint16(16);
    s.writeRawData("data", 4);
    s << quint32(data.size());
    s.writeRawData(data.constData(), data.size());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { return {}; }
    file.write(wav);
    return path;
}
}

TEST_CASE("SoundPlayer: destroying a player while it loads does not crash")
{
    QTemporaryDir dir;
    const QString wav = writeSilentWav(dir);
    REQUIRE_FALSE(wav.isEmpty());
    // Undo deletes a clip soon after its player starts to load; the media thread then still reads the source.
    for (int i = 0; i < 300; i++)
    {
        SoundClip clip;
        clip.init(wav);
        auto p = new SoundPlayer;
        p->init(&clip);
        QElapsedTimer t; t.start();
        while (t.elapsed() < i % 7) { QCoreApplication::processEvents(); }
    }
}

TEST_CASE("UndoRedoManager: undo/redo of sound import")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = writeSilentWav(dir);
    REQUIRE_FALSE(wav.isEmpty());

    TestSession session;
    LayerSound* sound = session.object->addNewSoundLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(sound);
    editor->scrubTo(3);

    REQUIRE(editor->sound()->importSoundClip(wav, "Beep").ok());
    REQUIRE(sound->keyExists(3));
    const QString copied = sound->getKeyFrameAt(3)->fileName();

    QObject parent;
    editor->undoRedo()->createUndoAction(&parent, QIcon())->trigger();
    CHECK_FALSE(sound->keyExists(3));
    editor->undoRedo()->createRedoAction(&parent, QIcon())->trigger();
    REQUIRE(sound->keyExists(3));
    SoundClip* clip = static_cast<SoundClip*>(sound->getKeyFrameAt(3));
    CHECK(clip->soundClipName() == "Beep");
    CHECK(clip->fileName() == copied);
    CHECK(clip->player() != nullptr);
}

TEST_CASE("UndoRedoManager: undo of a removed sound frame restores a playable clip")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = writeSilentWav(dir);
    REQUIRE_FALSE(wav.isEmpty());

    TestSession session;
    LayerSound* sound = session.object->addNewSoundLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(sound);
    editor->scrubTo(3);
    REQUIRE(editor->sound()->importSoundClip(wav, "Beep").ok());
    const QString file = sound->getKeyFrameAt(3)->fileName();

    editor->removeKey();
    REQUIRE_FALSE(sound->keyExists(3));
    QObject parent;
    editor->undoRedo()->createUndoAction(&parent, QIcon())->trigger();
    REQUIRE(sound->keyExists(3));
    SoundClip* clip = static_cast<SoundClip*>(sound->getKeyFrameAt(3));
    CHECK(clip->fileName() == file);
    CHECK(clip->player() != nullptr);
}

TEST_CASE("UndoRedoManager: deleting a sound layer stops its playing clip")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString wav = writeSilentWav(dir);
    REQUIRE_FALSE(wav.isEmpty());

    TestSession session;
    session.object->addNewCameraLayer();
    LayerSound* sound = session.object->addNewSoundLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(sound);
    editor->scrubTo(1);
    REQUIRE(editor->sound()->importSoundClip(wav, "Beep").ok());
    SoundClip* clip = static_cast<SoundClip*>(sound->getKeyFrameAt(1));

    clip->play();
    REQUIRE(clip->isPlaying());
    REQUIRE(editor->layers()->deleteLayer(editor->layers()->currentLayerIndex()) == Status::OK);
    CHECK_FALSE(clip->isPlaying()); // the undo stack keeps the layer and its clip alive
}

TEST_CASE("UndoRedoManager: change line color is one undo step")
{
    TestSession session;
    Object* object = session.object;
    object->addNewCameraLayer();
    LayerBitmap* layer = object->addNewBitmapLayer();
    for (int pos : { 1, 3, 6 })
    {
        layer->addNewKeyFrameAt(pos);
        layer->getBitmapImageAtFrame(pos)->setPixel(pos, 0, qRgba(0, 0, 0, 255));
    }

    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(3);
    editor->color()->setFrontColor(QColor(255, 0, 0));
    ActionCommands commands(nullptr);
    commands.setCore(editor);

    // Lists "position=pixel" for every Drawing, last to first.
    auto pixels = [&]
    {
        QString list;
        layer->foreachKeyFrame([&](KeyFrame* key)
        {
            list += QString("%1=%2 ").arg(key->pos()).arg(static_cast<BitmapImage*>(key)->pixel(key->pos(), 0), 8, 16);
        });
        return list.trimmed().toStdString();
    };
    const std::string before = pixels();
    REQUIRE(before == "6=ff000000 3=ff000000 1=ff000000");

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    auto checkUndoRedo = [&](const std::string& after)
    {
        REQUIRE(pixels() == after);
        REQUIRE(editor->currentFrame() == 3);
        undoAction->trigger();
        REQUIRE(pixels() == before);
        REQUIRE_FALSE(undoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(pixels() == after);
    };

    SECTION("Current Drawing")
    {
        commands.changeKeyframeLineColor();
        checkUndoRedo("6=ff000000 3=ffff0000 1=ff000000");
    }

    SECTION("All Drawings on the layer")
    {
        commands.changeallKeyframeLineColor();
        checkUndoRedo("6=ffff0000 3=ffff0000 1=ffff0000");
    }
}

TEST_CASE("Change line color refuses a hidden layer")
{
    TestSession session;
    session.object->addNewCameraLayer();
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->getBitmapImageAtFrame(1)->setPixel(0, 0, qRgba(0, 0, 0, 255));
    layer->setVisible(false);
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->color()->setFrontColor(QColor(255, 0, 0));
    ActionCommands commands(nullptr);
    commands.setCore(editor);

    commands.changeKeyframeLineColor();
    commands.changeallKeyframeLineColor();
    CHECK(layer->getBitmapImageAtFrame(1)->pixel(0, 0) == qRgba(0, 0, 0, 255));
}

TEST_CASE("LayerOpacityDialog: a slider drag or a typed value is one undo step")
{
    TestSession session;
    session.object->addNewCameraLayer();
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    int records = 0;
    QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [&] { ++records; });

    LayerOpacityDialog dialog(nullptr);
    dialog.setCore(editor);
    dialog.initUI();
    QSlider* slider = dialog.findChild<QSlider*>("chooseOpacitySlider");
    QDoubleSpinBox* spinBox = dialog.findChild<QDoubleSpinBox*>("chooseOpacitySpinBox");
    REQUIRE(slider);
    REQUIRE(spinBox);
    KeyFrame* key = layer->getKeyFrameAt(1);
    REQUIRE(key->getOpacity() == Approx(1.0));

    SECTION("Slider drag")
    {
        slider->setSliderDown(true);
        for (int value : { 400, 300, 200 }) { slider->setValue(value); }
        CHECK(records == 0);
        CHECK(key->getOpacity() == Approx(0.4));
        slider->setSliderDown(false);
        CHECK(records == 1);
    }

    SECTION("Typed value")
    {
        spinBox->setValue(40.0);
        CHECK(records == 1);
        CHECK(key->getOpacity() == Approx(0.4));
    }

    undoAction->trigger();
    CHECK(key->getOpacity() == Approx(1.0));
    CHECK_FALSE(undoAction->isEnabled());
    CHECK(slider->value() == slider->maximum()); // the open dialog shows the undone value
    CHECK(spinBox->value() == Approx(100.0));
}

TEST_CASE("ActionCommands: new Bitmap Vector and Sound layers need no dialog and undo in one step")
{
    TestSession session;
    session.object->addNewCameraLayer();
    session.start();
    Editor* editor = session.editor.get();
    ActionCommands commands(nullptr);
    commands.setCore(editor);
    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());

    // A dialog would block the test; close it so the check fails instead.
    QTimer closer;
    QObject::connect(&closer, &QTimer::timeout, [] { if (QWidget* w = QApplication::activeModalWidget()) { w->close(); } });
    closer.start(0);

    const struct { Layer::LAYER_TYPE type; const char* name; Status (ActionCommands::*add)(); } kinds[] = {
        { Layer::BITMAP, "Bitmap Layer", &ActionCommands::addNewBitmapLayer },
        { Layer::VECTOR, "Vector Layer", &ActionCommands::addNewVectorLayer },
        { Layer::SOUND, "Sound Layer", &ActionCommands::addNewSoundLayer },
    };
    for (const auto& kind : kinds)
    {
        const int count = editor->layers()->count();
        (commands.*kind.add)();
        REQUIRE(editor->layers()->count() == count + 1);
        REQUIRE(editor->layers()->currentLayer()->type() == kind.type);
        REQUIRE(editor->layers()->currentLayer()->name() == QString(kind.name));
        (commands.*kind.add)();
        REQUIRE(editor->layers()->currentLayer()->name() == QString(kind.name) + " 2");

        undoAction->trigger();
        undoAction->trigger();
        REQUIRE(editor->layers()->count() == count);
    }
}

TEST_CASE("ActionCommands: a new Camera layer with its size undoes in one step")
{
    TestSession session;
    session.object->addNewCameraLayer();
    session.start();
    Editor* editor = session.editor.get();
    ActionCommands commands(nullptr);
    commands.setCore(editor);
    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    const int count = editor->layers()->count();

    REQUIRE((commands.addNewCameraLayer("Close-up", QSize(320, 240)) == Status::OK));
    REQUIRE(editor->layers()->count() == count + 1);
    auto* camera = static_cast<LayerCamera*>(editor->layers()->currentLayer());
    REQUIRE(camera->type() == Layer::CAMERA);
    CHECK(camera->name() == "Close-up");
    CHECK(camera->getViewRect().size() == QSize(320, 240));

    undoAction->trigger();
    CHECK(editor->layers()->count() == count);
    CHECK_FALSE(undoAction->isEnabled());
}

namespace
{

struct DrawingShot
{
    QRect bounds;
    QImage pixels;
    bool operator==(const DrawingShot& o) const { return bounds == o.bounds && pixels == o.pixels; }
};

DrawingShot shoot(BitmapImage* image)
{
    const QRect bounds = image->bounds();
    return { bounds, image->image()->copy() };
}

void canvasStroke(Editor* editor, ScribbleArea* area, QPointF from, QPointF to)
{
    sendTablet(area, QEvent::TabletPress, editor->view()->mapCanvasToScreen(from));
    for (int i = 1; i <= 10; ++i)
    {
        sendTablet(area, QEvent::TabletMove, editor->view()->mapCanvasToScreen(from + (to - from) * i / 10.0));
    }
    sendTablet(area, QEvent::TabletRelease, editor->view()->mapCanvasToScreen(to));
}

} // namespace

TEST_CASE("UndoRedoManager: undo and redo of strokes restore the same pixels and bounds")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    editor->color()->setFrontColor(Qt::blue);
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    image->drawRect(QRectF(-100, -100, 200, 200), Qt::NoPen, QBrush(Qt::gray), QPainter::CompositionMode_Source, false);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    struct Step { ToolType tool; QPointF from; QPointF to; int width; const char* what; };
    const Step steps[] = {
        { PENCIL, QPointF(-50, 0), QPointF(50, 0), 20, "stroke inside" },
        { PEN, QPointF(50, 30), QPointF(250, 30), 20, "extend" },
        { ERASER, QPointF(-100, -150), QPointF(-100, 150), 60, "auto-crop" },
        { SMUDGE, QPointF(0, -30), QPointF(0, 30), 20, "smudge" },
    };
    std::vector<DrawingShot> shots{ shoot(image) };
    for (const Step& step : steps)
    {
        INFO(step.what);
        editor->tools()->setCurrentTool(step.tool);
        const ToolSettingsGuard guard(editor->tools()->currentTool());
        auto* tool = static_cast<StrokeTool*>(editor->tools()->currentTool());
        tool->set(ToolProperty::Width, step.width);
        if (step.tool == ERASER)
        {
            tool->set(ToolProperty::FeatherEnabled, false);
            tool->set(ToolProperty::PressureEnabled, false);
        }
        canvasStroke(editor, area, step.from, step.to);
        shots.push_back(shoot(image));
        CHECK_FALSE(shots.back() == shots[shots.size() - 2]);
    }
    CHECK(shots[2].bounds.right() > shots[1].bounds.right());
    CHECK(shots[3].bounds.left() > shots[2].bounds.left());

    for (size_t i = shots.size() - 1; i > 0; --i)
    {
        INFO("undo to " << i - 1);
        undoAction->trigger();
        CHECK(shoot(image) == shots[i - 1]);
    }
    for (size_t i = 1; i < shots.size(); ++i)
    {
        INFO("redo to " << i);
        redoAction->trigger();
        CHECK(shoot(image) == shots[i]);
    }
}

TEST_CASE("UndoRedoManager: an Edit told of two rects undoes both")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    image->drawRect(QRectF(-100, -100, 200, 200), Qt::NoPen, QBrush(Qt::gray), QPainter::CompositionMode_Source, false);
    const DrawingShot start = shoot(image);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    {
        Edit edit = editor->edit(layer, 1, "two rects");
        const QRect first(-80, -80, 20, 20);
        const QRect second(150, 40, 20, 20); // outside the Drawing: it grows
        edit.willChange(first);
        image->drawRect(first, Qt::NoPen, QBrush(Qt::red), QPainter::CompositionMode_Source, false);
        edit.willChange(second);
        image->drawRect(second, Qt::NoPen, QBrush(Qt::red), QPainter::CompositionMode_Source, false);
        edit.commit();
    }
    const DrawingShot end = shoot(image);
    REQUIRE_FALSE(end == start);

    undoAction->trigger();
    CHECK(shoot(image) == start);
    redoAction->trigger();
    CHECK(shoot(image) == end);
}

TEST_CASE("UndoRedoManager: a stroke inside a Drawing paints the Drawing in place")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL);
    editor->scrubTo(1);
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    image->drawRect(QRectF(-100, -100, 200, 200), Qt::NoPen, QBrush(Qt::gray), QPainter::CompositionMode_Source, false);
    const uchar* pixels = image->image()->constBits();

    canvasStroke(editor, area, QPointF(-50, 0), QPointF(50, 0));

    REQUIRE(qBlue(image->pixel(QPoint(0, 0))) != qBlue(QColor(Qt::gray).rgba()));
    // The undo record copies the stroke rect, not the whole Drawing.
    CHECK(image->image()->constBits() == pixels);

    // The step before shares the Drawing until the next stroke starts.
    canvasStroke(editor, area, QPointF(-50, 30), QPointF(50, 30));
    CHECK(image->image()->constBits() == pixels);
}

namespace
{
// A session with a Drawing at 1 and 150; the canvas shows frame 1 of the bitmap layer.
struct HeldSession
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    BitmapImage* image = nullptr;

    bool start()
    {
        if (!layer->addNewKeyFrameAt(150) || !session.start()) { return false; }
        session.area->init();
        session.area->resize(800, 600);
        session.editor->layers()->setCurrentLayer(layer);
        session.editor->scrubTo(1);
        image = layer->getBitmapImageAtFrame(1);
        return true;
    }
    void stroke(ToolType type, QPointF from, QPointF to, int width)
    {
        session.editor->tools()->setCurrentTool(type);
        const ToolSettingsGuard guard(session.editor->tools()->currentTool());
        auto* tool = static_cast<StrokeTool*>(session.editor->tools()->currentTool());
        tool->set(ToolProperty::Width, width);
        tool->set(ToolProperty::FeatherEnabled, false);
        tool->set(ToolProperty::PressureEnabled, false);
        canvasStroke(session.editor.get(), session.area.get(), from, to);
    }
};
} // namespace

TEST_CASE("UndoRedoManager: the undo stack keeps no copy of a Drawing that a save cropped")
{
    HeldSession s;
    REQUIRE(s.start());
    // Transparent pixels around the stroke: the save crops them.
    s.image->drawRect(QRectF(-200, -200, 400, 400), Qt::NoPen, QBrush(Qt::transparent), QPainter::CompositionMode_Source, false);
    s.stroke(PENCIL, QPointF(-50, 0), QPointF(50, 0), 10);
    const QImage beforeSave = *s.image->image(); // shares the pixels the stack keeps

    QTemporaryDir dir;
    ProjectSession project(s.session.editor.get(), nullptr);
    REQUIRE(project.saveAs(dir.filePath("crop.pclx")).ok());
    REQUIRE(s.image->image()->cacheKey() != beforeSave.cacheKey()); // the save cropped into a new buffer
    CHECK(beforeSave.isDetached());
}

TEST_CASE("UndoRedoManager: the undo stack keeps no copy of a Drawing that the frame pool unloaded")
{
    HeldSession s;
    REQUIRE(s.start());
    s.image->drawRect(QRectF(-100, -100, 200, 200), Qt::NoPen, QBrush(Qt::gray), QPainter::CompositionMode_Source, false);
    s.stroke(ERASER, QPointF(-100, -150), QPointF(-100, 150), 60);

    QTemporaryDir dir;
    ProjectSession project(s.session.editor.get(), nullptr);
    REQUIRE(project.saveAs(dir.filePath("unload.pclx")).ok());
    const QImage beforeUnload = *s.image->image(); // shares the pixels the stack keeps
    s.session.editor->scrubTo(150);
    REQUIRE_FALSE(s.image->isLoaded());
    CHECK(beforeUnload.isDetached());
}