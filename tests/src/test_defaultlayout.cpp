#include "catch.hpp"

#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QTextEdit>

#include "defaultlayout.h"

namespace
{
QDockWidget* makeDock(QMainWindow* win, const char* name)
{
    auto* dock = new QDockWidget(name, win);
    dock->setObjectName(name);
    auto* body = new QWidget(dock);
    body->setMinimumSize(120, 90);
    dock->setWidget(body);
    return dock;
}
}

TEST_CASE("Default dock layout", "[DockLayout]")
{
    for (const QSize screen : {QSize(1366, 768), QSize(3840, 2160)})
    {
        QMainWindow win;
        win.setCentralWidget(new QTextEdit);
        DefaultDockLayout docks;
        docks.left = {makeDock(&win, "a"), makeDock(&win, "b"), makeDock(&win, "c")};
        docks.right = {makeDock(&win, "d"), makeDock(&win, "e"), makeDock(&win, "f")};
        docks.bottom = makeDock(&win, "g");
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
