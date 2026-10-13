#include "vectorimage.h"
#include "catch.hpp"

#include "editor.h"
#include "object.h"
#include "layermanager.h"
#include "layervector.h"
#include "scribblearea.h"

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
    Object* obj = new Object;
    obj->init();

    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(obj);
    editor->init();
    scribbleArea->setEditor(editor);
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

    delete scribbleArea;
    delete editor;
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
