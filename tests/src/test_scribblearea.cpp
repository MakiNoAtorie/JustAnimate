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

#include <QApplication>
#include <QPixmapCache>
#include <QTabletEvent>
#include <QWheelEvent>
#include <QTemporaryDir>
#include <QtMath>
#include "mathutils.h"

#include "bitmapimage.h"
#include "brushpresets.h"
#include "brushtool.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "pencildef.h"
#include "pencilsettings.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "stageprofiler.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "viewmanager.h"

static void sendWheel(ScribbleArea* area, QPoint pixels, QPoint angle, Qt::KeyboardModifiers mods)
{
    const QPointF pos(300, 200);
    QWheelEvent e(pos, area->mapToGlobal(pos), pixels, angle, Qt::NoButton, mods, Qt::NoScrollPhase, false);
    QApplication::sendEvent(area, &e);
}

TEST_CASE("ScribbleArea - wheel pans zooms and rotates by modifier")
{
    TestSession s;
    s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    ScribbleArea* area = s.area.get();
    ViewManager* view = s.editor->view();
    area->init();
    area->resize(800, 600);
    const QPointF origin = view->mapCanvasToScreen(QPointF(0, 0));

    for (bool trackpad : {false, true})
    {
        const QPoint px = trackpad ? QPoint(0, 30) : QPoint();
        view->scale(1.0);
        view->translate(0, 0);
        view->rotate(0);

        sendWheel(area, px, QPoint(0, 120), Qt::NoModifier);
        CHECK(view->scaling() == Approx(1.0));
        CHECK(view->mapCanvasToScreen(QPointF(0, 0)).x() == Approx(origin.x()));
        CHECK(view->mapCanvasToScreen(QPointF(0, 0)).y() > origin.y());

        view->translate(0, 0);
        sendWheel(area, trackpad ? QPoint(0, 30) : QPoint(), QPoint(0, 120), Qt::ShiftModifier);
        CHECK(view->mapCanvasToScreen(QPointF(0, 0)).x() > origin.x());
        CHECK(view->mapCanvasToScreen(QPointF(0, 0)).y() == Approx(origin.y()));

        view->translate(0, 0);
        const QPointF anchor = view->mapScreenToCanvas(QPointF(300, 200));
        sendWheel(area, px, QPoint(0, 120), Qt::ControlModifier);
        CHECK(view->scaling() > 1.0);
        CHECK(view->mapCanvasToScreen(anchor).x() == Approx(300));
        CHECK(view->mapCanvasToScreen(anchor).y() == Approx(200));

        view->scale(1.0);
        view->translate(0, 0);
        const QPointF anchor2 = view->mapScreenToCanvas(QPointF(300, 200));
        sendWheel(area, px, QPoint(0, 120), Qt::AltModifier);
        CHECK(view->rotation() != Approx(0.0));
        CHECK(view->mapCanvasToScreen(anchor2).x() == Approx(300));
        CHECK(view->mapCanvasToScreen(anchor2).y() == Approx(200));
    }
}

TEST_CASE("ScribbleArea - canvas HUD shows the brush ring and view values")
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
    brush->set(ToolProperty::Width, 20);

    editor->view()->scale(2.0);
    editor->view()->rotate(30);

    CanvasHud hud = area->canvasHud();
    CHECK(hud.ringRadius == Approx(20)); // 20 px brush at 200 % zoom: 40 px across on screen
    CHECK(hud.zoomPercent == 200);
    CHECK(hud.rotation == Approx(30));

    editor->tools()->setCurrentTool(HAND);
    hud = area->canvasHud();
    CHECK(hud.ringRadius == 0); // no brush size: no ring
    CHECK(hud.zoomPercent == 200);
}

TEST_CASE("radialSliceAt - slice 0 is at the top and slices go clockwise")
{
    CHECK(MathUtils::radialSliceAt(QPointF(0, -50), 4, 20, 100) == 0);
    CHECK(MathUtils::radialSliceAt(QPointF(50, 0), 4, 20, 100) == 1);
    CHECK(MathUtils::radialSliceAt(QPointF(0, 50), 4, 20, 100) == 2);
    CHECK(MathUtils::radialSliceAt(QPointF(-50, 0), 4, 20, 100) == 3);
    CHECK(MathUtils::radialSliceAt(QPointF(-30, -40), 4, 20, 100) == 0); // 37 degrees left of up: still the top slice
    CHECK(MathUtils::radialSliceAt(QPointF(-40, -30), 4, 20, 100) == 3);
    CHECK(MathUtils::radialSliceAt(QPointF(5, 5), 4, 20, 100) == -1);    // centre: no slice
    CHECK(MathUtils::radialSliceAt(QPointF(0, -150), 4, 20, 100) == -1); // outside the ring
    CHECK(MathUtils::radialSliceAt(QPointF(0, -50), 0, 20, 100) == -1);
}

namespace
{

void pen(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0.8, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

/// The middle of the slice with this label in the open menu.
QPointF slicePos(const RadialMenu& menu, const QString& label)
{
    const int i = menu.labels.indexOf(label);
    REQUIRE(i >= 0);
    const qreal a = 2 * M_PI * i / menu.labels.size();
    const qreal r = (RadialMenu::kInner + RadialMenu::kOuter) / 2;
    return menu.center + QPointF(qSin(a), -qCos(a)) * r;
}

}

TEST_CASE("ScribbleArea - radial tool menu picks a tool with the pen")
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

    area->openToolMenu(QPointF(400, 300));
    const RadialMenu& menu = area->radialMenu();
    REQUIRE(menu.kind == RadialMenu::Tools);
    CHECK(menu.labels.contains(BaseTool::TypeName(BRUSH)));
    CHECK_FALSE(menu.labels.contains(BaseTool::TypeName(CAMERA)));

    // Press on one slice, slide to another, lift: the slice under the pen wins.
    pen(area, QEvent::TabletPress, slicePos(menu, BaseTool::TypeName(ERASER)));
    pen(area, QEvent::TabletMove, slicePos(menu, BaseTool::TypeName(BRUSH)));
    CHECK(menu.hovered == menu.labels.indexOf(BaseTool::TypeName(BRUSH)));
    pen(area, QEvent::TabletRelease, slicePos(menu, BaseTool::TypeName(BRUSH)));
    CHECK(area->radialMenu().kind == RadialMenu::Closed);
    CHECK(editor->tools()->currentTool()->type() == BRUSH);
    CHECK(layer->getLastBitmapImageAtFrame(1)->bounds().isEmpty()); // the menu took the pen, not the tool

    // A tap in the centre closes the menu and keeps the tool.
    area->openToolMenu(QPointF(400, 300));
    pen(area, QEvent::TabletPress, QPointF(400, 300));
    pen(area, QEvent::TabletRelease, QPointF(400, 300));
    CHECK(area->radialMenu().kind == RadialMenu::Closed);
    CHECK(editor->tools()->currentTool()->type() == BRUSH);
}

TEST_CASE("ScribbleArea - quick preset picker applies a preset with the pen")
{
    QTemporaryDir dir;
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
    BrushPresets presets(dir.path());
    brush->set(ToolProperty::Width, 3);
    REQUIRE(presets.save("Fine", *brush));
    brush->set(ToolProperty::Width, 60);
    REQUIRE(presets.save("Wide", *brush));
    REQUIRE(presets.save("Rub", *brush, QString(), "eraser")); // not listed for the brush
    brush->set(ToolProperty::Width, 10);

    area->openPresetPicker(QPointF(400, 300), dir.path());
    const RadialMenu& menu = area->radialMenu();
    REQUIRE(menu.kind == RadialMenu::Presets);
    CHECK(menu.labels == QStringList{ "Fine", "Wide" });

    const QPointF wide = slicePos(menu, "Wide");
    pen(area, QEvent::TabletPress, wide);
    pen(area, QEvent::TabletRelease, wide);
    CHECK(area->radialMenu().kind == RadialMenu::Closed);
    CHECK(brush->get(ToolProperty::Width) == 60);
}

namespace
{

/// A right-button pen press at a canvas point, stamped `ms` after the test epoch.
void tap(ScribbleArea* area, Editor* editor, QPointF canvasPos, ulong ms)
{
    const QPointF pos = editor->view()->mapCanvasToScreen(canvasPos);
    QTabletEvent e(QEvent::TabletPress, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0.8, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::RightButton, Qt::RightButton);
    e.setTimestamp(ms);
    QApplication::sendEvent(area, &e);
    QTabletEvent r(QEvent::TabletRelease, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::RightButton, Qt::NoButton);
    r.setTimestamp(ms + 10);
    QApplication::sendEvent(area, &r);
}

}

TEST_CASE("ScribbleArea - pen double tap needs two taps close in time and place")
{
    TestSession s;
    s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->tools()->setCurrentTool(HAND);

    // The hand tool resets the view on a right-button double click.
    auto zoomed = [&] { editor->view()->scale(2.0); return editor->view()->scaling() != 1.0; };

    REQUIRE(zoomed());
    tap(area, editor, QPointF(100, 100), 1000);
    tap(area, editor, QPointF(100, 100), 1100);
    CHECK(editor->view()->scaling() == 1.0);

    REQUIRE(zoomed());
    tap(area, editor, QPointF(100, 100), 5000);
    tap(area, editor, QPointF(100, 100), 5600);
    CHECK(editor->view()->scaling() != 1.0);

    tap(area, editor, QPointF(100, 100), 9000);
    tap(area, editor, QPointF(110, 100), 9100);
    CHECK(editor->view()->scaling() != 1.0);
}

TEST_CASE("ScribbleArea - a held frame repaints from the frame cache")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(200, 200);
    area->show(); // grab() on a hidden widget resizes it, which clears the cache
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(HAND);
    BitmapImage* key = layer->getBitmapImageAtFrame(1);
    REQUIRE(key);
    editor->scrubTo(3); // held by the key at 1

    const auto fill = [&](QColor c) {
        key->drawRect(QRectF(-20, -20, 40, 40), Qt::NoPen, QBrush(c), QPainter::CompositionMode_Source, false);
        area->onFrameModified(1);
    };
    const auto centre = [&] { return area->grab().toImage().pixelColor(100, 100); };

    const bool profiling = StageProfiler::enabled();
    StageProfiler::enabled() = true;
    fill(Qt::red);
    StageProfiler::reset();
    CHECK(centre() == QColor(Qt::red));
    CHECK(centre() == QColor(Qt::red));
    CHECK(StageProfiler::calls(StageProfiler::Composite) == 1);

    // An edit on the key reaches its held frames.
    fill(Qt::blue);
    CHECK(centre() == QColor(Qt::blue));

    // A tweened hold changes inside the hold: it skips the cache.
    REQUIRE(layer->addNewKeyFrameAt(5));
    layer->getKeyFrameAt(1)->setTween(true);
    area->onFrameModified(1);
    StageProfiler::reset();
    area->grab();
    area->grab();
    CHECK(StageProfiler::calls(StageProfiler::Composite) == 2);

    StageProfiler::enabled() = profiling;
    StageProfiler::reset();
}

TEST_CASE("ScribbleArea - a 48-frame loop repaints from the frame cache from the second pass")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    QPixmapCache::setCacheLimit(12345);
    area->init();
    CHECK(QPixmapCache::cacheLimit() == 12345); // the frame cache keeps its own limit
    QPixmapCache::setCacheLimit(10240);         // Qt's default
    area->resize(1280, 720);
    area->show(); // grab() on a hidden widget resizes it, which clears the cache
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(HAND);
    const int frames = 48;
    for (int f = 1; f <= frames; ++f)
    {
        if (f > 1) REQUIRE(layer->addNewKeyFrameAt(f));
        layer->getBitmapImageAtFrame(f)->drawRect(QRectF(-300 + f * 10, -100, 60, 60), Qt::NoPen, QBrush(Qt::red),
                                                  QPainter::CompositionMode_SourceOver, false);
    }
    area->onObjectLoaded();

    const bool profiling = StageProfiler::enabled();
    StageProfiler::enabled() = true;
    std::vector<QImage> firstPass;
    for (int f = 1; f <= frames; ++f)
    {
        editor->scrubTo(f);
        firstPass.push_back(area->grab().toImage());
    }
    StageProfiler::reset();
    for (int f = 1; f <= frames; ++f)
    {
        editor->scrubTo(f);
        CAPTURE(f);
        CHECK(area->grab().toImage() == firstPass[f - 1]); // a cached frame matches its composite
    }
    CHECK(StageProfiler::calls(StageProfiler::Composite) == 0);
    StageProfiler::enabled() = profiling;
    StageProfiler::reset();
}

TEST_CASE("ScribbleArea - the frame cache limit follows RAM and the viewport")
{
    const qint64 gb = qint64(1) << 30;
    const qsizetype frame1080Kb = 1920 * 1080 * 4 / 1024;
    CHECK(ScribbleArea::frameCacheLimitKb(QSize(1920, 1080), 64 * gb) == 240 * frame1080Kb);
    CHECK(ScribbleArea::frameCacheLimitKb(QSize(1920, 1080), 1 * gb) == gb / 4 / 1024); // a quarter of RAM
    CHECK(ScribbleArea::frameCacheLimitKb(QSize(3840, 2160), 64 * gb) > 240 * frame1080Kb);
    CHECK(ScribbleArea::frameCacheLimitKb(QSize(100, 100), 64 * gb) == 100 * 1024);     // the old 100 MB floor
    CHECK(ScribbleArea::frameCacheLimitKb(QSize(1920, 1080), 0) == 100 * 1024);         // RAM unknown

    TestSession s;
    s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(640, 360);
    area->show();
    const qsizetype before = area->frameCacheLimitKb();
    area->resize(1280, 720);
    CHECK(area->frameCacheLimitKb() > before);
}

TEST_CASE("ScribbleArea - an edit on another layer's key repaints its held frames")
{
    TestSession s;
    LayerBitmap* below = s.object->addNewBitmapLayer();
    LayerBitmap* current = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(200, 200);
    area->show(); // grab() on a hidden widget resizes it, which clears the cache
    editor->layers()->setCurrentLayer(current);
    editor->tools()->setCurrentTool(HAND);
    REQUIRE(current->addNewKeyFrameAt(3)); // the current layer's key at 3 does not hold frame 1
    BitmapImage* key = below->getBitmapImageAtFrame(1);
    REQUIRE(key);
    editor->scrubTo(3); // held by the lower layer's key at 1

    const auto fill = [&](QColor c) {
        key->drawRect(QRectF(-20, -20, 40, 40), Qt::NoPen, QBrush(c), QPainter::CompositionMode_Source, false);
        area->onFrameModified(1);
    };
    const auto centre = [&] { return area->grab().toImage().pixelColor(100, 100); };

    fill(Qt::red);
    CHECK(centre() == QColor(Qt::red));
    fill(Qt::blue);
    CHECK(centre() == QColor(Qt::blue));
}

namespace
{

/// Bounds of the pixels the Brush outline changes, around a pointer hovering at the canvas origin.
QRect outlineBounds(Editor* editor, ScribbleArea* area)
{
    const QPointF pos = editor->view()->mapCanvasToScreen(QPointF(0, 0));
    QMouseEvent move(QEvent::MouseMove, pos, area->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(area, &move);
    PreferenceManager* prefs = editor->preference();
    prefs->set<SETTING::CURSOR_STYLE>(CURSOR_CROSS_ONLY);
    const QImage cross = area->grab().toImage();
    prefs->set<SETTING::CURSOR_STYLE>(CURSOR_BRUSH_OUTLINE);
    const QImage outline = area->grab().toImage();
    QRect bounds;
    for (int y = 0; y < cross.height(); ++y)
        for (int x = 0; x < cross.width(); ++x)
            if (cross.pixel(x, y) != outline.pixel(x, y)) { bounds |= QRect(x, y, 1, 1); }
    return bounds;
}

}

TEST_CASE("ScribbleArea - Brush outline follows size zoom and View rotation")
{
    const SettingsGuard settings({ SETTING_CURSOR_STYLE });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(200, 200);
    area->show();
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->setValue(StrokeToolProperties::TIP_ASPECT_VALUE, 100);
    brush->setValue(StrokeToolProperties::TIP_ANGLE_VALUE, 0);
    brush->setValue(StrokeToolProperties::TIP_SQUARE_ENABLED, false);

    brush->set(ToolProperty::Width, 40);
    QRect b = outlineBounds(editor, area);
    CHECK(b.width() == Approx(41).margin(2));  // 40 px across at 100 %
    CHECK(b.height() == Approx(41).margin(2));

    editor->view()->scale(2.0);
    b = outlineBounds(editor, area);
    CHECK(b.width() == Approx(81).margin(2)); // 80 px across at 200 %

    // A flat tip lies along x; a quarter turn of the View stands it up.
    editor->view()->scale(1.0);
    brush->setValue(StrokeToolProperties::TIP_ASPECT_VALUE, 50);
    b = outlineBounds(editor, area);
    CHECK(b.width() == Approx(41).margin(2));
    CHECK(b.height() == Approx(21).margin(2));
    editor->view()->rotate(90);
    b = outlineBounds(editor, area);
    CHECK(b.width() == Approx(21).margin(2));
    CHECK(b.height() == Approx(41).margin(2));
}

TEST_CASE("ScribbleArea - the Brush outline paints its centre cross")
{
    const SettingsGuard settings({ SETTING_CURSOR_STYLE });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(200, 200);
    area->show();
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BRUSH);
    BrushTool* brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);
    brush->set(ToolProperty::Width, 40);

    // The hardware cursor is not in a grab, so a cross in the grab is painted.
    const QPoint centre = editor->view()->mapCanvasToScreen(QPointF(0, 0)).toPoint();
    const QRect b = outlineBounds(editor, area);
    REQUIRE(b.contains(centre));
    const QImage outline = area->grab().toImage();
    editor->preference()->set<SETTING::CURSOR_STYLE>(CURSOR_CROSS_ONLY);
    const QImage cross = area->grab().toImage();
    CHECK(outline.pixel(centre + QPoint(3, 0)) != cross.pixel(centre + QPoint(3, 0)));
    CHECK(outline.pixel(centre + QPoint(0, 3)) != cross.pixel(centre + QPoint(0, 3)));
}

TEST_CASE("ScribbleArea - below 6 px on screen only the cross shows")
{
    const SettingsGuard settings({ SETTING_CURSOR_STYLE });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(200, 200);
    area->show();
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL);
    StrokeTool* pencil = static_cast<StrokeTool*>(editor->tools()->getTool(PENCIL));
    const ToolSettingsGuard guard(pencil);

    pencil->set(ToolProperty::Width, 5);
    CHECK(outlineBounds(editor, area).isEmpty());
    editor->view()->scale(2.0); // 10 px on screen
    CHECK_FALSE(outlineBounds(editor, area).isEmpty());
}

TEST_CASE("Preferences - the dotted-cursor key migrates once to the cursor style")
{
    const SettingsGuard settings({ SETTING_CURSOR_STYLE, SETTING_CANVAS_CURSOR, SETTING_TOOL_CURSOR });
    QSettings& store = pencilSettings();
    store.remove(SETTING_CURSOR_STYLE);
    store.setValue(SETTING_CANVAS_CURSOR, false);
    store.setValue(SETTING_TOOL_CURSOR, true);
    {
        TestSession s;
        REQUIRE(s.start());
        CHECK(s.editor->preference()->get<SETTING::CURSOR_STYLE>() == CURSOR_CROSS_ONLY);
        s.editor->preference()->set<SETTING::CURSOR_STYLE>(CURSOR_BRUSH_OUTLINE);
    }
    CHECK_FALSE(store.contains(SETTING_CANVAS_CURSOR));
    CHECK_FALSE(store.contains(SETTING_TOOL_CURSOR));

    store.setValue(SETTING_CANVAS_CURSOR, false); // a stale old key no longer wins
    TestSession s;
    REQUIRE(s.start());
    CHECK(s.editor->preference()->get<SETTING::CURSOR_STYLE>() == CURSOR_BRUSH_OUTLINE);
}