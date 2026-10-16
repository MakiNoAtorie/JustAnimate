#include "catch.hpp"

#include <QPainter>

#include "layermanager.h"
#include "filemanager.h"
#include "scribblearea.h"
#include "undoredomanager.h"

#include "layerbitmap.h"

#include "object.h"
#include "editor.h"
#include "bitmapbucket.h"
#include "bitmapimage.h"

#include "basetool.h"
#include "testfixture.h"

void dragAndFill(QPointF movePoint, Editor* editor, QColor color, QRect bounds, BucketToolProperties properties, int fillCountThreshold) {
    int moveX = 0;

    BitmapBucket bucket = BitmapBucket(editor, color, bounds, movePoint, properties);
    QPointF movingPoint = movePoint;

    int fillCount = 0;
    while (moveX < bounds.width()) {
        moveX++;
        movingPoint.setX(movingPoint.x()+1);

        bucket.paint(movingPoint, [&fillCount] (BucketState state, int, int ) {

            if (state == BucketState::DidFillTarget) {
                fillCount++;
            }
        });
    }

    REQUIRE(fillCount == fillCountThreshold);
}

void verifyOnlyPixelsInsideSegmentsAreFilled(QPoint referencePoint, const BitmapImage* image, QRgb fillColor)
{
    REQUIRE(image->constScanLine(referencePoint.x(), referencePoint.y()) == fillColor);

    // pixels that are not transparent nor the given fill color, should be left untouched
    REQUIRE(image->constScanLine(referencePoint.x()+4, referencePoint.y()) != fillColor);
    REQUIRE(image->constScanLine(referencePoint.x()+5, referencePoint.y()) != fillColor);
    REQUIRE(image->constScanLine(referencePoint.x()+6, referencePoint.y()) != fillColor);
}

/**
 *    Ascii representation of test project
 *    The "*" represent black strokes.
 *    The space inbetween represents transparency
 *    ***************
 *    *  *   *   *  *
 *    ***************
 *
 *    The test cases are based around filling on the initially transparent area and dragging across the four segments.
 */
TEST_CASE("BitmapBucket - Fill drag behaviour across four segments")
{
    FileManager fm;
    TestSession session(fm.load(":/fill-drag-test/fill-drag-test.pcl"));
    Object* obj = session.object;
    REQUIRE(obj != nullptr);
    session.start();
    Editor* editor = session.editor.get();

    BucketToolProperties properties;
    QSettings settings;

    QHash<int, PropertyInfo> info;

    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
    info[BucketToolProperties::FILLEXPAND_ENABLED] = false;
    info[BucketToolProperties::FILLMODE_VALUE] = 0;
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = 25;
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = true;
    properties.toolProperties().insertProperties(info);
    properties.toolProperties().loadFrom("BucketTest", settings);

    QColor fillColor = QColor(255,255,0,100);

    BitmapImage beforeFill = *static_cast<LayerBitmap*>(editor->layers()->currentLayer())->getBitmapImageAtFrame(1);

    QPoint pressPoint = beforeFill.bounds().topLeft();

    pressPoint.setX(pressPoint.x()+3);
    pressPoint.setY(pressPoint.y()+7);

    REQUIRE(beforeFill.constScanLine(pressPoint.x(), pressPoint.y()) == 0);

    // The dragging logic is based around that we only fill on either transparent or the same color as the fill color.
    SECTION("Filling on current layer - layer is not pre filled") {
        Layer* strokeLayer = editor->layers()->currentLayer();
        SECTION("When reference is current layer, only transparent color is filled")
        {
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 0);
            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, qPremultiply(fillColor.rgba()));
        }

        SECTION("When reference is all layers, only transparent color is filled")
        {
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 1);

            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, qPremultiply(fillColor.rgba()));
        }
    }

    SECTION("Filling on current layer - layer is pre-filled") {

        // Fill mode is set to `replace` because it makes it easier to compare colors...
        properties.toolProperties().setBaseValue(BucketToolProperties::FILLMODE_VALUE, 1);
        Layer* strokeLayer = editor->layers()->currentLayer();
        SECTION("When reference is current layer, only pixels matching the fill color are filled"){
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 0);

            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);
            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            fillColor = QColor(0,255,0,255);
            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, fillColor.rgba());
        }

        SECTION("When reference is all layers")
        {
            properties.toolProperties().setBaseValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, 1);

            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);
            BitmapImage* image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            fillColor = QColor(0,255,0,255);
            dragAndFill(pressPoint, editor, fillColor, beforeFill.bounds(), properties, 4);

            image = static_cast<LayerBitmap*>(strokeLayer)->getLastBitmapImageAtFrame(1);

            verifyOnlyPixelsInsideSegmentsAreFilled(pressPoint, image, fillColor.rgba());
        }
    }
}

TEST_CASE("BitmapBucket - Fill before the layer's first keyframe does nothing")
{
    TestSession session;
    Object* obj = session.object;
    LayerBitmap* layer = obj->addNewBitmapLayer();
    layer->addNewKeyFrameAt(5);
    layer->removeKeyFrame(1);

    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    REQUIRE(layer->getLastKeyFrameAtPosition(1) == nullptr);

    BucketToolProperties properties;
    QHash<int, PropertyInfo> info;
    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
    info[BucketToolProperties::FILLEXPAND_ENABLED] = false;
    info[BucketToolProperties::FILLMODE_VALUE] = 0;
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = 25;
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = true;
    properties.toolProperties().insertProperties(info);

    BitmapBucket bucket(editor, Qt::red, QRect(0, 0, 10, 10), QPointF(2, 2), properties);

    int fillCount = 0;
    bucket.paint(QPointF(2, 2), [&fillCount](BucketState, int, int) { fillCount++; });

    REQUIRE(fillCount == 0);
    REQUIRE(layer->keyFrameCount() == 1);
    REQUIRE_FALSE(layer->keyExists(1));
}

TEST_CASE("BitmapBucket - Click outside the image fills the empty area around the drawing")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);

    const QRgb black = qPremultiply(qRgba(0, 0, 0, 255));
    BitmapImage* image = layer->getLastBitmapImageAtFrame(1);
    BitmapImage canvas(QRect(0, 0, 10, 10), Qt::transparent);
    image->paste(&canvas);

    const bool wholeBorderOpaque = GENERATE(false, true);
    if (wholeBorderOpaque) {
        for (int i = 0; i < 10; i++) {
            image->setPixel(i, 0, black);
            image->setPixel(i, 9, black);
            image->setPixel(0, i, black);
            image->setPixel(9, i, black);
        }
    } else {
        image->setPixel(0, 0, black);
    }

    BucketToolProperties properties;
    QHash<int, PropertyInfo> info;
    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
    info[BucketToolProperties::FILLEXPAND_ENABLED] = false;
    info[BucketToolProperties::FILLMODE_VALUE] = 0;
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = 25;
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = true;
    properties.toolProperties().insertProperties(info);

    BitmapBucket bucket(editor, Qt::red, QRect(0, 0, 10, 10), QPointF(-5, -5), properties);
    int fillCount = 0;
    bucket.paint(QPointF(-5, -5), [&fillCount](BucketState s, int, int) { if (s == BucketState::DidFillTarget) fillCount++; });

    image = layer->getLastBitmapImageAtFrame(1);
    if (wholeBorderOpaque) {
        REQUIRE(fillCount == 0);
        REQUIRE(image->constScanLine(5, 5) == 0);
        REQUIRE(editor->undoRedo()->pendingStateCount() == 0);
    } else {
        REQUIRE(fillCount == 1);
        REQUIRE(image->constScanLine(0, 0) == black);
        REQUIRE(image->constScanLine(5, 5) == qRgba(255, 0, 0, 255));
    }
}

TEST_CASE("BitmapBucket - Behind mode fills under antialiased line art")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);

    // Antialiased vertical line at x = 5
    const QRgb edge = qPremultiply(qRgba(0, 0, 0, 128));
    BitmapImage* image = layer->getLastBitmapImageAtFrame(1);
    BitmapImage canvas(QRect(0, 0, 10, 10), Qt::transparent);
    image->paste(&canvas);
    for (int y = 0; y < 10; y++) image->setPixel(5, y, edge);

    const bool behind = GENERATE(false, true);
    BucketToolProperties properties;
    QHash<int, PropertyInfo> info;
    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = 0;
    info[BucketToolProperties::FILLEXPAND_ENABLED] = true;
    info[BucketToolProperties::FILLEXPAND_VALUE] = { 1, 25, 1 };
    info[BucketToolProperties::FILLMODE_VALUE] = behind ? 2 : 0;
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = 25;
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = true;
    properties.toolProperties().insertProperties(info);

    BitmapBucket bucket(editor, Qt::red, QRect(0, 0, 10, 10), QPointF(2, 2), properties);
    bucket.paint(QPointF(2, 2), [](BucketState, int, int) {});

    image = layer->getLastBitmapImageAtFrame(1);
    REQUIRE(image->constScanLine(2, 2) != 0);
    // Overlay eats the edge; Behind puts the fill under the edge (DestinationOver).
    QImage expected(1, 1, QImage::Format_ARGB32_Premultiplied);
    expected.setPixel(0, 0, edge);
    QPainter painter(&expected);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    painter.fillRect(expected.rect(), Qt::red);
    painter.end();
    REQUIRE(image->constScanLine(5, 5) == (behind ? reinterpret_cast<const QRgb*>(expected.constScanLine(0))[0] : qRgba(255, 0, 0, 255)));
}
