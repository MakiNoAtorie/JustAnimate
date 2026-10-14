#include "catch.hpp"

#include "layermanager.h"
#include "filemanager.h"
#include "scribblearea.h"

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
