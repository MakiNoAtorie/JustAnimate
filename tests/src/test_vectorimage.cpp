#include "testfixture.h"
#include "vectorimage.h"
#include "catch.hpp"

#include "editor.h"
#include "object.h"
#include "layermanager.h"
#include "layervector.h"
#include "scribblearea.h"
#include "selectionmanager.h"

TEST_CASE("VectorImage removeColor")
{
    auto vImage = VectorImage();
    auto bezier = BezierCurve({ QPoint(50,50), QPoint(100,100)});
    SECTION("Ensure that number is changed")
    {
        bezier.setColorNumber(3);
        vImage.addCurve(bezier, 1.0);

        vImage.removeColor(3);

        REQUIRE(vImage.curve(0).getColorNumber() == 2);
    }

    SECTION("Can't get below zero")
    {
        bezier.setColorNumber(0);
        vImage.addCurve(bezier, 1.0);

        vImage.removeColor(0);

        REQUIRE(vImage.curve(0).getColorNumber() == 0);
    }
}

TEST_CASE("Editor - Vector copy and paste")
{
    TestSession session;
    Object* obj = session.object;

    session.start();
    Editor* editor = session.editor.get();
    ScribbleArea* scribbleArea = session.area.get();
    scribbleArea->init();

    LayerVector* layer = obj->addNewVectorLayer();
    editor->layers()->setCurrentLayer(layer);
    layer->addNewKeyFrameAt(2);

    VectorImage* source = layer->getVectorImageAtFrame(1);
    for (int color : { 1, 2 })
    {
        auto curve = BezierCurve({ QPoint(50 * color, 50), QPoint(50 * color, 100) });
        curve.setColorNumber(color);
        source->addCurve(curve, 1.0, false);
    }
    VectorImage* target = layer->getVectorImageAtFrame(2);
    REQUIRE(target != source);

    auto copyFrame1PasteFrame2 = [&] {
        editor->scrubTo(1);
        editor->copy();
        editor->scrubTo(2);
        editor->paste();
        target->selectAll();
        return target->getSelectedCurveNumbers().size();
    };

    SECTION("With a selection, only the selected curves are pasted")
    {
        source->setSelected(1, true);

        REQUIRE(copyFrame1PasteFrame2() == 1);
        REQUIRE(target->curve(0).getColorNumber() == 2);
    }

    SECTION("Without a selection, the whole frame is pasted")
    {
        REQUIRE(copyFrame1PasteFrame2() == 2);
    }

    SECTION("The selection covers the pasted curves")
    {
        source->setSelected(1, true);
        editor->scrubTo(1);
        editor->copy();
        editor->scrubTo(2);
        editor->paste();

        REQUIRE(editor->select()->mySelectionRect() == target->curve(0).getBoundingRect());
    }
}

TEST_CASE("VectorImage fillSelectedPath")
{
    VectorImage vImage;
    BezierCurve square({ QPointF(10, 10), QPointF(90, 10), QPointF(90, 90), QPointF(10, 90), QPointF(10, 10) });
    vImage.addCurve(square, 1.0);
    vImage.selectAll();

    REQUIRE_FALSE(vImage.curve(0).isFilled());
    REQUIRE(vImage.getFirstAreaNumber(QPointF(50, 50)) == -1);

    vImage.fillSelectedPath(3);

    REQUIRE(vImage.curve(0).isFilled());
    REQUIRE(vImage.getFirstAreaNumber(QPointF(50, 50)) >= 0);
    REQUIRE(vImage.getFirstAreaNumber(QPointF(200, 200)) == -1);
}

namespace {
// Adds a one-curve vector frame at frame 1 coloured `colour`.
void addCurve(LayerVector* layer, int colour)
{
    layer->addNewKeyFrameAt(1);
    BezierCurve curve({ QPoint(50, 50), QPoint(100, 100) });
    curve.setColorNumber(colour);
    layer->getVectorImageAtFrame(1)->addCurve(curve, 1.0);
}
}

TEST_CASE("Object importVectorColors remaps once")
{
    Object source;
    for (int i = 0; i < 5; i++)
        source.addColor(ColorRef(QColor(i * 40, 0, 0), QString("c%1").arg(i)));
    LayerVector* layerA = source.addNewVectorLayer();
    addCurve(layerA, 0);
    BezierCurve second({ QPoint(0, 0), QPoint(9, 9) });
    second.setColorNumber(2);
    layerA->getVectorImageAtFrame(1)->addCurve(second, 1.0);
    LayerVector* layerB = source.addNewVectorLayer();
    addCurve(layerB, 2);

    Object target;
    target.addColor(ColorRef(Qt::white, "t0"));
    target.addColor(ColorRef(Qt::black, "t1"));
    QMap<int, int> imported;

    target.importVectorColors(layerA, source, imported);
    target.importVectorColors(layerB, source, imported);

    VectorImage* img = layerA->getVectorImageAtFrame(1);
    REQUIRE(target.getColor(img->curve(0).getColorNumber()).color == source.getColor(0).color);
    REQUIRE(target.getColor(img->curve(1).getColorNumber()).color == source.getColor(2).color);
    REQUIRE(target.getColor(layerB->getVectorImageAtFrame(1)->curve(0).getColorNumber()).color == source.getColor(2).color);
    REQUIRE(target.getColorCount() == 4);
}
