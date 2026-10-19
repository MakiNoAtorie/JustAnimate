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

#include <cmath>

#include <QApplication>
#include <QCheckBox>
#include <QPainter>
#include <QMenu>
#include <QToolButton>
#include <QWheelEvent>

#include "colormanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "overlaymanager.h"
#include "pencilsettings.h"
#include "preferencemanager.h"
#include "screenshots.h"
#include "scribblearea.h"
#include "stroketool.h"
#include "toolmanager.h"
#include "transformtool.h"
#include "viewmanager.h"

namespace
{
using Points = QList<QPointF>;

QPointF onScreen(ShotWindow& shot, QPointF canvas) { return shot.editor->view()->mapCanvasToScreen(canvas); }

void tabletAt(ShotWindow& shot, QEvent::Type type, QPointF pos, qreal pressure, Qt::KeyboardModifiers mods = {}, bool down = true)
{
    sendTablet(shot.editor->getScribbleArea(), type, { pos, pressure }, mods, down);
}

void tablet(ShotWindow& shot, QEvent::Type type, QPointF canvas, qreal pressure, Qt::KeyboardModifiers mods = {}, bool down = true)
{
    tabletAt(shot, type, onScreen(shot, canvas), pressure, mods, down);
}

// One pen stroke through canvas points; the pressure rises and falls along it when `taper` is set.
void stroke(ShotWindow& shot, const Points& points, qreal pressure = 0.7, bool taper = false, Qt::KeyboardModifiers mods = {})
{
    const int steps = 8;
    tablet(shot, QEvent::TabletPress, points.first(), taper ? 0.4 : pressure, mods);
    const int total = (points.size() - 1) * steps;
    for (int i = 1; i <= total; ++i)
    {
        const QPointF a = points[(i - 1) / steps];
        const QPointF b = points[(i - 1) / steps + 1];
        const qreal t = qreal((i - 1) % steps + 1) / steps;
        const qreal p = taper ? 0.4 + 0.6 * std::sin(M_PI * qreal(i) / total) : pressure;
        tablet(shot, QEvent::TabletMove, a + (b - a) * t, p, mods);
    }
    tablet(shot, QEvent::TabletRelease, points.last(), taper ? 0.4 : pressure, mods);
}

void tap(ShotWindow& shot, QPointF canvas, Qt::KeyboardModifiers mods = {})
{
    tablet(shot, QEvent::TabletPress, canvas, 0.7, mods);
    tablet(shot, QEvent::TabletMove, canvas, 0.7, mods);
    tablet(shot, QEvent::TabletRelease, canvas, 0.7, mods);
}

Points wave(qreal x0, qreal x1, qreal y, qreal amplitude, qreal periods)
{
    Points points;
    for (int i = 0; i <= 24; ++i)
    {
        const qreal t = qreal(i) / 24;
        points << QPointF(x0 + (x1 - x0) * t, y + amplitude * std::sin(2 * M_PI * periods * t));
    }
    return points;
}

Points ring(QPointF centre, qreal rx, qreal ry)
{
    Points points;
    for (int i = 0; i <= 32; ++i)
    {
        const qreal a = 2 * M_PI * i / 32;
        points << centre + QPointF(std::cos(a) * rx, std::sin(a) * ry);
    }
    return points;
}

// A clean bitmap layer on top, so strokes are not faded like the Rough layer.
void useTool(ShotWindow& shot, ToolType type, const QColor& colour = QColor(30, 30, 30), qreal width = -1)
{
    shot.editor->layers()->setCurrentLayer(shot.editor->layers()->createBitmapLayer("Practice"));
    shot.editor->tools()->setCurrentTool(type);
    shot.editor->color()->setFrontColor(colour);
    if (width > 0)
    {
        auto* tool = static_cast<StrokeTool*>(shot.editor->tools()->currentTool());
        tool->set(ToolProperty::Width, width);
    }
}

// Tools is a tab behind Brush Presets in the default Layout; the shots show it.
void showTools(ShotWindow& shot) { raisePanel(&shot.window, "ToolBox"); }

void settle(ShotWindow& shot)
{
    showTools(shot);
    shot.editor->getScribbleArea()->update();
    QApplication::processEvents();
}

// Each shot starts with the default Tool groups: a group button shows the tool used last.
struct FreshGroups
{
    FreshGroups() { pencilSettings().remove("ToolGroup"); }
    ~FreshGroups() { pencilSettings().remove("ToolGroup"); }
};

void openMenu(QMenu* menu, QWidget* near)
{
    menu->popup(near->mapToGlobal(QPoint(near->width(), 0)));
    QApplication::processEvents();
}
} // namespace

// Shots for docs/manual/02-drawing-tools.md. Each SECTION saves images/drawing-tools-<shot>.png.
TEST_CASE("Manual screenshots: Drawing tools", "[.screenshots]")
{
    SECTION("tool-column")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->tools()->setCurrentTool(PENCIL);
        settle(shot);
        saveShot(panelDock(&shot.window, "ToolBox"), "drawing-tools-tool-column");
    }

    SECTION("tool-group")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->tools()->setCurrentTool(PENCIL);
        auto* menu = shot.window.findChild<QMenu*>("ToolGroupMenu_CmdToolPencil");
        REQUIRE(menu);
        auto* button = shot.window.findChild<QToolButton*>("pencilButton");
        QWidget* column = panelDock(&shot.window, "ToolBox");
        showTools(shot);
        openMenu(menu, button);
        // The menu is its own window: paste it under the button in a grab of the Tool column.
        QPixmap picture = column->grab();
        const QPoint at = button->mapTo(column, QPoint(0, button->height()));
        QPainter painter(&picture);
        painter.drawPixmap(at, menu->grab());
        painter.end();
        menu->hide();
        savePicture(picture, "drawing-tools-tool-group");
    }

    SECTION("pencil")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, PENCIL, QColor(40, 40, 40), 6);
        stroke(shot, wave(60, 330, -170, 20, 1.5), 0.7, true);
        stroke(shot, wave(60, 330, -120, 14, 2.5), 0.5, true);
        stroke(shot, ring({ 200, -10 }, 90, 50), 0.6, true);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-pencil");
    }

    SECTION("pen")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, PEN, QColor(20, 20, 20), 8);
        stroke(shot, wave(60, 330, -170, 22, 1.5), 0.8, true);
        stroke(shot, ring({ 200, -10 }, 90, 50), 0.8, true);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-pen");
    }

    SECTION("brush")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, BRUSH, QColor(200, 70, 50), 30);
        stroke(shot, wave(40, 340, -160, 30, 1.2), 0.8, true);
        shot.editor->color()->setFrontColor(QColor(40, 100, 200));
        stroke(shot, wave(40, 340, -90, 24, 1.8), 0.6, true);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-brush");
    }

    SECTION("eraser")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, PEN, QColor(40, 40, 40), 22);
        stroke(shot, wave(40, 340, -150, 20, 1.2), 1.0);
        stroke(shot, wave(40, 340, -70, 20, 1.2), 1.0);
        shot.editor->tools()->setCurrentTool(ERASER);
        static_cast<StrokeTool*>(shot.editor->tools()->currentTool())->set(ToolProperty::Width, 40);
        stroke(shot, { { 190, -230 }, { 190, 20 } }, 1.0);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-eraser");
    }

    SECTION("smudge")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, BRUSH, QColor(200, 70, 50), 36);
        stroke(shot, { { 60, -150 }, { 340, -150 } }, 0.8);
        shot.editor->color()->setFrontColor(QColor(40, 100, 200));
        stroke(shot, { { 60, -100 }, { 340, -100 } }, 0.8);
        shot.editor->tools()->setCurrentTool(SMUDGE);
        static_cast<StrokeTool*>(shot.editor->tools()->currentTool())->set(ToolProperty::Width, 50);
        stroke(shot, { { 130, -190 }, { 150, -130 }, { 190, -80 }, { 200, -30 } }, 0.8);
        stroke(shot, { { 250, -190 }, { 270, -130 }, { 300, -80 }, { 300, -30 } }, 0.8);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-smudge");
    }

    SECTION("polyline")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, POLYLINE, QColor(30, 30, 30), 5);
        for (QPointF p : Points{ { 80, -200 }, { 200, -230 }, { 320, -150 }, { 250, -60 } }) { tap(shot, p); }
        tablet(shot, QEvent::TabletMove, { 140, -30 }, 0, {}, false);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-polyline");
    }

    SECTION("shape")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, SHAPE, QColor(30, 30, 30), 5);
        auto* tool = static_cast<StrokeTool*>(shot.editor->tools()->currentTool());
        tool->set(ToolProperty::ShapeMode, 1); // Rectangle
        stroke(shot, { { 60, -230 }, { 190, -140 } });
        tool->set(ToolProperty::ShapeMode, 2); // Ellipse
        stroke(shot, { { 230, -230 }, { 340, -140 } });
        tool->set(ToolProperty::ShapeMode, 0); // Line
        stroke(shot, { { 60, -90 }, { 190, -20 } });
        tool->set(ToolProperty::ShapeMode, 3); // Polygon
        for (QPointF p : Points{ { 250, -80 }, { 330, -90 }, { 350, -20 }, { 290, 20 } }) { tap(shot, p); }
        tap(shot, { 250, -80 });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-shape");
    }

    SECTION("bucket")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, SHAPE, QColor(30, 30, 30), 5);
        auto* tool = static_cast<StrokeTool*>(shot.editor->tools()->currentTool());
        tool->set(ToolProperty::ShapeMode, 2);
        stroke(shot, { { 80, -230 }, { 200, -120 } });
        tool->set(ToolProperty::ShapeMode, 1);
        stroke(shot, { { 230, -230 }, { 340, -120 } });
        shot.editor->tools()->setCurrentTool(BUCKET);
        shot.editor->color()->setFrontColor(QColor(240, 170, 60));
        tap(shot, { 140, -175 });
        shot.editor->color()->setFrontColor(QColor(110, 190, 120));
        tap(shot, { 285, -175 });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-bucket");
    }

    SECTION("lasso-fill")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, LASSOFILL, QColor(120, 170, 230));
        stroke(shot, { { 100, -220 }, { 240, -240 }, { 340, -170 }, { 310, -60 }, { 200, -30 }, { 110, -90 }, { 100, -220 } });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-lasso-fill");
    }

    SECTION("eyedropper")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, BRUSH, QColor(200, 70, 50), 40);
        stroke(shot, { { 60, -170 }, { 340, -170 } }, 0.8);
        shot.editor->color()->setFrontColor(QColor(40, 100, 200));
        stroke(shot, { { 60, -100 }, { 340, -100 } }, 0.8);
        shot.editor->color()->setFrontColor(QColor(30, 30, 30));
        shot.editor->tools()->setCurrentTool(EYEDROPPER);
        tablet(shot, QEvent::TabletMove, { 200, -170 }, 0, {}, false);
        tap(shot, { 200, -170 });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-eyedropper");
    }

    SECTION("select")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->layers()->setCurrentLayer(shot.editor->layers()->createBitmapLayer("Practice"));
        shot.editor->tools()->setCurrentTool(PENCIL);
        shot.editor->color()->setFrontColor(QColor(30, 30, 30));
        stroke(shot, ring({ 200, -140 }, 60, 40), 0.7, true);
        shot.editor->tools()->setCurrentTool(SELECT);
        stroke(shot, { { 120, -200 }, { 290, -75 } });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-select");
    }

    SECTION("move")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->layers()->setCurrentLayer(shot.editor->layers()->createBitmapLayer("Practice"));
        shot.editor->tools()->setCurrentTool(PENCIL);
        shot.editor->color()->setFrontColor(QColor(30, 30, 30));
        stroke(shot, ring({ 150, -140 }, 60, 40), 0.7, true);
        shot.editor->tools()->setCurrentTool(SELECT);
        stroke(shot, { { 70, -200 }, { 240, -75 } });
        shot.editor->tools()->setCurrentTool(MOVE);
        tablet(shot, QEvent::TabletMove, { 150, -140 }, 0, {}, false);
        stroke(shot, { { 150, -140 }, { 230, -110 } });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-move");

    }

    SECTION("transform-key")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->tools()->setCurrentTool(MOVE);
        settle(shot);
        QCheckBox* box = nullptr;
        for (QCheckBox* c : shot.window.findChildren<QCheckBox*>())
        {
            if (c->text() == "Transform Key") { box = c; }
        }
        REQUIRE(box);
        box->click();
        stroke(shot, { { -260, -150 }, { -200, -110 } });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-transform-key");
    }

    SECTION("hand")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->tools()->setCurrentTool(HAND);
        // Screen points: the View moves under the pointer during a pan.
        const QPointF from = onScreen(shot, { 0, 0 });
        tabletAt(shot, QEvent::TabletPress, from, 0.5);
        for (int i = 1; i <= 8; ++i) { tabletAt(shot, QEvent::TabletMove, from + QPointF(-12, 5) * i, 0.5); }
        tabletAt(shot, QEvent::TabletRelease, from + QPointF(-96, 40), 0.5);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-hand");
    }

    SECTION("zoom")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->tools()->setCurrentTool(ZOOM);
        tap(shot, { -200, -100 });
        settle(shot);
        saveShot(&shot.window, "drawing-tools-zoom");
    }

    SECTION("rotate-view")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->tools()->setCurrentTool(ROTATE);
        shot.editor->view()->rotate(25);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-rotate-view");
    }

    SECTION("brush-outline")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, BRUSH, QColor(200, 70, 50), 70);
        stroke(shot, wave(40, 280, -150, 25, 1), 0.8, true);
        tablet(shot, QEvent::TabletMove, { 240, -50 }, 0, {}, false);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-brush-outline");
    }

    SECTION("ruler")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, PEN, QColor(40, 90, 180), 12);
        shot.editor->overlays()->setRuler(QLineF(40, -200, 340, -40));
        shot.editor->overlays()->setRulerEnabled(true);
        stroke(shot, { { 90, -172 }, { 150, -135 }, { 220, -90 }, { 300, -60 } }, 0.9);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-ruler");
    }

    SECTION("overlays-menu")
    {
        FreshGroups fresh;
        ShotWindow shot;
        saveMenu(shot.window.findChild<QMenu*>("menuOverlays"), "drawing-tools-overlays-menu");
    }

    SECTION("perspective")
    {
        FreshGroups fresh;
        ShotWindow shot;
        useTool(shot, PEN, QColor(40, 90, 180), 6);
        shot.editor->preference()->set<SETTING::OVERLAY_PERSPECTIVE1>(true);
        shot.editor->overlays()->setPerspectiveSnapEnabled(true);
        stroke(shot, { { 300, -225 }, { 220, -150 }, { 170, -140 }, { 90, -68 } }, 0.9);
        stroke(shot, { { 300, 225 }, { 220, 150 }, { 170, 140 }, { 90, 68 } }, 0.9);
        settle(shot);
        saveShot(&shot.window, "drawing-tools-perspective");
        shot.editor->preference()->set<SETTING::OVERLAY_PERSPECTIVE1>(false);
    }

    SECTION("wheel")
    {
        FreshGroups fresh;
        ShotWindow shot;
        shot.editor->tools()->setCurrentTool(PENCIL);
        ScribbleArea* area = shot.editor->getScribbleArea();
        const QPointF pos = onScreen(shot, { 0, 0 });
        for (int i = 0; i < 3; ++i)
        {
            QWheelEvent e(pos, area->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
                          Qt::NoScrollPhase, false);
            QApplication::sendEvent(area, &e);
        }
        settle(shot);
        saveShot(&shot.window, "drawing-tools-wheel");
    }
}
