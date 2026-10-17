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

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QTemporaryDir>

#include "beziercurve.h"
#include "bitmapimage.h"
#include "brushpresets.h"
#include "brushtool.h"
#include "buckettool.h"
#include "colormanager.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "overlaymanager.h"
#include "pointerevent.h"
#include "preferencemanager.h"
#include "pressurecurve.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "selectionmanager.h"
#include "stageprofiler.h"
#include "tiledbuffer.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "undoredomanager.h"
#include "util.h"
#include "vectorimage.h"
#include "viewmanager.h"

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
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 2; f <= 4; ++f)
        layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
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
}

TEST_CASE("StrokeTool - a stabilized stroke ends at the pen on lift")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(2);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(10);
    brush->setPressureEnabled(false);
    brush->setFeatherEnabled(false);

    auto rightEdge = [](BitmapImage* image) {
        const int left = image->left();
        const QImage* img = image->image();
        int right = -1;
        for (int y = 0; y < img->height(); ++y)
            for (int x = right + 1; x < img->width(); ++x)
                if (qAlpha(img->pixel(x, y)) != 0) right = x;
        return left + right;
    };

    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));
    editor->scrubTo(1);
    stroke(area, QPointF(100, 300), QPointF(500, 300), 1.0);
    const int pen = rightEdge(layer->getBitmapImageAtFrame(1));

    for (SmoothingMode mode : { SmoothingMode::Weighted, SmoothingMode::Stabilizer })
    {
        CAPTURE(static_cast<int>(mode));
        brush->setSmoothingMode(static_cast<int>(mode));
        brush->setSmoothingStrength(100);
        layer->removeKeyFrame(2);
        layer->addNewKeyFrameAt(2);
        editor->scrubTo(2);
        stroke(area, QPointF(100, 300), QPointF(500, 300), 1.0);
        CHECK(rightEdge(layer->getBitmapImageAtFrame(2)) == pen);
    }
}

TEST_CASE("DabTool - dabs are evenly spaced at any input speed and take pressure per dab")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(2);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);
    brush->setFeatherEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    const bool profiling = StageProfiler::enabled();
    StageProfiler::enabled() = true;
    // Counts the dabs of one straight 400 px stroke sent as `events` move events.
    auto dabsFor = [&](int events) {
        const QPointF from(100, 300), to(500, 300);
        StageProfiler::reset();
        tablet(area, QEvent::TabletPress, from, 1.0);
        for (int i = 1; i <= events; ++i)
            tablet(area, QEvent::TabletMove, from + (to - from) * i / events, 1.0);
        tablet(area, QEvent::TabletRelease, to, 1.0);
        return static_cast<double>(StageProfiler::calls(StageProfiler::DabMask));
    };
    brush->setPressureEnabled(false);
    editor->scrubTo(1);
    const double slow = dabsFor(400);
    const double fast = dabsFor(4);
    StageProfiler::enabled() = profiling;
    INFO("slow " << slow << " fast " << fast);
    CHECK(fast > 0);
    CHECK(slow == Approx(fast).epsilon(0.05));

    // A fast ramp from light to hard: the light end stays thin.
    brush->setPressureEnabled(true);
    editor->scrubTo(2);
    tablet(area, QEvent::TabletPress, QPointF(100, 300), 0.1);
    tablet(area, QEvent::TabletMove, QPointF(500, 300), 1.0);
    tablet(area, QEvent::TabletRelease, QPointF(500, 300), 1.0);
    const QImage* img = layer->getBitmapImageAtFrame(2)->image();
    int left = img->width();
    for (int y = 0; y < img->height(); ++y)
        for (int x = 0; x < left; ++x)
            if (qAlpha(img->pixel(x, y)) != 0)
                left = x;
    int lightEnd = 0;
    for (int y = 0; y < img->height(); ++y)
        lightEnd += qAlpha(img->pixel(left + 20, y)) != 0;
    INFO("ink height 20 px from the light end " << lightEnd);
    CHECK(lightEnd > 0);
    CHECK(lightEnd < 15);
}

TEST_CASE("DabTool - Spacing percent sets the dab density and defaults to 12.5")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));
    editor->scrubTo(1);

    const bool profiling = StageProfiler::enabled();
    StageProfiler::enabled() = true;
    auto dabs = [&]() {
        StageProfiler::reset();
        tablet(area, QEvent::TabletPress, QPointF(100, 300), 1.0);
        tablet(area, QEvent::TabletMove, QPointF(500, 300), 1.0);
        tablet(area, QEvent::TabletRelease, QPointF(500, 300), 1.0);
        return static_cast<double>(StageProfiler::calls(StageProfiler::DabMask));
    };
    const double normal = dabs();
    brush->setSpacing(25.0);
    const double sparse = dabs();
    StageProfiler::enabled() = profiling;
    INFO("default " << normal << " 25% " << sparse);
    CHECK(normal == Approx(161).epsilon(0.05));
    CHECK(sparse == Approx(normal / 2).epsilon(0.05));
}

TEST_CASE("DabTool - a flat tip keeps its angle or follows the stroke direction")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 2; f <= 4; ++f)
        layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(40);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));
    CHECK(brush->strokeToolProperties().tipAspect() == 100); // the round tip
    brush->setTip(StrokeToolProperties::TIP_ASPECT_VALUE, 20); // 40 px wide, 8 px tall

    // A vertical stroke with the tip at 0 degrees: the ink is as wide as the tip.
    editor->scrubTo(1);
    stroke(area, QPointF(300, 100), QPointF(300, 400), 1.0);
    const int fixedWidth = inkWidth(layer->getBitmapImageAtFrame(1));

    brush->setTip(StrokeToolProperties::TIP_ANGLE_VALUE, 90);
    editor->scrubTo(2);
    stroke(area, QPointF(300, 100), QPointF(300, 400), 1.0);
    const int turnedWidth = inkWidth(layer->getBitmapImageAtFrame(2));

    // Follow direction: the tip width lies along the stroke, so the ink is as wide as the tip is tall.
    brush->setTip(StrokeToolProperties::TIP_ANGLE_VALUE, 0);
    brush->setTip(StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED, true);
    editor->scrubTo(3);
    stroke(area, QPointF(300, 100), QPointF(300, 400), 1.0);
    const int followWidth = inkWidth(layer->getBitmapImageAtFrame(3));

    brush->setTip(StrokeToolProperties::TIP_SQUARE_ENABLED, true);
    brush->setTip(StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED, false);
    brush->setTip(StrokeToolProperties::TIP_ASPECT_VALUE, 100);
    editor->scrubTo(4);
    tap(area, QPointF(300, 300), 1.0);
    const QImage* img = layer->getBitmapImageAtFrame(4)->image();
    int painted = 0;
    for (int y = 0; y < img->height(); ++y)
        for (int x = 0; x < img->width(); ++x)
            painted += qAlpha(img->pixel(x, y)) != 0;

    INFO("fixed " << fixedWidth << " turned " << turnedWidth << " follow " << followWidth << " square " << painted);
    CHECK(fixedWidth >= 39);
    CHECK(turnedWidth <= 10);
    CHECK(followWidth <= 10);
    CHECK(painted >= 39 * 39); // a 40 px circle covers about 1257 pixels
}

TEST_CASE("StrokeTool - setTip sets each tip value and signals one change")
{
    TestSession s;
    s.start();
    s.editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(s.editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);

    int changes = 0;
    QObject::connect(brush, &StrokeTool::tipChanged, [&] { ++changes; });
    brush->setTip(StrokeToolProperties::TIP_ASPECT_VALUE, 40);
    brush->setTip(StrokeToolProperties::TIP_ANGLE_VALUE, -30);
    brush->setTip(StrokeToolProperties::TIP_SQUARE_ENABLED, 1);
    brush->setTip(StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED, 1);

    const StrokeToolProperties& p = brush->strokeToolProperties();
    CHECK(p.tipAspect() == 40);
    CHECK(p.tipAngle() == -30);
    CHECK(p.tipSquare());
    CHECK(p.tipFollowsDirection());
    CHECK(changes == 4);
    brush->setTip(StrokeToolProperties::TIP_SQUARE_ENABLED, 0);
    CHECK_FALSE(brush->strokeToolProperties().tipSquare());
}

TEST_CASE("DabTool - an image tip from a file paints its shape and a bad file keeps the old tip")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // 20 x 20 with the left half dark: a 40 px dab paints about 20 px wide.
    QImage png(20, 20, QImage::Format_RGB32);
    png.fill(Qt::white);
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 10; ++x)
            png.setPixel(x, y, qRgb(0, 0, 0));
    REQUIRE(png.save(dir.filePath("tip.png")));
    QFile bad(dir.filePath("bad.gbr"));
    REQUIRE(bad.open(QIODevice::WriteOnly));
    bad.write(QByteArray("\0\0\0\x1c\0\0\0\x02", 8)); // a .gbr cut off in its header
    bad.close();

    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 2; f <= 3; ++f)
        layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(40);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(false);

    editor->scrubTo(1);
    tap(area, QPointF(300, 300), 1.0);
    const int roundWidth = inkWidth(layer->getBitmapImageAtFrame(1));

    REQUIRE(brush->setTipImage(dir.filePath("tip.png")).ok());
    editor->scrubTo(2);
    tap(area, QPointF(300, 300), 1.0);
    const int imageWidth = inkWidth(layer->getBitmapImageAtFrame(2));

    CHECK_FALSE(brush->setTipImage(dir.filePath("bad.gbr")).ok());
    editor->scrubTo(3);
    tap(area, QPointF(300, 300), 1.0);
    const int afterBadWidth = inkWidth(layer->getBitmapImageAtFrame(3));
    brush->setTipImage(QString());

    INFO("round " << roundWidth << " image " << imageWidth << " after bad file " << afterBadWidth);
    CHECK(roundWidth >= 39);
    CHECK(imageWidth >= 19);
    CHECK(imageWidth <= 23);
    CHECK(afterBadWidth == imageWidth);
}

TEST_CASE("Pencil, Brush and Eraser - a tap paints a dab and a vector stroke edits curves")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    LayerVector* vector = s.object->addNewVectorLayer();
    bitmap->getBitmapImageAtFrame(1)->drawRect(QRectF(-50, -50, 100, 100), Qt::NoPen, QColor(0, 0, 0),
                                                QPainter::CompositionMode_SourceOver, false);
    VectorImage* vimage = vector->getVectorImageAtFrame(1);
    BezierCurve curve({ QPointF(0, 0), QPointF(5, 0) });
    curve.setColorNumber(1);
    vimage->addCurve(curve, 1.0);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->scrubTo(1);
    const QPointF origin = editor->view()->mapCanvasToScreen(QPointF(0, 0));

    for (ToolType tool : { PENCIL, BRUSH, ERASER })
    {
        CAPTURE(tool);
        editor->tools()->setCurrentTool(tool);
        const ToolSettingsGuard guard(editor->tools()->currentTool());
        static_cast<StrokeTool*>(editor->tools()->currentTool())->setWidth(20);

        editor->layers()->setCurrentLayer(bitmap);
        const QImage before = *bitmap->getBitmapImageAtFrame(1)->image();
        tap(area, origin + QPointF(30, 30), 1.0);
        CHECK(*bitmap->getBitmapImageAtFrame(1)->image() != before);

        editor->layers()->setCurrentLayer(vector);
        const int curves = vector->getVectorImageAtFrame(1)->getLastCurveNumber();
        stroke(area, origin + QPointF(-10, 0), origin + QPointF(10, 0), 1.0);
        if (tool == ERASER)
            CHECK(vector->getVectorImageAtFrame(1)->getLastCurveNumber() < curves);
        else
            CHECK(vector->getVectorImageAtFrame(1)->getLastCurveNumber() == curves + 1);
    }
}

TEST_CASE("Pencil - a stroke on a translated key lands where it was drawn")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    LayerVector* vector = s.object->addNewVectorLayer();
    TweenState moved;
    moved.tx = 40;
    bitmap->getKeyFrameAt(1)->setTweenState(moved);
    vector->getKeyFrameAt(1)->setTweenState(moved);

    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->scrubTo(1);
    editor->tools()->setCurrentTool(PENCIL);
    const ToolSettingsGuard guard(editor->tools()->currentTool());
    static_cast<StrokeTool*>(editor->tools()->currentTool())->setWidth(6);
    const QPointF origin = editor->view()->mapCanvasToScreen(QPointF(0, 0));

    editor->layers()->setCurrentLayer(bitmap);
    stroke(area, origin + QPointF(-10, 0), origin + QPointF(10, 0), 1.0);
    editor->layers()->setCurrentLayer(vector);
    stroke(area, origin + QPointF(-10, 0), origin + QPointF(10, 0), 1.0);

    // Canvas (0, 0) is image pixel (100, 50).
    vector->setVisible(false);
    QImage image(200, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setWorldTransform(QTransform::fromTranslate(100, 50));
    s.object->paintImage(painter, 1, false, true);
    painter.end();
    CHECK(qAlpha(image.pixel(100, 50)) > 0);
    CHECK(qAlpha(image.pixel(140, 50)) == 0);

    // In key space the stroke sits 40 px left of where it was drawn.
    VectorImage* vimage = vector->getVectorImageAtFrame(1);
    const int curve = vimage->getLastCurveNumber();
    REQUIRE(curve >= 0);
    CHECK(vimage->getVertex(curve, -1).x() == Approx(-50).margin(3)); // the curve origin
    CHECK(vimage->getVertex(curve, vimage->getCurveSize(curve) - 1).x() < -30 + 3);
}

TEST_CASE("Tools on a translated key act where the key is drawn")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    LayerVector* vector = s.object->addNewVectorLayer();
    LayerBitmap* lines = s.object->addNewBitmapLayer();
    TweenState moved;
    moved.tx = 40;
    bitmap->getKeyFrameAt(1)->setTweenState(moved);
    vector->getKeyFrameAt(1)->setTweenState(moved);
    BezierCurve curve({ QPointF(0, -10), QPointF(0, 10) });
    vector->getVectorImageAtFrame(1)->addCurve(curve, 1.0, false);

    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->scrubTo(1);
    // Key point (0, 0) is drawn at canvas (40, 0).
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };
    auto alphaAt = [&](int x, int y) { return qAlpha(bitmap->getBitmapImageAtFrame(1)->pixel(QPoint(x, y))); };

    SECTION("Vector eraser")
    {
        editor->layers()->setCurrentLayer(vector);
        editor->tools()->setCurrentTool(ERASER);
        const ToolSettingsGuard guard(editor->tools()->currentTool());
        static_cast<StrokeTool*>(editor->tools()->currentTool())->setWidth(10);
        stroke(area, screen({ 40, -12 }), screen({ 40, 12 }), 1.0);
        CHECK(vector->getVectorImageAtFrame(1)->getCurveSize(0) == -1); // no curve left
    }
    SECTION("Vector smudge")
    {
        editor->layers()->setCurrentLayer(vector);
        editor->tools()->setCurrentTool(SMUDGE);
        stroke(area, screen({ 40, -10 }), screen({ 50, -10 }), 0.5);
        CHECK(vector->getVectorImageAtFrame(1)->getVertex(0, -1).x() == Approx(10).margin(2));
    }
    SECTION("Bitmap smudge")
    {
        bitmap->getBitmapImageAtFrame(1)->drawRect(QRectF(-20, -20, 20, 40), Qt::NoPen, QBrush(Qt::red),
                                                   QPainter::CompositionMode_Source, false);
        editor->layers()->setCurrentLayer(bitmap);
        editor->tools()->setCurrentTool(SMUDGE);
        StrokeTool* smudge = static_cast<StrokeTool*>(editor->tools()->currentTool());
        const ToolSettingsGuard guard(smudge);
        smudge->setWidth(20);
        smudge->setFeather(48);
        smudge->setSmoothingMode(static_cast<int>(SmoothingMode::None));
        smudge->setDynamic(StrokeToolProperties::SMUDGE_LENGTH_VALUE, 90);
        stroke(area, screen({ 30, 0 }), screen({ 70, 0 }), 1.0);
        CHECK(alphaAt(20, 0) > 0); // red carried right in key space
    }
    SECTION("Bitmap bucket")
    {
        bitmap->getBitmapImageAtFrame(1)->drawRect(QRectF(-10, -10, 20, 20), QPen(Qt::black, 2), Qt::NoBrush,
                                                   QPainter::CompositionMode_SourceOver, false);
        editor->layers()->setCurrentLayer(bitmap);
        editor->tools()->setCurrentTool(BUCKET);
        tap(area, screen({ 40, 0 }), 0.5);
        CHECK(alphaAt(0, 0) == 255);
    }
    SECTION("Bitmap bucket with all layers as reference")
    {
        lines->getBitmapImageAtFrame(1)->drawRect(QRectF(30, -10, 20, 20), QPen(Qt::black, 2), Qt::NoBrush,
                                                  QPainter::CompositionMode_SourceOver, false);
        // A loaded target: the fill skips an empty key.
        bitmap->getBitmapImageAtFrame(1)->drawRect(QRectF(60, 60, 2, 2), Qt::NoPen, QBrush(Qt::red),
                                                   QPainter::CompositionMode_Source, false);
        editor->layers()->setCurrentLayer(bitmap);
        editor->tools()->setCurrentTool(BUCKET);
        BucketTool* bucket = static_cast<BucketTool*>(editor->tools()->currentTool());
        const ToolSettingsGuard guard(bucket);
        bucket->setFillReferenceMode(1);
        tap(area, screen({ 40, 0 }), 0.5);
        CHECK(alphaAt(0, 0) == 255);
        CHECK(alphaAt(15, 0) == 0); // outside the lines
    }
    SECTION("Select and move")
    {
        editor->layers()->setCurrentLayer(bitmap);
        editor->tools()->setCurrentTool(SELECT);
        stroke(area, screen({ 10, -30 }), screen({ 70, 30 }), 1.0);
        auto selectionCenter = [&] {
            auto select = editor->select();
            return select->mapToSelection(QPolygonF(select->mySelectionRect())).boundingRect().center();
        };
        CHECK(selectionCenter().x() == Approx(0).margin(1));
        editor->tools()->setCurrentTool(MOVE);
        // A hover picks the move mode.
        QTabletEvent hover(QEvent::TabletMove, QPointingDevice::primaryPointingDevice(), screen({ 40, 0 }),
                           area->mapToGlobal(screen({ 40, 0 })), 0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::NoButton, Qt::NoButton);
        QApplication::sendEvent(area, &hover);
        stroke(area, screen({ 40, 0 }), screen({ 50, 0 }), 1.0);
        CHECK(selectionCenter().x() == Approx(10).margin(1));
    }
}

TEST_CASE("Pen - a stroke after an arrow-key nudge applies the nudge first")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    editor->tools()->setCurrentTool(PEN);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());

    layer->getBitmapImageAtFrame(1)->setPixel(10, 20, qRgba(0, 0, 0, 255));
    editor->select()->setSelection(QRectF(0, 0, 40, 40), true);
    auto pixelAt = [&](int x) { return qAlpha(layer->getBitmapImageAtFrame(1)->pixel(x, 20)) != 0; };
    auto nudgeRight = [&]
    {
        QKeyEvent e(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(area, &e);
    };
    auto screen = [&](QPointF canvas) { return editor->view()->mapCanvasToScreen(canvas); };

    SECTION("Undo restores the position before the nudge")
    {
        nudgeRight();
        REQUIRE_FALSE(editor->select()->selectionTransform().isIdentity());
        undoAction->trigger();
        CHECK(editor->select()->selectionTransform().isIdentity());
        CHECK(pixelAt(10));
        CHECK_FALSE(pixelAt(11));
    }

    SECTION("A stroke paints with an identity selection transform")
    {
        nudgeRight();
        tablet(area, QEvent::TabletPress, screen(QPointF(200, 200)), 1.0);
        tablet(area, QEvent::TabletMove, screen(QPointF(250, 200)), 1.0);
        area->grab(); // paints while drawing: asserts on a pending selection transform
        tablet(area, QEvent::TabletRelease, screen(QPointF(250, 200)), 1.0);

        CHECK(editor->select()->selectionTransform().isIdentity());
        CHECK_FALSE(pixelAt(10));
        CHECK(pixelAt(11));
    }

    SECTION("Undo and redo of a stroke after a nudge restore the right pixels")
    {
        QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
        nudgeRight();
        if (GENERATE(false, true))
        {
            // The nudge comes back from a redo.
            undoAction->trigger();
            redoAction->trigger();
        }
        auto strokeAt = [&] { return qAlpha(layer->getBitmapImageAtFrame(1)->pixel(225, 200)) != 0; };
        tablet(area, QEvent::TabletPress, screen(QPointF(200, 200)), 1.0);
        tablet(area, QEvent::TabletMove, screen(QPointF(250, 200)), 1.0);
        tablet(area, QEvent::TabletRelease, screen(QPointF(250, 200)), 1.0);
        REQUIRE(strokeAt());

        undoAction->trigger();
        CHECK_FALSE(strokeAt());
        undoAction->trigger();
        CHECK(pixelAt(10));
        CHECK_FALSE(pixelAt(11));
        redoAction->trigger();
        redoAction->trigger();
        CHECK(strokeAt());
        CHECK_FALSE(pixelAt(10));
        CHECK(pixelAt(11));
    }

    SECTION("Enter applies the nudge")
    {
        nudgeRight();
        QKeyEvent e(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(area, &e);
        CHECK_FALSE(editor->select()->somethingSelected());
        CHECK_FALSE(pixelAt(10));
        CHECK(pixelAt(11));
    }
}

TEST_CASE("Vector tools - undo/redo of a moved vertex and of a fill")
{
    TestSession session;
    Object* obj = session.object;
    LayerVector* layer = obj->addNewVectorLayer();

    session.start();
    Editor* editor = session.editor.get();
    ScribbleArea* area = session.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);

    BezierCurve curve({ QPointF(50, 50), QPointF(100, 100) });
    curve.setColorNumber(1);
    layer->getVectorImageAtFrame(1)->addCurve(curve, 1.0);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    // Undo and redo replace the keyframe: get the image again each time.
    auto image = [&] { return layer->getVectorImageAtFrame(1); };
    const QPointF curveStart = editor->view()->mapCanvasToScreen(QPointF(50, 50));

    SECTION("Smudge tool moves a vertex")
    {
        editor->tools()->setCurrentTool(SMUDGE);
        auto moved = [&] { return image()->getVertex(0, -1).x() > 70; };

        stroke(area, curveStart, curveStart + QPointF(40, 0), 0.5);
        REQUIRE(moved());
        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE_FALSE(moved());
        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(moved());
    }

    SECTION("Bucket tool colors the selected curve")
    {
        editor->tools()->setCurrentTool(BUCKET);
        editor->color()->setColorNumber(2);
        image()->setSelected(0, true);
        auto colorNumber = [&] { return image()->curve(0).getColorNumber(); };

        tap(area, curveStart, 0.5);
        REQUIRE(colorNumber() == 2);
        REQUIRE(undoAction->isEnabled());
        undoAction->trigger();
        REQUIRE(colorNumber() == 1);
        REQUIRE(redoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(colorNumber() == 2);
    }
}

TEST_CASE("PressureCurve - monotone cubic through its control points")
{
    const PressureCurve identity;
    for (qreal x : { 0.0, 0.25, 0.5, 1.0 })
        CHECK(identity.value(x) == Approx(x));

    const PressureCurve soft = PressureCurve::fromString("0,0 0.5,0.2 0.8,0.9 1,1");
    CHECK(soft.value(0.5) == Approx(0.2));
    CHECK(soft.value(0.8) == Approx(0.9));
    CHECK(soft.value(-1) == Approx(0));
    CHECK(soft.value(2) == Approx(1));
    qreal last = 0;
    for (int i = 0; i <= 100; ++i)
    {
        const qreal y = soft.value(i / 100.0);
        CHECK(y >= last);
        CHECK(y <= 1.0);
        last = y;
    }
    CHECK(PressureCurve::fromString(soft.toString()).points() == soft.points());

    // Bad strings give the identity curve.
    for (const char* bad : { "", "0,0", "0,0 x,1", "0,0 0.5,1 0.4,1 1,1", "0,0 2,1" })
    {
        CAPTURE(bad);
        CHECK(PressureCurve::fromString(bad).points() == identity.points());
    }
}

TEST_CASE("DabTool - the pressure curve maps pen pressure to dab size and opacity")
{
    const SettingsGuard settingsGuard({ SETTING_PRESSURE_CURVE });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(2);
    layer->addNewKeyFrameAt(3);
    layer->addNewKeyFrameAt(4);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    OnScopeExit(brush->setPressureCurve(QString()));
    brush->setWidth(20);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(true);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    // Ink height and peak alpha at x = 300 of a light stroke.
    auto lightStroke = [&](int frame) {
        editor->scrubTo(frame);
        tablet(area, QEvent::TabletPress, QPointF(100, 300), 0.2);
        tablet(area, QEvent::TabletMove, QPointF(500, 300), 0.2);
        tablet(area, QEvent::TabletRelease, QPointF(500, 300), 0.2);
        BitmapImage* bitmap = layer->getBitmapImageAtFrame(frame);
        const int x = editor->view()->mapScreenToCanvas(QPointF(300, 300)).toPoint().x() - bitmap->left();
        const QImage* img = bitmap->image();
        int height = 0, alpha = 0;
        for (int y = 0; y < img->height(); ++y)
        {
            height += qAlpha(img->pixel(x, y)) != 0;
            alpha = qMax(alpha, qAlpha(img->pixel(x, y)));
        }
        return std::make_pair(height, alpha);
    };
    editor->preference()->set(SETTING::PRESSURE_CURVE, QString());
    const auto linear = lightStroke(1);
    editor->preference()->set(SETTING::PRESSURE_CURVE, QString("0,0.9 1,1"));
    const auto firm = lightStroke(2);
    INFO("linear " << linear.first << "px a" << linear.second << " firm " << firm.first << "px a" << firm.second);
    CHECK(linear.first < 8);
    CHECK(firm.first > 15);
    CHECK(firm.second > linear.second);

    // A tool curve (from a preset) overrides the global one; empty gives the global curve back.
    editor->preference()->set(SETTING::PRESSURE_CURVE, QString());
    brush->setPressureCurve("0,0.9 1,1");
    CHECK(lightStroke(3) == firm);
    brush->setPressureCurve(QString());
    CHECK(lightStroke(4) == linear);
}

TEST_CASE("DabTool - a mouse stroke tapers at both ends")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(2);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    auto mouse = [&](QEvent::Type type, QPointF pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, area->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QApplication::sendEvent(area, &e);
    };
    // Ink height at screen x of a mouse stroke from x = 100 to 500.
    auto strokeHeights = [&](int frame) {
        editor->scrubTo(frame);
        mouse(QEvent::MouseButtonPress, QPointF(100, 300), Qt::LeftButton);
        for (int i = 1; i <= 40; ++i)
            mouse(QEvent::MouseMove, QPointF(100 + i * 10, 300), Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, QPointF(500, 300), Qt::NoButton);
        BitmapImage* bitmap = layer->getBitmapImageAtFrame(frame);
        const QImage* img = bitmap->image();
        std::vector<int> heights;
        for (qreal sx : { 110.0, 300.0, 490.0 })
        {
            const int x = editor->view()->mapScreenToCanvas(QPointF(sx, 300)).toPoint().x() - bitmap->left();
            int height = 0;
            for (int y = 0; y < img->height(); ++y)
                height += x >= 0 && x < img->width() && qAlpha(img->pixel(x, y)) != 0;
            heights.push_back(height);
        }
        return heights;
    };

    brush->setDynamic(StrokeToolProperties::TAPER_START_VALUE, 100);
    brush->setDynamic(StrokeToolProperties::TAPER_END_VALUE, 100);
    const auto tapered = strokeHeights(1);
    brush->setDynamic(StrokeToolProperties::TAPER_START_VALUE, 0);
    brush->setDynamic(StrokeToolProperties::TAPER_END_VALUE, 0);
    const auto plain = strokeHeights(2);
    INFO("tapered " << tapered[0] << "/" << tapered[1] << "/" << tapered[2]
         << " plain " << plain[0] << "/" << plain[1] << "/" << plain[2]);
    CHECK(tapered[1] >= 18);
    CHECK(tapered[0] <= 6);
    CHECK(tapered[2] <= 6);
    CHECK(plain[0] >= 18);
    CHECK(plain[2] >= 18);
}

TEST_CASE("DabTool - Classic with pressure off paints full alpha like before v0.3")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    s.start();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    s.editor->layers()->setCurrentLayer(layer);

    s.editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(s.editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    QTemporaryDir dir;
    BrushPresets(dir.path()).addDefaults();
    REQUIRE(BrushPresets(dir.path()).apply("Classic", *brush));
    brush->setWidth(20);
    brush->setFeather(1);
    brush->setPressureEnabled(false);

    tap(area, QPointF(400, 300), 0.3);
    const QImage* img = layer->getBitmapImageAtFrame(1)->image();
    int alpha = 0;
    for (int y = 0; y < img->height(); ++y)
        for (int x = 0; x < img->width(); ++x)
            alpha = qMax(alpha, qAlpha(img->pixel(x, y)));
    CHECK(alpha >= 250); // opacity 1 before v0.3; feather 1 takes 1%
}

TEST_CASE("DabTool - dabs held back for the end taper keep their own pen tilt")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(2);
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));
    brush->setDynamic(StrokeToolProperties::TILT_TO_SIZE_VALUE, 100);
    brush->setDynamic(StrokeToolProperties::TAPER_END_VALUE, 100);

    // A tilted stroke from x = 100 to 500; the pen stands up at the end if `uprightEnd`.
    // Returns the ink height at x = 460, which waits for the end taper until release.
    auto strokeAt = [&](int frame, bool uprightEnd) {
        editor->scrubTo(frame);
        auto send = [&](QEvent::Type type, qreal x, qreal tilt) {
            const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
            const QPointF pos(x, 300);
            QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                           1.0, tilt, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
            QApplication::sendEvent(area, &e);
        };
        const qreal end = uprightEnd ? 0 : 60;
        send(QEvent::TabletPress, 100, 60);
        for (int i = 1; i < 40; ++i)
            send(QEvent::TabletMove, 100 + 10 * i, 60);
        send(QEvent::TabletMove, 500, end);
        send(QEvent::TabletRelease, 500, end);
        BitmapImage* bitmap = layer->getBitmapImageAtFrame(frame);
        const int x = editor->view()->mapScreenToCanvas(QPointF(460, 300)).toPoint().x() - bitmap->left();
        const QImage* img = bitmap->image();
        int height = 0;
        for (int y = 0; y < img->height(); ++y)
            height += qAlpha(img->pixel(x, y)) != 0;
        return height;
    };
    const int tilted = strokeAt(1, false);
    const int uprightEnd = strokeAt(2, true);
    INFO("tilted " << tilted << "px upright end " << uprightEnd << "px");
    CHECK(tilted > 12);
    CHECK(uprightEnd == Approx(tilted).margin(1));
}

TEST_CASE("DabTool - flow builds up and opacity caps the stroke")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 1; f <= 4; ++f) layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);
    brush->setFeather(1); // Brush dabs always fade over the feather.
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    // Peak alpha of a stroke that runs right, then back over itself.
    auto peakAlpha = [&](int frame) {
        editor->scrubTo(frame);
        tablet(area, QEvent::TabletPress, QPointF(100, 300), 1.0);
        tablet(area, QEvent::TabletMove, QPointF(500, 300), 1.0);
        tablet(area, QEvent::TabletMove, QPointF(100, 302), 1.0);
        tablet(area, QEvent::TabletRelease, QPointF(100, 302), 1.0);
        const QImage* img = layer->getBitmapImageAtFrame(frame)->image();
        int alpha = 0;
        for (int y = 0; y < img->height(); ++y)
            for (int x = 0; x < img->width(); ++x)
                alpha = qMax(alpha, qAlpha(img->pixel(x, y)));
        return alpha;
    };

    brush->setStrokeOpacity(50);
    brush->setWashEnabled(false);
    const int buildUp = peakAlpha(1);
    brush->setWashEnabled(true);
    const int wash = peakAlpha(2);
    brush->setFlow(30);
    const int washLowFlow = peakAlpha(3);
    INFO("build-up " << buildUp << " wash " << wash << " wash at 30% flow " << washLowFlow);
    CHECK(buildUp > 160); // 1 - 0.5 * 0.5 where the passes overlap
    CHECK(wash == Approx(127).margin(3));
    CHECK(washLowFlow <= wash);
    CHECK(washLowFlow > 40);
}

TEST_CASE("DabTool - spacing does not change how dark a low-flow stroke is")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(2);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);
    brush->setFeather(1); // Brush dabs always fade over the feather.
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));
    brush->setFlow(30);

    // Alpha on the stroke's centre line at x = 300.
    auto centreAlpha = [&](int frame) {
        editor->scrubTo(frame);
        tablet(area, QEvent::TabletPress, QPointF(100, 300), 1.0);
        tablet(area, QEvent::TabletMove, QPointF(500, 300), 1.0);
        tablet(area, QEvent::TabletRelease, QPointF(500, 300), 1.0);
        BitmapImage* bitmap = layer->getBitmapImageAtFrame(frame);
        const QPoint p = editor->view()->mapScreenToCanvas(QPointF(300, 300)).toPoint() - bitmap->topLeft();
        return qAlpha(bitmap->image()->pixel(p));
    };

    brush->setSpacing(12.5);
    const int dense = centreAlpha(1);
    brush->setSpacing(50);
    const int sparse = centreAlpha(2);
    INFO("12.5% " << dense << " 50% " << sparse);
    CHECK(dense == Approx(76).margin(8));
    CHECK(sparse == Approx(dense).margin(8));
}

TEST_CASE("DabTool - speed and tilt change dab size and angle and seeded jitter replays the same")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 2; f <= 6; ++f)
        layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(20);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    // A stroke from x = 100 to 500 in `events` moves 8 ms apart; returns the ink height at x = 300.
    auto strokeAt = [&](int frame, int events, QPointF tilt) {
        editor->scrubTo(frame);
        auto send = [&](QEvent::Type type, qreal x, quint64 t) {
            const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
            const QPointF pos(x, 300);
            QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                           1.0, tilt.x(), tilt.y(), 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
            e.setTimestamp(t);
            QApplication::sendEvent(area, &e);
        };
        send(QEvent::TabletPress, 100, 1000);
        for (int i = 1; i <= events; ++i)
            send(QEvent::TabletMove, 100 + 400.0 * i / events, 1000 + 8 * i);
        send(QEvent::TabletRelease, 500, 1000 + 8 * events);
        BitmapImage* bitmap = layer->getBitmapImageAtFrame(frame);
        const int x = editor->view()->mapScreenToCanvas(QPointF(300, 300)).toPoint().x() - bitmap->left();
        const QImage* img = bitmap->image();
        int height = 0;
        for (int y = 0; y < img->height(); ++y)
            height += qAlpha(img->pixel(x, y)) != 0;
        return height;
    };

    SECTION("Speed to size: a fast stroke is thinner than a slow one")
    {
        brush->setDynamic(StrokeToolProperties::SPEED_TO_SIZE_VALUE, -100);
        const int slow = strokeAt(1, 100, QPointF()); // 0.5 px/ms
        const int fast = strokeAt(2, 5, QPointF());   // 10 px/ms
        INFO("slow " << slow << "px fast " << fast << "px");
        CHECK(slow > 15);
        CHECK(fast < slow / 2);
    }
    SECTION("Tilt to size: a tilted pen paints a wider stroke")
    {
        brush->setDynamic(StrokeToolProperties::TILT_TO_SIZE_VALUE, 100);
        const int upright = strokeAt(1, 20, QPointF());
        const int tilted = strokeAt(2, 20, QPointF(60, 0));
        INFO("upright " << upright << "px tilted " << tilted << "px");
        CHECK(upright < 23);
        CHECK(tilted > 35);
    }
    SECTION("Tilt to angle: a flat tip turns with the pen tilt")
    {
        brush->setTip(StrokeToolProperties::TIP_ASPECT_VALUE, 10); // 20 px wide, 2 px tall
        brush->setDynamic(StrokeToolProperties::TILT_TO_ANGLE_VALUE, 100);
        const int upright = strokeAt(1, 20, QPointF());
        const int tilted = strokeAt(2, 20, QPointF(0, -60)); // tip turns to 90 degrees
        INFO("upright " << upright << "px tilted " << tilted << "px");
        CHECK(upright < 6);
        CHECK(tilted > 15);
    }
    SECTION("Size jitter: the same seed paints the same stroke")
    {
        strokeAt(1, 20, QPointF());
        brush->setDynamic(StrokeToolProperties::SIZE_JITTER_VALUE, 100);
        brush->seedRandom(7);
        strokeAt(2, 20, QPointF());
        brush->seedRandom(7);
        strokeAt(3, 20, QPointF());
        brush->seedRandom(8);
        strokeAt(4, 20, QPointF());
        const QImage plain = *layer->getBitmapImageAtFrame(1)->image();
        const QImage a = *layer->getBitmapImageAtFrame(2)->image();
        const QImage b = *layer->getBitmapImageAtFrame(3)->image();
        const QImage other = *layer->getBitmapImageAtFrame(4)->image();
        CHECK(a == b);
        CHECK(a != plain);
        CHECK(a != other);
    }
    SECTION("Scatter: position, angle and dab count scatter dabs and a seed replays them")
    {
        const int plain = strokeAt(1, 20, QPointF());
        brush->setDynamic(StrokeToolProperties::SCATTER_VALUE, 100); // up to one dab width off the line
        brush->seedRandom(7);
        const int scattered = strokeAt(2, 20, QPointF());
        brush->seedRandom(7);
        strokeAt(3, 20, QPointF());
        CHECK(*layer->getBitmapImageAtFrame(2)->image() == *layer->getBitmapImageAtFrame(3)->image());
        INFO("plain " << plain << "px scattered " << scattered << "px");
        CHECK(plain < 23);
        CHECK(scattered > 30);

        // More dabs per step cover more of the scatter band.
        brush->setDynamic(StrokeToolProperties::DAB_COUNT_VALUE, 8);
        brush->seedRandom(7);
        strokeAt(4, 20, QPointF());
        auto inked = [&](int frame) {
            const QImage* img = layer->getBitmapImageAtFrame(frame)->image();
            int n = 0;
            for (int y = 0; y < img->height(); ++y)
                for (int x = 0; x < img->width(); ++x)
                    n += qAlpha(img->pixel(x, y)) != 0;
            return n;
        };
        INFO("one dab " << inked(2) << " eight dabs " << inked(4));
        CHECK(inked(4) > inked(2) * 1.15);

        // Angle jitter turns a flat tip off the line.
        brush->setDynamic(StrokeToolProperties::SCATTER_VALUE, 0);
        brush->setDynamic(StrokeToolProperties::DAB_COUNT_VALUE, 1);
        brush->setTip(StrokeToolProperties::TIP_ASPECT_VALUE, 10);
        const int flat = strokeAt(5, 20, QPointF());
        brush->setDynamic(StrokeToolProperties::ANGLE_JITTER_VALUE, 180);
        const int turned = strokeAt(6, 20, QPointF());
        INFO("flat " << flat << "px turned " << turned << "px");
        CHECK(flat < 6);
        CHECK(turned > 8);
    }
}

TEST_CASE("DabTool - grain is fixed to the paper or moves with each dab and replays the same")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 1; f <= 9; ++f) layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(40);
    brush->setFeather(1); // solid to 99% of the radius
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    const QPoint p = editor->view()->mapScreenToCanvas(QPointF(300, 300)).toPoint();
    const QPoint shift(3, 5);
    auto tapAt = [&](int frame, QPoint offset) {
        editor->scrubTo(frame);
        tap(area, editor->view()->mapCanvasToScreen(QPointF(p + offset)), 1.0);
    };
    auto alphaAt = [&](int frame, QPoint canvas) {
        BitmapImage* bitmap = layer->getBitmapImageAtFrame(frame);
        return qAlpha(bitmap->image()->pixel(canvas - bitmap->topLeft()));
    };
    // Alpha values on a 10x10 block inside both dabs, from canvas point p + offset.
    auto block = [&](int frame, QPoint offset) {
        std::vector<int> v;
        for (int y = -5; y < 5; ++y)
            for (int x = -5; x < 5; ++x)
                v.push_back(alphaAt(frame, p + offset + QPoint(x, y)));
        return v;
    };
    auto distinct = [](std::vector<int> v) {
        std::sort(v.begin(), v.end());
        return int(std::unique(v.begin(), v.end()) - v.begin());
    };

    tapAt(1, QPoint());
    CHECK(distinct(block(1, QPoint())) == 1);
    const int plain = alphaAt(1, p);

    brush->setDynamic(StrokeToolProperties::GRAIN_MODE_VALUE, 1);
    tapAt(2, QPoint());
    tapAt(3, shift);
    INFO("paper grain levels " << distinct(block(2, QPoint())));
    CHECK(distinct(block(2, QPoint())) > 10);
    CHECK(block(2, QPoint()) == block(3, QPoint())); // fixed to the canvas

    brush->setDynamic(StrokeToolProperties::GRAIN_MODE_VALUE, 2);
    tapAt(4, QPoint());
    tapAt(5, shift);
    CHECK(distinct(block(4, QPoint())) > 10);
    CHECK(block(4, QPoint()) == block(5, shift)); // moves with the dab
    CHECK(block(4, QPoint()) != block(5, QPoint()));

    // Contrast 0 flattens the grain to its mid level; brightness lifts it.
    brush->setDynamic(StrokeToolProperties::GRAIN_CONTRAST_VALUE, 0);
    tapAt(6, QPoint());
    CHECK(distinct(block(6, QPoint())) == 1);
    CHECK(alphaAt(6, p) == Approx(plain / 2.0).margin(2));
    brush->setDynamic(StrokeToolProperties::GRAIN_BRIGHTNESS_VALUE, 100);
    tapAt(7, QPoint());
    CHECK(alphaAt(7, p) == plain);

    // A seeded jittered stroke with grain replays the same.
    brush->setDynamic(StrokeToolProperties::GRAIN_CONTRAST_VALUE, 100);
    brush->setDynamic(StrokeToolProperties::GRAIN_BRIGHTNESS_VALUE, 0);
    brush->setDynamic(StrokeToolProperties::SIZE_JITTER_VALUE, 50);
    for (int frame : { 8, 9 })
    {
        editor->scrubTo(frame);
        brush->seedRandom(7);
        stroke(area, QPointF(100, 300), QPointF(500, 300), 1.0);
    }
    CHECK(*layer->getBitmapImageAtFrame(8)->image() == *layer->getBitmapImageAtFrame(9)->image());
}

TEST_CASE("DabTool - every bundled grain texture loads and shapes the grain")
{
    constexpr int count = TiledBuffer::grainTextureCount;
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 1; f <= count; ++f) layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(40);
    brush->setFeather(1);
    brush->setPressureEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));
    brush->setDynamic(StrokeToolProperties::GRAIN_MODE_VALUE, 1);

    // The property range is the list the options panel shows.
    CHECK(brush->strokeToolProperties().getInfo(StrokeToolProperties::GRAIN_TEXTURE_VALUE).maxInt() == count - 1);

    const QPoint p = editor->view()->mapScreenToCanvas(QPointF(300, 300)).toPoint();
    std::vector<std::vector<int>> blocks;
    for (int i = 0; i < count; ++i)
    {
        CHECK(TiledBuffer::grainTextureLoaded(i));
        CHECK(QByteArray(TiledBuffer::grainTextureName(i)).size() > 0);
        brush->setDynamic(StrokeToolProperties::GRAIN_TEXTURE_VALUE, i);
        CHECK(brush->strokeToolProperties().grainTexture() == i);
        editor->scrubTo(i + 1);
        tap(area, editor->view()->mapCanvasToScreen(QPointF(p)), 1.0);
        BitmapImage* bitmap = layer->getBitmapImageAtFrame(i + 1);
        std::vector<int> v;
        for (int y = -10; y < 10; ++y)
            for (int x = -10; x < 10; ++x)
                v.push_back(qAlpha(bitmap->image()->pixel(p + QPoint(x, y) - bitmap->topLeft())));
        std::vector<int> sorted = v;
        std::sort(sorted.begin(), sorted.end());
        CHECK(std::unique(sorted.begin(), sorted.end()) - sorted.begin() > 10);
        blocks.push_back(v);
    }
    for (int i = 1; i < count; ++i) CHECK(blocks[i] != blocks[0]);
}

TEST_CASE("SmudgeTool - smudge drags and mixes colour through the brush engine")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 1; f <= 4; ++f) layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);

    editor->tools()->setCurrentTool(SMUDGE);
    StrokeTool* smudge = static_cast<StrokeTool*>(editor->tools()->getTool(SMUDGE));
    const ToolSettingsGuard guard(smudge);
    smudge->setWidth(20);
    smudge->setFeather(48); // a hard opaque dab repaints its own last colour at any length
    smudge->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    // Opaque red left of canvas x = 0, transparent right of it.
    auto prepare = [&](int frame) {
        editor->scrubTo(frame);
        layer->getBitmapImageAtFrame(frame)->drawRect(QRectF(-100, -50, 100, 100), Qt::NoPen, QBrush(Qt::red),
                                                      QPainter::CompositionMode_Source, false);
    };
    auto pixel = [&](int frame, QPoint canvas) { return layer->getBitmapImageAtFrame(frame)->pixel(canvas); };
    auto drag = [&](QPointF from, QPointF to) {
        stroke(area, editor->view()->mapCanvasToScreen(from), editor->view()->mapCanvasToScreen(to), 1.0);
    };

    // A long smudge length carries red far into the empty part; length 0 much less.
    smudge->setDynamic(StrokeToolProperties::SMUDGE_LENGTH_VALUE, 90);
    prepare(1);
    drag(QPointF(-30, 0), QPointF(40, 0));
    const QRgb carried = pixel(1, QPoint(20, 0));
    INFO("carried alpha " << qAlpha(carried));
    CHECK(qAlpha(carried) > 60);
    CHECK(qRed(qUnpremultiply(carried)) > 200);
    CHECK(qBlue(carried) == 0);
    REQUIRE(undoAction->isEnabled());
    undoAction->trigger();
    CHECK(qAlpha(pixel(1, QPoint(20, 0))) == 0);
    REQUIRE(redoAction->isEnabled());
    redoAction->trigger();
    CHECK(pixel(1, QPoint(20, 0)) == carried);

    smudge->setDynamic(StrokeToolProperties::SMUDGE_LENGTH_VALUE, 0);
    prepare(2);
    drag(QPointF(-30, 0), QPointF(40, 0));
    CHECK(qAlpha(pixel(2, QPoint(20, 0))) < qAlpha(carried) / 2);

    // Smudge smears transparency into paint too: it lowers alpha, not only adds.
    smudge->setDynamic(StrokeToolProperties::SMUDGE_LENGTH_VALUE, 90);
    prepare(3);
    drag(QPointF(40, 0), QPointF(-40, 0));
    CHECK(qAlpha(pixel(3, QPoint(-20, 0))) < 200);
    CHECK(qAlpha(pixel(3, QPoint(-20, 40))) == 255); // outside the dabs

    // Full colour rate paints the front colour, like a wet brush.
    smudge->setDynamic(StrokeToolProperties::SMUDGE_COLOR_RATE_VALUE, 100);
    editor->color()->setFrontColor(Qt::blue);
    prepare(4);
    drag(QPointF(20, 200), QPointF(80, 200));
    const QRgb painted = pixel(4, QPoint(50, 200));
    CHECK(qAlpha(painted) > 250);
    CHECK(qBlue(painted) == qAlpha(painted));
    CHECK(qRed(painted) + qGreen(painted) == 0);
}

TEST_CASE("StrokeTool - a stroke that starts near the ruler snaps to it")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(4);
    brush->setPressureEnabled(false);
    brush->setFeatherEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    // Canvas rows that hold ink after a stroke on a new key, as [top, bottom].
    auto draw = [&](int frame, QPointF a, QPointF b) {
        layer->addNewKeyFrameAt(frame);
        editor->scrubTo(frame);
        stroke(area, a, b, 1.0);
        BitmapImage* image = layer->getBitmapImageAtFrame(frame);
        const QImage* img = image->image();
        int top = INT_MAX, bottom = INT_MIN;
        for (int y = 0; y < img->height(); ++y)
            for (int x = 0; x < img->width(); ++x)
                if (qAlpha(img->pixel(x, y)) != 0)
                {
                    top = std::min(top, image->top() + y);
                    bottom = std::max(bottom, image->top() + y);
                }
        return std::make_pair(top, bottom);
    };

    // A horizontal ruler on canvas row 0; the pen starts 6 px below it and drifts down.
    editor->overlays()->setRuler(QLineF(-200, 0, 200, 0));
    const QPointF origin = editor->view()->mapCanvasToScreen(QPointF(0, 0));
    const QPointF from = origin + QPointF(-150, 6), to = origin + QPointF(150, 30);

    auto rows = draw(2, from, to);
    CHECK(rows.second - rows.first > 20); // ruler off: the stroke keeps its slope

    editor->overlays()->setRulerEnabled(true);
    rows = draw(3, from, to);
    CHECK(rows.first >= -3);
    CHECK(rows.second <= 3);

    // A stroke that starts far from the ruler is not snapped.
    rows = draw(4, from + QPointF(0, 100), to + QPointF(0, 100));
    CHECK(rows.second - rows.first > 20);
}
TEST_CASE("MoveTool - dragging a ruler end moves only that end")
{
    TestSession s;
    s.object->addNewCameraLayer();
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(MOVE);

    OverlayManager* overlays = editor->overlays();
    overlays->setRuler(QLineF(-200, 0, 200, 0));
    overlays->setRulerEnabled(true);
    const QPointF end = editor->view()->mapCanvasToScreen(QPointF(200, 0));
    tablet(area, QEvent::TabletPress, end, 1.0);
    tablet(area, QEvent::TabletMove, end + QPointF(0, 50), 1.0);
    tablet(area, QEvent::TabletRelease, end + QPointF(0, 50), 1.0);
    overlays->setRulerEnabled(false);

    const qreal scale = editor->view()->scaling();
    CHECK(overlays->ruler().p1() == QPointF(-200, 0));
    CHECK(overlays->ruler().p2().x() == Approx(200).margin(0.5));
    CHECK(overlays->ruler().p2().y() == Approx(50 / scale).margin(0.5));
}

TEST_CASE("StrokeTool - perspective snap locks a stroke to the line toward the nearest vanishing point")
{
    TestSession s;
    LayerCamera* camera = s.object->addNewCameraLayer();
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(4);
    brush->setPressureEnabled(false);
    brush->setFeatherEnabled(false);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    PreferenceManager* prefs = editor->preference();
    const bool twoPointWas = prefs->isOn(SETTING::OVERLAY_PERSPECTIVE2);
    prefs->set(SETTING::OVERLAY_PERSPECTIVE2, true);

    // Vanishing points live in camera space; strokes are drawn on the canvas.
    const QTransform toCanvas = camera->getViewAtFrame(1).inverted();
    const QPointF left = toCanvas.map(editor->overlays()->getLeftPerspectivePoint());
    const QPointF right = toCanvas.map(editor->overlays()->getRightPerspectivePoint());
    const QPointF start = (left + right) / 2 + QPointF(0, 120);
    const QPointF onLine = start + (right - start) / 2; // halfway to the right point
    const QPointF penEnd = onLine + QPointF(0, 25);     // the pen drifts off that line
    const QPointF snapped = start + (right - start) / 4; // on the line, before the pen's projection ends

    auto inkAt = [&](int frame, QPointF canvas) {
        BitmapImage* image = layer->getBitmapImageAtFrame(frame);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (qAlpha(image->pixel(canvas.toPoint() + QPoint(dx, dy))) != 0) { return true; }
        return false;
    };
    auto draw = [&](int frame) {
        layer->addNewKeyFrameAt(frame);
        editor->scrubTo(frame);
        stroke(area, editor->view()->mapCanvasToScreen(start), editor->view()->mapCanvasToScreen(penEnd), 1.0);
    };

    draw(2); // snap off
    CHECK(inkAt(2, penEnd));
    CHECK_FALSE(inkAt(2, snapped));

    editor->overlays()->setPerspectiveSnapEnabled(true);
    draw(3);
    CHECK(inkAt(3, snapped));
    CHECK_FALSE(inkAt(3, penEnd));

    // Snap needs a visible perspective overlay.
    prefs->set(SETTING::OVERLAY_PERSPECTIVE2, false);
    draw(4);
    CHECK(inkAt(4, penEnd));

    prefs->set(SETTING::OVERLAY_PERSPECTIVE2, twoPointWas);
}

TEST_CASE("Eraser - a stroke shrinks the bitmap bounds to the remaining ink and undo restores them")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    const QRect blobA(-80, -10, 20, 20);
    const QRect blobB(60, -10, 20, 20);
    BitmapImage a(blobA, Qt::black);
    BitmapImage b(blobB, Qt::black);
    image->paste(&a);
    image->paste(&b);
    // Minimal bounds before the stroke, like a frame loaded from a file.
    const QRect beforeBounds = image->bounds();
    REQUIRE(beforeBounds == blobA.united(blobB));
    REQUIRE(image->isMinimallyBounded());
    const QImage beforePixels = *image->image();

    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->scrubTo(1);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(ERASER);
    auto* eraser = static_cast<StrokeTool*>(editor->tools()->currentTool());
    const ToolSettingsGuard guard(eraser);
    eraser->setWidth(100);
    eraser->setFeatherEnabled(false);
    eraser->setPressureEnabled(false);
    auto erase = [&](QPointF from, QPointF to) {
        stroke(area, editor->view()->mapCanvasToScreen(from), editor->view()->mapCanvasToScreen(to), 1.0);
    };

    erase(QPointF(70, -40), QPointF(70, 40));
    image = layer->getBitmapImageAtFrame(1);
    CHECK(image->isMinimallyBounded());
    CHECK(image->bounds() == blobA);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());
    REQUIRE(undoAction->isEnabled());
    undoAction->trigger();
    image = layer->getBitmapImageAtFrame(1);
    CHECK(image->bounds() == beforeBounds);
    CHECK(*image->image() == beforePixels);

    REQUIRE(redoAction->isEnabled());
    redoAction->trigger();
    CHECK(layer->getBitmapImageAtFrame(1)->bounds() == blobA);
    erase(QPointF(-70, -40), QPointF(-70, 40));
    image = layer->getBitmapImageAtFrame(1);
    CHECK(image->isMinimallyBounded());
    CHECK(image->bounds().isEmpty());
}

TEST_CASE("Pencil - a stroke on a held frame makes a new Drawing by default")
{
    const SettingsGuard settingsGuard({ SETTING_DRAW_ON_EMPTY_FRAME_ACTION });
    pencilSettings().remove(SETTING_DRAW_ON_EMPTY_FRAME_ACTION);

    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL);
    const ToolSettingsGuard guard(editor->tools()->currentTool());
    static_cast<StrokeTool*>(editor->tools()->currentTool())->setWidth(6);
    const QPointF origin = editor->view()->mapCanvasToScreen(QPointF(0, 0));

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    SECTION("Default: a new Drawing, undone with its stroke in one step")
    {
        // Reload so the test reads the default, not a value another test set.
        editor->preference()->loadPrefs();
        REQUIRE(editor->preference()->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION) == CREATE_NEW_KEY);

        editor->scrubTo(3);
        stroke(area, origin + QPointF(-10, 0), origin + QPointF(10, 0), 1.0);
        REQUIRE(layer->keyExists(3));
        CHECK(layer->getBitmapImageAtFrame(1)->bounds().isEmpty());
        CHECK_FALSE(layer->getBitmapImageAtFrame(3)->bounds().isEmpty());

        undoAction->trigger();
        CHECK_FALSE(layer->keyExists(3));
        CHECK(layer->getBitmapImageAtFrame(1)->bounds().isEmpty());
        CHECK_FALSE(undoAction->isEnabled());
        redoAction->trigger();
        REQUIRE(layer->keyExists(3));
        CHECK_FALSE(layer->getBitmapImageAtFrame(3)->bounds().isEmpty());
    }

    SECTION("A saved Keep drawing on previous choice still loads and draws on the held Drawing")
    {
        pencilSettings().setValue(SETTING_DRAW_ON_EMPTY_FRAME_ACTION, int(KEEP_DRAWING_ON_PREVIOUS_KEY));
        editor->preference()->loadPrefs();
        REQUIRE(editor->preference()->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION) == KEEP_DRAWING_ON_PREVIOUS_KEY);

        editor->scrubTo(3);
        stroke(area, origin + QPointF(-10, 0), origin + QPointF(10, 0), 1.0);
        CHECK_FALSE(layer->keyExists(3));
        CHECK_FALSE(layer->getBitmapImageAtFrame(1)->bounds().isEmpty());
    }
}

TEST_CASE("Polyline lasso fill and Delete selection on a held frame undo their new Drawing in one step")
{
    const SettingsGuard settingsGuard({ SETTING_DRAW_ON_EMPTY_FRAME_ACTION });
    pencilSettings().remove(SETTING_DRAW_ON_EMPTY_FRAME_ACTION);

    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->getBitmapImageAtFrame(1)->drawRect(QRectF(-30, -30, 60, 60), Qt::NoPen, QBrush(Qt::black),
                                              QPainter::CompositionMode_Source, false);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    editor->preference()->loadPrefs();
    REQUIRE(editor->preference()->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION) == CREATE_NEW_KEY);
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    const QPointF origin = editor->view()->mapCanvasToScreen(QPointF(0, 0));
    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    editor->scrubTo(3);

    SECTION("Polyline")
    {
        editor->tools()->setCurrentTool(POLYLINE);
        const ToolSettingsGuard guard(editor->tools()->currentTool());
        tap(area, origin + QPointF(-20, 0), 1.0);
        tap(area, origin + QPointF(20, 0), 1.0);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(area, &enter);
    }

    SECTION("Lasso fill")
    {
        editor->tools()->setCurrentTool(LASSOFILL);
        tablet(area, QEvent::TabletPress, origin + QPointF(-20, -20), 1.0);
        tablet(area, QEvent::TabletMove, origin + QPointF(20, -20), 1.0);
        tablet(area, QEvent::TabletMove, origin + QPointF(20, 20), 1.0);
        tablet(area, QEvent::TabletRelease, origin + QPointF(-20, 20), 1.0);
    }

    SECTION("Delete selection")
    {
        editor->select()->setSelection(QRectF(-10, -10, 20, 20));
        area->deleteSelection();
    }

    REQUIRE(layer->keyExists(3));
    undoAction->trigger();
    CHECK_FALSE(layer->keyExists(3));
    CHECK_FALSE(undoAction->isEnabled());
}

TEST_CASE("A double click that Windows makes from a pen tap starts no mouse stroke")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL);
    BaseTool* pencil = editor->tools()->currentTool();
    const ToolSettingsGuard guard(pencil);

    const QPointF at(300, 300);
    auto pen = [&](QEvent::Type type, quint64 t) {
        QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), at, area->mapToGlobal(at), 0.2, 0, 0, 0, 0, 0,
                       Qt::NoModifier, Qt::LeftButton, type == QEvent::TabletPress ? Qt::LeftButton : Qt::NoButton);
        e.setTimestamp(t);
        QApplication::sendEvent(area, &e);
    };
    pen(QEvent::TabletPress, 1000);
    pen(QEvent::TabletRelease, 1010);
    REQUIRE_FALSE(pencil->isActive());

    QMouseEvent dbl(QEvent::MouseButtonDblClick, at, area->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    dbl.setTimestamp(1020);
    QApplication::sendEvent(area, &dbl);
    CHECK_FALSE(pencil->isActive());
}

TEST_CASE("DabTool - no event sequence balloons a light stroke past its pressure width")
{
    const SettingsGuard settingsGuard({ SETTING_PRESSURE_CURVE });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 2; f <= 20; ++f)
        layer->addNewKeyFrameAt(f);

    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->preference()->set(SETTING::PRESSURE_CURVE, QString());

    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(40);
    brush->setFeatherEnabled(false);
    brush->setPressureEnabled(true);
    brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));

    const QPointingDevice eraser("balloon eraser", 175, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Eraser,
                                 QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    quint64 clock = 1000;
    auto pen = [&](QEvent::Type type, QPointF pos, qreal pressure, quint64 t, QPointF tilt = QPointF(),
                   const QPointingDevice* device = QPointingDevice::primaryPointingDevice()) {
        const bool down = type == QEvent::TabletPress || type == QEvent::TabletMove;
        QTabletEvent e(type, device, pos, area->mapToGlobal(pos), pressure, tilt.x(), tilt.y(), 0, 0, 0,
                       Qt::NoModifier, Qt::LeftButton, down && pressure > 0 ? Qt::LeftButton : Qt::NoButton);
        e.setTimestamp(t);
        QApplication::sendEvent(area, &e);
        clock = qMax(clock, t);
    };
    auto mouse = [&](QEvent::Type type, QPointF pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, area->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        e.setTimestamp(clock);
        QApplication::sendEvent(area, &e);
    };
    // A horizontal stroke along y = 300 at `pressure`, 8 ms per event.
    auto lightStroke = [&](qreal pressure, int events = 20) {
        pen(QEvent::TabletPress, QPointF(100, 300), pressure, clock += 1000);
        for (int i = 1; i <= events; ++i)
            pen(QEvent::TabletMove, QPointF(100 + 400.0 * i / events, 300), pressure, clock += 8);
        pen(QEvent::TabletRelease, QPointF(500, 300), pressure, clock += 8);
    };
    // Thickest ink column of a frame: the widest dab of a horizontal stroke.
    auto thickest = [&](int frame) {
        const QImage* img = layer->getBitmapImageAtFrame(frame)->image();
        int most = 0;
        for (int x = 0; x < img->width(); ++x)
        {
            int n = 0;
            for (int y = 0; y < img->height(); ++y)
                n += qAlpha(img->pixel(x, y)) != 0;
            most = qMax(most, n);
        }
        return most;
    };
    // Width 40 at pressure 0.2 is 8 px; dynamics multiply it by at most `dynamics`; 2 px of edge.
    auto checkLight = [&](int frame, qreal dynamics = 1) {
        const int ink = thickest(frame);
        INFO("frame " << frame << " thickest " << ink << " px");
        CHECK(ink > 0);
        CHECK(ink <= 40 * 0.2 * dynamics + 2);
    };

    SECTION("Mouse and synthesized mouse events inside a tablet stroke")
    {
        editor->scrubTo(1);
        pen(QEvent::TabletPress, QPointF(100, 300), 0.2, clock += 1000);
        for (int i = 1; i <= 20; ++i)
        {
            const QPointF pos(100 + 20.0 * i, 300);
            if (i == 5) mouse(QEvent::MouseButtonPress, pos, Qt::LeftButton);
            if (i == 6) mouse(QEvent::MouseMove, pos, Qt::LeftButton);
            if (i == 7) mouse(QEvent::MouseButtonDblClick, pos, Qt::LeftButton);
            if (i == 8) mouse(QEvent::MouseButtonRelease, pos, Qt::NoButton);
            if (i == 9)
            {
                // Past the ScribbleArea filter: a mouse move straight to the tool.
                QMouseEvent m(QEvent::MouseMove, pos, area->mapToGlobal(pos), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                PointerEvent pe(&m, editor->view()->mapScreenToCanvas(pos));
                brush->pointerMoveEvent(&pe);
            }
            pen(QEvent::TabletMove, pos, 0.2, clock += 8);
        }
        pen(QEvent::TabletRelease, QPointF(500, 300), 0.2, clock += 8);
        // Mouse events Windows makes from the release, then a light stroke.
        mouse(QEvent::MouseButtonPress, QPointF(500, 300), Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, QPointF(500, 300), Qt::NoButton);
        checkLight(1);
        editor->scrubTo(2);
        lightStroke(0.2);
        checkLight(2);
    }
    SECTION("Eraser end and proximity events around a light stroke")
    {
        editor->scrubTo(3);
        pen(QEvent::TabletMove, QPointF(300, 200), 0, clock += 8, QPointF(), &eraser);
        pen(QEvent::TabletPress, QPointF(600, 500), 1.0, clock += 8, QPointF(), &eraser);
        pen(QEvent::TabletRelease, QPointF(600, 500), 1.0, clock += 8, QPointF(), &eraser);
        QTabletEvent enter(QEvent::TabletEnterProximity, QPointingDevice::primaryPointingDevice(), QPointF(), QPointF(),
                           1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::NoButton, Qt::NoButton);
        QApplication::sendEvent(qApp, &enter);
        pen(QEvent::TabletMove, QPointF(100, 300), 0, clock += 8);
        lightStroke(0.2);
        QTabletEvent leave(QEvent::TabletLeaveProximity, QPointingDevice::primaryPointingDevice(), QPointF(), QPointF(),
                           1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::NoButton, Qt::NoButton);
        QApplication::sendEvent(qApp, &leave);
        CHECK(editor->tools()->currentTool() == brush);
        checkLight(3);
    }
    SECTION("The full-pressure end of a tapered stroke and of a shape stays on its own stroke")
    {
        brush->setDynamic(StrokeToolProperties::TAPER_END_VALUE, 50);
        editor->scrubTo(4);
        lightStroke(1.0);
        editor->scrubTo(5);
        lightStroke(0.2);
        checkLight(5);
        // A stroke whose release reports full pressure.
        editor->scrubTo(6);
        pen(QEvent::TabletPress, QPointF(100, 300), 0.2, clock += 1000);
        for (int i = 1; i <= 20; ++i)
            pen(QEvent::TabletMove, QPointF(100 + 20.0 * i, 300), 0.2, clock += 8);
        pen(QEvent::TabletRelease, QPointF(500, 300), 1.0, clock += 8);
        checkLight(6);
        // A shape places full-pressure dabs, then the brush draws light.
        editor->tools()->setCurrentTool(SHAPE);
        editor->scrubTo(7);
        pen(QEvent::TabletPress, QPointF(100, 100), 1.0, clock += 1000);
        pen(QEvent::TabletMove, QPointF(200, 150), 1.0, clock += 8);
        pen(QEvent::TabletRelease, QPointF(200, 150), 1.0, clock += 8);
        editor->tools()->setCurrentTool(BRUSH);
        editor->scrubTo(8);
        lightStroke(0.2);
        checkLight(8);
    }
    SECTION("A quick re-press or double tap after a hard stroke")
    {
        editor->scrubTo(9);
        lightStroke(1.0);
        editor->scrubTo(10);
        // Within the double-click time and distance of the last press: ScribbleArea sends a double click.
        pen(QEvent::TabletPress, QPointF(102, 300), 0.2, clock += 50);
        for (int i = 1; i <= 20; ++i)
            pen(QEvent::TabletMove, QPointF(100 + 20.0 * i, 300), 0.2, clock += 8);
        pen(QEvent::TabletRelease, QPointF(500, 300), 0.2, clock += 8);
        checkLight(10);
        // Light double tap, then the mouse double click Windows makes from it.
        editor->scrubTo(11);
        pen(QEvent::TabletPress, QPointF(300, 300), 0.2, clock += 600);
        pen(QEvent::TabletRelease, QPointF(300, 300), 0.2, clock += 8);
        pen(QEvent::TabletPress, QPointF(300, 300), 0.2, clock += 50);
        pen(QEvent::TabletRelease, QPointF(300, 300), 0.2, clock += 8);
        mouse(QEvent::MouseButtonPress, QPointF(300, 300), Qt::LeftButton);
        mouse(QEvent::MouseButtonDblClick, QPointF(300, 300), Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease, QPointF(300, 300), Qt::NoButton);
        lightStroke(0.2);
        checkLight(11);
    }
    SECTION("Speed, tilt and jitter stay inside their size range at odd timestamps and tilts")
    {
        brush->setDynamic(StrokeToolProperties::SPEED_TO_SIZE_VALUE, 100);
        brush->setDynamic(StrokeToolProperties::TILT_TO_SIZE_VALUE, 100);
        brush->setDynamic(StrokeToolProperties::SIZE_JITTER_VALUE, 100);
        brush->seedRandom(175);
        editor->scrubTo(12);
        pen(QEvent::TabletPress, QPointF(100, 300), 0.2, clock += 1000, QPointF(90, -90));
        const quint64 start = clock;
        for (int i = 1; i <= 40; ++i)
        {
            // Same, backward, zero and huge timestamp steps; tilt past the 60 degree range.
            const quint64 t = (i % 4 == 0) ? start : (i % 4 == 1) ? 0 : (i % 4 == 2) ? start + 100000 * i : start + i;
            pen(QEvent::TabletMove, QPointF(100 + 10.0 * i, 300), 0.2, t, QPointF(i % 2 ? 90 : -90, 90));
        }
        pen(QEvent::TabletRelease, QPointF(500, 300), 0.2, clock += 1);
        checkLight(12, 4);
    }
    SECTION("Smoothing does not carry a hard stroke into the next light one")
    {
        brush->setSmoothingMode(static_cast<int>(SmoothingMode::OneEuro));
        brush->setSmoothingStrength(100);
        editor->scrubTo(13);
        lightStroke(1.0, 3);
        editor->scrubTo(14);
        // Back and forth along the line, so smoothing overshoots the turns.
        pen(QEvent::TabletPress, QPointF(100, 300), 0.2, clock += 1000);
        for (int i = 1; i <= 30; ++i)
            pen(QEvent::TabletMove, QPointF(i % 2 ? 450 : 150, 300), 0.2, clock += 1);
        pen(QEvent::TabletRelease, QPointF(300, 300), 0.2, clock += 1);
        checkLight(14);
    }
}

TEST_CASE("DabTool - the pressure debug log writes one line per dab only while it is on")
{
    const SettingsGuard settingsGuard({ SETTING_PRESSURE_CURVE, SETTING_PRESSURE_DEBUG_LOG });
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString oldPath = DabTool::pressureLogPath();
    OnScopeExit(DabTool::pressureLogPath() = oldPath);
    DabTool::pressureLogPath() = dir.filePath("pressure-log.csv");

    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(2);
    s.start();
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->preference()->set(SETTING::PRESSURE_CURVE, QString());
    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setWidth(40);
    brush->setPressureEnabled(true);

    editor->preference()->set(SETTING::PRESSURE_DEBUG_LOG, false);
    stroke(area, QPointF(100, 300), QPointF(300, 300), 0.5);
    CHECK_FALSE(QFile::exists(DabTool::pressureLogPath()));

    editor->preference()->set(SETTING::PRESSURE_DEBUG_LOG, true);
    editor->scrubTo(2);
    tablet(area, QEvent::TabletPress, QPointF(100, 300), 0.5);
    tablet(area, QEvent::TabletMove, QPointF(300, 300), 0.5);
    tablet(area, QEvent::TabletRelease, QPointF(300, 300), 0.5);
    QFile log(DabTool::pressureLogPath());
    REQUIRE(log.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList lines = QString::fromUtf8(log.readAll()).split('\n', Qt::SkipEmptyParts);
    REQUIRE(lines.size() > 3);
    CHECK(lines[0] == "input,pressure,curve,taper,size,width");
    CHECK(lines[1].startsWith("# stroke"));
    // A dab in the middle: tablet input at half pressure on a straight curve is 20 px wide.
    CHECK(lines[lines.size() / 2] == "tablet,0.500,0.500,1.000,1.000,20.00");
}
