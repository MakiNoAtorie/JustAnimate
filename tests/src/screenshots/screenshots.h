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

#include <QSize>
#include <QString>

#include "mainwindow2.h"
#include "testfixture.h"

class Editor;
class Object;

// Manual screenshots (docs/CONVENTIONS.md, "Shots"). Chapter shot files use only this header.

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

#endif // SCREENSHOTS_H
