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
#include <QCryptographicHash>
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

namespace
{

// One bitmap layer, keyframes 1-5; frame 5 crosses the right canvas edge, frame 2 is 70 % opaque.
std::unique_ptr<Object> onionFrames()
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    LayerBitmap* layer = object->addNewBitmapLayer();
    for (int f = 1; f <= 5; ++f)
    {
        layer->addNewKeyFrameAt(f);
        layer->getBitmapImageAtFrame(f)->drawEllipse(QRectF(-160 + f * 50, -80 + f * 12, 110, 90), QPen(Qt::black, 3),
                                                     QBrush(QColor(60 + f * 30, 180, 90)), QPainter::CompositionMode_SourceOver, true);
    }
    layer->getBitmapImageAtFrame(2)->setOpacity(0.7);
    return object;
}

void setUpOnion(CanvasPainter& painter, const Object* object, const QTransform& view, bool colorize)
{
    painter.setViewTransform(view, view.inverted());
    CanvasPainterOptions options;
    options.eLayerVisibility = LayerVisibility::ALL;
    painter.setOptions(options);
    OnionSkinPainterOptions onion;
    onion.skinPrevFrames = onion.skinNextFrames = true;
    onion.colorizePrevFrames = onion.colorizeNextFrames = colorize;
    onion.framesToSkinPrev = onion.framesToSkinNext = 2;
    onion.minOpacity = 20;
    onion.maxOpacity = 50;
    painter.setOnionSkinOptions(onion);
    painter.setPaintSettings(object, 0, 3, nullptr);
}

const QTransform kOnionViews[] = { QTransform::fromTranslate(150, 100),
                                   QTransform().translate(150, 100).rotate(30).scale(1.5, 1.5) };

QByteArray sha1(const QImage& image)
{
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return QCryptographicHash::hash(QByteArrayView(argb.constBits(), argb.sizeInBytes()), QCryptographicHash::Sha1).toHex();
}

} // namespace

// Hash of these onion renders before tinted onion frames were cached (commit 235bf79).
TEST_CASE("Onion skin: tinted onion frames render as before", "[CanvasPainter]")
{
    std::unique_ptr<Object> object = onionFrames();
    QCryptographicHash all(QCryptographicHash::Sha1);
    for (bool colorize : { true, false })
    {
        for (const QTransform& view : kOnionViews)
        {
            QPixmap canvas(kCanvas.size());
            CanvasPainter painter(canvas);
            setUpOnion(painter, object.get(), view, colorize);
            painter.paint(kCanvas);
            const QImage full = canvas.toImage();
            all.addData(sha1(full));

            // Small updates and a repaint after a scrub give the same pixels.
            painter.resetLayerCache();
            painter.paintCached(QRect(10, 10, 40, 40));
            painter.paintCached(QRect(240, 50, 60, 100));
            painter.paintCached(kCanvas);
            CHECK(canvas.toImage() == full);
        }
    }
    CHECK(all.result().toHex().toStdString() == "fa4395ff4b7fd0fc2fe848b040d8b65431ce330b");
}

TEST_CASE("Onion skin: a scrub back reuses the tinted onion frames", "[CanvasPainter]")
{
    std::unique_ptr<Object> object = onionFrames();
    QPixmap canvas(kCanvas.size());
    CanvasPainter painter(canvas);
    setUpOnion(painter, object.get(), kOnionViews[0], true);
    painter.paint(kCanvas);
    const QImage frame3 = canvas.toImage();
    const int tinted = painter.onionFramesTinted();
    CHECK(tinted == 4);

    painter.setPaintSettings(object.get(), 0, 4, nullptr);
    painter.resetLayerCache(); // as on every scrub
    painter.paint(kCanvas);
    painter.setPaintSettings(object.get(), 0, 3, nullptr);
    painter.resetLayerCache();
    painter.paint(kCanvas);
    CHECK(canvas.toImage() == frame3);
    const int afterScrubBack = painter.onionFramesTinted();
    CHECK(afterScrubBack == tinted + 3); // only frame 4's onion frames were new

    // A drawn-on onion frame is tinted again.
    static_cast<LayerBitmap*>(object->getLayer(0))->getBitmapImageAtFrame(2)->drawRect(
        QRectF(-100, -60, 40, 40), Qt::NoPen, QColor(20, 120, 40), QPainter::CompositionMode_SourceOver, false);
    painter.resetLayerCache(); // as on frameModified
    painter.paint(kCanvas);
    CHECK(painter.onionFramesTinted() == afterScrubBack + 1);
    CHECK(canvas.toImage() != frame3);

    QPixmap fresh(kCanvas.size());
    CanvasPainter freshPainter(fresh);
    setUpOnion(freshPainter, object.get(), kOnionViews[0], true);
    freshPainter.paint(kCanvas);
    CHECK(canvas.toImage() == fresh.toImage());
}
