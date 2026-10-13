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

#include <QApplication>
#include <QPointingDevice>
#include <QTabletEvent>

#include "bitmapimage.h"
#include "brushtool.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "toolmanager.h"

namespace
{

void tablet(ScribbleArea* area, QEvent::Type type, QPointF pos, qreal pressure)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   pressure, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

// A stroke from `from` to `to` whose pressure ends at `endPressure`.
void stroke(ScribbleArea* area, QPointF from, QPointF to, qreal endPressure)
{
    tablet(area, QEvent::TabletPress, from, 0.5);
    for (int i = 1; i <= 10; ++i)
        tablet(area, QEvent::TabletMove, from + (to - from) * i / 10.0, endPressure);
    tablet(area, QEvent::TabletRelease, to, endPressure);
}

void tap(ScribbleArea* area, QPointF pos, qreal pressure)
{
    tablet(area, QEvent::TabletPress, pos, pressure);
    tablet(area, QEvent::TabletRelease, pos, pressure);
}

// Width in pixels of the painted (non-transparent) part of the image.
int inkWidth(BitmapImage* image)
{
    const QImage* img = image->image();
    int left = img->width(), right = -1;
    for (int y = 0; y < img->height(); ++y)
        for (int x = 0; x < img->width(); ++x)
            if (qAlpha(img->pixel(x, y)) != 0)
            {
                left = std::min(left, x);
                right = std::max(right, x);
            }
    return right - left + 1;
}

} // namespace

TEST_CASE("StrokeTool - A tap is sized by its own pressure, not the previous stroke's")
{
    Object* obj = new Object;
    obj->init();
    LayerBitmap* layer = obj->addNewBitmapLayer();
    for (int f = 2; f <= 4; ++f)
        layer->addNewKeyFrameAt(f);

    Editor* editor = new Editor;
    ScribbleArea* area = new ScribbleArea(nullptr);
    editor->setScribbleArea(area);
    editor->setObject(obj);
    editor->init();
    area->setEditor(editor);
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    brush->setWidth(40);
    brush->setPressureEnabled(true);
    brush->setFeatherEnabled(false);

    // A light stroke, then a hard tap.
    editor->scrubTo(1);
    stroke(area, QPointF(100, 100), QPointF(300, 100), 0.1);
    editor->scrubTo(2);
    tap(area, QPointF(400, 300), 1.0);

    // A hard stroke, then a light tap.
    editor->scrubTo(3);
    stroke(area, QPointF(100, 200), QPointF(300, 200), 1.0);
    editor->scrubTo(4);
    tap(area, QPointF(400, 400), 0.2);

    const int hardDot = inkWidth(layer->getBitmapImageAtFrame(2));
    const int lightDot = inkWidth(layer->getBitmapImageAtFrame(4));
    INFO("hard " << hardDot << " light " << lightDot);
    CHECK(hardDot >= 30);
    CHECK(lightDot > 0);
    CHECK(lightDot <= 15);

    delete area;
    delete editor;
}
