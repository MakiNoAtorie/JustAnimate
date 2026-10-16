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
