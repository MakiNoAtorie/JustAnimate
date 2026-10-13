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

// Benchmark (ROADMAP 1.1): open, scrub, play and draw on a generated project.
// Full run:  cmake --build <build> --target benchmark   (see docs/benchmarks.md)
// The small "smoke" case below runs in the normal suite to keep this code working.

#include "catch.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QWindow>

#include "bitmapimage.h"
#include "editor.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "scribblearea.h"
#include "toolmanager.h"

namespace
{

struct BenchConfig
{
    int frames = 500;
    int layers = 10;  // bitmap layers, plus the default camera layer
    int strokes = 20; // per draw scenario
    int reps = 3;     // each rep is a fresh open of the project
};

struct Scenario
{
    const char* name;
    const char* unit;
    int units = 0;
    std::vector<double> ms;
};

QString createProject(const QString& dir, const BenchConfig& cfg)
{
    Object* object = new Object;
    object->init();
    object->addNewCameraLayer();
    for (int l = 0; l < cfg.layers; ++l)
    {
        LayerBitmap* layer = object->addNewBitmapLayer();
        const QColor color = QColor::fromHsv((l * 36) % 360, 160, 220);
        for (int f = 1; f <= cfg.frames; ++f)
        {
            layer->addNewKeyFrameAt(f);
            const QRectF r(-400 + (f * 7 + l * 60) % 700, -250 + l * 50, 100, 100);
            layer->getBitmapImageAtFrame(f)->drawEllipse(r, QPen(Qt::black, 4), QBrush(color),
                                                         QPainter::CompositionMode_SourceOver, true);
        }
    }
    object->data()->setCurrentLayer(1);

    const QString path = dir + "/benchmark.pclx";
    FileManager fm;
    const Status st = fm.save(object, path);
    delete object;
    REQUIRE(st.ok());
    return path;
}

// Counts paint events, so a scenario that silently paints nothing fails.
struct PaintCounter : QObject
{
    int paints = 0;
    bool eventFilter(QObject*, QEvent* e) override
    {
        if (e->type() == QEvent::Paint) ++paints;
        return false;
    }
};

// Mirrors the app's setup in MainWindow2 (editor, scribble area, connections).
struct Session
{
    Editor* editor = new Editor;
    ScribbleArea* area = new ScribbleArea(nullptr);
    PaintCounter counter;

    Session()
    {
        editor->setScribbleArea(area);
        editor->init();
        Object* object = new Object;
        object->init();
        object->addNewCameraLayer();
        object->addNewBitmapLayer();
        editor->setObject(object);
        area->setEditor(editor);
        area->init();

        QObject::connect(editor, &Editor::scrubbed, area, &ScribbleArea::onScrubbed);
        QObject::connect(editor, &Editor::frameModified, area, &ScribbleArea::onFrameModified);
        QObject::connect(editor, &Editor::framesModified, area, &ScribbleArea::onFramesModified);
        QObject::connect(editor, &Editor::objectLoaded, area, &ScribbleArea::onObjectLoaded);
        QObject::connect(editor->layers(), &LayerManager::currentLayerChanged, area, &ScribbleArea::onLayerChanged);
        QObject::connect(editor->playback(), &PlaybackManager::playStateChanged, area, &ScribbleArea::onPlayStateChanged);

        area->resize(1280, 720);
        area->show();
        // repaint() paints nothing until the window is exposed.
        QElapsedTimer wait;
        wait.start();
        while (!area->windowHandle()->isExposed() && wait.elapsed() < 5000)
            QApplication::processEvents();
        REQUIRE(area->windowHandle()->isExposed());
        area->installEventFilter(&counter);
    }
    ~Session()
    {
        delete area;
        delete editor;
    }
};

void mouse(QWidget* w, QEvent::Type type, QPointF pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, w->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &e);
}

// Deterministic zig-zag strokes across the canvas, repainting after each move like the app.
void drawStrokes(Session& s, ToolType tool, int strokes)
{
    s.editor->tools()->setCurrentTool(tool);
    s.editor->scrubTo(1);
    for (int i = 0; i < strokes; ++i)
    {
        const qreal y = 100 + (i * 37) % 500;
        mouse(s.area, QEvent::MouseButtonPress, QPointF(200, y), Qt::LeftButton);
        for (int p = 1; p <= 100; ++p)
        {
            mouse(s.area, QEvent::MouseMove, QPointF(200 + p * 8, y + ((p % 10) - 5) * 6), Qt::LeftButton);
            s.area->repaint();
        }
        mouse(s.area, QEvent::MouseButtonRelease, QPointF(1000, y), Qt::NoButton);
        s.area->repaint();
    }
}

std::vector<Scenario> runBenchmark(const BenchConfig& cfg)
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = createProject(dir.path(), cfg);

    std::vector<Scenario> sc = {
        { "open",         "file",   1 },
        { "scrub",        "frame",  cfg.frames },
        { "play",         "frame",  cfg.frames },
        { "draw pencil",  "stroke", cfg.strokes },
        { "draw brush",   "stroke", cfg.strokes },
    };

    for (int rep = 0; rep < cfg.reps; ++rep)
    {
        Session s;
        QElapsedTimer t;

        t.start();
        const Status st = s.editor->openObject(path, [](int) {}, [](int) {});
        sc[0].ms.push_back(t.nsecsElapsed() / 1e6);
        REQUIRE(st.ok());
        REQUIRE(s.editor->object()->getLayerCount() == cfg.layers + 1);

        // Scrub: step through every frame once, frames load from disk on first use.
        s.area->onObjectLoaded();
        s.counter.paints = 0;
        t.start();
        for (int f = 1; f <= cfg.frames; ++f)
        {
            s.editor->scrubTo(f);
            s.area->repaint();
        }
        sc[1].ms.push_back(t.nsecsElapsed() / 1e6);
        REQUIRE(s.counter.paints == cfg.frames);

        // Play: playback state on, frames rendered back to back as fast as possible.
        s.area->onObjectLoaded();
        s.editor->scrubTo(1);
        s.editor->playback()->play();
        s.counter.paints = 0;
        t.start();
        for (int f = 1; f <= cfg.frames; ++f)
        {
            s.area->repaint();
            s.editor->scrubForward();
        }
        sc[2].ms.push_back(t.nsecsElapsed() / 1e6);
        REQUIRE(s.counter.paints == cfg.frames);
        s.editor->playback()->stop();
        REQUIRE(s.editor->currentFrame() == cfg.frames + 1);

        t.start();
        drawStrokes(s, PENCIL, cfg.strokes);
        sc[3].ms.push_back(t.nsecsElapsed() / 1e6);
        auto* layer = static_cast<LayerBitmap*>(s.editor->layers()->currentLayer());
        REQUIRE(layer->getBitmapImageAtFrame(1)->bounds().width() > 500); // the strokes landed

        t.start();
        drawStrokes(s, BRUSH, cfg.strokes);
        sc[4].ms.push_back(t.nsecsElapsed() / 1e6);
    }
    return sc;
}

void printResults(const BenchConfig& cfg, std::vector<Scenario>& sc)
{
    std::printf("\nJustAnimate benchmark: %d frames x %d bitmap layers (+camera), 1280x720 canvas, "
                "%d strokes per draw, %d reps\n", cfg.frames, cfg.layers, cfg.strokes, cfg.reps);
    std::printf("%-12s %10s %10s %10s %14s\n", "scenario", "median ms", "min ms", "max ms", "median per");
    for (Scenario& s : sc)
    {
        std::sort(s.ms.begin(), s.ms.end());
        const double median = s.ms[s.ms.size() / 2];
        std::printf("%-12s %10.1f %10.1f %10.1f %8.2f ms/%s\n", s.name, median, s.ms.front(), s.ms.back(),
                    median / s.units, s.unit);
    }
    std::fflush(stdout);
}

} // namespace

TEST_CASE("Benchmark smoke: every scenario runs on a tiny project", "[BenchmarkSmoke]")
{
    BenchConfig cfg;
    cfg.frames = 3;
    cfg.layers = 2;
    cfg.strokes = 1;
    cfg.reps = 1;

    std::vector<Scenario> sc = runBenchmark(cfg);

    REQUIRE(sc.size() == 5);
    for (const Scenario& s : sc)
    {
        REQUIRE(s.ms.size() == 1);
        REQUIRE(s.ms[0] >= 0.0);
    }
}

// Hidden: run with `pencil2d_tests "[benchmark]"` or the `benchmark` build target.
TEST_CASE("Benchmark: open, scrub, play and draw on a 500-frame, 10-layer project", "[.benchmark]")
{
    BenchConfig cfg;
    std::vector<Scenario> sc = runBenchmark(cfg);
    printResults(cfg, sc);
}
