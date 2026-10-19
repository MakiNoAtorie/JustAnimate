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
#ifndef SCREENSHOTS_H
#define SCREENSHOTS_H

#include <functional>

#include <QDockWidget>
#include <QEvent>
#include <QPointF>
#include <QSize>
#include <QString>

#include "mainwindow2.h"
#include "testfixture.h"

class Editor;
class Layer;
class Object;
class PreferencesDialog;
class QAction;
class QMenu;
class QPixmap;

// Manual screenshots (docs/CONVENTIONS.md, "Shots"). This header is the only shared shot helper.

/// The demo project, built from a fixed seed. Layers bottom first: Camera (moves from frame 1 to 24),
/// Background (bitmap), Ball group with Rough (bitmap, Marked Drawings on 1 4 7 10 13) and Line (vector,
/// on 1 7 13), and Voice (sound from frame 1, a lip-sync marker on 5). Global markers on 1 and 13.
Object* makeDemoProject();

/// The real main window at shotSize(), Light theme, default Accent colour and default Layout,
/// on the demo project at frame 1 with the Rough layer current. Settings go back at scope exit.
struct ShotWindow
{
    ShotWindow();
    ~ShotWindow();
    Q_DISABLE_COPY(ShotWindow)

    SettingsGuard guard;
    MainWindow2 window;
    Editor* editor = nullptr;
};

QSize shotSize();

/// Saves a grab of `widget` as <dir>/<name>.png and returns the path. The default dir is
/// $JUSTANIMATE_SHOT_DIR, which the `screenshots` target sets to <build>/manual/images.
QString saveShot(QWidget* widget, const QString& name, QString dir = QString());

/// Saves a picture made in the shot (a composite, or a grab of a window that is closed now).
QString savePicture(const QPixmap& picture, const QString& name);

/// A popup menu is its own window: opens `menu`, grabs it, closes it and saves the grab.
QString saveMenu(QMenu* menu, const QString& name);

/// Calls `use` with the popup menu that the next exec() opens, then closes that menu.
void whenMenuOpens(const std::function<void(QMenu*)>& use);

/// Finders by objectName; the shot fails when the object is missing.
QAction* actionNamed(QWidget* window, const char* objectName);
QDockWidget* panelDock(QWidget* window, const char* objectName);

/// Shows the Panel in front of the other tabs of its Panel group.
QDockWidget* raisePanel(QWidget* window, const char* objectName);

/// Shows the Panel as its own window at `size`, so that no control hides behind a scroll bar.
QDockWidget* floatPanel(QWidget* window, const char* objectName, QSize size);

/// Shows the Timeline Panel, tall enough for every layer row of the demo project.
QDockWidget* tallTimeline(QWidget* window);

Layer* layerNamed(Editor* editor, const QString& name);

/// Opens the Preferences dialog on page `page` (0 General, 1 Files, 2 Timeline, 3 Tools, 4 Shortcuts).
PreferencesDialog* openPreferences(MainWindow2& window, int page);

/// One pen sample; `time` 0 keeps the event's own timestamp.
struct PenPoint
{
    QPointF pos;
    qreal pressure = 0.7;
    int tiltX = 0;
    int tiltY = 0;
    ulong time = 0;
};

/// Sends a tablet event at widget point `p.pos`. With `down` false, a TabletMove is a hover.
void sendTablet(QWidget* target, QEvent::Type type, const PenPoint& p, Qt::KeyboardModifiers mods = {}, bool down = true);

#endif // SCREENSHOTS_H
