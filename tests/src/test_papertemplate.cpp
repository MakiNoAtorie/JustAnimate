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
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QTemporaryDir>

#include "framerenderer.h"
#include "camera.h"
#include "fileformat.h"
#include "filemanager.h"
#include "layercamera.h"
#include "objectdata.h"
#include "paperpainter.h"
#include "testfixture.h"
#include "undoredomanager.h"
#include "viewmanager.h"

namespace
{
PaperTemplate filledTemplate()
{
    PaperTemplate t;
    t.on = true;
    t.title = "Cut 12";
    t.id = "A-012";
    t.name = "Maki";
    t.state = "Genga";
    t.date = "2026-10-17";
    t.notes = "Hold <3>\n& pan left";
    return t;
}
} // namespace

TEST_CASE("The Paper template round-trips through the project file", "[PaperTemplate]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("paper.pclx");
    FileManager fm;
    TestSession s;
    s.object->addNewCameraLayer();
    REQUIRE(s.start());

    SECTION("A file without the element loads with the template off")
    {
        REQUIRE(fm.save(s.object, path).ok());
        QFile mainXml(QDir(s.object->workingDir()).filePath(PFF_XML_FILE_NAME));
        REQUIRE(mainXml.open(QIODevice::ReadOnly));
        CHECK(mainXml.readAll().count("<paperTemplate") == 0);
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        CHECK(loaded->data()->paperTemplate() == PaperTemplate());
    }

    SECTION("Every field survives save and load")
    {
        s.object->data()->setPaperTemplate(filledTemplate());
        REQUIRE(fm.save(s.object, path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        CHECK(loaded->data()->paperTemplate() == filledTemplate());
    }

    SECTION("The Timesheet values are optional attributes")
    {
        PaperTemplate t = filledTemplate();
        s.object->data()->setPaperTemplate(t);
        REQUIRE(fm.save(s.object, path).ok());
        {
            QFile mainXml(QDir(s.object->workingDir()).filePath(PFF_XML_FILE_NAME));
            REQUIRE(mainXml.open(QIODevice::ReadOnly));
            CHECK(mainXml.readAll().count("sheetScene") == 0);
        }

        t.sheetScene = "12";
        t.sheetShot = "034";
        t.sheetAnimator = "Rin";
        s.object->data()->setPaperTemplate(t);
        REQUIRE(fm.save(s.object, path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        CHECK(loaded->data()->paperTemplate() == t);
    }
}

TEST_CASE("Timesheet export prefills from the Title block", "[PaperTemplate]")
{
    PaperTemplate t;
    t.id = "12-034";
    t.name = "Maki";
    TimesheetInfo info = timesheetPrefill(t);
    CHECK(info.scene == "12");
    CHECK(info.shot == "034");
    CHECK(info.animator == "Maki");

    t.id = "Cut 12";
    info = timesheetPrefill(t);
    CHECK(info.scene.isEmpty());
    CHECK(info.shot.isEmpty());
    t.id = "1-2-3";
    CHECK(timesheetPrefill(t).scene.isEmpty());
    t.id = "-034";
    CHECK(timesheetPrefill(t).shot.isEmpty());

    // A typed value differing from the Title block is kept and wins; one matching it is not stored.
    t.id = "12-034";
    info.scene = "12";
    info.shot = "035";
    info.animator = "Rin";
    rememberTimesheet(t, info);
    CHECK(t.sheetScene.isEmpty());
    CHECK(t.sheetShot == "035");
    CHECK(t.sheetAnimator == "Rin");
    info = timesheetPrefill(t);
    CHECK(info.scene == "12");
    CHECK(info.shot == "035");
    CHECK(info.animator == "Rin");
}

TEST_CASE("Undoing a Title block edit keeps the Timesheet values", "[PaperTemplate]")
{
    TestSession s;
    s.object->addNewCameraLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    PaperTemplate renamed = filledTemplate();
    renamed.name = "Rin";
    editor->setPaperTemplate(renamed);
    PaperTemplate typed = editor->object()->data()->paperTemplate();
    typed.sheetShot = "035";
    editor->object()->data()->setPaperTemplate(typed);
    undo->trigger();
    CHECK(editor->object()->data()->paperTemplate().sheetShot == "035");
}

TEST_CASE("A Paper template edit is one undo step per changed field", "[PaperTemplate]")
{
    TestSession s;
    s.object->addNewCameraLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    editor->object()->data()->setPaperTemplate(filledTemplate());
    PaperTemplate renamed = filledTemplate();
    renamed.title = "Cut 13";
    editor->setPaperTemplate(renamed);
    undo->trigger();
    CHECK(editor->object()->data()->paperTemplate() == filledTemplate());
    redo->trigger();
    CHECK(editor->object()->data()->paperTemplate() == renamed);
    undo->trigger();

    // Two fields changed in one dialog OK: two steps, the last field first.
    PaperTemplate two = filledTemplate();
    two.title = "Cut 13";
    two.state = "Clean";
    editor->setPaperTemplate(two);
    undo->trigger();
    PaperTemplate half = filledTemplate();
    half.title = "Cut 13";
    CHECK(editor->object()->data()->paperTemplate() == half);
    undo->trigger();
    CHECK(editor->object()->data()->paperTemplate() == filledTemplate());
    redo->trigger();
    redo->trigger();
    CHECK(editor->object()->data()->paperTemplate() == two);
}

TEST_CASE("Editing every Paper template field is one undo step per field", "[PaperTemplate]")
{
    TestSession s;
    s.object->addNewCameraLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    REQUIRE_FALSE(undo->isEnabled());

    // filledTemplate changes on and the six Title block fields.
    editor->setPaperTemplate(filledTemplate());
    int steps = 0;
    while (undo->isEnabled() && steps < 20)
    {
        undo->trigger();
        ++steps;
    }
    CHECK(steps == 7);
    CHECK(editor->object()->data()->paperTemplate() == PaperTemplate());
}

TEST_CASE("The canvas shows the Title block and exports leave it out", "[PaperTemplate]")
{
    TestSession s;
    LayerCamera* camera = s.object->addNewCameraLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(400, 300);
    area->show();
    QApplication::processEvents();
    // The wiring MainWindow2 makes: an edit drops the canvas caches.
    QObject::connect(editor, &Editor::framesModified, area, &ScribbleArea::onFramesModified);
    editor->view()->scale(0.2);
    editor->scrubTo(1);

    // The band above the camera frame that holds the Title block.
    const QRectF frame = camera->getViewRect();
    const QRect band = editor->view()->mapCanvasToScreen(QRectF(frame.left(), frame.top() - frame.height() * 0.2,
                                                                frame.width(), frame.height() * 0.12)).toRect();
    REQUIRE(area->rect().contains(band));
    const QImage before = area->grab().toImage();

    editor->setPaperTemplate(filledTemplate());
    const QImage after = area->grab().toImage();
    int changed = 0;
    for (int y = band.top(); y <= band.bottom(); ++y)
    {
        for (int x = band.left(); x <= band.right(); ++x) { changed += after.pixel(x, y) != before.pixel(x, y); }
    }
    CHECK(changed > band.width());

    const QSize size = frame.size().toSize();
    const QImage exported = FrameRenderer::renderForExport(*editor->object(), 1, QTransform(), size, { size, true, false });
    editor->object()->data()->setPaperTemplate(PaperTemplate());
    CHECK(exported == FrameRenderer::renderForExport(*editor->object(), 1, QTransform(), size, { size, true, false }));
}

TEST_CASE("The Paper sheet follows a moved camera and the dimming leaves it white", "[PaperTemplate]")
{
    TestSession s;
    LayerCamera* camera = s.object->addNewCameraLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 400);
    area->show();
    QApplication::processEvents();
    QObject::connect(editor, &Editor::framesModified, area, &ScribbleArea::onFramesModified);
    const QRectF frame = camera->getViewRect();
    editor->view()->scale(150 / frame.width());
    camera->getCameraAtFrame(1)->translate(QPointF(frame.width() * 1.2, 0));
    editor->scrubTo(1);
    editor->setPaperTemplate(filledTemplate());
    const QImage shot = area->grab().toImage();

    // The middle of the sheet margin left of the frame: where the camera moved it, and where it was before.
    const QPointF margin(frame.left() - frame.height() * 0.03, frame.center().y());
    const QPoint moved = editor->view()->mapCanvasToScreen(camera->getViewAtFrame(1).inverted().map(margin)).toPoint();
    const QPoint still = editor->view()->mapCanvasToScreen(margin).toPoint();
    REQUIRE(area->rect().contains(moved));
    REQUIRE(area->rect().contains(still));
    CHECK(shot.pixel(moved) == qRgb(255, 255, 255));
    CHECK(shot.pixel(still) != qRgb(255, 255, 255));
}

TEST_CASE("The NOTES cell shows three lines", "[PaperTemplate]")
{
    PaperTemplate t;
    t.on = true;
    auto paint = [&t](const QString& notes) {
        QImage image(600, 600, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        t.notes = notes;
        QPainter painter(&image);
        paintPaperTemplate(painter, QRectF(50, 200, 500, 300), t);
        return image;
    };
    CHECK(paint("Hold\nPan left\nFade out") != paint("Hold\nPan left\n"));
}

TEST_CASE("paintPaperTemplate draws nothing when off", "[PaperTemplate]")
{
    QImage image(200, 200, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const QImage empty = image;
    PaperTemplate t = filledTemplate();
    t.on = false;
    {
        QPainter painter(&image);
        paintPaperTemplate(painter, QRectF(50, 80, 100, 60), t);
    }
    CHECK(image == empty);

    t.on = true;
    {
        QPainter painter(&image);
        paintPaperTemplate(painter, QRectF(50, 80, 100, 60), t);
    }
    CHECK(image != empty);
}
