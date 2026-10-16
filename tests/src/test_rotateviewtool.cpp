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

#include <QAbstractButton>
#include <QApplication>
#include <QTabletEvent>
#include <QtMath>

#include "bitmapimage.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "mainwindow2.h"
#include "object.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "viewmanager.h"

namespace
{

void pen(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0.8, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

}

TEST_CASE("Rotate view tool turns the view by the drag angle about the centre", "[RotateViewTool]")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(ROTATE);
    REQUIRE(editor->tools()->currentTool()->type() == ROTATE);

    const QPointF centre(area->width() / 2, area->height() / 2);
    const auto at = [&](qreal degrees) {
        return centre + 200 * QPointF(qCos(qDegreesToRadians(degrees)), qSin(qDegreesToRadians(degrees)));
    };
    editor->view()->rotate(170);
    editor->object()->setModified(false);

    pen(area, QEvent::TabletPress, at(20));
    pen(area, QEvent::TabletMove, at(35));
    pen(area, QEvent::TabletMove, at(50));
    pen(area, QEvent::TabletRelease, at(50));
    CHECK(editor->view()->rotation() == Approx(-160)); // 200 wraps
    CHECK(layer->getLastBitmapImageAtFrame(1)->bounds().isEmpty());
    CHECK(!editor->object()->isModified());

    // A flipped view turns the other way, like the hand tool.
    editor->view()->resetRotation();
    editor->view()->flipHorizontal(true);
    pen(area, QEvent::TabletPress, at(20));
    pen(area, QEvent::TabletMove, at(50));
    pen(area, QEvent::TabletRelease, at(50));
    CHECK(editor->view()->rotation() == Approx(-30));
}

TEST_CASE("Tool settings show a Reset view button for the Rotate view tool", "[RotateViewTool]")
{
    MainWindow2 window;
    window.show();
    QApplication::processEvents();
    Editor* editor = window.findChild<Editor*>();
    REQUIRE(editor);
    auto* button = window.findChild<QAbstractButton*>("resetViewButton");
    REQUIRE(button);

    editor->tools()->setCurrentTool(PENCIL);
    CHECK(!button->isVisibleTo(&window));
    editor->tools()->setCurrentTool(ROTATE);
    CHECK(button->isVisibleTo(&window));

    editor->view()->rotate(45);
    button->click();
    CHECK(editor->view()->rotation() == 0);
}
