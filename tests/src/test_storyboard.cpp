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

#include <memory>
#include <QAction>
#include <QDomDocument>
#include <QIcon>
#include <QTableWidget>
#include <QTemporaryDir>

#include "editor.h"
#include "filemanager.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "playbackmanager.h"
#include "storyboardpanel.h"
#include "testfixture.h"
#include "undoredomanager.h"
#include "util.h"

namespace
{
QString cell(QTableWidget* table, int row, int column) { return table->item(row, column)->text(); }
}

TEST_CASE("Seconds+frames text", "[Storyboard]")
{
    CHECK(formatSecondsFrames(84, 24) == "3+12");
    CHECK(formatSecondsFrames(24, 24) == "1+0");
    CHECK(formatSecondsFrames(5, 24) == "0+5");
    CHECK(formatSecondsFrames(7, 0) == "0+7");
}

TEST_CASE("Storyboard flag and Panel fields round-trip through save and load", "[Storyboard]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("board.pclx");
    std::unique_ptr<Object> object(new Object);
    object->init();
    LayerVector* board = object->addNewVectorLayer();
    board->setStoryboard(true);
    board->addNewKeyFrameAt(1);
    board->addNewKeyFrameAt(4);
    board->getKeyFrameAt(1)->setPanelFields({ "1", "12", "Pan left", QString::fromUtf8("\"Hi\" & <bye>") });
    LayerBitmap* plain = object->addNewBitmapLayer();
    plain->addNewKeyFrameAt(1);

    FileManager fm;
    REQUIRE(fm.save(object.get(), path).ok());
    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);

    const Layer* b = loaded->findLayerById(board->id());
    CHECK(b->isStoryboard());
    CHECK(b->getKeyFrameAt(1)->panelFields() == PanelFields{ "1", "12", "Pan left", QString::fromUtf8("\"Hi\" & <bye>") });
    CHECK(b->getKeyFrameAt(4)->panelFields() == PanelFields());
    CHECK_FALSE(loaded->findLayerById(plain->id())->isStoryboard());

    // Empty fields and the flag off write no attribute, so old files and new files without them match.
    QDomDocument doc;
    const QDomElement elem = plain->createDomElement(doc);
    CHECK_FALSE(elem.hasAttribute("storyboard"));
    const QDomElement image = elem.firstChildElement("image");
    for (const char* name : { "scene", "cut", "notes", "audio" }) { CHECK_FALSE(image.hasAttribute(name)); }
}

TEST_CASE("Storyboard toggle and Panel field edits undo one step each", "[Storyboard]")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(5);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    editor->layers()->setStoryboard(layer, true);
    REQUIRE(layer->isStoryboard());
    CHECK(editor->layers()->duplicateLayer(layer, "Copy")->isStoryboard());
    undo->trigger(); // the duplicate

    // Frame 3 is a hold of the panel at 1.
    REQUIRE(editor->layers()->setPanelFields(layer, 3, { "1", "", "", "" }));
    REQUIRE(editor->layers()->setPanelFields(layer, 1, { "1", "2", "", "" }));
    CHECK(layer->getKeyFrameAt(1)->panelFields().cut == "2");
    CHECK_FALSE(editor->layers()->setPanelFields(layer, 1, { "1", "2", "", "" })); // no change, no step

    undo->trigger();
    CHECK(layer->getKeyFrameAt(1)->panelFields() == PanelFields{ "1", "", "", "" });
    undo->trigger();
    CHECK(layer->getKeyFrameAt(1)->panelFields() == PanelFields());
    undo->trigger();
    CHECK_FALSE(layer->isStoryboard());
    redo->trigger();
    redo->trigger();
    CHECK(layer->isStoryboard());
    CHECK(layer->getKeyFrameAt(1)->panelFields().scene == "1");
}

TEST_CASE("Storyboard Panel lists panels and its Time is the Exposure both ways", "[Storyboard]")
{
    TestSession s;
    LayerBitmap* other = s.object->addNewBitmapLayer();
    other->addNewKeyFrameAt(1);
    LayerBitmap* board = s.object->addNewBitmapLayer();
    board->addNewKeyFrameAt(1);
    board->addNewKeyFrameAt(4);
    board->addNewKeyFrameAt(6);
    board->getKeyFrameAt(4)->setPanelFields({ "1", "2", "Zoom", "Hey" });
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    editor->playback()->setFps(24);
    editor->layers()->setCurrentLayer(other);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());

    StoryboardPanel panel(nullptr);
    panel.setEditor(editor);
    panel.initUI();
    QTableWidget* table = panel.findChild<QTableWidget*>();
    REQUIRE(table);
    CHECK(table->rowCount() == 0); // no Storyboard layer yet

    editor->layers()->setStoryboard(board, true);
    REQUIRE(table->rowCount() == 3);
    using C = StoryboardPanel::Column;
    CHECK(cell(table, 1, C::Scene) == "1");
    CHECK(cell(table, 1, C::Cut) == "2");
    CHECK(cell(table, 1, C::Notes) == "Zoom");
    CHECK(cell(table, 1, C::Audio) == "Hey");
    CHECK(cell(table, 0, C::Time) == "0+3");
    CHECK(cell(table, 1, C::Time) == "0+2");

    // Time -> Exposure: "1+0" is 24 frames, so the later panels move by 21.
    table->item(0, C::Time)->setText("1+0");
    CHECK(board->getKeyFrameAt(25) != nullptr);
    CHECK(board->getKeyFrameAt(27) != nullptr);
    CHECK(cell(table, 0, C::Time) == "1+0");

    // Exposure -> Time
    editor->layers()->setCurrentLayer(board);
    REQUIRE(editor->layers()->changeExposure(25, 4));
    CHECK(cell(table, 1, C::Time) == "0+6");
    undo->trigger();
    CHECK(cell(table, 1, C::Time) == "0+2");

    // A text edit is one undo step.
    table->item(2, C::Notes)->setText("Fade out");
    CHECK(board->getKeyFrameAt(27)->panelFields().notes == "Fade out");
    undo->trigger();
    CHECK(board->getKeyFrameAt(27)->panelFields().notes.isEmpty());
    CHECK(cell(table, 2, C::Notes).isEmpty());

    // Choosing a panel jumps the playhead to it.
    table->setCurrentCell(1, C::Scene);
    CHECK(editor->currentFrame() == 25);
}

TEST_CASE("Playback holds each storyboard panel for its Time", "[Storyboard]")
{
    SettingsGuard guard({ SETTING_FPS });
    TestSession s;
    LayerBitmap* board = s.object->addNewBitmapLayer();
    board->setStoryboard(true);
    board->addNewKeyFrameAt(1);
    board->addNewKeyFrameAt(4);
    board->addNewKeyFrameAt(6);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    PlaybackManager* pb = editor->playback();
    pb->setLooping(false);
    pb->enableRangedPlayback(false);
    editor->scrubTo(1);

    QList<int> shown;
    pb->play();
    for (int i = 0; i < 20 && pb->isPlaying(); ++i)
    {
        shown << board->getLastKeyFrameAtPosition(editor->currentFrame())->pos();
        pb->advance();
    }
    pb->stop();
    CHECK(shown == QList<int>{ 1, 1, 1, 4, 4, 6 });
}
