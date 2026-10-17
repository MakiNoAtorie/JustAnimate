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
#include <QGuiApplication>
#include <QKeyEvent>
#include <QStyleHints>
#include <QPointingDevice>
#include <QTabletEvent>

#include "bitmapimage.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "scribblearea.h"
#include "shapetool.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "undoredomanager.h"
#include "vectorimage.h"
#include "viewmanager.h"

namespace
{

void tablet(ScribbleArea* area, QEvent::Type type, QPointF pos, Qt::KeyboardModifiers modifiers, ulong time = 0)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, modifiers, Qt::LeftButton, buttons);
    e.setTimestamp(time);
    QApplication::sendEvent(area, &e);
}

void drag(ScribbleArea* area, QPointF from, QPointF to, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    tablet(area, QEvent::TabletPress, from, modifiers);
    for (int i = 1; i <= 5; ++i)
        tablet(area, QEvent::TabletMove, from + (to - from) * i / 5.0, modifiers);
    tablet(area, QEvent::TabletRelease, to, modifiers);
}

QRectF bounds(const QPolygonF& outline) { return outline.boundingRect(); }

} // namespace

TEST_CASE("ShapeTool - outline geometry with Shift and Alt")
{
    const QPointF a(10, 10), b(30, 20);
    const Qt::KeyboardModifiers shift = Qt::ShiftModifier, alt = Qt::AltModifier;

    SECTION("Rectangle")
    {
        const QPolygonF r = ShapeTool::outline(ShapeTool::RECTANGLE, a, b, Qt::NoModifier);
        CHECK(r.isClosed());
        CHECK(bounds(r) == QRectF(10, 10, 20, 10));
        CHECK(bounds(ShapeTool::outline(ShapeTool::RECTANGLE, a, b, shift)) == QRectF(10, 10, 20, 20));
        CHECK(bounds(ShapeTool::outline(ShapeTool::RECTANGLE, a, b, alt)) == QRectF(-10, 0, 40, 20));
        CHECK(bounds(ShapeTool::outline(ShapeTool::RECTANGLE, a, b, shift | alt)) == QRectF(-10, -10, 40, 40));
        // Shift keeps the drag direction.
        CHECK(bounds(ShapeTool::outline(ShapeTool::RECTANGLE, a, QPointF(0, 5), shift)) == QRectF(0, 0, 10, 10));
    }

    SECTION("Ellipse")
    {
        const QPolygonF e = ShapeTool::outline(ShapeTool::ELLIPSE, a, b, Qt::NoModifier);
        CHECK(e.isClosed());
        CHECK(e.size() > 8);
        const QRectF r = bounds(e);
        CHECK(r.left() == Approx(10));
        CHECK(r.right() == Approx(30));
        CHECK(r.top() == Approx(10));
        CHECK(r.bottom() == Approx(20));
        CHECK(bounds(ShapeTool::outline(ShapeTool::ELLIPSE, a, b, shift | alt)).width() == Approx(40));
        CHECK(bounds(ShapeTool::outline(ShapeTool::ELLIPSE, a, b, shift | alt)).height() == Approx(40));
    }

    SECTION("Line")
    {
        CHECK(ShapeTool::outline(ShapeTool::LINE, a, b, Qt::NoModifier) == QPolygonF({ a, b }));
        CHECK(ShapeTool::outline(ShapeTool::LINE, a, b, alt) == QPolygonF({ QPointF(-10, 0), b }));
        // Shift snaps to 45 degree steps and keeps the length.
        const QPolygonF flat = ShapeTool::outline(ShapeTool::LINE, QPointF(0, 0), QPointF(10, 1), shift);
        CHECK(flat[1].x() == Approx(std::sqrt(101.0)));
        CHECK(flat[1].y() == Approx(0).margin(1e-9));
        const QPolygonF diagonal = ShapeTool::outline(ShapeTool::LINE, QPointF(0, 0), QPointF(10, 9), shift);
        CHECK(diagonal[1].x() == Approx(std::sqrt(181.0 / 2)));
        CHECK(diagonal[1].y() == Approx(std::sqrt(181.0 / 2)));
    }
}

TEST_CASE("ShapeTool - draws each shape on bitmap and vector layers")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    LayerVector* vector = s.object->addNewVectorLayer();
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->tools()->setCurrentTool(SHAPE);
    ShapeTool* tool = static_cast<ShapeTool*>(editor->tools()->getTool(SHAPE));
    const ToolSettingsGuard guard(tool);
    tool->setWidth(4);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    auto canvas = [&](QPointF screen) { return editor->view()->mapScreenToCanvas(screen); };

    SECTION("Bitmap: outline only, through the brush, undoable")
    {
        editor->layers()->setCurrentLayer(bitmap);
        auto inkAt = [&](QPointF screen) {
            return qAlpha(bitmap->getBitmapImageAtFrame(1)->pixel(canvas(screen).toPoint())) != 0;
        };
        for (ShapeTool::Mode mode : { ShapeTool::LINE, ShapeTool::RECTANGLE, ShapeTool::ELLIPSE })
        {
            CAPTURE(mode);
            tool->setShapeMode(mode);
            drag(area, QPointF(200, 200), QPointF(400, 300));
            if (mode == ShapeTool::LINE)
                CHECK(inkAt(QPointF(300, 250)));
            if (mode == ShapeTool::RECTANGLE)
                CHECK(inkAt(QPointF(300, 200)));
            if (mode == ShapeTool::ELLIPSE)
                CHECK(inkAt(QPointF(300, 300)));
            CHECK_FALSE(inkAt(QPointF(250, 280)));
            undoAction->trigger();
            CHECK_FALSE(inkAt(QPointF(300, 250)));
            CHECK_FALSE(inkAt(QPointF(300, 200)));
            CHECK_FALSE(inkAt(QPointF(300, 300)));
            redoAction->trigger();
            undoAction->trigger();
        }
    }

    SECTION("Vector: one curve per shape")
    {
        editor->layers()->setCurrentLayer(vector);
        // getLastCurveNumber() is 0 for no curve and for one, so start with one.
        BezierCurve first({ QPointF(0, 0), QPointF(5, 0) });
        first.setColorNumber(1);
        vector->getVectorImageAtFrame(1)->addCurve(first, 1.0);
        for (ShapeTool::Mode mode : { ShapeTool::LINE, ShapeTool::RECTANGLE, ShapeTool::ELLIPSE })
        {
            CAPTURE(mode);
            tool->setShapeMode(mode);
            const int before = vector->getVectorImageAtFrame(1)->getLastCurveNumber();
            drag(area, QPointF(200, 200), QPointF(400, 300));
            VectorImage* image = vector->getVectorImageAtFrame(1);
            REQUIRE(image->getLastCurveNumber() == before + 1);
            BezierCurve& curve = image->curve(image->getLastCurveNumber());
            const QRectF box = curve.getBoundingRect();
            CHECK(box.left() == Approx(canvas(QPointF(200, 200)).x()).margin(3)); // the box includes half the width
            CHECK(box.right() == Approx(canvas(QPointF(400, 300)).x()).margin(3));
            CHECK(box.bottom() == Approx(canvas(QPointF(400, 300)).y()).margin(3));
            undoAction->trigger();
            CHECK(vector->getVectorImageAtFrame(1)->getLastCurveNumber() == before);
            redoAction->trigger();
            CHECK(vector->getVectorImageAtFrame(1)->getLastCurveNumber() == before + 1);
        }
    }
}

TEST_CASE("ShapeTool - polygon closes on the first point or a double click")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    LayerVector* vector = s.object->addNewVectorLayer();
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->tools()->setCurrentTool(SHAPE);
    ShapeTool* tool = static_cast<ShapeTool*>(editor->tools()->getTool(SHAPE));
    const ToolSettingsGuard guard(tool);
    tool->setWidth(4);
    tool->setShapeMode(ShapeTool::POLYGON);
    REQUIRE(tool->shapeMode() == ShapeTool::POLYGON);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    auto canvas = [&](QPointF screen) { return editor->view()->mapScreenToCanvas(screen); };
    auto click = [&](QPointF p) {
        tablet(area, QEvent::TabletPress, p, Qt::NoModifier);
        tablet(area, QEvent::TabletRelease, p, Qt::NoModifier);
    };
    const QPointF A(200, 200), B(400, 200), C(300, 400);
    const QPointF edgeAB(300, 200), edgeBC(350, 300), edgeCA(250, 300), inside(300, 250);

    SECTION("Bitmap")
    {
        editor->layers()->setCurrentLayer(bitmap);
        auto inkAt = [&](QPointF screen) {
            return qAlpha(bitmap->getBitmapImageAtFrame(1)->pixel(canvas(screen).toPoint())) != 0;
        };
        click(A);
        click(B);
        click(C);
        CHECK_FALSE(inkAt(edgeAB)); // still open: nothing is drawn yet
        click(A + QPointF(3, 2));   // on the first point
        CHECK(inkAt(edgeAB));
        CHECK(inkAt(edgeBC));
        CHECK(inkAt(edgeCA)); // the closing edge
        CHECK_FALSE(inkAt(inside));
        undoAction->trigger();
        CHECK_FALSE(inkAt(edgeAB));
        CHECK_FALSE(inkAt(edgeCA));
    }

    SECTION("Vector, double click ends")
    {
        editor->layers()->setCurrentLayer(vector);
        BezierCurve first({ QPointF(0, 0), QPointF(5, 0) });
        first.setColorNumber(1);
        vector->getVectorImageAtFrame(1)->addCurve(first, 1.0);
        click(A);
        click(B);
        tablet(area, QEvent::TabletPress, C, Qt::NoModifier);
        tablet(area, QEvent::TabletRelease, C, Qt::NoModifier);
        CHECK(vector->getVectorImageAtFrame(1)->getLastCurveNumber() == 0);
        tablet(area, QEvent::TabletPress, C, Qt::NoModifier); // second press at once is a double click
        tablet(area, QEvent::TabletRelease, C, Qt::NoModifier);
        VectorImage* image = vector->getVectorImageAtFrame(1);
        REQUIRE(image->getLastCurveNumber() == 1);
        BezierCurve& curve = image->curve(1);
        CHECK(curve.getVertexSize() == 3); // A, B, C; the closing point joins the first
        CHECK(curve.getBoundingRect().top() == Approx(canvas(A).y()).margin(3));
        CHECK(curve.getBoundingRect().bottom() == Approx(canvas(C).y()).margin(3));
        undoAction->trigger();
        CHECK(vector->getVectorImageAtFrame(1)->getLastCurveNumber() == 0);
    }
}

TEST_CASE("ShapeTool - a tablet double click follows the system interval and distance")
{
    TestSession s;
    LayerVector* vector = s.object->addNewVectorLayer();
    s.start();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    s.editor->tools()->setCurrentTool(SHAPE);
    ShapeTool* tool = static_cast<ShapeTool*>(s.editor->tools()->getTool(SHAPE));
    const ToolSettingsGuard guard(tool);
    tool->setShapeMode(ShapeTool::POLYGON);
    s.editor->layers()->setCurrentLayer(vector);
    BezierCurve first({ QPointF(0, 0), QPointF(5, 0) });
    first.setColorNumber(1);
    vector->getVectorImageAtFrame(1)->addCurve(first, 1.0);

    QStyleHints* hints = QGuiApplication::styleHints();
    const int interval = hints->mouseDoubleClickInterval();
    const qreal distance = hints->mouseDoubleClickDistance(); // no setter in Qt
    hints->setMouseDoubleClickInterval(300);

    // Two taps on C after A and B; the polygon ends only when the second tap is a double click.
    auto curves = [&](ulong gap, QPointF second) {
        for (QPointF p : { QPointF(200, 200), QPointF(400, 200), QPointF(300, 400) })
        {
            tablet(area, QEvent::TabletPress, p, Qt::NoModifier, 1000);
            tablet(area, QEvent::TabletRelease, p, Qt::NoModifier, 1001);
        }
        tablet(area, QEvent::TabletPress, second, Qt::NoModifier, 1000 + gap);
        tablet(area, QEvent::TabletRelease, second, Qt::NoModifier, 1001 + gap);
        return vector->getVectorImageAtFrame(1)->getLastCurveNumber();
    };
    SECTION("Inside the interval") { CHECK(curves(200, QPointF(300, 400)) == 1); }
    SECTION("Outside the interval") { CHECK(curves(400, QPointF(300, 400)) == 0); }
    SECTION("Inside the distance") { CHECK(curves(100, QPointF(300, 400 + distance)) == 1); }
    SECTION("Outside the distance") { CHECK(curves(100, QPointF(300, 402 + distance)) == 0); }

    hints->setMouseDoubleClickInterval(interval);
}

TEST_CASE("ShapeTool - an open polygon ends on layer change or Esc")
{
    TestSession s;
    LayerBitmap* layerA = s.object->addNewBitmapLayer();
    LayerBitmap* layerB = s.object->addNewBitmapLayer();
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->tools()->setCurrentTool(SHAPE);
    ShapeTool* tool = static_cast<ShapeTool*>(editor->tools()->getTool(SHAPE));
    const ToolSettingsGuard guard(tool);
    tool->setWidth(4);
    tool->setShapeMode(ShapeTool::POLYGON);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    auto canvas = [&](QPointF screen) { return editor->view()->mapScreenToCanvas(screen); };
    auto click = [&](QPointF p) {
        tablet(area, QEvent::TabletPress, p, Qt::NoModifier);
        tablet(area, QEvent::TabletRelease, p, Qt::NoModifier);
    };
    auto inkAt = [&](LayerBitmap* layer, QPointF screen) {
        return qAlpha(layer->getBitmapImageAtFrame(1)->pixel(canvas(screen).toPoint())) != 0;
    };
    const QPointF A(200, 200), B(400, 200), C(300, 400), edgeAB(300, 200);
    const QPointF closeA = A + QPointF(3, 2);

    editor->layers()->setCurrentLayer(layerA);
    click(A);
    click(B);
    click(C);

    SECTION("Layer change")
    {
        editor->layers()->setCurrentLayer(layerB);
        click(closeA); // starts a new polygon, does not close the old one
        CHECK_FALSE(inkAt(layerA, edgeAB));
        CHECK_FALSE(inkAt(layerB, edgeAB));
        CHECK_FALSE(undoAction->isEnabled());

        click(B);
        click(C);
        click(A);
        CHECK(inkAt(layerB, edgeAB));
        CHECK_FALSE(inkAt(layerA, edgeAB));
        undoAction->trigger();
        CHECK_FALSE(inkAt(layerB, edgeAB));
        CHECK_FALSE(undoAction->isEnabled());
    }

    SECTION("Esc")
    {
        QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(area, &esc);
        click(closeA);
        CHECK_FALSE(inkAt(layerA, edgeAB));
        CHECK_FALSE(undoAction->isEnabled());
    }

    SECTION("Tool switch")
    {
        editor->tools()->setCurrentTool(PENCIL);
        editor->tools()->setCurrentTool(SHAPE);
        click(closeA);
        CHECK_FALSE(inkAt(layerA, edgeAB));
        CHECK_FALSE(undoAction->isEnabled());
    }
}
