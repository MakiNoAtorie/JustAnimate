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

#include <QAction>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QShortcutEvent>
#include <QTabletEvent>

#include "editor.h"
#include "object.h"
#include "scribblearea.h"
#include "shortcutfilter.h"
#include "testfixture.h"

namespace
{
void sendPen(ScribbleArea* area, QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QPointingDevice pen("test pen", 4242, QInputDevice::DeviceType::Stylus,
                        QPointingDevice::PointerType::Pen,
                        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure,
                        1, 3);
    QPointF pos(10, 10);
    QTabletEvent e(type, &pen, pos, pos, 0.5, 0, 0, 0, 0, 0, Qt::NoModifier, button, buttons);
    QCoreApplication::sendEvent(area, &e);
}

void sendMouse(ScribbleArea* area, QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QPointF pos(10, 10);
    QMouseEvent e(type, pos, pos, button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(area, &e);
}

// Ask the filter directly: Qt asserts on a QShortcutEvent sent to an action with a different shortcut id.
bool shortcutTriggers(QObject& filter, QAction* action)
{
    QShortcutEvent e(action->shortcut(), 0);
    return !filter.eventFilter(action, &e);
}
}

TEST_CASE("ShortcutFilter: shortcuts are blocked while a pointer is drawing")
{
    TestSession session;
    Object* object = session.object;
    object->addNewBitmapLayer();

    session.start();
    ScribbleArea* scribbleArea = session.area.get();
    scribbleArea->init();

    ShortcutFilter filter(scribbleArea, nullptr);
    QAction action;
    action.setShortcut(QKeySequence(Qt::Key_E));
    action.installEventFilter(&filter);

    SECTION("No pointer down: the shortcut fires")
    {
        REQUIRE(shortcutTriggers(filter, &action));
    }

    SECTION("Pen down: the shortcut is ignored until pen up")
    {
        sendPen(scribbleArea, QEvent::TabletPress, Qt::LeftButton, Qt::LeftButton);
        REQUIRE(scribbleArea->isTabletInUse());
        REQUIRE_FALSE(shortcutTriggers(filter, &action));

        sendPen(scribbleArea, QEvent::TabletRelease, Qt::LeftButton, Qt::NoButton);
        REQUIRE(shortcutTriggers(filter, &action));
    }

    SECTION("Mouse down: the shortcut is ignored until mouse up")
    {
        sendMouse(scribbleArea, QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
        REQUIRE(scribbleArea->isMouseInUse());
        REQUIRE_FALSE(shortcutTriggers(filter, &action));

        sendMouse(scribbleArea, QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
        REQUIRE(shortcutTriggers(filter, &action));
    }
}
