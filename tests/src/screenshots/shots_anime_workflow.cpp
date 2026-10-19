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
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QPainterPath>
#include <QScrollBar>
#include <QTimer>

#include "bitmapimage.h"
#include "cellmenus.h"
#include "colormanager.h"
#include "editor.h"
#include "exporter.h"
#include "keyframe.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "object.h"
#include "viewmanager.h"
#include "pagewriter.h"
#include "papertemplatedialog.h"
#include "paperpainter.h"
#include "screenshots.h"
#include "welcomedialog.h"

namespace
{
QDockWidget* panel(ShotWindow& shot, const char* name, QSize size)
{
    auto* dock = shot.window.findChild<QDockWidget*>(name);
    REQUIRE(dock);
    dock->setFloating(true);
    dock->resize(size);
    dock->show();
    dock->raise();
    QApplication::processEvents();
    return dock;
}

Layer* layerNamed(Editor* editor, const QString& name)
{
    for (int i = 0; i < editor->object()->getLayerCount(); ++i)
    {
        if (editor->object()->getLayer(i)->name() == name) { return editor->object()->getLayer(i); }
    }
    FAIL("No layer named " << name.toStdString());
    return nullptr;
}

PaperTemplate titleBlock()
{
    PaperTemplate paper;
    paper.on = true;
    paper.title = "Bouncing Ball";
    paper.id = "12-034";
    paper.name = "Maki";
    paper.state = "Genga";
    paper.date = "2026-10-19";
    paper.notes = "Squash on frame 7.\nBall leaves the frame on 13.";
    return paper;
}

// A page of an export, scaled to a width that suits the manual.
void savePage(const PageSpec& spec, const PagePaintFn& paint, int page, const QString& name)
{
    QLabel label;
    label.setPixmap(QPixmap::fromImage(renderPage(spec, paint, page)).scaledToWidth(spec.orientation == QPageLayout::Landscape ? 900 : 640, Qt::SmoothTransformation));
    label.resize(label.pixmap(Qt::ReturnByValue).size());
    saveShot(&label, name);
}

void makeStoryboard(ShotWindow& shot)
{
    Layer* board = layerNamed(shot.editor, "Background");
    shot.editor->layers()->setStoryboard(board, true);
    shot.editor->layers()->setCurrentLayer(board);
    for (int frame : { 13, 25, 37 }) { board->addNewKeyFrameAt(frame); }
    const PanelFields fields[] = {
        { "1", "1", "Ball falls from the left.", "Music starts" },
        { "1", "2", "Ball hits the floor.", "Thud" },
        { "1", "3", "Ball bounces to the right.", "" },
        { "2", "1", "Ball leaves the frame.", "Music ends" },
    };
    int i = 0;
    for (int frame : { 1, 13, 25, 37 })
    {
        shot.editor->layers()->setPanelFields(board, frame, fields[i]);
        // A ball on each panel, lower and further right than on the one before.
        QPainterPath ball;
        ball.addEllipse(QPointF(-300 + 200 * i, -100 + 80 * (i == 3 ? 0 : i)), 50, 50);
        static_cast<LayerBitmap*>(board)->getBitmapImageAtFrame(frame)->drawPath(ball, QPen(Qt::black, 5), Qt::NoBrush, QPainter::CompositionMode_SourceOver, true);
        ++i;
    }
    shot.editor->layers()->setSceneLength(48);
}
} // namespace

// Shots for docs/manual/*-anime-workflow.md. Each SECTION saves images/anime-workflow-<shot>.png.
TEST_CASE("Manual screenshots: Anime workflow", "[.screenshots]")
{
    ShotWindow shot;

    SECTION("templates")
    {
        WelcomeDialog welcome(0, QStringList(), QStringLiteral("Anime 1080p"), &shot.window);
        welcome.show();
        saveShot(&welcome, "anime-workflow-templates");
    }

    SECTION("paper-dialog")
    {
        PaperTemplateDialog dialog(titleBlock(), &shot.window);
        dialog.show();
        saveShot(&dialog, "anime-workflow-paper-dialog");
    }

    SECTION("paper-template")
    {
        shot.editor->setPaperTemplate(titleBlock());
        shot.editor->view()->scale50();
        QApplication::processEvents();
        saveShot(&shot.window, "anime-workflow-paper-template");
    }

    SECTION("genga-palette")
    {
        shot.editor->object()->loadBuiltinPalette(1);
        shot.editor->color()->setColorNumber(0);
        QDockWidget* palette = panel(shot, "ColorPalette", QSize(300, 260));
        saveShot(palette, "anime-workflow-genga-palette");
    }

    SECTION("timesheet-menu")
    {
        Layer* line = layerNamed(shot.editor, "Line");
        QTimer::singleShot(0, [=] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            REQUIRE(menu);
            for (QAction* a : menu->actions())
            {
                if (a->menu() && a->text() == "Timesheet Column")
                {
                    a->menu()->popup(menu->pos() + QPoint(menu->width(), menu->actionGeometry(a).y()));
                    saveShot(a->menu(), "anime-workflow-timesheet-menu");
                    a->menu()->close();
                }
            }
            menu->close();
        });
        CellMenus::showLayerMenu(shot.editor, line, QPoint(300, 300));
    }

    SECTION("timesheet")
    {
        SettingsGuard layout({ SETTING_XSHEET_LAYOUT });
        shot.editor->layers()->setTimesheetColumn(layerNamed(shot.editor, "Line"), TimesheetColumn::InbetweenA);
        QDockWidget* xsheet = panel(shot, "XSheet", QSize(900, 420));
        auto* toggle = shot.window.findChild<QAction*>("actionXSheetTimesheet");
        REQUIRE(toggle);
        toggle->setChecked(true);
        for (QScrollBar* bar : xsheet->findChildren<QScrollBar*>())
        {
            if (bar->orientation() == Qt::Vertical) { bar->setValue(0); }
        }
        saveShot(xsheet, "anime-workflow-timesheet");
    }

    SECTION("storyboard")
    {
        makeStoryboard(shot);
        saveShot(panel(shot, "Storyboard", QSize(700, 240)), "anime-workflow-storyboard");
    }

    SECTION("export-paper")
    {
        shot.editor->setPaperTemplate(titleBlock());
        PageSpec spec;
        const LayerCamera* camera = Exporter::camera(*shot.editor->object(), QString());
        REQUIRE(camera);
        savePage(spec, paperFramePages(*shot.editor->object(), *camera, 7), 0, "anime-workflow-export-paper");
    }

    SECTION("export-storyboard")
    {
        makeStoryboard(shot);
        shot.editor->setPaperTemplate(titleBlock());
        const Layer* board = layerNamed(shot.editor, "Background");
        const LayerCamera* camera = Exporter::camera(*shot.editor->object(), QString());
        REQUIRE(camera);
        QList<QPair<int, int>> panels;
        for (const Layer::Run& run : board->runs(48)) { panels << qMakePair(run.first, run.last); }
        PageSpec spec;
        spec.orientation = QPageLayout::Portrait;
        savePage(spec, storyboardPages(*shot.editor->object(), *board, *camera, panels, 12), 0, "anime-workflow-export-storyboard");
    }

    SECTION("export-timesheet")
    {
        shot.editor->setPaperTemplate(titleBlock());
        TimesheetInfo info;
        info.scene = "12";
        info.shot = "034";
        info.animator = "Maki";
        info.length = 24;
        info.fps = 12;
        PageSpec spec;
        spec.orientation = QPageLayout::Portrait;
        savePage(spec, timesheetPages(*shot.editor->object(), info), 0, "anime-workflow-export-timesheet");
    }
}
