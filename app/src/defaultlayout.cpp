#include "defaultlayout.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QtGlobal>

#include "pencildef.h"

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

namespace
{
// Untranslated names are the settings keys; menus show QCoreApplication::translate("Workspace", name).
const char* const kWorkspaces[] = {QT_TRANSLATE_NOOP("Workspace", "Draw"), QT_TRANSLATE_NOOP("Workspace", "Animate"),
                                   QT_TRANSLATE_NOOP("Workspace", "Ink")};

void applyPreset(QMainWindow* window, const DefaultDockLayout& docks, QSize screen, const QString& name)
{
    applyDefaultDockLayout(window, docks, screen);
    QStringList hidden;
    if (name == "Draw") { hidden = {"TimeLine", "Onion Skin"}; }
    if (name == "Ink") { hidden = {"TimeLine", "Onion Skin", "Color Inspector", "ColorPalette"}; }
    for (QDockWidget* dock : window->findChildren<QDockWidget*>())
    {
        if (hidden.contains(dock->objectName())) { dock->hide(); }
    }
}
}

QStringList workspaceNames()
{
    QStringList names;
    for (const char* name : kWorkspaces) { names << QString::fromLatin1(name); }
    return names;
}

QString currentWorkspace(QSettings& settings)
{
    const QString name = settings.value(SETTING_WORKSPACE_CURRENT).toString();
    return workspaceNames().contains(name) ? name : "Animate";
}

void saveCurrentWorkspace(QMainWindow* window, QSettings& settings)
{
    settings.setValue(SETTING_WORKSPACE_STATE + currentWorkspace(settings), window->saveState());
}

void switchWorkspace(QMainWindow* window, const DefaultDockLayout& docks, QSize screen, QSettings& settings, const QString& name)
{
    if (!workspaceNames().contains(name)) { return; }
    saveCurrentWorkspace(window, settings);
    settings.setValue(SETTING_WORKSPACE_CURRENT, name);

    const QByteArray saved = settings.value(SETTING_WORKSPACE_STATE + name).toByteArray();
    if (!saved.isEmpty() && window->restoreState(saved)) { return; }
    applyPreset(window, docks, screen, name);
}

void resetCurrentWorkspace(QMainWindow* window, const DefaultDockLayout& docks, QSize screen, QSettings& settings)
{
    const QString name = currentWorkspace(settings);
    settings.remove(SETTING_WORKSPACE_STATE + name);
    applyPreset(window, docks, screen, name);
}
