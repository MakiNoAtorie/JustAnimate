#include "defaultlayout.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
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

QStringList workspaceNames()
{
    return {"Draw", "Animate", "Ink"};
}

QString currentWorkspace(QSettings& settings)
{
    const QString name = settings.value("Workspace/Current").toString();
    return workspaceNames().contains(name) ? name : "Animate";
}

void saveCurrentWorkspace(QMainWindow* window, QSettings& settings)
{
    settings.setValue("Workspace/State/" + currentWorkspace(settings), window->saveState());
}

void switchWorkspace(QMainWindow* window, const DefaultDockLayout& docks, QSize screen, QSettings& settings, const QString& name)
{
    if (!workspaceNames().contains(name)) { return; }
    saveCurrentWorkspace(window, settings);
    settings.setValue("Workspace/Current", name);

    const QByteArray saved = settings.value("Workspace/State/" + name).toByteArray();
    if (!saved.isEmpty() && window->restoreState(saved)) { return; }

    applyDefaultDockLayout(window, docks, screen);
    QStringList hidden;
    if (name == "Draw") { hidden = {"TimeLine", "Onion Skin"}; }
    if (name == "Ink") { hidden = {"TimeLine", "Onion Skin", "Color Inspector", "ColorPalette"}; }
    for (QDockWidget* dock : window->findChildren<QDockWidget*>())
    {
        if (hidden.contains(dock->objectName())) { dock->hide(); }
    }
}
