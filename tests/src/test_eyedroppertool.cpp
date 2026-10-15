/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "catch.hpp"

#include "editor.h"
#include "object.h"
#include "layermanager.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "colormanager.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "scribblearea.h"
#include "eyedroppertool.h"

TEST_CASE("EyedropperTool - Picking a half-transparent red pixel gives full red")
{
    TestSession session;
    Object* obj = session.object;
    LayerBitmap* layer = obj->addNewBitmapLayer();
    layer->getBitmapImageAtFrame(1)->drawRect(QRectF(0, 0, 10, 10), Qt::NoPen, QColor(255, 0, 0, 128),
                                              QPainter::CompositionMode_Source, false);

    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);

    EyedropperTool* eyedropper = static_cast<EyedropperTool*>(editor->tools()->getTool(EYEDROPPER));
    eyedropper->updateFrontColor(QPointF(5, 5));

    REQUIRE(editor->color()->frontColor() == QColor(255, 0, 0, 128));
}
