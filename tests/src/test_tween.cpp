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
#include <QAction>
#include <QDomDocument>
#include <QIcon>
#include <QPixmap>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "canvaspainter.h"
#include "editor.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"
#include "vectorimage.h"

namespace
{

const QRgb kRed = qRgb(255, 0, 0);

// A bitmap layer with a 10 px red square at the origin on keys 1 and 5; key 5 sits 20 px right.
std::unique_ptr<Object> tweenedSquare(bool tween, CameraEasingType easing = CameraEasingType::LINEAR)
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    LayerBitmap* layer = object->addNewBitmapLayer();
    for (int pos : { 1, 5 })
    {
        layer->addNewKeyFrameAt(pos);
        layer->getBitmapImageAtFrame(pos)->drawRect(QRectF(-5, -5, 10, 10), Qt::NoPen, QBrush(QColor(kRed)),
                                                    QPainter::CompositionMode_Source, false);
    }
    layer->getKeyFrameAt(1)->setTween(tween);
    layer->getKeyFrameAt(1)->setTweenEasing(easing);
    TweenState end;
    end.tx = 20;
    layer->getKeyFrameAt(5)->setTweenState(end);
    return object;
}

QImage exportFrame(const Object* object, int frame)
{
    QImage image(80, 40, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setWorldTransform(QTransform::fromTranslate(30, 20));
    object->paintImage(painter, frame, false, true);
    painter.end();
    return image;
}

QImage canvasFrame(const Object* object, int frame)
{
    QPixmap canvas(80, 40);
    CanvasPainter painter(canvas);
    const QTransform view = QTransform::fromTranslate(30, 20);
    painter.setViewTransform(view, view.inverted());
    CanvasPainterOptions options;
    options.eLayerVisibility = LayerVisibility::ALL;
    painter.setOptions(options);
    painter.setPaintSettings(object, 0, frame, nullptr);
    painter.paint(QRect(0, 0, 80, 40));
    return canvas.toImage();
}

} // namespace

TEST_CASE("A tweened key interpolates its transform on the frames it holds", "[Tween]")
{
    SECTION("Linear")
    {
        auto object = tweenedSquare(true);
        const Layer* layer = object->getLayer(0);
        CHECK(layer->tweenStateAt(1).tx == 0);
        CHECK(layer->tweenStateAt(2).tx == Approx(5));
        CHECK(layer->tweenStateAt(3).tx == Approx(10));
        CHECK(layer->tweenStateAt(5).tx == 20);
        CHECK(layer->tweenStateAt(9).tx == 20);
    }
    SECTION("Eased with the camera easing curves")
    {
        auto object = tweenedSquare(true, CameraEasingType::INQUAD);
        CHECK(object->getLayer(0)->tweenStateAt(3).tx == Approx(5)); // 20 * 0.5^2
    }
    SECTION("Without tween a key holds its own transform")
    {
        auto object = tweenedSquare(false);
        CHECK(object->getLayer(0)->tweenStateAt(3).tx == 0);
    }
}

TEST_CASE("In-between frames render with the tweened transform", "[Tween]")
{
    auto object = tweenedSquare(true);
    for (const QImage& image : { exportFrame(object.get(), 3), canvasFrame(object.get(), 3) })
    {
        CHECK(image.pixel(30 + 10, 20) == kRed);
        CHECK(image.pixel(30, 20) != kRed);
    }
    auto held = tweenedSquare(false);
    CHECK(exportFrame(held.get(), 3).pixel(30, 20) == kRed);
}

TEST_CASE("A tweened key interpolates its opacity to the next key", "[Tween]")
{
    auto object = tweenedSquare(true);
    object->getLayer(0)->getKeyFrameAt(5)->setOpacity(0);
    CHECK(object->getLayer(0)->tweenStateAt(1).opacity == 1);
    CHECK(object->getLayer(0)->tweenStateAt(3).opacity == Approx(0.5));
    CHECK(object->getLayer(0)->tweenStateAt(5).opacity == 0);
    for (const QImage& image : { exportFrame(object.get(), 3), canvasFrame(object.get(), 3) })
    {
        CHECK(qAlpha(image.pixel(30 + 10, 20)) == Approx(128).margin(2));
    }

    auto held = tweenedSquare(false);
    held->getLayer(0)->getKeyFrameAt(5)->setOpacity(0);
    CHECK(held->getLayer(0)->tweenStateAt(3).opacity == 1);
    CHECK(qAlpha(exportFrame(held.get(), 3).pixel(30, 20)) == 255);
}

TEST_CASE("An opacity tween round-trips through save and load", "[Tween]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("opacity.pclx");
    auto object = tweenedSquare(true);
    object->getLayer(0)->getKeyFrameAt(1)->setOpacity(0.8);
    object->getLayer(0)->getKeyFrameAt(5)->setOpacity(0.2);
    FileManager fm;
    REQUIRE(fm.save(object.get(), path).ok());
    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);

    const Layer* layer = loaded->findLayerById(object->getLayer(0)->id());
    REQUIRE(layer->getKeyFrameAt(1)->getOpacity() == Approx(0.8));
    REQUIRE(layer->getKeyFrameAt(5)->getOpacity() == Approx(0.2));
    REQUIRE(layer->tweenStateAt(3).opacity == Approx(0.5));
}

TEST_CASE("Tween attributes round-trip through save and load", "[Tween]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("tween.pclx");
    FileManager fm;
    std::unique_ptr<Object> object(new Object);
    object->init();
    LayerVector* vector = object->addNewVectorLayer();
    vector->addNewKeyFrameAt(1);
    vector->addNewKeyFrameAt(4);
    KeyFrame* key = vector->getKeyFrameAt(1);
    key->setTween(true);
    key->setTweenEasing(CameraEasingType::OUTBOUNCE);
    key->setTweenState(TweenState{ 1.5, -2, 30, 0.5 });
    LayerBitmap* bitmap = object->addNewBitmapLayer();
    bitmap->addNewKeyFrameAt(1);
    bitmap->getBitmapImageAtFrame(1)->drawRect(QRectF(0, 0, 4, 4), Qt::NoPen, QBrush(Qt::black),
                                               QPainter::CompositionMode_Source, false);
    bitmap->getKeyFrameAt(1)->setTween(true);
    bitmap->getKeyFrameAt(1)->setTweenState(TweenState{ 7, 8, -45, 2 });

    REQUIRE(fm.save(object.get(), path).ok());
    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);

    const KeyFrame* v = loaded->findLayerById(vector->id())->getKeyFrameAt(1);
    REQUIRE(v->tween());
    REQUIRE(v->tweenEasing() == CameraEasingType::OUTBOUNCE);
    REQUIRE(v->tweenState().tx == 1.5);
    REQUIRE(v->tweenState().ty == -2);
    REQUIRE(v->tweenState().rot == 30);
    REQUIRE(v->tweenState().scale == 0.5);
    REQUIRE_FALSE(loaded->findLayerById(vector->id())->getKeyFrameAt(4)->tween());

    const KeyFrame* b = loaded->findLayerById(bitmap->id())->getKeyFrameAt(1);
    REQUIRE(b->tween());
    REQUIRE(b->tweenEasing() == CameraEasingType::LINEAR);
    REQUIRE(b->tweenState().tx == 7);
    REQUIRE(b->tweenState().scale == 2);
}

TEST_CASE("A file without tween attributes loads unchanged", "[Tween]")
{
    QDomDocument doc;
    LayerBitmap saved(1);
    saved.addNewKeyFrameAt(1);
    const QDomElement image = saved.createDomElement(doc).firstChildElement("image");
    REQUIRE_FALSE(image.isNull());
    for (const char* attr : { "tween", "easing", "tx", "ty", "rot", "scale" })
    {
        CHECK_FALSE(image.hasAttribute(attr));
    }

    // Inline vector data: no src, so the key loads without a data folder.
    QDomElement layerElem = doc.createElement("layer");
    QDomElement legacy = doc.createElement("image");
    legacy.setAttribute("frame", 2);
    legacy.setAttribute("opacity", 0.5);
    layerElem.appendChild(legacy);
    LayerVector loaded(2);
    loaded.loadDomElement(layerElem, QString(), [] {});
    const KeyFrame* key = loaded.getKeyFrameAt(2);
    REQUIRE(key != nullptr);
    REQUIRE_FALSE(key->tween());
    REQUIRE(key->tweenState().isIdentity());
    REQUIRE(loaded.getVectorImageAtFrame(2)->getOpacity() == 0.5);
}

TEST_CASE("Untrusted tween attributes load as defaults", "[Tween]")
{
    QDomDocument doc;
    QDomElement layerElem = doc.createElement("layer");
    QDomElement image = doc.createElement("image");
    image.setAttribute("frame", 1);
    image.setAttribute("tween", 1);
    image.setAttribute("easing", 999);
    image.setAttribute("scale", "nan");
    layerElem.appendChild(image);
    LayerVector loaded(1);
    loaded.loadDomElement(layerElem, QString(), [] {});
    const KeyFrame* key = loaded.getKeyFrameAt(1);
    REQUIRE(key->tweenEasing() == CameraEasingType::LINEAR);
    REQUIRE(key->tweenState().scale == 1);

    for (const char* scale : { "0", "-2" })
    {
        CAPTURE(scale);
        image.setAttribute("scale", scale);
        LayerVector zero(1);
        zero.loadDomElement(layerElem, QString(), [] {});
        CHECK(zero.getKeyFrameAt(1)->tweenState().scale == 1);
    }
}

TEST_CASE("A key tween change undoes and redoes", "[Tween]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(5);
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    editor->layers()->setKeyTween(5, false, CameraEasingType::LINEAR, TweenState{ 20, 0, 0, 1 });
    editor->layers()->setKeyTween(1, true, CameraEasingType::INQUAD, TweenState());
    REQUIRE(layer->tweenStateAt(3).tx == Approx(5));

    undo->trigger();
    REQUIRE_FALSE(layer->getKeyFrameAt(1)->tween());
    REQUIRE(layer->tweenStateAt(3).tx == 0);
    undo->trigger();
    REQUIRE(layer->getKeyFrameAt(5)->tweenState().tx == 0);
    redo->trigger();
    redo->trigger();
    REQUIRE(layer->getKeyFrameAt(1)->tweenEasing() == CameraEasingType::INQUAD);
    REQUIRE(layer->tweenStateAt(3).tx == Approx(5));
}
