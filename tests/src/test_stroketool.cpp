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

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTabletEvent>

#include "beziercurve.h"
#include "bitmapimage.h"
#include "brushtool.h"
#include "colormanager.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "preferencemanager.h"
#include "pressurecurve.h"
#include "scribblearea.h"
#include "stageprofiler.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "undoredomanager.h"
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
    brush->setTipAspect(20); // 40 px wide, 8 px tall

    // A vertical stroke with the tip at 0 degrees: the ink is as wide as the tip.
    editor->scrubTo(1);
    stroke(area, QPointF(300, 100), QPointF(300, 400), 1.0);
    const int fixedWidth = inkWidth(layer->getBitmapImageAtFrame(1));

    brush->setTipAngle(90);
    editor->scrubTo(2);
    stroke(area, QPointF(300, 100), QPointF(300, 400), 1.0);
    const int turnedWidth = inkWidth(layer->getBitmapImageAtFrame(2));

    // Follow direction: the tip width lies along the stroke, so the ink is as wide as the tip is tall.
    brush->setTipAngle(0);
    brush->setTipFollowsDirection(true);
    editor->scrubTo(3);
    stroke(area, QPointF(300, 100), QPointF(300, 400), 1.0);
    const int followWidth = inkWidth(layer->getBitmapImageAtFrame(3));

    brush->setTipSquare(true);
    brush->setTipFollowsDirection(false);
    brush->setTipAspect(100);
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
}

TEST_CASE("DabTool - a mouse stroke tapers at both ends")
{
    const SettingsGuard settingsGuard({ SETTING_TAPER_START, SETTING_TAPER_END });
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

    editor->preference()->set(SETTING::TAPER_START, 100);
    editor->preference()->set(SETTING::TAPER_END, 100);
    const auto tapered = strokeHeights(1);
    editor->preference()->set(SETTING::TAPER_START, 0);
    editor->preference()->set(SETTING::TAPER_END, 0);
    const auto plain = strokeHeights(2);
    INFO("tapered " << tapered[0] << "/" << tapered[1] << "/" << tapered[2]
         << " plain " << plain[0] << "/" << plain[1] << "/" << plain[2]);
    CHECK(tapered[1] >= 18);
    CHECK(tapered[0] <= 6);
    CHECK(tapered[2] <= 6);
    CHECK(plain[0] >= 18);
    CHECK(plain[2] >= 18);
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
    const SettingsGuard settingsGuard({ SETTING_SPEED_TO_SIZE, SETTING_TILT_TO_SIZE, SETTING_TILT_TO_ANGLE, SETTING_SIZE_JITTER });
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
        editor->preference()->set(SETTING::SPEED_TO_SIZE, -100);
        const int slow = strokeAt(1, 100, QPointF()); // 0.5 px/ms
        const int fast = strokeAt(2, 5, QPointF());   // 10 px/ms
        INFO("slow " << slow << "px fast " << fast << "px");
        CHECK(slow > 15);
        CHECK(fast < slow / 2);
    }
    SECTION("Tilt to size: a tilted pen paints a wider stroke")
    {
        editor->preference()->set(SETTING::TILT_TO_SIZE, 100);
        const int upright = strokeAt(1, 20, QPointF());
        const int tilted = strokeAt(2, 20, QPointF(60, 0));
        INFO("upright " << upright << "px tilted " << tilted << "px");
        CHECK(upright < 23);
        CHECK(tilted > 35);
    }
    SECTION("Tilt to angle: a flat tip turns with the pen tilt")
    {
        brush->setTipAspect(10); // 20 px wide, 2 px tall
        editor->preference()->set(SETTING::TILT_TO_ANGLE, 100);
        const int upright = strokeAt(1, 20, QPointF());
        const int tilted = strokeAt(2, 20, QPointF(0, -60)); // tip turns to 90 degrees
        INFO("upright " << upright << "px tilted " << tilted << "px");
        CHECK(upright < 6);
        CHECK(tilted > 15);
    }
    SECTION("Size jitter: the same seed paints the same stroke")
    {
        strokeAt(1, 20, QPointF());
        editor->preference()->set(SETTING::SIZE_JITTER, 100);
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
}
