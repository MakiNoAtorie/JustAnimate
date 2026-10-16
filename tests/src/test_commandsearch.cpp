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
#include <QFocusEvent>
#include <QMouseEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>

#include "commandsearch.h"

namespace
{
struct Bar
{
    QMenuBar bar;
    QAction* copy = nullptr;
    QAction* paste = nullptr;
    int copied = 0;
    int pasted = 0;

    Bar()
    {
        QMenu* edit = bar.addMenu("&Edit");
        copy = edit->addAction("&Copy");
        copy->setShortcut(QKeySequence("Ctrl+C"));
        edit->addSeparator();
        paste = edit->addAction("Paste");
        paste->setEnabled(false);
        QMenu* view = bar.addMenu("View");
        view->addMenu("Zoom")->addAction("Zoom In...");
        QObject::connect(copy, &QAction::triggered, [this] { copied++; });
        QObject::connect(paste, &QAction::triggered, [this] { pasted++; });
    }
};
}

TEST_CASE("Command search lists menu commands with path, shortcut and state")
{
    Bar b;
    const QList<CommandEntry> all = collectCommands(&b.bar);
    REQUIRE(all.size() == 3);
    CHECK(all[0].name == "Edit > Copy");
    CHECK(all[0].shortcut == QKeySequence("Ctrl+C").toString(QKeySequence::NativeText));
    CHECK(all[0].enabled);
    CHECK_FALSE(all[1].enabled);
    CHECK(all[2].name == "View > Zoom > Zoom In");
}

TEST_CASE("Command search matches every word, ignoring case")
{
    Bar b;
    const QList<CommandEntry> all = collectCommands(&b.bar);
    CHECK(filterCommands(all, "").size() == 3);
    CHECK(filterCommands(all, "ZOOM in").size() == 1);
    CHECK(filterCommands(all, "edit paste").size() == 1);
    CHECK(filterCommands(all, "zoom copy").isEmpty());
}

TEST_CASE("Command search runs an enabled command and refuses a disabled one")
{
    Bar b;
    CommandSearch search(collectCommands(&b.bar), nullptr);
    CHECK(search.runCurrent());
    CHECK(b.copied == 1);

    CommandSearch disabled(filterCommands(collectCommands(&b.bar), "paste"), nullptr);
    CHECK_FALSE(disabled.runCurrent());
    CHECK(b.pasted == 0);
}

TEST_CASE("Command search field opens once per click and not on focus return")
{
    CommandSearchField field(nullptr);
    int opened = 0;
    QObject::connect(&field, &CommandSearchField::activated, [&] { opened++; });

    QFocusEvent clickFocus(QEvent::FocusIn, Qt::MouseFocusReason);
    QCoreApplication::sendEvent(&field, &clickFocus);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&field, &press);
    CHECK(opened == 1);

    QFocusEvent back(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
    QCoreApplication::sendEvent(&field, &back);
    CHECK(opened == 1);

    QFocusEvent tab(QEvent::FocusIn, Qt::TabFocusReason);
    QCoreApplication::sendEvent(&field, &tab);
    CHECK(opened == 2);
}
