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
#include <QDockWidget>
#include <QLineEdit>
#include <QMenuBar>
#include <QPainterPath>
#include <QTemporaryDir>
#include <QToolBar>

#include "bitmapimage.h"
#include "commandsearch.h"
#include "editor.h"
#include "exportmoviedialog.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "preferencesdialog.h"
#include "screenshots.h"
#include "theme.h"
#include "welcomedialog.h"
#include "welcomehelper.h"

namespace
{
QAction* actionNamed(QWidget* window, const QString& text)
{
    for (QAction* a : window->findChildren<QAction*>())
    {
        if (a->text() == text) { return a; }
    }
    return nullptr;
}

// A new project from the "Anime 720p" template with a ball on `count` Drawings, one every two frames.
void newBallProject(Editor* editor, int count)
{
    editor->setObject(Welcome::newObjectFromTemplate(Welcome::templateIndex("Anime 720p")));
    auto* layer = static_cast<LayerBitmap*>(editor->layers()->currentLayer());
    const QPen pen(QColor(40, 40, 40), 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (int i = 0; i < count; ++i)
    {
        const int frame = 1 + 2 * i;
        if (!layer->keyExists(frame)) { layer->addNewKeyFrameAt(frame); }
        QPainterPath ball;
        ball.addEllipse(QPointF(-300 + 150 * i, -100 + 60 * i), 60, 60);
        layer->getBitmapImageAtFrame(frame)->drawPath(ball, pen, Qt::NoBrush, QPainter::CompositionMode_SourceOver, true);
    }
    editor->scrubTo(count > 0 ? 1 + 2 * (count - 1) : 1);
}

QDockWidget* panel(QWidget* window, const char* objectName)
{
    auto* dock = window->findChild<QDockWidget*>(objectName);
    REQUIRE(dock);
    return dock;
}
} // namespace

// Shots for docs/manual/*-getting-started.md. Each SECTION saves images/getting-started-<shot>.png.
TEST_CASE("Manual screenshots: Getting started", "[.screenshots]")
{
    ShotWindow shot;

    SECTION("main-window")
    {
        saveShot(&shot.window, "getting-started-main-window");
    }

    SECTION("welcome-screen")
    {
        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        QStringList recent;
        for (const char* name : { "Bouncing ball", "Walk cycle", "Title card" })
        {
            QFile file(dir.filePath(QString(name) + ".pclx"));
            REQUIRE(file.open(QFile::WriteOnly));
            recent << file.fileName();
        }
        WelcomeDialog welcome(0, recent, QStringLiteral("Anime 720p"), &shot.window);
        welcome.show();
        saveShot(&welcome, "getting-started-welcome-screen");
    }

    SECTION("tool-column")
    {
        panel(&shot.window, "ToolBox")->raise();
        saveShot(panel(&shot.window, "ToolBox"), "getting-started-tool-column");
    }

    SECTION("command-bar")
    {
        auto* bar = shot.window.findChild<QToolBar*>("CommandBar");
        REQUIRE(bar);
        saveShot(bar, "getting-started-command-bar");
    }

    SECTION("tool-settings")
    {
        saveShot(panel(&shot.window, "ToolOption"), "getting-started-tool-settings");
    }

    SECTION("timeline")
    {
        saveShot(panel(&shot.window, "TimeLine"), "getting-started-timeline");
    }

    SECTION("layout-draw")
    {
        QAction* draw = actionNamed(&shot.window, "Draw");
        REQUIRE(draw);
        draw->trigger();
        saveShot(&shot.window, "getting-started-layout-draw");
    }

    SECTION("layout-ink")
    {
        QAction* ink = actionNamed(&shot.window, "Ink");
        REQUIRE(ink);
        ink->trigger();
        saveShot(&shot.window, "getting-started-layout-ink");
    }

    SECTION("command-search")
    {
        shot.window.openCommandSearch();
        auto* search = shot.window.findChild<CommandSearch*>();
        REQUIRE(search);
        search->findChild<QLineEdit*>()->setText("export");
        saveShot(search, "getting-started-command-search");
        search->close();
    }

    SECTION("theme-dark")
    {
        Theme::saveAccent(Theme::Teal);
        Theme::save(Theme::Dark);
        Theme::apply(Theme::Dark);
        saveShot(&shot.window, "getting-started-theme-dark");
    }

    SECTION("preferences-appearance")
    {
        shot.window.preferences();
        auto* prefs = shot.window.findChild<PreferencesDialog*>();
        REQUIRE(prefs);
        saveShot(prefs, "getting-started-preferences-appearance");
        prefs->close();
    }

    SECTION("first-drawing")
    {
        newBallProject(shot.editor, 1);
        panel(&shot.window, "ToolBox")->raise();
        saveShot(&shot.window, "getting-started-first-drawing");
    }

    SECTION("more-drawings")
    {
        newBallProject(shot.editor, 3);
        panel(&shot.window, "ToolBox")->raise();
        saveShot(&shot.window, "getting-started-more-drawings");
    }

    SECTION("export-movie")
    {
        newBallProject(shot.editor, 3);
        ExportMovieDialog dialog(&shot.window, ImportExportDialog::Export, FileType::MOVIE);
        dialog.init();
        dialog.setCamerasInfo({ { "Camera", QSize(1280, 720) } });
        dialog.setDefaultRange(1, 5, 5);
        dialog.findChild<QLineEdit*>("fileEdit")->setText("\"ball.mp4\"");
        dialog.show();
        saveShot(&dialog, "getting-started-export-movie");
    }
}
