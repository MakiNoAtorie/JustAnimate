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
#include <QPointingDevice>
#include <QTabletEvent>

#include "bitmapimage.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "viewmanager.h"

namespace
{

void pen(ScribbleArea* area, QEvent::Type type, QPointF pos, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0.8, 0, 0, 0, 0, 0, modifiers, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

struct ZoomSession
{
    TestSession s;
    LayerBitmap* layer = nullptr;
    Editor* editor = nullptr;
    ScribbleArea* area = nullptr;

    ZoomSession()
    {
        layer = s.object->addNewBitmapLayer();
        REQUIRE(s.start());
        editor = s.editor.get();
        area = s.area.get();
        area->init();
        area->resize(800, 600);
        editor->layers()->setCurrentLayer(layer);
        editor->tools()->setCurrentTool(ZOOM);
        REQUIRE(editor->tools()->currentTool()->type() == ZOOM);
        editor->object()->setModified(false);
    }

    // Presses at canvas point `anchor`, drags `dx` screen px, releases; returns where the anchor is on screen after.
    QPointF gesture(QPointF anchor, qreal dx, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        const QPointF press = editor->view()->mapCanvasToScreen(anchor);
        pen(area, QEvent::TabletPress, press, modifiers);
        if (dx != 0)
        {
            pen(area, QEvent::TabletMove, press + QPointF(dx / 2, 0), modifiers);
            pen(area, QEvent::TabletMove, press + QPointF(dx, 0), modifiers);
        }
        pen(area, QEvent::TabletRelease, press + QPointF(dx, 0), modifiers);
        const QPointF after = editor->view()->mapCanvasToScreen(anchor);
        CHECK(after.x() == Approx(press.x()).margin(0.01));
        CHECK(after.y() == Approx(press.y()).margin(0.01));
        return after;
    }

    void checkNoDrawingChange()
    {
        CHECK(layer->getLastBitmapImageAtFrame(1)->bounds().isEmpty());
        CHECK_FALSE(editor->object()->isModified());
    }
};

}

TEST_CASE("Zoom tool click zooms in 2x and Alt+click zooms out about the pointer", "[ZoomTool]")
{
    ZoomSession z;
    const QPointF anchor(120, -80);
    const qreal start = z.editor->view()->scaling();

    z.gesture(anchor, 0);
    CHECK(z.editor->view()->scaling() == Approx(start * 2));
    z.gesture(anchor, 0, Qt::AltModifier);
    CHECK(z.editor->view()->scaling() == Approx(start));
    z.checkNoDrawingChange();
}

TEST_CASE("Zoom tool drag right zooms in and drag left zooms out about the press point", "[ZoomTool]")
{
    ZoomSession z;
    const QPointF anchor(-60, 40);
    const qreal start = z.editor->view()->scaling();

    z.gesture(anchor, 100);
    const qreal zoomedIn = z.editor->view()->scaling();
    CHECK(zoomedIn > start);
    z.gesture(anchor, -200);
    CHECK(z.editor->view()->scaling() < zoomedIn);
    CHECK(z.editor->view()->scaling() < start);
    z.checkNoDrawingChange();
}

TEST_CASE("View tools work on a hidden or lock-all layer", "[ZoomTool]")
{
    ZoomSession z;
    const QPointF anchor(30, 30);

    z.layer->setLockAll(true);
    qreal before = z.editor->view()->scaling();
    z.gesture(anchor, 0);
    CHECK(z.editor->view()->scaling() == Approx(before * 2));

    z.layer->setLockAll(false);
    z.layer->setVisible(false);
    before = z.editor->view()->scaling();
    z.gesture(anchor, 0, Qt::AltModifier);
    CHECK(z.editor->view()->scaling() == Approx(before / 2));

    z.editor->tools()->setCurrentTool(ROTATE);
    const QPointF centre(z.area->width() / 2, z.area->height() / 2);
    pen(z.area, QEvent::TabletPress, centre + QPointF(200, 0));
    pen(z.area, QEvent::TabletMove, centre + QPointF(0, 200));
    pen(z.area, QEvent::TabletRelease, centre + QPointF(0, 200));
    CHECK(z.editor->view()->rotation() != Approx(0));
}
