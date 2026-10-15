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

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QPointingDevice>
#include <QTabletEvent>

#include "bitmapimage.h"
#include "colormanager.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "undoredomanager.h"
#include "vectorimage.h"
#include "viewmanager.h"

namespace
{

void tablet(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

// Draws an open loop around (300, 250); the tool closes it.
void lasso(ScribbleArea* area)
{
    const QPointF loop[] = { { 200, 200 }, { 400, 200 }, { 400, 300 }, { 200, 300 } };
    tablet(area, QEvent::TabletPress, loop[0]);
    for (const QPointF& p : loop)
        tablet(area, QEvent::TabletMove, p);
    tablet(area, QEvent::TabletRelease, loop[3]);
}

} // namespace

TEST_CASE("LassoFillTool - fills the loop on bitmap and vector layers and undoes")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    LayerVector* vector = s.object->addNewVectorLayer();
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->tools()->setCurrentTool(LASSOFILL);
    editor->color()->setFrontColor(QColor(255, 0, 0));

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    auto canvas = [&](QPointF screen) { return editor->view()->mapScreenToCanvas(screen); };

    SECTION("Bitmap")
    {
        editor->layers()->setCurrentLayer(bitmap);
        auto pixelAt = [&](QPointF screen) { return bitmap->getBitmapImageAtFrame(1)->pixel(canvas(screen).toPoint()); };
        lasso(area);
        CHECK(pixelAt(QPointF(300, 250)) == qRgba(255, 0, 0, 255));
        CHECK(pixelAt(QPointF(250, 220)) == qRgba(255, 0, 0, 255));
        CHECK(qAlpha(pixelAt(QPointF(450, 250))) == 0);
        undoAction->trigger();
        CHECK(qAlpha(pixelAt(QPointF(300, 250))) == 0);
        redoAction->trigger();
        CHECK(pixelAt(QPointF(300, 250)) == qRgba(255, 0, 0, 255));
    }

    SECTION("Vector")
    {
        editor->layers()->setCurrentLayer(vector);
        auto areaAt = [&](QPointF screen) { return vector->getVectorImageAtFrame(1)->getFirstAreaNumber(canvas(screen)); };
        lasso(area);
        CHECK(areaAt(QPointF(300, 250)) != -1);
        CHECK(areaAt(QPointF(350, 220)) != -1);
        CHECK(areaAt(QPointF(450, 250)) == -1);
        undoAction->trigger();
        CHECK(areaAt(QPointF(300, 250)) == -1);
        redoAction->trigger();
        CHECK(areaAt(QPointF(300, 250)) != -1);
    }
}
