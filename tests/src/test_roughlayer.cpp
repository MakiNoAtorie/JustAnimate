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
#include <QDomDocument>
#include <QIcon>

#include "bitmapimage.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "testfixture.h"
#include "undoredomanager.h"
#include "viewmanager.h"

namespace
{
LayerBitmap* filledLayer(Object* object, QColor color)
{
    LayerBitmap* layer = object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->getBitmapImageAtFrame(1)->drawRect(QRectF(-20, -20, 40, 40), Qt::NoPen, QBrush(color),
                                              QPainter::CompositionMode_Source, false);
    return layer;
}
} // namespace

TEST_CASE("The Rough flag is saved in the project", "[RoughLayer]")
{
    QDomDocument doc;
    LayerBitmap saved(1);
    REQUIRE_FALSE(saved.createBaseDomElement(doc).hasAttribute("rough"));

    saved.setRough(true);
    const QDomElement elem = saved.createBaseDomElement(doc);
    CHECK(elem.attribute("rough") == "1");
    LayerBitmap loaded(2);
    loaded.loadBaseDomElement(elem);
    CHECK(loaded.isRough());

    QDomElement old = LayerBitmap(3).createBaseDomElement(doc);
    loaded.loadBaseDomElement(old);
    CHECK_FALSE(loaded.isRough());
}

TEST_CASE("Rough toggles undo and duplicates keep the flag", "[RoughLayer]")
{
    TestSession s;
    LayerBitmap* layer = filledLayer(s.object, Qt::black);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    editor->layers()->setRough(layer, true);
    REQUIRE(layer->isRough());
    CHECK(editor->layers()->duplicateLayer(layer, "Copy")->isRough());
    undo->trigger(); // the duplicate
    undo->trigger();
    CHECK_FALSE(layer->isRough());
    redo->trigger();
    CHECK(layer->isRough());
}

TEST_CASE("Exports leave Rough layers out", "[RoughLayer]")
{
    Object object;
    object.init();
    filledLayer(&object, Qt::red);
    filledLayer(&object, Qt::blue)->setRough(true);

    const QImage image = object.renderFrame(1, QTransform(), QSize(60, 60), QSize(60, 60), false, true);
    CHECK(image.pixel(30, 30) == qRgb(255, 0, 0));
}

TEST_CASE("A Rough layer shows tinted and faded on the canvas", "[RoughLayer]")
{
    TestSession s;
    LayerBitmap* layer = filledLayer(s.object, Qt::black);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(400, 300);
    area->show(); // sizes the canvas pixmaps
    QApplication::processEvents();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    // The wiring MainWindow2 makes: a layer change drops the canvas caches.
    QObject::connect(editor->layers(), &LayerManager::currentLayerChanged, area, &ScribbleArea::onLayerChanged);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    const QPoint center = editor->view()->mapCanvasToScreen(QPointF(0, 0)).toPoint();

    REQUIRE(area->grab().toImage().pixel(center) == qRgb(0, 0, 0));

    editor->layers()->setRough(layer, true);
    const QColor tinted = area->grab().toImage().pixelColor(center);
    INFO("tinted " << tinted.name().toStdString());
    CHECK(tinted.blue() > tinted.red() + 20);
    CHECK(tinted.lightness() > 128); // faded towards the paper

    undo->trigger();
    CHECK(area->grab().toImage().pixel(center) == qRgb(0, 0, 0));
}
