#ifndef DEFAULTLAYOUT_H
#define DEFAULTLAYOUT_H

#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

class QDockWidget;
class QMainWindow;
class QSettings;

// Layout version saved with the window state. Raise it to drop every saved Layout once.
#define SETTING_LAYOUT_VERSION "Workspace/LayoutVersion"

struct DefaultDockLayout
{
    QList<QDockWidget*> left;
    QList<QDockWidget*> right;
    QDockWidget* bottom = nullptr;
};

// Docks every widget in its default area and sizes the areas for a screen of the given size.
void applyDefaultDockLayout(QMainWindow* window, const DefaultDockLayout& docks, QSize screen);

// Built-in workspaces: Draw, Animate, Ink. Animate is the default layout.
QStringList workspaceNames();

// Name of the active workspace; Animate when none is stored.
QString currentWorkspace(QSettings& settings);

// Stores the window's docks as the active workspace, so edits survive a restart.
void saveCurrentWorkspace(QMainWindow* window, QSettings& settings);

// Saves the active workspace, then shows `name`: its saved state, or its built-in layout.
void switchWorkspace(QMainWindow* window, const DefaultDockLayout& docks, QSize screen, QSettings& settings, const QString& name);

// Drops the active workspace's saved state and shows its built-in layout. Other workspaces keep theirs.
void resetCurrentWorkspace(QMainWindow* window, const DefaultDockLayout& docks, QSize screen, QSettings& settings);

// Drops the saved window state and every saved workspace state when they come from an older Layout version.
// Returns true when it dropped them.
bool dropOutdatedLayouts(QSettings& settings);

#endif // DEFAULTLAYOUT_H
