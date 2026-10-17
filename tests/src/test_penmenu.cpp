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
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointingDevice>
#include <qpa/qwindowsysteminterface.h>

#include "duplicatepressfilter.h"

// A pen tap on Windows reaches the app twice: the mouse press Qt synthesizes from the
// unaccepted QTabletEvent, and a second press from the tablet driver. The second press
// toggled the menu closed.
TEST_CASE("Pen tap on a menu bar entry leaves the menu open", "[PenMenu]")
{
    DuplicatePressFilter filter;
    qApp->installEventFilter(&filter);
    // The test sends the synthesized mouse events itself, the same on every platform plugin.
    const bool synthesize = qApp->testAttribute(Qt::AA_SynthesizeMouseForUnhandledTabletEvents);
    qApp->setAttribute(Qt::AA_SynthesizeMouseForUnhandledTabletEvents, false);

    QMainWindow window;
    QMenu* menu = window.menuBar()->addMenu("File");
    menu->addAction("New");
    window.resize(400, 300);
    window.show();
    QApplication::processEvents();

    QPointingDevice pen("pen", 4242, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
                        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 3);
    QWindowSystemInterface::registerInputDevice(&pen);

    QWindow* w = window.windowHandle();
    const QRect r = window.menuBar()->actionGeometry(menu->menuAction());
    const QPointF local = window.menuBar()->mapTo(&window, r.center());
    const QPointF global = window.mapToGlobal(local);

    auto tablet = [&](ulong t, Qt::MouseButtons b, qreal p) {
        QWindowSystemInterface::handleTabletEvent(w, t, &pen, local, global, b, p, 0, 0, 0, 0, 0, Qt::NoModifier);
        QApplication::processEvents();
    };
    auto mouse = [&](ulong t, const QPointingDevice* device, QEvent::Type type, Qt::MouseButtons b) {
        QWindowSystemInterface::handleMouseEvent(w, t, device, local, global, b, Qt::LeftButton, type);
        QApplication::processEvents();
    };
    auto penTap = [&](ulong t) {
        tablet(t, Qt::LeftButton, 0.5);
        mouse(t, &pen, QEvent::MouseButtonPress, Qt::LeftButton);
        mouse(t + 2, QPointingDevice::primaryPointingDevice(), QEvent::MouseButtonPress, Qt::LeftButton);
        tablet(t + 70, Qt::NoButton, 0);
        mouse(t + 70, &pen, QEvent::MouseButtonRelease, Qt::NoButton);
        mouse(t + 72, QPointingDevice::primaryPointingDevice(), QEvent::MouseButtonRelease, Qt::NoButton);
    };

    tablet(1000, Qt::NoButton, 0);
    penTap(1010);
    CHECK(menu->isVisible());

    SECTION("A second tap on the entry still closes the menu")
    {
        penTap(1600);
        CHECK_FALSE(menu->isVisible());
    }

    menu->hide();
    qApp->setAttribute(Qt::AA_SynthesizeMouseForUnhandledTabletEvents, synthesize);
}
