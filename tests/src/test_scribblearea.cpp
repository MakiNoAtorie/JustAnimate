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
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QtMath>
#include "mathutils.h"

#include "brushpresets.h"
#include "brushtool.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "viewmanager.h"

TEST_CASE("ScribbleArea - canvas HUD shows the brush ring and view values")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);

    editor->view()->scale(2.0);
    editor->view()->rotate(30);

    CanvasHud hud = area->canvasHud();
    CHECK(hud.ringRadius == Approx(20)); // 20 px brush at 200 % zoom: 40 px across on screen
    CHECK(hud.zoomPercent == 200);
    CHECK(hud.rotation == Approx(30));

    editor->tools()->setCurrentTool(HAND);
    hud = area->canvasHud();
    CHECK(hud.ringRadius == 0); // no brush size: no ring
    CHECK(hud.zoomPercent == 200);
}

TEST_CASE("radialSliceAt - slice 0 is at the top and slices go clockwise")
{
    CHECK(MathUtils::radialSliceAt(QPointF(0, -50), 4, 20, 100) == 0);
    CHECK(MathUtils::radialSliceAt(QPointF(50, 0), 4, 20, 100) == 1);
    CHECK(MathUtils::radialSliceAt(QPointF(0, 50), 4, 20, 100) == 2);
    CHECK(MathUtils::radialSliceAt(QPointF(-50, 0), 4, 20, 100) == 3);
    CHECK(MathUtils::radialSliceAt(QPointF(-30, -40), 4, 20, 100) == 0); // 37 degrees left of up: still the top slice
    CHECK(MathUtils::radialSliceAt(QPointF(-40, -30), 4, 20, 100) == 3);
    CHECK(MathUtils::radialSliceAt(QPointF(5, 5), 4, 20, 100) == -1);    // centre: no slice
    CHECK(MathUtils::radialSliceAt(QPointF(0, -150), 4, 20, 100) == -1); // outside the ring
    CHECK(MathUtils::radialSliceAt(QPointF(0, -50), 0, 20, 100) == -1);
}

namespace
{

void pen(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0.8, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

/// The middle of the slice with this label in the open menu.
QPointF slicePos(const RadialMenu& menu, const QString& label)
{
    const int i = menu.labels.indexOf(label);
    REQUIRE(i >= 0);
    const qreal a = 2 * M_PI * i / menu.labels.size();
    const qreal r = (RadialMenu::kInner + RadialMenu::kOuter) / 2;
    return menu.center + QPointF(qSin(a), -qCos(a)) * r;
}

}

TEST_CASE("ScribbleArea - radial tool menu picks a tool with the pen")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL);

    area->openToolMenu(QPointF(400, 300));
    const RadialMenu& menu = area->radialMenu();
    REQUIRE(menu.kind == RadialMenu::Tools);
    CHECK(menu.labels.contains(BaseTool::TypeName(BRUSH)));
    CHECK_FALSE(menu.labels.contains(BaseTool::TypeName(CAMERA)));

    // Press on one slice, slide to another, lift: the slice under the pen wins.
    pen(area, QEvent::TabletPress, slicePos(menu, BaseTool::TypeName(ERASER)));
    pen(area, QEvent::TabletMove, slicePos(menu, BaseTool::TypeName(BRUSH)));
    CHECK(menu.hovered == menu.labels.indexOf(BaseTool::TypeName(BRUSH)));
    pen(area, QEvent::TabletRelease, slicePos(menu, BaseTool::TypeName(BRUSH)));
    CHECK(area->radialMenu().kind == RadialMenu::Closed);
    CHECK(editor->tools()->currentTool()->type() == BRUSH);
    CHECK(layer->getLastBitmapImageAtFrame(1)->bounds().isEmpty()); // the menu took the pen, not the tool

    // A tap in the centre closes the menu and keeps the tool.
    area->openToolMenu(QPointF(400, 300));
    pen(area, QEvent::TabletPress, QPointF(400, 300));
    pen(area, QEvent::TabletRelease, QPointF(400, 300));
    CHECK(area->radialMenu().kind == RadialMenu::Closed);
    CHECK(editor->tools()->currentTool()->type() == BRUSH);
}

TEST_CASE("ScribbleArea - quick preset picker applies a preset with the pen")
{
    QTemporaryDir dir;
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    BrushPresets presets(dir.path());
    brush->setWidth(3);
    REQUIRE(presets.save("Fine", *brush));
    brush->setWidth(60);
    REQUIRE(presets.save("Wide", *brush));
    brush->setWidth(10);

    area->openPresetPicker(QPointF(400, 300), dir.path());
    const RadialMenu& menu = area->radialMenu();
    REQUIRE(menu.kind == RadialMenu::Presets);
    CHECK(menu.labels == QStringList{ "Fine", "Wide" });

    const QPointF wide = slicePos(menu, "Wide");
    pen(area, QEvent::TabletPress, wide);
    pen(area, QEvent::TabletRelease, wide);
    CHECK(area->radialMenu().kind == RadialMenu::Closed);
    CHECK(brush->strokeToolProperties().width() == 60);
}

namespace
{

/// A right-button pen press at a canvas point, stamped `ms` after the test epoch.
void tap(ScribbleArea* area, Editor* editor, QPointF canvasPos, ulong ms)
{
    const QPointF pos = editor->view()->mapCanvasToScreen(canvasPos);
    QTabletEvent e(QEvent::TabletPress, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0.8, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::RightButton, Qt::RightButton);
    e.setTimestamp(ms);
    QApplication::sendEvent(area, &e);
    QTabletEvent r(QEvent::TabletRelease, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::RightButton, Qt::NoButton);
    r.setTimestamp(ms + 10);
    QApplication::sendEvent(area, &r);
}

}

TEST_CASE("ScribbleArea - pen double tap needs two taps close in time and place")
{
    TestSession s;
    s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->tools()->setCurrentTool(HAND);

    // The hand tool resets the view on a right-button double click.
    auto zoomed = [&] { editor->view()->scale(2.0); return editor->view()->scaling() != 1.0; };

    REQUIRE(zoomed());
    tap(area, editor, QPointF(100, 100), 1000);
    tap(area, editor, QPointF(100, 100), 1100);
    CHECK(editor->view()->scaling() == 1.0);

    REQUIRE(zoomed());
    tap(area, editor, QPointF(100, 100), 5000);
    tap(area, editor, QPointF(100, 100), 5600);
    CHECK(editor->view()->scaling() != 1.0);

    tap(area, editor, QPointF(100, 100), 9000);
    tap(area, editor, QPointF(110, 100), 9100);
    CHECK(editor->view()->scaling() != 1.0);
}
