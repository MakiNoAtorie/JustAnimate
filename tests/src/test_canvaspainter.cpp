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

#include <memory>
#include <QPixmap>

#include "bitmapimage.h"
#include "canvaspainter.h"
#include "layerbitmap.h"
#include "object.h"

namespace
{

const QRect kCanvas(0, 0, 300, 200); // several 64 px cells, the last ones partial

// Three bitmap layers; layer 2 is the painted layer.
std::unique_ptr<Object> threeLayers()
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    const QColor colors[] = { QColor(90, 160, 220), QColor(240, 200, 40, 200), QColor(200, 30, 90) };
    for (int l = 0; l < 3; ++l)
    {
        LayerBitmap* layer = object->addNewBitmapLayer();
        layer->addNewKeyFrameAt(1);
        layer->getBitmapImageAtFrame(1)->drawEllipse(QRectF(-140 + l * 60, -90 + l * 30, 160, 120), QPen(Qt::black, 3),
                                                     QBrush(colors[l]), QPainter::CompositionMode_SourceOver, true);
    }
    return object;
}

void setUp(CanvasPainter& painter, const Object* object, int paintedLayer = 2, bool fastAbove = false)
{
    QTransform view = QTransform::fromTranslate(150, 100);
    painter.setViewTransform(view, view.inverted());
    CanvasPainterOptions options;
    options.eLayerVisibility = LayerVisibility::ALL;
    options.bFastLayersAbove = fastAbove;
    painter.setOptions(options);
    painter.setPaintSettings(object, paintedLayer, 1, nullptr);
}

// The frame rendered by a fresh painter, with nothing cached.
QImage uncached(const Object* object, int paintedLayer = 2)
{
    QPixmap canvas(kCanvas.size());
    CanvasPainter painter(canvas);
    setUp(painter, object, paintedLayer);
    painter.paint(kCanvas);
    return canvas.toImage();
}

} // namespace

TEST_CASE("Below-layers cache: cells filled by small updates match an uncached render", "[CanvasPainter]")
{
    std::unique_ptr<Object> object = threeLayers();
    QPixmap canvas(kCanvas.size());
    CanvasPainter painter(canvas);
    setUp(painter, object.get());

    painter.paintCached(QRect(10, 10, 40, 40));
    painter.paintCached(QRect(100, 50, 30, 80));
    painter.paintCached(kCanvas);
    CHECK(canvas.toImage() == uncached(object.get()));

    painter.paint(QRect(200, 20, 50, 50));
    painter.paint(kCanvas);
    CHECK(canvas.toImage() == uncached(object.get()));
}

TEST_CASE("Below-layers cache: a change to a layer below the painted layer shows on the next paint", "[CanvasPainter]")
{
    std::unique_ptr<Object> object = threeLayers();
    QPixmap canvas(kCanvas.size());
    CanvasPainter painter(canvas);
    setUp(painter, object.get());
    painter.paint(kCanvas);
    const QImage before = canvas.toImage();

    // No signal and no resetLayerCache(): the cache must notice the change itself.
    auto* below = static_cast<LayerBitmap*>(object->getLayer(0));
    below->getBitmapImageAtFrame(1)->drawRect(QRectF(-150, -100, 120, 80), Qt::NoPen, QColor(20, 120, 40),
                                              QPainter::CompositionMode_SourceOver, false);
    painter.paintCached(kCanvas);
    CHECK(canvas.toImage() != before);
    CHECK(canvas.toImage() == uncached(object.get()));

    object->getLayer(1)->switchVisibility();
    painter.paint(kCanvas);
    CHECK(canvas.toImage() == uncached(object.get()));
}

TEST_CASE("Fast mode is off by default", "[CanvasPainter]")
{
    CHECK_FALSE(CanvasPainterOptions().bFastLayersAbove);
}

TEST_CASE("Fast mode: cells of the layers above filled by small updates match an uncached render", "[CanvasPainter]")
{
    std::unique_ptr<Object> object = threeLayers();
    QPixmap canvas(kCanvas.size());
    CanvasPainter painter(canvas);
    setUp(painter, object.get(), 0, true);

    painter.paint(QRect(10, 10, 40, 40));
    painter.paintCached(QRect(100, 50, 30, 80));
    painter.paint(kCanvas);
    CHECK(canvas.toImage() == uncached(object.get(), 0));
}

TEST_CASE("Fast mode: a change to a layer above the painted layer shows on the next paint", "[CanvasPainter]")
{
    std::unique_ptr<Object> object = threeLayers();
    QPixmap canvas(kCanvas.size());
    CanvasPainter painter(canvas);
    setUp(painter, object.get(), 0, true);
    painter.paint(kCanvas);
    const QImage before = canvas.toImage();

    // No signal and no resetLayerCache(): the cache must notice the change itself.
    auto* above = static_cast<LayerBitmap*>(object->getLayer(2));
    above->getBitmapImageAtFrame(1)->drawRect(QRectF(-150, -100, 120, 80), Qt::NoPen, QColor(20, 120, 40),
                                              QPainter::CompositionMode_SourceOver, false);
    painter.paintCached(kCanvas);
    CHECK(canvas.toImage() != before);
    CHECK(canvas.toImage() == uncached(object.get(), 0));

    object->getLayer(1)->switchVisibility();
    painter.paint(kCanvas);
    CHECK(canvas.toImage() == uncached(object.get(), 0));
}
