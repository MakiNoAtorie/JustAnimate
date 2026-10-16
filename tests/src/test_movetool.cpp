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
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "undoredomanager.h"
#include "viewmanager.h"

namespace
{

void tablet(ScribbleArea* area, QEvent::Type type, QPointF pos, Qt::MouseButtons buttons)
{
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

/// Hovers at `from` so the tool picks its handle, then drags to `to`.
void drag(ScribbleArea* area, QPointF from, QPointF to)
{
    tablet(area, QEvent::TabletMove, from, Qt::NoButton);
    tablet(area, QEvent::TabletPress, from, Qt::LeftButton);
    for (int i = 1; i <= 5; ++i)
        tablet(area, QEvent::TabletMove, from + (to - from) * i / 5.0, Qt::LeftButton);
    tablet(area, QEvent::TabletRelease, to, Qt::NoButton);
}

} // namespace

TEST_CASE("MoveTool: skew handles skew the selection and undo")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    editor->tools()->setCurrentTool(MOVE);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    const QRgb black = qRgba(0, 0, 0, 255);
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    image->setPixel(10, 2, black);  // near the top edge: a skew moves it
    image->setPixel(10, 20, black); // on the centre line: a skew keeps it
    editor->select()->setSelection(QRectF(0, 0, 40, 40), true);

    // An undo or redo step applies a pending skew and deselects; a skew shows either way.
    auto skewed = [&]
    {
        BitmapImage* img = layer->getBitmapImageAtFrame(1);
        const bool pending = editor->select()->somethingSelected() && editor->select()->mySkewX() != 0;
        const bool applied = qAlpha(img->pixel(10, 2)) == 0 && qAlpha(img->pixel(10, 20)) != 0;
        return pending || applied;
    };
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };

    REQUIRE_FALSE(skewed());
    // Drag the top edge handle right: the top leans right, the centre line stays.
    drag(area, screen(QPointF(20, 0)), screen(QPointF(40, 0)));
    REQUIRE(editor->select()->mySkewX() != 0);
    REQUIRE(editor->select()->mySkewY() == 0);
    // A click outside applies the skew to the pixels.
    drag(area, screen(QPointF(200, 200)), screen(QPointF(200, 200)));
    REQUIRE(skewed());

    undoAction->trigger();
    REQUIRE(skewed()); // the skew is pending again
    undoAction->trigger();
    REQUIRE_FALSE(skewed());
    redoAction->trigger();
    REQUIRE(skewed());
}

TEST_CASE("MoveTool: a corner drag on a skewed selection follows the cursor")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    editor->tools()->setCurrentTool(MOVE);
    SelectionManager* select = editor->select();
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };

    select->setSelection(QRectF(0, 0, 40, 40), true);
    drag(area, screen(QPointF(20, 0)), screen(QPointF(40, 0)));
    const QPolygonF before = select->mapToSelection(QPolygonF(select->mySelectionRect()));

    // Drag the bottom-right corner 10 right and 10 down: the top-left corner stays.
    drag(area, screen(before[2]), screen(before[2] + QPointF(10, 10)));
    const QPolygonF after = select->mapToSelection(QPolygonF(select->mySelectionRect()));
    CHECK(after[0].x() == Approx(before[0].x()).margin(0.5));
    CHECK(after[0].y() == Approx(before[0].y()).margin(0.5));
    CHECK(after[2].x() == Approx(before[2].x() + 10).margin(0.5));
    CHECK(after[2].y() == Approx(before[2].y() + 10).margin(0.5));
}