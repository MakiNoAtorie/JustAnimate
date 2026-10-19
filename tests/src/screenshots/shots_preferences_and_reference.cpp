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
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>

#include "preferencesdialog.h"
#include "screenshots.h"

namespace
{
// Opens the Preferences dialog on page `row` (0 General, 1 Files, 2 Timeline, 3 Tools, 4 Shortcuts).
PreferencesDialog* openPage(ShotWindow& shot, int row)
{
    shot.window.preferences();
    auto* prefs = shot.window.findChild<PreferencesDialog*>();
    REQUIRE(prefs);
    auto* pages = prefs->findChild<QListWidget*>("contentsWidget");
    REQUIRE(pages);
    pages->setCurrentRow(row);
    prefs->show();
    QApplication::processEvents();
    return prefs;
}

// A popup menu is its own window: grab it into a label.
void saveMenu(ShotWindow& shot, const char* menuName, const QString& name)
{
    auto* menu = shot.window.findChild<QMenu*>(menuName);
    REQUIRE(menu);
    menu->popup(QPoint(20, 20));
    QApplication::processEvents();
    const QPixmap picture = menu->grab();
    menu->hide();
    QLabel label;
    label.setPixmap(picture);
    label.resize(picture.size() / picture.devicePixelRatio());
    saveShot(&label, name);
}

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
        saveMenu(shot, "menuEdit", "preferences-and-reference-edit-menu");
    }

    SECTION("general-top")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 0);
        scrollPage(prefs, 0);
        saveShot(prefs, "preferences-and-reference-general-top");
        prefs->close();
    }

    SECTION("general-middle")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 0);
        scrollPage(prefs, 0.5);
        saveShot(prefs, "preferences-and-reference-general-middle");
        prefs->close();
    }

    SECTION("general-bottom")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 0);
        scrollPage(prefs, 1);
        saveShot(prefs, "preferences-and-reference-general-bottom");
        prefs->close();
    }

    SECTION("files")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 1);
        saveShot(prefs, "preferences-and-reference-files");
        prefs->close();
    }

    SECTION("timeline")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 2);
        saveShot(prefs, "preferences-and-reference-timeline");
        prefs->close();
    }

    SECTION("timeline-bottom")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 2);
        scrollPage(prefs, 1);
        saveShot(prefs, "preferences-and-reference-timeline-bottom");
        prefs->close();
    }

    SECTION("tools")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 3);
        saveShot(prefs, "preferences-and-reference-tools");
        prefs->close();
    }

    SECTION("shortcuts")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 4);
        saveShot(prefs, "preferences-and-reference-shortcuts");
        prefs->close();
    }

    SECTION("search")
    {
        ShotWindow shot;
        auto* prefs = openPage(shot, 0);
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
        // The app shows this box from ShortcutsPage: same texts, built here and never exec()ed.
        QMessageBox box(&shot.window);
        box.setText("Shortcut Conflict!");
        box.setInformativeText("P is already used by \"Pen Tool\", overwrite?");
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::No);
        box.setIcon(QMessageBox::Warning);
        box.show();
        QApplication::processEvents();
        saveShot(&box, "preferences-and-reference-conflict");
    }

    SECTION("layer-menu")
    {
        ShotWindow shot;
        saveMenu(shot, "menuLayer", "preferences-and-reference-layer-menu");
    }

    SECTION("main-window")
    {
        ShotWindow shot;
        saveShot(&shot.window, "preferences-and-reference-main-window");
    }
}
