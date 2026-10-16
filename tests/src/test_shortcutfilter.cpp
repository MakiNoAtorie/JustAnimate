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
#include <QSettings>
#include <QShortcutEvent>
#include <QTemporaryDir>
#include <QTabletEvent>
#include <QWidget>

#include "editor.h"
#include "object.h"
#include "scribblearea.h"
#include "shortcutconflict.h"
#include "shortcutfilter.h"
#include "pencildef.h"
#include "pencilsettings.h"
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

// Records the shortcut events that reach it and stops them before QAction::event.
struct ShortcutProbe : QObject
{
    bool reached = false;
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() != QEvent::Shortcut) { return false; }
        reached = true;
        return true;
    }
};
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

TEST_CASE("ShortcutFilter: every action under the main window is guarded")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    session.start();
    ScribbleArea* scribbleArea = session.area.get();
    scribbleArea->init();

    // An action in a nested widget, as in a dock or the timeline.
    QWidget window;
    QWidget* dock = new QWidget(&window);
    QAction* action = new QAction(dock);
    action->setShortcut(QKeySequence(Qt::Key_Period));
    ShortcutProbe probe;
    action->installEventFilter(&probe);

    ShortcutFilter filter(scribbleArea, nullptr);
    filter.installOnActions(&window);

    QShortcutEvent e(action->shortcut(), 0);
    sendPen(scribbleArea, QEvent::TabletPress, Qt::LeftButton, Qt::LeftButton);
    QCoreApplication::sendEvent(action, &e);
    REQUIRE_FALSE(probe.reached);

    sendPen(scribbleArea, QEvent::TabletRelease, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::sendEvent(action, &e);
    REQUIRE(probe.reached);
}

TEST_CASE("ShortcutConflict names the action that already uses a key sequence")
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("s.ini"), QSettings::IniFormat);
    settings.setValue("CmdA", "Ctrl+J");
    settings.setValue("CmdB", "Ctrl+K");
    settings.setValue("CmdC", "");

    REQUIRE(findShortcutConflict(settings, "CmdB", QKeySequence("Ctrl+J")) == "CmdA");
    REQUIRE(findShortcutConflict(settings, "CmdA", QKeySequence("Ctrl+J")).isEmpty());
    REQUIRE(findShortcutConflict(settings, "CmdA", QKeySequence("Ctrl+L")).isEmpty());
    REQUIRE(findShortcutConflict(settings, "CmdA", QKeySequence()).isEmpty());
}

TEST_CASE("The default shortcut preset has no duplicate keys")
{
    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    const QStringList cmds = preset.allKeys();
    REQUIRE(cmds.contains("CmdMarkKey"));
    for (const QString& cmd : cmds)
    {
        CAPTURE(cmd);
        CHECK(findShortcutConflict(preset, cmd, QKeySequence(preset.value(cmd).toString())).isEmpty());
    }
}

TEST_CASE("Default shortcuts: R selects the Rotate view tool and Z rotates nothing")
{
    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    CHECK(findShortcutConflict(preset, "", QKeySequence("R")) == CMD_TOOL_ROTATE);
    CHECK(findShortcutConflict(preset, "", QKeySequence("Z")).isEmpty());
    CHECK(preset.value(CMD_ROTATE_CLOCK).toString().isEmpty());
    CHECK(preset.value(CMD_ROTATE_ANTI_CLOCK).toString().isEmpty());
}

TEST_CASE("Saved shortcuts move R from the old rotate commands to the Rotate view tool")
{
    QSettings& settings = pencilSettings();
    const QString clock = QStringLiteral("shortcuts/") + CMD_ROTATE_CLOCK;
    const QString anti = QStringLiteral("shortcuts/") + CMD_ROTATE_ANTI_CLOCK;
    const QString tool = QStringLiteral("shortcuts/") + CMD_TOOL_ROTATE;
    const QByteArray c = clock.toUtf8(), a = anti.toUtf8(), t = tool.toUtf8();
    const SettingsGuard guard({ c.constData(), a.constData(), t.constData() });
    settings.setValue(clock, "R");
    settings.setValue(anti, "Z");
    settings.remove(tool);

    checkExistingShortcuts();
    CHECK(settings.value(clock).toString().isEmpty());
    CHECK(settings.value(anti).toString().isEmpty());
    CHECK(settings.value(tool).toString() == "R");

    // A key the user chose stays.
    settings.setValue(clock, "Ctrl+R");
    settings.remove(tool);
    checkExistingShortcuts();
    CHECK(settings.value(clock).toString() == "Ctrl+R");
}

TEST_CASE("Default shortcuts: Ctrl+K opens Preferences and Ctrl+Shift+P opens Command search")
{
    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    CHECK(findShortcutConflict(preset, "", QKeySequence("Ctrl+K")) == CMD_PREFERENCE);
    CHECK(findShortcutConflict(preset, "", QKeySequence("Ctrl+Shift+P")) == CMD_COMMAND_SEARCH);
}
