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
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QWindow>

#include "duplicatepressfilter.h"

// A pen tap on Windows reaches the app twice: the mouse press Qt synthesizes from the
// unaccepted QTabletEvent, and a second press from the tablet driver. The second press
// toggled the menu closed.
TEST_CASE("Pen tap on a menu bar entry leaves the menu open", "[PenMenu]")
{
    DuplicatePressFilter filter;
    qApp->installEventFilter(&filter);

    QMainWindow window;
    QMenu* menu = window.menuBar()->addMenu("File");
    menu->addAction("New");
    window.resize(400, 300);
    window.show();
    QApplication::processEvents();

    const QPointingDevice pen("pen", 4242, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
                              QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 3);
    const QPointingDevice* mouseDevice = QPointingDevice::primaryPointingDevice();
    const QRect r = window.menuBar()->actionGeometry(menu->menuAction());
    const QPoint global = window.menuBar()->mapToGlobal(r.center());

    // Qt delivers pointer events to the open popup's window first, else to the window under the pen.
    auto target = [&]() {
        QWidget* popup = QApplication::activePopupWidget();
        return popup ? popup->windowHandle() : window.windowHandle();
    };
    auto tablet = [&](ulong t, QEvent::Type type, Qt::MouseButtons b, qreal p) {
        QWindow* w = target();
        const QPointF local = w->mapFromGlobal(global);
        QTabletEvent e(type, &pen, local, global, p, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, b);
        e.setTimestamp(t);
        QCoreApplication::sendEvent(w, &e);
    };
    auto mouse = [&](ulong t, const QPointingDevice* device, QEvent::Type type, Qt::MouseButtons b) {
        QWindow* w = target();
        const QPointF local = w->mapFromGlobal(global);
        QMouseEvent e(type, local, local, global, Qt::LeftButton, b, Qt::NoModifier, device);
        e.setTimestamp(t);
        QCoreApplication::sendEvent(w, &e);
        QApplication::processEvents();
    };
    auto penTap = [&](ulong t) {
        tablet(t, QEvent::TabletPress, Qt::LeftButton, 0.5);
        mouse(t, &pen, QEvent::MouseButtonPress, Qt::LeftButton);
        mouse(t + 2, mouseDevice, QEvent::MouseButtonPress, Qt::LeftButton);
        tablet(t + 70, QEvent::TabletRelease, Qt::NoButton, 0);
        mouse(t + 70, &pen, QEvent::MouseButtonRelease, Qt::NoButton);
        mouse(t + 72, mouseDevice, QEvent::MouseButtonRelease, Qt::NoButton);
    };

    penTap(1010);
    CHECK(menu->isVisible());

    SECTION("A second tap on the entry still closes the menu")
    {
        penTap(1600);
        CHECK_FALSE(menu->isVisible());
    }

    menu->hide();
}
