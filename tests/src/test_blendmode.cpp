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
#include <random>
#include <vector>
#include <QAction>
#include <QDomDocument>
#include <QIcon>
#include <QPixmap>

#include "framerenderer.h"
#include "bitmapimage.h"
#include "blendmode.h"
#include "canvaspainter.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "scribblearea.h"
#include "editor.h"
#include "stageprofiler.h"
#include "testfixture.h"
#include "undoredomanager.h"

namespace
{

const QRgb kBelow = qRgb(200, 100, 50);
const QRgb kAbove = qRgb(128, 128, 128);

// Opaque results of kAbove over kBelow, worked out from the W3C compositing formulas.
QRgb expected(BlendMode mode)
{
    switch (mode)
    {
    case BlendMode::Multiply: return qRgb(100, 50, 25);
    case BlendMode::Screen: return qRgb(228, 178, 153);
    case BlendMode::Overlay: return qRgb(200, 100, 50);
    case BlendMode::Add: return qRgb(255, 228, 178);
    default: return kAbove;
    }
}

// Two bitmap layers filled with one color each; the top one uses `mode`.
std::unique_ptr<Object> twoLayers(BlendMode mode)
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    for (QRgb color : { kBelow, kAbove })
    {
        LayerBitmap* layer = object->addNewBitmapLayer();
        layer->addNewKeyFrameAt(1);
        layer->getBitmapImageAtFrame(1)->drawRect(QRectF(-20, -20, 40, 40), Qt::NoPen, QBrush(QColor(color)),
                                                  QPainter::CompositionMode_Source, false);
    }
    object->getLayer(1)->setBlendMode(mode);
    return object;
}

QRgb canvasCenter(const Object* object, int paintedLayer)
{
    QImage canvas(60, 60, QImage::Format_ARGB32_Premultiplied);
    CanvasPainter painter(canvas);
    const QTransform view = QTransform::fromTranslate(30, 30);
    painter.setViewTransform(view, view.inverted());
    CanvasPainterOptions options;
    options.eLayerVisibility = LayerVisibility::ALL;
    painter.setOptions(options);
    painter.setPaintSettings(object, paintedLayer, 1, nullptr);
    painter.paint(QRect(0, 0, 60, 60));
    return canvas.pixel(30, 30);
}

const BlendMode kModes[] = { BlendMode::Multiply, BlendMode::Screen, BlendMode::Overlay, BlendMode::Add };

} // namespace

TEST_CASE("Blend modes composite opaque pixels", "[BlendMode]")
{
    for (BlendMode mode : kModes)
    {
        quint32 dst = kBelow;
        const quint32 src = kAbove;
        blendPixels(mode, &dst, &src, 1);
        CHECK(dst == expected(mode));
    }
}

TEST_CASE("Blend modes keep the layer over transparent pixels and the backdrop under them", "[BlendMode]")
{
    const quint32 layer = qPremultiply(qRgba(200, 100, 50, 128));
    for (BlendMode mode : kModes)
    {
        quint32 dst = 0;
        blendPixels(mode, &dst, &layer, 1);
        CHECK(dst == layer);

        const quint32 clear = 0;
        dst = layer;
        blendPixels(mode, &dst, &clear, 1);
        CHECK(dst == layer);
    }
}

TEST_CASE("The vector blend matches the scalar blend byte for byte", "[BlendMode]")
{
    // Every source and backdrop alpha pair, random premultiplied colours, 3 extra pixels for the tail.
    const int count = 256 * 256 + 3;
    std::mt19937 rng(96);
    std::vector<quint32> src(count), dst(count);
    for (int i = 0; i < count; ++i)
    {
        const int sa = i & 0xff;
        const int da = (i >> 8) & 0xff;
        auto channel = [&](int a) { return int(rng() % (a + 1)); };
        src[i] = qRgba(channel(sa), channel(sa), channel(sa), sa);
        dst[i] = qRgba(channel(da), channel(da), channel(da), da);
    }
    for (BlendMode mode : { BlendMode::Normal, BlendMode::Multiply, BlendMode::Screen, BlendMode::Overlay, BlendMode::Add })
    {
        std::vector<quint32> vector = dst, scalar = dst;
        blendPixels(mode, vector.data(), src.data(), count);
        blendPixelsScalar(mode, scalar.data(), src.data(), count);
        int mismatches = 0;
        for (int i = 0; i < count; ++i)
            mismatches += vector[i] != scalar[i];
        INFO("mode " << int(mode));
        CHECK(mismatches == 0);
    }
}

TEST_CASE("A blend layer composites over the layers below on the canvas", "[BlendMode]")
{
    for (BlendMode mode : kModes)
    {
        auto object = twoLayers(mode);
        CHECK(canvasCenter(object.get(), 1) == expected(mode)); // painting the blend layer
        CHECK(canvasCenter(object.get(), 0) == expected(mode)); // painting the layer below it
    }
}

TEST_CASE("A blend layer composites over the layers below on export", "[BlendMode]")
{
    for (BlendMode mode : kModes)
    {
        auto object = twoLayers(mode);
        QImage image(60, 60, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setWorldTransform(QTransform::fromTranslate(30, 30));
        FrameRenderer::paint(*object, 1, painter, { true, false });
        painter.end();
        CHECK(image.pixel(30, 30) == expected(mode));
    }
}

TEST_CASE("A blend layer on export blends into the image without copies", "[BlendMode]")
{
    const bool profiling = StageProfiler::enabled();
    StageProfiler::enabled() = true;
    for (BlendMode mode : kModes)
    {
        auto object = twoLayers(mode);
        StageProfiler::reset();
        QImage image(60, 60, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setWorldTransform(QTransform::fromTranslate(30, 30));
        FrameRenderer::paint(*object, 1, painter, { true, false });
        painter.end();
        CHECK(StageProfiler::calls(StageProfiler::BlendCopy) == 0);
        CHECK(image.pixel(30, 30) == expected(mode));
    }
    StageProfiler::enabled() = profiling;
    StageProfiler::reset();
}

TEST_CASE("A blend layer plays on the canvas without copies", "[BlendMode]")
{
    TestSession s;
    Layer* top = nullptr;
    for (QRgb color : { kBelow, kAbove })
    {
        LayerBitmap* layer = s.object->addNewBitmapLayer();
        layer->addNewKeyFrameAt(1);
        layer->getBitmapImageAtFrame(1)->drawRect(QRectF(-20, -20, 40, 40), Qt::NoPen, QBrush(QColor(color)),
                                                  QPainter::CompositionMode_Source, false);
        top = layer;
    }
    top->setBlendMode(BlendMode::Multiply);
    REQUIRE(s.start());
    s.area->init();
    s.area->resize(200, 200);
    s.area->show();

    const bool profiling = StageProfiler::enabled();
    StageProfiler::enabled() = true;
    StageProfiler::reset();
    s.editor->playback()->play();
    const QImage shown = s.area->grab().toImage();
    s.editor->playback()->stop();
    CHECK(StageProfiler::calls(StageProfiler::BlendCopy) == 0);
    CHECK(shown.pixel(shown.width() / 2, shown.height() / 2) == expected(BlendMode::Multiply));
    StageProfiler::enabled() = profiling;
    StageProfiler::reset();
}

// A partial repaint at a fractional and a whole device pixel ratio keeps the pixels outside it.
TEST_CASE("A blend layer repaints part of a high-DPI canvas as the full paint does", "[BlendMode]")
{
    auto object = twoLayers(BlendMode::Screen);
    for (qreal dpr : { 1.5, 2.0 })
    {
        QImage canvas(QSize(60, 60) * dpr, QImage::Format_ARGB32_Premultiplied);
        canvas.setDevicePixelRatio(dpr);
        CanvasPainter painter(canvas);
        const QTransform view = QTransform::fromTranslate(30, 30);
        painter.setViewTransform(view, view.inverted());
        CanvasPainterOptions options;
        options.eLayerVisibility = LayerVisibility::ALL;
        painter.setOptions(options);
        painter.setPaintSettings(object.get(), 0, 1, nullptr);
        painter.paint(QRect(0, 0, 60, 60));
        const QImage full = canvas.copy();

        canvas.fill(Qt::transparent);
        const QImage cleared = canvas.copy();
        const QRect part(13, 7, 25, 31);
        painter.paint(part);
        const QRect device = QTransform::fromScale(dpr, dpr).mapRect(QRectF(part)).toRect();
        int wrong = 0;
        for (int y = 0; y < canvas.height(); ++y)
            for (int x = 0; x < canvas.width(); ++x)
            {
                const bool inside = device.adjusted(1, 1, -1, -1).contains(x, y);
                const bool outside = !device.adjusted(-1, -1, 1, 1).contains(x, y);
                if (inside) wrong += canvas.pixel(x, y) != full.pixel(x, y);
                if (outside) wrong += canvas.pixel(x, y) != cleared.pixel(x, y);
            }
        INFO("dpr " << dpr);
        CHECK(wrong == 0);
    }
}

TEST_CASE("The layer blend mode is saved in the project", "[BlendMode]")
{
    QDomDocument doc;

    SECTION("Each mode round-trips")
    {
        for (BlendMode mode : kModes)
        {
            LayerBitmap saved(1);
            saved.setBlendMode(mode);
            LayerBitmap loaded(2);
            loaded.loadBaseDomElement(saved.createBaseDomElement(doc));
            CHECK(loaded.blendMode() == mode);
        }
    }
    SECTION("Normal writes no attribute and old files load as normal")
    {
        LayerBitmap saved(1);
        const QDomElement elem = saved.createBaseDomElement(doc);
        REQUIRE_FALSE(elem.hasAttribute("blend"));
        LayerBitmap loaded(2);
        loaded.setBlendMode(BlendMode::Screen);
        loaded.loadBaseDomElement(elem);
        REQUIRE(loaded.blendMode() == BlendMode::Normal);
    }
    SECTION("An unknown mode loads as normal")
    {
        QDomElement elem = LayerBitmap(1).createBaseDomElement(doc);
        elem.setAttribute("blend", 99);
        LayerBitmap loaded(2);
        loaded.loadBaseDomElement(elem);
        REQUIRE(loaded.blendMode() == BlendMode::Normal);
    }
}

TEST_CASE("A blend mode change undoes and redoes", "[BlendMode]")
{
    TestSession session;
    Layer* layer = session.object->addNewBitmapLayer();
    session.start();
    Editor* editor = session.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    editor->layers()->setBlendMode(layer, BlendMode::Multiply);
    REQUIRE(layer->blendMode() == BlendMode::Multiply);
    REQUIRE(editor->undoRedo()->hasUnsavedChanges());

    undo->trigger();
    REQUIRE(layer->blendMode() == BlendMode::Normal);
    redo->trigger();
    REQUIRE(layer->blendMode() == BlendMode::Multiply);
}
