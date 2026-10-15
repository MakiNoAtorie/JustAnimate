#include "defaultlayout.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QtGlobal>

void applyDefaultDockLayout(QMainWindow* window, const DefaultDockLayout& docks, QSize screen)
{
    const auto place = [window](Qt::DockWidgetArea area, const QList<QDockWidget*>& list)
    {
        for (QDockWidget* dock : list)
        {
            dock->setFloating(false);
            window->addDockWidget(area, dock);
            dock->show();
        }
    };
    place(Qt::RightDockWidgetArea, docks.right);
    place(Qt::LeftDockWidgetArea, docks.left);
    place(Qt::BottomDockWidgetArea, {docks.bottom});

    // Side columns run full height; the timeline spans only the canvas width
    window->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    window->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    const int sideWidth = qBound(180, screen.width() * 15 / 100, 340);
    const int bottomHeight = qBound(150, screen.height() * 22 / 100, 320);
    window->resizeDocks(docks.left + docks.right, QList<int>(docks.left.size() + docks.right.size(), sideWidth), Qt::Horizontal);
    window->resizeDocks({docks.bottom}, {bottomHeight}, Qt::Vertical);
}
