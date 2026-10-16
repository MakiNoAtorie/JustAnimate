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
#include <QKeyEvent>
#include <QPointingDevice>
#include <QTabletEvent>

#include "bitmapimage.h"
#include "colormanager.h"
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

TEST_CASE("MoveTool: typed transform values apply exactly")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    s.start();
    Editor* editor = s.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    SelectionManager* select = editor->select();

    select->setSelection(QRectF(0, 0, 40, 40), true);
    select->setNumericTransform(QPointF(123.25, -7.5), 30, 1.5, -0.25);

    CHECK(select->selectionCentre().x() == Approx(123.25).margin(1e-9));
    CHECK(select->selectionCentre().y() == Approx(-7.5).margin(1e-9));
    CHECK(select->myRotation() == 30);
    CHECK(select->myScaleX() == 1.5);
    CHECK(select->myScaleY() == -0.25);
    // The top-left corner is (-20, -20) from the centre: scaled, then rotated 30 degrees.
    const QPointF corner = select->mapToSelection(QPointF(0, 0));
    CHECK(corner.x() == Approx(123.25 + (-30 * 0.8660254037844387 - 5 * 0.5)).margin(1e-9));
    CHECK(corner.y() == Approx(-7.5 + (-30 * 0.5 + 5 * 0.8660254037844387)).margin(1e-9));
}

TEST_CASE("MoveTool: a typed move applies and undoes")
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

    layer->getBitmapImageAtFrame(1)->setPixel(10, 20, qRgba(0, 0, 0, 255));
    editor->select()->setSelection(QRectF(0, 0, 40, 40), true);

    // An undo or redo step applies a pending move and deselects; a move shows either way.
    auto moved = [&]
    {
        BitmapImage* img = layer->getBitmapImageAtFrame(1);
        const bool pending = editor->select()->somethingSelected() && editor->select()->selectionCentre() == QPointF(120, 70);
        const bool applied = qAlpha(img->pixel(10, 20)) == 0 && qAlpha(img->pixel(110, 70)) != 0;
        return pending || applied;
    };
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };

    REQUIRE_FALSE(moved());
    editor->select()->setNumericTransform(QPointF(120, 70), 0, 1, 1);
    REQUIRE(moved());
    // A click outside applies the move to the pixels.
    drag(area, screen(QPointF(300, 300)), screen(QPointF(300, 300)));
    REQUIRE(moved());

    undoAction->trigger();
    REQUIRE(moved()); // the move is pending again
    undoAction->trigger();
    REQUIRE_FALSE(moved());
    redoAction->trigger();
    REQUIRE(moved());
}

TEST_CASE("MoveTool: a stroke after a redone move paints with an identity selection transform")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
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
    layer->getBitmapImageAtFrame(1)->setPixel(10, 20, black);
    editor->select()->setSelection(QRectF(0, 0, 40, 40), true);
    auto blackAt = [&](int x) { return layer->getBitmapImageAtFrame(1)->pixel(x, 20) == black; };
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };

    // Move the selection 20 right, undo, switch to the pen, redo, draw.
    drag(area, screen(QPointF(20, 20)), screen(QPointF(40, 20)));
    undoAction->trigger();
    REQUIRE(blackAt(10));
    editor->tools()->setCurrentTool(PEN);
    redoAction->trigger();
    tablet(area, QEvent::TabletPress, screen(QPointF(200, 200)), Qt::LeftButton);
    tablet(area, QEvent::TabletMove, screen(QPointF(250, 200)), Qt::LeftButton);
    area->grab(); // paints while drawing: asserts on a pending selection transform
    tablet(area, QEvent::TabletRelease, screen(QPointF(250, 200)), Qt::NoButton);

    CHECK(editor->select()->selectionTransform().isIdentity());
    CHECK_FALSE(blackAt(10));
    CHECK(blackAt(30));

    undoAction->trigger(); // the stroke
    undoAction->trigger(); // the move
    CHECK(blackAt(10));
    CHECK_FALSE(blackAt(30));
}

TEST_CASE("Polyline and bucket apply a pending selection transform before they paint")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);

    const QRgb black = qRgba(0, 0, 0, 255);
    // A block wider than the bucket's fill expand, so a fill around it keeps its middle.
    for (int x = 5; x <= 15; ++x)
        for (int y = 15; y <= 25; ++y)
            layer->getBitmapImageAtFrame(1)->setPixel(x, y, black);
    editor->select()->setSelection(QRectF(0, 0, 40, 40), true);
    auto blackAt = [&](int x) { return layer->getBitmapImageAtFrame(1)->pixel(x, 20) == black; };
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };
    auto nudgeRight = [&]
    {
        QKeyEvent e(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(area, &e);
        REQUIRE_FALSE(editor->select()->selectionTransform().isIdentity());
    };

    SECTION("Polyline")
    {
        editor->tools()->setCurrentTool(POLYLINE);
        nudgeRight();
        tablet(area, QEvent::TabletPress, screen(QPointF(200, 200)), Qt::LeftButton);
        tablet(area, QEvent::TabletRelease, screen(QPointF(200, 200)), Qt::NoButton);
        tablet(area, QEvent::TabletMove, screen(QPointF(250, 200)), Qt::NoButton);
        area->grab(); // paints the polyline buffer: asserts on a pending selection transform
        QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(area, &esc);
    }

    SECTION("Bucket")
    {
        editor->color()->setFrontColor(Qt::red);
        editor->tools()->setCurrentTool(BUCKET);
        nudgeRight();
        tablet(area, QEvent::TabletPress, screen(QPointF(200, 200)), Qt::LeftButton);
        tablet(area, QEvent::TabletRelease, screen(QPointF(200, 200)), Qt::NoButton);
    }

    CHECK(editor->select()->selectionTransform().isIdentity());
    CHECK_FALSE(blackAt(5));
    CHECK(blackAt(11));
}
