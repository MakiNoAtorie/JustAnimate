#include "catch.hpp"

#include <deque>

#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextEdit>

#include "defaultlayout.h"

namespace
{
struct TestDock
{
    TestDock(QMainWindow* win, const char* name) : dock(name, win)
    {
        dock.setObjectName(name);
        body.setMinimumSize(120, 90);
        dock.setWidget(&body);
    }
    QDockWidget dock;
    QWidget body;
};

// Declare after the window so the docks go first.
struct Docks
{
    QDockWidget* add(QMainWindow* win, const char* name) { return &owned.emplace_back(win, name).dock; }
    std::deque<TestDock> owned;
};
}

TEST_CASE("Default dock layout", "[DockLayout]")
{
    for (const QSize screen : {QSize(1366, 768), QSize(3840, 2160)})
    {
        QMainWindow win;
        QTextEdit central;
        win.setCentralWidget(&central);
        Docks owned;
        DefaultDockLayout docks;
        docks.left = {owned.add(&win, "a"), owned.add(&win, "b"), owned.add(&win, "c")};
        docks.right = {owned.add(&win, "d"), owned.add(&win, "e"), owned.add(&win, "f")};
        docks.bottom = owned.add(&win, "g");
        const QList<QDockWidget*> all = docks.left + docks.right + QList<QDockWidget*>{docks.bottom};

        // Scramble first so the test proves the function restores the layout
        for (QDockWidget* d : all) { win.addDockWidget(Qt::TopDockWidgetArea, d); }
        win.resize(screen);
        applyDefaultDockLayout(&win, docks, screen);
        win.show();
        QApplication::processEvents();

        const QRect inside = win.rect();
        for (int i = 0; i < all.size(); ++i)
        {
            REQUIRE(all[i]->isVisible());
            REQUIRE(!all[i]->isFloating());
            REQUIRE(inside.contains(all[i]->geometry()));
            for (int j = i + 1; j < all.size(); ++j)
            {
                INFO(all[i]->objectName().toStdString() << " vs " << all[j]->objectName().toStdString());
                REQUIRE(!all[i]->geometry().intersects(all[j]->geometry()));
            }
        }
        REQUIRE(win.centralWidget()->width() > screen.width() / 2);
        REQUIRE(win.dockWidgetArea(docks.bottom) == Qt::BottomDockWidgetArea);
        REQUIRE(win.dockWidgetArea(docks.left[0]) == Qt::LeftDockWidgetArea);
        REQUIRE(win.dockWidgetArea(docks.right[0]) == Qt::RightDockWidgetArea);
    }
}

TEST_CASE("Workspaces", "[DockLayout]")
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("ws.ini"), QSettings::IniFormat);
    QMainWindow win;
    QTextEdit central;
    win.setCentralWidget(&central);
    Docks owned;
    DefaultDockLayout docks;
    docks.left = {owned.add(&win, "ToolBox")};
    docks.right = {owned.add(&win, "ColorPalette"), owned.add(&win, "Onion Skin")};
    docks.bottom = owned.add(&win, "TimeLine");
    const QSize screen(1366, 768);
    win.resize(screen);
    win.show();

    switchWorkspace(&win, docks, screen, settings, "Animate");
    REQUIRE(docks.bottom->isVisible());

    switchWorkspace(&win, docks, screen, settings, "Ink");
    QApplication::processEvents();
    REQUIRE(!docks.bottom->isVisible());
    REQUIRE(!docks.right[0]->isVisible());
    REQUIRE(docks.left[0]->isVisible());

    // An edit to a workspace comes back when it is selected again
    docks.right[1]->show();
    switchWorkspace(&win, docks, screen, settings, "Draw");
    REQUIRE(!docks.right[1]->isVisible());
    switchWorkspace(&win, docks, screen, settings, "Ink");
    QApplication::processEvents();
    REQUIRE(docks.right[1]->isVisible());
    REQUIRE(currentWorkspace(settings) == "Ink");
}

TEST_CASE("Reset restores only the current workspace", "[DockLayout]")
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("ws.ini"), QSettings::IniFormat);
    QMainWindow win;
    QTextEdit central;
    win.setCentralWidget(&central);
    Docks owned;
    DefaultDockLayout docks;
    docks.left = {owned.add(&win, "ToolBox")};
    docks.right = {owned.add(&win, "ColorPalette"), owned.add(&win, "Onion Skin")};
    docks.bottom = owned.add(&win, "TimeLine");
    const QSize screen(1366, 768);
    win.resize(screen);
    win.show();

    // Ink keeps an edit: Onion Skin shown
    switchWorkspace(&win, docks, screen, settings, "Ink");
    docks.right[1]->show();
    switchWorkspace(&win, docks, screen, settings, "Draw");
    docks.bottom->show();
    saveCurrentWorkspace(&win, settings);

    resetCurrentWorkspace(&win, docks, screen, settings);
    QApplication::processEvents();
    REQUIRE(currentWorkspace(settings) == "Draw");
    REQUIRE(!docks.bottom->isVisible());

    switchWorkspace(&win, docks, screen, settings, "Ink");
    QApplication::processEvents();
    REQUIRE(docks.right[1]->isVisible());
}
