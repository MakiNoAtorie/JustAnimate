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
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>

#include "basetool.h"
#include "editor.h"
#include "mainwindow2.h"
#include "pencilsettings.h"
#include "toolmanager.h"

namespace
{

QAction* menuAction(QMenu* menu, ToolType type)
{
    for (QAction* a : menu->actions())
        if (a->data().toInt() == type)
            return a;
    return nullptr;
}

bool shows(QToolButton* button, const QString& name)
{
    return button->toolTip().contains(name);
}

} // namespace

TEST_CASE("Tool groups share one button that shows the last tool used", "[ToolGroups]")
{
    const QString key = "ToolGroup/CmdToolPencil";
    pencilSettings().remove(key);
    {
        MainWindow2 window;
        window.show();
        QApplication::processEvents();
        Editor* editor = window.findChild<Editor*>();
        auto* pencil = window.findChild<QToolButton*>("pencilButton");
        auto* menu = window.findChild<QMenu*>("ToolGroupMenu_CmdToolPencil");
        REQUIRE(editor);
        REQUIRE(pencil);
        REQUIRE(menu);
        CHECK(!window.findChild<QToolButton*>("brushButton")->isVisibleTo(&window));

        QContextMenuEvent e(QContextMenuEvent::Mouse, QPoint(2, 2), pencil->mapToGlobal(QPoint(2, 2)));
        QApplication::sendEvent(pencil, &e);
        CHECK(menu->isVisible());
        QAction* brush = menuAction(menu, BRUSH);
        REQUIRE(brush);
        brush->trigger();
        CHECK(editor->tools()->currentTool()->type() == BRUSH);
        CHECK(shows(pencil, "Brush"));
        CHECK(pencilSettings().value(key).toInt() == BRUSH);

        editor->tools()->setCurrentTool(ERASER);
        pencil->click();
        CHECK(editor->tools()->currentTool()->type() == BRUSH);

        // a shortcut selects a grouped tool and updates its button
        window.findChild<QAction*>("actionPen")->trigger();
        CHECK(editor->tools()->currentTool()->type() == PEN);
        CHECK(shows(pencil, "Pen Tool"));
        CHECK(pencil->isChecked());
        CHECK(pencilSettings().value(key).toInt() == PEN);

        // a long press opens the menu
        menu->hide();
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(2, 2), QPointF(pencil->mapToGlobal(QPoint(2, 2))),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(pencil, &press);
        auto* timer = window.findChild<QTimer*>("ToolGroupPress");
        REQUIRE(timer);
        CHECK(timer->isActive());
        QMetaObject::invokeMethod(timer, "timeout");
        CHECK(menu->isVisible());
        menu->hide();
    }
    {
        MainWindow2 window;
        auto* pencil = window.findChild<QToolButton*>("pencilButton");
        REQUIRE(pencil);
        CHECK(shows(pencil, "Pen Tool"));
    }
    pencilSettings().remove(key);
}

TEST_CASE("The Hand group lists Zoom between Hand and Rotate view and Z selects it", "[ToolGroups]")
{
    const QString key = "ToolGroup/CmdToolHand";
    pencilSettings().remove(key);
    {
        MainWindow2 window;
        window.show();
        QApplication::processEvents();
        Editor* editor = window.findChild<Editor*>();
        auto* hand = window.findChild<QToolButton*>("handButton");
        auto* menu = window.findChild<QMenu*>("ToolGroupMenu_CmdToolHand");
        auto* zoom = window.findChild<QAction*>("actionZoomTool");
        REQUIRE(editor);
        REQUIRE(hand);
        REQUIRE(menu);
        REQUIRE(zoom);

        QList<int> order;
        for (QAction* a : menu->actions()) order << a->data().toInt();
        CHECK(order == QList<int>{ HAND, ZOOM, ROTATE });
        CHECK(zoom->shortcut() == QKeySequence("Z"));

        zoom->trigger();
        CHECK(editor->tools()->currentTool()->type() == ZOOM);
        CHECK(shows(hand, "Zoom Tool"));
        CHECK(hand->isChecked());
        CHECK(pencilSettings().value(key).toInt() == ZOOM);
    }
    pencilSettings().remove(key);
}
