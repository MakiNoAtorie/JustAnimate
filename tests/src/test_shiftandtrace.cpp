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
#include <QKeySequence>
#include <QTabletEvent>

#include "bitmapimage.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "preferencemanager.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "undoredomanager.h"
#include "viewmanager.h"

namespace
{
void sendTablet(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = type == QEvent::TabletRelease ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

void drag(ScribbleArea* area, QPointF from, QPointF to)
{
    sendTablet(area, QEvent::TabletPress, from);
    sendTablet(area, QEvent::TabletMove, (from + to) / 2);
    sendTablet(area, QEvent::TabletMove, to);
    sendTablet(area, QEvent::TabletRelease, to);
}
} // namespace

TEST_CASE("Shift and Trace moves and rotates onion neighbours without changing a Drawing", "[ShiftAndTrace]")
{
    const SettingsGuard guard({ SETTING_PREV_ONION, SETTING_NEXT_ONION, SETTING_ONION_RED, SETTING_ONION_PREV_FRAMES_NUM,
                                SETTING_ONION_MAX_OPACITY, SETTING_ONION_TYPE, SETTING_ONION_MARKS });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(200, 200);
    area->show();
    QApplication::processEvents();
    // The wiring MainWindow2 makes.
    QObject::connect(editor, &Editor::scrubbed, area, &ScribbleArea::onScrubbed);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL); // a missed handle would draw on the current frame
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());

    PreferenceManager* prefs = editor->preference();
    prefs->set<SETTING::PREV_ONION>(true);
    prefs->set<SETTING::NEXT_ONION>(false);
    prefs->set<SETTING::ONION_RED>(false);
    prefs->set<SETTING::ONION_PREV_FRAMES_NUM>(2);
    prefs->set<SETTING::ONION_MAX_OPACITY>(100);
    prefs->set<SETTING::ONION_MARKS>(MarkFilter({ Mark::Key }).toInt());
    prefs->set<SETTING::ONION_TYPE>(QString("marked"));

    REQUIRE(layer->addNewKeyFrameAt(2));
    REQUIRE(layer->addNewKeyFrameAt(3));
    layer->getKeyFrameAt(1)->setMark(Mark::Key);
    layer->getKeyFrameAt(2)->setMark(Mark::Inbetween);
    layer->getBitmapImageAtFrame(1)->drawRect(QRectF(-60, -10, 20, 20), Qt::NoPen, QBrush(Qt::black), QPainter::CompositionMode_Source, false);
    layer->getBitmapImageAtFrame(2)->drawRect(QRectF(40, -10, 20, 20), Qt::NoPen, QBrush(Qt::black), QPainter::CompositionMode_Source, false);
    editor->scrubTo(3);
    area->onFrameModified(1);
    area->onFrameModified(2);

    const QImage drawings[] = { layer->getBitmapImageAtFrame(1)->image()->copy(), layer->getBitmapImageAtFrame(2)->image()->copy(),
                                layer->getBitmapImageAtFrame(3)->image()->copy() };
    ViewManager* view = editor->view();
    const QPoint key = view->mapCanvasToScreen(QPointF(-50, 0)).toPoint();
    const QColor empty = area->grab().toImage().pixelColor(view->mapCanvasToScreen(QPointF(0, 0)).toPoint());
    REQUIRE(area->grab().toImage().pixelColor(key) != empty);

    area->setShiftAndTrace(true);
    REQUIRE(area->isShiftAndTrace());
    CHECK(area->shiftAndTraceFrames() == QVector<int>{ 1 }); // the Inbetween fails the onion Mark filter
    const QPointF handle = area->shiftAndTraceHandle(1);
    CHECK(handle.x() == Approx(key.x()).margin(1.5));
    CHECK(handle.y() == Approx(key.y()).margin(1.5));

    // Checked after each gesture.
    const auto leavesDrawingsAndClears = [&] {
        CHECK_FALSE(area->shiftAndTraceOffset(1).isIdentity());
        CHECK_FALSE(undo->isEnabled());
        CHECK(layer->getBitmapImageAtFrame(1)->image()->copy() == drawings[0]);
        CHECK(layer->getBitmapImageAtFrame(2)->image()->copy() == drawings[1]);
        CHECK(layer->getBitmapImageAtFrame(3)->image()->copy() == drawings[2]);

        SECTION("A frame change clears the offsets")
        {
            editor->scrubTo(2);
            CHECK(area->shiftAndTraceOffset(1).isIdentity());
        }
        SECTION("Turning the mode off clears the offsets")
        {
            area->setShiftAndTrace(false);
            CHECK(area->shiftAndTraceOffset(1).isIdentity());
            CHECK(area->grab().toImage().pixelColor(key) != empty);
        }
    };

    SECTION("Dragging the move handle moves the neighbour in the View")
    {
        drag(area, handle, handle + QPointF(0, 40));
        const QImage image = area->grab().toImage();
        CHECK(image.pixelColor(key) == empty);
        CHECK(image.pixelColor(key + QPoint(0, 40)) != empty);
        const QPointF moved = area->shiftAndTraceOffset(1).map(QPointF(-50, 0));
        CHECK(view->mapCanvasToScreen(moved).y() == Approx(key.y() + 40).margin(1.5));
        leavesDrawingsAndClears();
    }
    SECTION("Dragging the rotate handle turns the neighbour around its centre")
    {
        const QPointF arm = area->shiftAndTraceRotateHandle(1);
        const qreal r = QLineF(handle, arm).length();
        REQUIRE(r > 10);
        drag(area, arm, handle + QPointF(r, 0)); // a quarter turn clockwise on screen
        const QTransform offset = area->shiftAndTraceOffset(1);
        const QPointF centre = offset.map(QPointF(-50, 0));
        CHECK(centre.x() == Approx(-50).margin(0.5));
        CHECK(centre.y() == Approx(0).margin(0.5));
        const QPointF turned = view->mapCanvasToScreen(offset.map(view->mapScreenToCanvas(handle + QPointF(0, -10))));
        CHECK(turned.x() == Approx(handle.x() + 10).margin(1));
        CHECK(turned.y() == Approx(handle.y()).margin(1));
        leavesDrawingsAndClears();
    }
}

TEST_CASE("Shift and Trace has the default key Shift+T", "[ShiftAndTrace]")
{
    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    CHECK(QKeySequence(preset.value(CMD_SHIFT_AND_TRACE).toString()) == QKeySequence(Qt::SHIFT | Qt::Key_T));
}
