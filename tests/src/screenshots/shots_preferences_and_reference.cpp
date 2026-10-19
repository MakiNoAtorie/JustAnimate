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

#include <QApplication>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>

#include "preferencesdialog.h"
#include "screenshots.h"
#include "shortcutspage.h"

namespace
{
// Scrolls the current page's scroll area to `value` of its range, 0 for the top and 1 for the end.
void scrollPage(PreferencesDialog* prefs, double value)
{
    auto* pages = prefs->findChild<QStackedWidget*>("pagesWidget");
    REQUIRE(pages);
    auto* area = pages->currentWidget()->findChild<QScrollArea*>();
    REQUIRE(area);
    auto* bar = area->verticalScrollBar();
    bar->setValue(int(bar->maximum() * value));
    QApplication::processEvents();
}
} // namespace

// Shots for docs/manual/*-preferences-and-reference.md. Each SECTION saves images/preferences-and-reference-<shot>.png.
TEST_CASE("Manual screenshots: Preferences and reference", "[.screenshots]")
{
    SECTION("edit-menu")
    {
        ShotWindow shot;
        saveMenu(shot.window.findChild<QMenu*>("menuEdit"), "preferences-and-reference-edit-menu");
    }

    SECTION("general-top")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 0);
        scrollPage(prefs, 0);
        saveShot(prefs, "preferences-and-reference-general-top");
        prefs->close();
    }

    SECTION("general-middle")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 0);
        scrollPage(prefs, 0.5);
        saveShot(prefs, "preferences-and-reference-general-middle");
        prefs->close();
    }

    SECTION("general-bottom")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 0);
        scrollPage(prefs, 1);
        saveShot(prefs, "preferences-and-reference-general-bottom");
        prefs->close();
    }

    SECTION("files")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 1);
        saveShot(prefs, "preferences-and-reference-files");
        prefs->close();
    }

    SECTION("timeline")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 2);
        saveShot(prefs, "preferences-and-reference-timeline");
        prefs->close();
    }

    SECTION("timeline-bottom")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 2);
        scrollPage(prefs, 1);
        saveShot(prefs, "preferences-and-reference-timeline-bottom");
        prefs->close();
    }

    SECTION("tools")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 3);
        saveShot(prefs, "preferences-and-reference-tools");
        prefs->close();
    }

    SECTION("shortcuts")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 4);
        saveShot(prefs, "preferences-and-reference-shortcuts");
        prefs->close();
    }

    SECTION("search")
    {
        ShotWindow shot;
        auto* prefs = openPreferences(shot.window, 0);
        QLineEdit* search = nullptr;
        for (auto* edit : prefs->findChildren<QLineEdit*>())
        {
            if (edit->placeholderText() == "Search preferences") { search = edit; }
        }
        REQUIRE(search);
        search->setText("grid");
        QApplication::processEvents();
        scrollPage(prefs, 0.5);
        saveShot(prefs, "preferences-and-reference-search");
        prefs->close();
    }

    SECTION("conflict")
    {
        ShotWindow shot;
        // The app's own box set-up, shown here and never exec()ed.
        QMessageBox box(&shot.window);
        ShortcutsPage::setUpConflictBox(box, "P", "Pen Tool");
        box.show();
        QApplication::processEvents();
        saveShot(&box, "preferences-and-reference-conflict");
    }

    SECTION("layer-menu")
    {
        ShotWindow shot;
        saveMenu(shot.window.findChild<QMenu*>("menuLayer"), "preferences-and-reference-layer-menu");
    }

    SECTION("main-window")
    {
        ShotWindow shot;
        saveShot(&shot.window, "preferences-and-reference-main-window");
    }
}
