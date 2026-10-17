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
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtMath>

#include "bitmapimage.h"
#include "editor.h"
#include "filemanager.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "objectdata.h"
#include "pagewriter.h"
#include "paperpainter.h"
#include "playbackmanager.h"
#include "storyboardpanel.h"
#include "testfixture.h"
#include "undoredomanager.h"
#include "util.h"

namespace
{
QString cell(QTableWidget* table, int row, int column) { return table->item(row, column)->text(); }

// Pixels that differ between two images inside region; a null region is the whole image.
int diffCount(const QImage& a, const QImage& b, QRect region)
{
    if (region.isNull()) { region = a.rect(); }
    region &= a.rect();
    int count = 0;
    for (int y = region.top(); y <= region.bottom(); ++y)
    {
        for (int x = region.left(); x <= region.right(); ++x) { count += a.pixel(x, y) != b.pixel(x, y); }
    }
    return count;
}
}

TEST_CASE("Seconds+frames text", "[Storyboard]")
{
    CHECK(formatSecondsFrames(84, 24) == "3+12");
    CHECK(formatSecondsFrames(24, 24) == "1+0");
    CHECK(formatSecondsFrames(5, 24) == "0+5");
    CHECK(formatSecondsFrames(7, 0) == "0+7");
    for (int frames : { 0, 5, 24, 84, 1000 }) { CHECK(parseSecondsFrames(formatSecondsFrames(frames, 24), 24) == frames); }
    CHECK(parseSecondsFrames("36", 24) == 36);
    CHECK(parseSecondsFrames("1+x", 24) == 0);
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

TEST_CASE("The last storyboard panel's Time sets the saved project length", "[Storyboard]")
{
    SettingsGuard guard({ SETTING_FPS });
    TestSession s;
    LayerBitmap* board = s.object->addNewBitmapLayer();
    board->setStoryboard(true);
    board->addNewKeyFrameAt(1);
    board->addNewKeyFrameAt(4);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    editor->playback()->setFps(24);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());
    StoryboardPanel panel(nullptr);
    panel.setEditor(editor);
    panel.initUI();
    QTableWidget* table = panel.findChild<QTableWidget*>();
    REQUIRE(table->rowCount() == 2);
    using C = StoryboardPanel::Column;
    CHECK(cell(table, 1, C::Time) == "0+1");
    CHECK(table->item(1, C::Time)->flags() & Qt::ItemIsEditable);

    // 1+0 from frame 4 holds the Drawing to frame 27, in one undo step.
    table->item(1, C::Time)->setText("1+0");
    CHECK(editor->layers()->animationLength() == 27);
    CHECK(editor->object()->sceneLength() == 27);
    CHECK(board->getMaxKeyFramePosition() == 4);
    CHECK(cell(table, 1, C::Time) == "1+0");
    undo->trigger();
    CHECK(editor->layers()->animationLength() == 4);
    CHECK(cell(table, 1, C::Time) == "0+1");
    redo->trigger();
    CHECK(cell(table, 1, C::Time) == "1+0");

    // Playback runs to the end of the project length.
    PlaybackManager* pb = editor->playback();
    pb->setLooping(false);
    pb->enableRangedPlayback(false);
    editor->scrubTo(1);
    pb->play();
    for (int i = 0; i < 40 && pb->isPlaying(); ++i) { pb->advance(); }
    pb->stop();
    CHECK(editor->currentFrame() == 27);

    // Saved as <object length>; omitted when unset; a bad value loads as unset.
    QDomDocument doc;
    const QDomElement saved = editor->object()->saveXML(doc);
    CHECK(saved.attribute("length") == "27");
    Object loaded;
    REQUIRE(loaded.loadXML(saved, [] {}));
    CHECK(loaded.sceneLength() == 27);
    Object unset;
    CHECK_FALSE(unset.saveXML(doc).hasAttribute("length"));
    for (const char* bad : { "x", "-3", "0", "100000" })
    {
        QDomElement elem = saved.cloneNode(false).toElement();
        elem.setAttribute("length", bad);
        Object o;
        REQUIRE(o.loadXML(elem, [] {}));
        CHECK(o.sceneLength() == 0);
    }
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

TEST_CASE("Storyboard export has five panels a page", "[Storyboard]")
{
    CHECK(storyboardPageCount(0) == 0);
    CHECK(storyboardPageCount(5) == 1);
    CHECK(storyboardPageCount(11) == 3);

    Object object;
    object.init();
    LayerCamera* camera = object.addNewCameraLayer();
    LayerBitmap* board = object.addNewBitmapLayer();
    QList<QPair<int, int>> panels;
    for (int i = 0; i < 11; ++i)
    {
        board->addNewKeyFrameAt(1 + 2 * i);
        panels << qMakePair(1 + 2 * i, 2 + 2 * i);
    }
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    PageSpec spec;
    spec.orientation = QPageLayout::Portrait;
    spec.dpi = 30;
    REQUIRE(writePages(dir.filePath("board.png"), spec, storyboardPageCount(panels.size()),
                       storyboardPages(object, *board, *camera, panels, 24)).ok());
    CHECK(QFileInfo::exists(dir.filePath("board_0003.png")));
    CHECK_FALSE(QFileInfo::exists(dir.filePath("board_0004.png")));
}

TEST_CASE("Page exports draw a Drawing the frame pool unloaded", "[Storyboard]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("far.pclx");
    FileManager fm;
    {
        Object object;
        object.init();
        LayerCamera* camera = object.addNewCameraLayer();
        LayerBitmap* board = object.addNewBitmapLayer();
        board->addNewKeyFrameAt(1);
        board->addNewKeyFrameAt(100);
        board->getBitmapImageAtFrame(100)->drawRect(camera->getViewRect(), Qt::NoPen, Qt::black,
                                                    QPainter::CompositionMode_SourceOver, false);
        REQUIRE(fm.save(&object, path).ok());
    }
    std::unique_ptr<Object> object(fm.load(path));
    REQUIRE(object);
    const auto* camera = static_cast<LayerCamera*>(object->getLayer(0));
    auto* board = static_cast<LayerBitmap*>(object->getLayer(1));
    object->updateActiveFrames(100);
    object->updateActiveFrames(1);
    REQUIRE_FALSE(board->getBitmapImageAtFrame(100)->isLoaded());

    PageSpec spec;
    spec.dpi = 30;
    const QRectF page(QPointF(), QSizeF(spec.pixelSize()));
    const QImage sheet = renderPage(spec, storyboardPages(*object, *board, *camera, { qMakePair(100, 110) }, 24), 0);
    CHECK(sheet.pixel(storyboardPicture(storyboardCell(page, 0, 2), camera->getViewSize()).toRect().center()) == qRgb(0, 0, 0));
    board->getBitmapImageAtFrame(100)->unloadFile();
    REQUIRE_FALSE(board->getBitmapImageAtFrame(100)->isLoaded());
    const QImage paper = renderPage(spec, paperFramePages(*object, *camera, 100), 0);
    CHECK(paper.pixel(paperFrameOnPage(page, camera->getViewSize()).toRect().center()) == qRgb(0, 0, 0));
}

TEST_CASE("A storyboard page has each field in its cell", "[Storyboard]")
{
    Object object;
    object.init();
    LayerCamera* camera = object.addNewCameraLayer();
    LayerBitmap* board = object.addNewBitmapLayer();
    for (int i = 0; i < 7; ++i) { board->addNewKeyFrameAt(1 + 10 * i); }
    // Panel 7 (page 1, row 1) is 36 frames: 1+12 at 24 fps.
    QList<QPair<int, int>> panels;
    for (int i = 0; i < 7; ++i) { panels << qMakePair(1 + 10 * i, 10 + 10 * i); }
    panels.last().second = panels.last().first + 35;
    PageSpec spec;
    spec.orientation = QPageLayout::Portrait;
    spec.dpi = 60;
    const PagePaintFn paint = storyboardPages(object, *board, *camera, panels, 24);
    const QRectF page(QPointF(), QSizeF(spec.pixelSize()));
    const QImage blank = renderPage(spec, paint, 1);
    REQUIRE(blank.height() > blank.width()); // A4 portrait

    // TIME in seconds+frames: 1+12 against 0+5 (different lengths: offscreen glyphs can all look alike).
    QList<QPair<int, int>> shorter = panels;
    shorter.last().second = shorter.last().first + 4;
    const QImage other = renderPage(spec, storyboardPages(object, *board, *camera, shorter, 24), 1);
    const QRect time = storyboardCell(page, 1, 5).toAlignedRect();
    CHECK(diffCount(blank, other, time) > 5);
    CHECK(diffCount(blank, other, QRect()) == diffCount(blank, other, time));

    struct Field { QString PanelFields::*value; int column; };
    const Field fields[] = { { &PanelFields::scene, 0 }, { &PanelFields::cut, 1 },
                             { &PanelFields::notes, 3 }, { &PanelFields::audio, 4 } };
    for (const Field& field : fields)
    {
        PanelFields f;
        f.*field.value = "WWW";
        board->getKeyFrameAt(61)->setPanelFields(f);
        const QImage filled = renderPage(spec, paint, 1);
        const QRect cellRect = storyboardCell(page, 1, field.column).toAlignedRect();
        CHECK(diffCount(blank, filled, cellRect) > 20);
        CHECK(diffCount(blank, filled, QRect()) == diffCount(blank, filled, cellRect));
    }
    board->getKeyFrameAt(61)->setPanelFields(PanelFields());

    // PRODUCTION shows the Paper template TITLE above the table.
    PaperTemplate paper;
    paper.title = "WWW";
    object.data()->setPaperTemplate(paper);
    const QImage titled = renderPage(spec, paint, 1);
    CHECK(diffCount(blank, titled, QRect()) > 20);
    CHECK(diffCount(blank, titled, QRect()) == diffCount(blank, titled, QRect(0, 0, blank.width(), qFloor(storyboardCell(page, 0, 0).top()))));

    // The picture is the panel's Drawing in a frame at the camera aspect.
    board->getBitmapImageAtFrame(61)->drawRect(camera->getViewRect(), Qt::NoPen, Qt::black, QPainter::CompositionMode_SourceOver, false);
    const QRectF picture = storyboardPicture(storyboardCell(page, 1, 2), camera->getViewSize());
    const QSizeF view = camera->getViewSize();
    CHECK(picture.width() / picture.height() == Approx(view.width() / view.height()).epsilon(0.001));
    CHECK(storyboardCell(page, 1, 2).contains(picture));
    const QImage drawn = renderPage(spec, paint, 1);
    CHECK(drawn.pixel(picture.toRect().center()) == qRgb(0, 0, 0));
    CHECK(drawn.pixel(storyboardPicture(storyboardCell(page, 0, 2), view).toRect().center()) == qRgb(255, 255, 255));
}
