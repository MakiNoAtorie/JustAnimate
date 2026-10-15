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
#include <random>
#include <vector>

#include <QApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QWindow>

#include "bitmapimage.h"
#include "colormanager.h"
#include "editor.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "scribblearea.h"
#include "stageprofiler.h"
#include "stroketool.h"
#include "testfixture.h"
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
    std::unique_ptr<Object> object(new Object);
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
    const Status st = fm.save(object.get(), path);
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
    std::unique_ptr<Editor> ownedEditor{ new Editor };
    std::unique_ptr<ScribbleArea> ownedArea{ new ScribbleArea(nullptr) }; // destroyed before the editor
    Editor* editor = ownedEditor.get();
    ScribbleArea* area = ownedArea.get();
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
    Q_DISABLE_COPY(Session)
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

// Brush stroke replay (ROADMAP B0): replays pen strokes and hashes the painted layer and the
// composited frame, so brush engine changes can prove they keep (or knowingly change) the pixels.

struct Sample
{
    QPointF pos; // widget coordinates on the 1280x720 canvas
    qreal pressure;
};
using Stroke = std::vector<Sample>;

struct ReplayCase
{
    ToolType tool;
    const char* name;
    int size;
};

struct ReplayResult
{
    double ms = 0;
    QByteArray layerSha1;
    QByteArray frameSha1;
};

// Random-walk strokes from a seed. Only mt19937's raw output and + - * / are used:
// std distributions and libm differ between platforms, so the strokes would too.
std::vector<Stroke> seededStrokes(unsigned seed, int strokes, int samples)
{
    std::mt19937 rng(seed);
    auto unit = [&rng] { return (rng() % 10001) / 10000.0; }; // [0, 1]
    std::vector<Stroke> out;
    for (int s = 0; s < strokes; ++s)
    {
        const qreal x0 = 240 + unit() * 800;
        const qreal y0 = 140 + unit() * 440;
        QPointF p(x0, y0);
        const qreal vx = unit() * 12 - 6;
        const qreal vy = unit() * 12 - 6;
        QPointF v(vx, vy);
        qreal pressure = 0.2 + unit() * 0.6;
        Stroke stroke;
        for (int i = 0; i < samples; ++i)
        {
            stroke.push_back({ p, pressure });
            const qreal ax = unit() * 2 - 1;
            const qreal ay = unit() * 2 - 1;
            v = QPointF(qBound(-6.0, v.x() + ax, 6.0), qBound(-6.0, v.y() + ay, 6.0));
            p = QPointF(qBound(20.0, p.x() + v.x(), 1260.0), qBound(20.0, p.y() + v.y(), 700.0));
            pressure = qBound(0.05, pressure + (unit() - 0.5) * 0.1, 1.0);
        }
        out.push_back(stroke);
    }
    return out;
}

QByteArray layerHash(BitmapImage* image)
{
    QCryptographicHash h(QCryptographicHash::Sha1);
    const QRect b = image->bounds();
    h.addData(QString("%1 %2 %3 %4;").arg(b.x()).arg(b.y()).arg(b.width()).arg(b.height()).toLatin1());
    const QImage* img = image->image();
    if (!img->isNull())
    {
        REQUIRE(img->format() == QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < img->height(); ++y)
            h.addData(QByteArrayView(reinterpret_cast<const char*>(img->constScanLine(y)), img->width() * 4));
    }
    return h.result().toHex();
}

QByteArray frameHash(const Object* object)
{
    QImage frame(1280, 720, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::transparent);
    QPainter painter(&frame);
    painter.translate(640, 360);
    object->paintImage(painter, 1, true, true);
    painter.end();
    return QCryptographicHash::hash(QByteArrayView(frame.constBits(), frame.sizeInBytes()),
                                    QCryptographicHash::Sha1).toHex();
}

void tablet(QWidget* w, const QPointingDevice* pen, QEvent::Type type, const Sample& s,
            Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QTabletEvent e(type, pen, s.pos, w->mapToGlobal(s.pos), s.pressure, 0, 0, 0, 0, 0,
                   Qt::NoModifier, button, buttons);
    QApplication::sendEvent(w, &e);
}

// Replays the strokes as tablet input with one tool at its default settings and the given size,
// on a fresh canvas: a blue layer below and a half-transparent yellow block on the painted layer
// (so the eraser has something to erase). Events are sent synchronously, no event loop runs, so
// the stabilizer's poll timer never fires and the result only depends on the input.
ReplayResult replay(const ReplayCase& c, const std::vector<Stroke>& strokes)
{
    Session s;
    s.editor->color()->setFrontColor(QColor(200, 30, 90));
    auto* below = static_cast<LayerBitmap*>(s.editor->object()->getLayer(1));
    below->getBitmapImageAtFrame(1)->drawRect(QRectF(-500, -250, 1000, 500), Qt::NoPen, QColor(90, 160, 220),
                                              QPainter::CompositionMode_SourceOver, false);
    LayerBitmap* painted = s.editor->layers()->createBitmapLayer("painted");
    painted->getBitmapImageAtFrame(1)->drawRect(QRectF(-300, -150, 600, 300), Qt::NoPen, QColor(240, 200, 40, 200),
                                                QPainter::CompositionMode_SourceOver, false);
    s.editor->scrubTo(1);

    s.editor->tools()->setCurrentTool(c.tool);
    BaseTool* tool = s.editor->tools()->currentTool();
    const ToolSettingsGuard guard(tool);
    tool->resetSettings();
    static_cast<StrokeTool*>(tool)->setWidth(c.size);

    const QPointingDevice pen("replay pen", 1, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
                              QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    ReplayResult r;
    QElapsedTimer t;
    t.start();
    for (const Stroke& stroke : strokes)
    {
        tablet(s.area, &pen, QEvent::TabletPress, stroke.front(), Qt::LeftButton, Qt::LeftButton);
        for (size_t i = 1; i < stroke.size(); ++i)
            tablet(s.area, &pen, QEvent::TabletMove, stroke[i], Qt::NoButton, Qt::LeftButton);
        tablet(s.area, &pen, QEvent::TabletRelease, stroke.back(), Qt::LeftButton, Qt::NoButton);
    }
    r.ms = t.nsecsElapsed() / 1e6;

    r.layerSha1 = layerHash(painted->getBitmapImageAtFrame(1));
    r.frameSha1 = frameHash(s.editor->object());
    return r;
}

const ReplayCase kReplayCases[] = {
    { PENCIL, "pencil", 4 }, { PENCIL, "pencil", 24 }, { PENCIL, "pencil", 96 }, { PENCIL, "pencil", 200 },
    { BRUSH, "brush", 4 },   { BRUSH, "brush", 24 },   { BRUSH, "brush", 96 },   { BRUSH, "brush", 200 },
    { ERASER, "eraser", 4 }, { ERASER, "eraser", 24 }, { ERASER, "eraser", 96 }, { ERASER, "eraser", 200 },
};

std::vector<Stroke> goldenStrokes() { return seededStrokes(1, 6, 150); }

// Qt's raster engine isn't bit-exact across Qt versions, CPUs (SSE vs NEON) and compilers,
// so golden hashes are kept per platform (OS + CPU); the test prints the line to add for a new one.
// If one platform differs between Qt versions, the mismatch shows the actual hash: key it by Qt version too.
QString replayPlatform()
{
    return QString("%1 %2").arg(QSysInfo::kernelType(), QSysInfo::buildCpuArchitecture());
}

struct Golden
{
    const char* platform;
    const char* tool;
    int size;
    const char* layerSha1;
    const char* frameSha1;
};

// The look of Pencil, Brush and Eraser before the brush engine work (ROADMAP B0).
// Change these only in a commit that means to change brush output, and say so.
const Golden kGolden[] = {
    // winnt: MSVC, Qt 6.8.3 and 6.5.3. linux and darwin: CI, Qt 6.5.3.
    { "winnt x86_64", "pencil", 4, "95bcfd1fb8741d96b9cbc6e1a5fe36e463630cd5", "0e5839770095d7d4f2dd5c835f644d1c357f47fc" },
    { "winnt x86_64", "pencil", 24, "c93201fdd6162d2e2c8f2e545a18d94037f73060", "3c3623bb137089d3b7ddea42324d17575b949573" },
    { "winnt x86_64", "pencil", 96, "da215f2b465cc8c11f9b201079613ed8cf21ad09", "bf0001e4484bcb37862331e33d15097bc057311e" },
    { "winnt x86_64", "pencil", 200, "b2621f11c3e6214dbd89014eafc12d22ac374589", "928eabfdfac9b0e3d9ce87e8e087350185e67b79" },
    { "winnt x86_64", "brush", 4, "08ac3b83dbf641ffeb5b645d6b79fb4dc332f8e1", "646b6145475d7df921aa824535dbc5524350a989" },
    { "winnt x86_64", "brush", 24, "ac77c286b9b6e9f907000f19a4327ec3ddbcc3e6", "93688a6d99a914987bf51ff7dcccdd8b27754938" },
    { "winnt x86_64", "brush", 96, "875e33735b958bec8be54804e56cf8ae5220f03a", "fa9935dab752949aa2b61497f10381791b613ccc" },
    { "winnt x86_64", "brush", 200, "0fa12608f628ee6bb7e05af77c82b9f2b3287ccb", "27cd857dc3e459c11ee8d134dd6ab88b20fac4cc" },
    { "winnt x86_64", "eraser", 4, "d7e2f2e5e32b6b22059c24951ac933943fd957b7", "88130c67007948c838b5a64bb13667fcb08a1f69" },
    { "winnt x86_64", "eraser", 24, "939fd7b9c3de6707ae6cc51dbefc981f02ec55fb", "efda53b3b9f3c2ca4108e32bb8bf97197a14e2c2" },
    { "winnt x86_64", "eraser", 96, "bdb36e3ad35666b8bbe227f06a13eb553237f726", "7772380fa4e0d4b5ee3fb2b5aab8694a3c35ac3c" },
    { "winnt x86_64", "eraser", 200, "516a81d7c6b1d424b1a5e6f9beb9074cdd54dec2", "3aca31806a5bda175678e38b2d0043338db1c7ce" },
    { "linux x86_64", "pencil", 4, "95bcfd1fb8741d96b9cbc6e1a5fe36e463630cd5", "0e5839770095d7d4f2dd5c835f644d1c357f47fc" },
    { "linux x86_64", "pencil", 24, "c93201fdd6162d2e2c8f2e545a18d94037f73060", "3c3623bb137089d3b7ddea42324d17575b949573" },
    { "linux x86_64", "pencil", 96, "da215f2b465cc8c11f9b201079613ed8cf21ad09", "bf0001e4484bcb37862331e33d15097bc057311e" },
    { "linux x86_64", "pencil", 200, "b2621f11c3e6214dbd89014eafc12d22ac374589", "928eabfdfac9b0e3d9ce87e8e087350185e67b79" },
    { "linux x86_64", "brush", 4, "08ac3b83dbf641ffeb5b645d6b79fb4dc332f8e1", "646b6145475d7df921aa824535dbc5524350a989" },
    { "linux x86_64", "brush", 24, "ac77c286b9b6e9f907000f19a4327ec3ddbcc3e6", "93688a6d99a914987bf51ff7dcccdd8b27754938" },
    { "linux x86_64", "brush", 96, "875e33735b958bec8be54804e56cf8ae5220f03a", "fa9935dab752949aa2b61497f10381791b613ccc" },
    { "linux x86_64", "brush", 200, "0fa12608f628ee6bb7e05af77c82b9f2b3287ccb", "27cd857dc3e459c11ee8d134dd6ab88b20fac4cc" },
    { "linux x86_64", "eraser", 4, "d7e2f2e5e32b6b22059c24951ac933943fd957b7", "88130c67007948c838b5a64bb13667fcb08a1f69" },
    { "linux x86_64", "eraser", 24, "939fd7b9c3de6707ae6cc51dbefc981f02ec55fb", "efda53b3b9f3c2ca4108e32bb8bf97197a14e2c2" },
    { "linux x86_64", "eraser", 96, "bdb36e3ad35666b8bbe227f06a13eb553237f726", "7772380fa4e0d4b5ee3fb2b5aab8694a3c35ac3c" },
    { "linux x86_64", "eraser", 200, "516a81d7c6b1d424b1a5e6f9beb9074cdd54dec2", "3aca31806a5bda175678e38b2d0043338db1c7ce" },
    { "darwin arm64", "pencil", 4, "5000a2e5b14e7dbc08aa011dc157de2bf7929cae", "ae3fa4fe54391a65474202d40c6179662044e735" },
    { "darwin arm64", "pencil", 24, "3f9e24bb45c76fbe381aa547dc22553e4a016c12", "ee398f6ad88318aaf85b35b5bcc03883601e4564" },
    { "darwin arm64", "pencil", 96, "133828f4ae764ccd6532d1a7dded26147d2c86f8", "48657693ffed51c954aed70ee84b8efef8e89bd6" },
    { "darwin arm64", "pencil", 200, "763ca6ddca698fdfc128b5751f867e26310a0d3a", "fa59cd26a8df95b6d1d6100cd79c00439468c31e" },
    { "darwin arm64", "brush", 4, "6ba6764ce248bdaffc996826ed38461ece651182", "81b1e88689e4fe30fd68c01919b95d732cb2421f" },
    { "darwin arm64", "brush", 24, "745acceb3f282859c158f324584aa802aae3447c", "169bd9f89abad3f6313c8d30ca7fcd39ad4d85f0" },
    { "darwin arm64", "brush", 96, "754cc8fc17664d12718051dc5bd44902f0ad05f5", "0d9ca4ac77b726634b33da2bc7bbb989787b9e37" },
    { "darwin arm64", "brush", 200, "dfb745b451f8e9b93f47e96bb3b907e6cd0270e4", "c384456c5bb527b01888ef738f8fb3a9e8739a2d" },
    { "darwin arm64", "eraser", 4, "c059930a509377a716ed948c0ca35523fd6ff8e2", "4d4f6a380cbd0335356e91de2071ef1de48d69c6" },
    { "darwin arm64", "eraser", 24, "968522fda805d0c1805c5a7157914a6f2ae4fb2c", "6e09d76e867c69c92d60f6d72b2f38acdc2e1385" },
    { "darwin arm64", "eraser", 96, "d4543c7fb4b70a8f91cef3ee0c7ed0f528e8752c", "333353a675795c9bc0ec968faa4fdf013561dc1e" },
    { "darwin arm64", "eraser", 200, "f3512e3a650f8a8d4b3ea565731afa3a3e839b56", "1f52f221dffe72a5e0e5d65df20889433f3fe406" },
};

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

TEST_CASE("Brush replay: replaying the same strokes twice gives identical hashes", "[BrushReplay]")
{
    const std::vector<Stroke> strokes = seededStrokes(1, 2, 40);
    for (const ReplayCase& c : { ReplayCase{ PENCIL, "pencil", 24 }, ReplayCase{ BRUSH, "brush", 24 },
                                 ReplayCase{ ERASER, "eraser", 24 } })
    {
        CAPTURE(c.name);
        const ReplayResult a = replay(c, strokes);
        const ReplayResult b = replay(c, strokes);
        REQUIRE(a.layerSha1 == b.layerSha1);
        REQUIRE(a.frameSha1 == b.frameSha1);
        REQUIRE(a.layerSha1 != replay(c, {}).layerSha1); // the strokes changed the layer
    }
}

TEST_CASE("Brush replay: today's Pencil, Brush and Eraser match the golden hashes", "[BrushReplay]")
{
    const QString platform = replayPlatform();
    const std::vector<Stroke> strokes = goldenStrokes();
    std::vector<ReplayResult> results;
    QString lines; // ready to paste into kGolden
    for (const ReplayCase& c : kReplayCases)
    {
        results.push_back(replay(c, strokes));
        lines += QString("    { \"%1\", \"%2\", %3, \"%4\", \"%5\" },\n")
                     .arg(platform, c.name).arg(c.size).arg(results.back().layerSha1, results.back().frameSha1);
    }
    INFO("Hashes on this platform:\n" << lines.toStdString());

    int checked = 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        const ReplayCase& c = kReplayCases[i];
        for (const Golden& g : kGolden)
        {
            if (platform != g.platform || qstrcmp(c.name, g.tool) != 0 || c.size != g.size) continue;
            CAPTURE(c.name, c.size);
            CHECK(results[i].layerSha1 == g.layerSha1);
            CHECK(results[i].frameSha1 == g.frameSha1);
            ++checked;
        }
    }
    if (checked == 0)
        WARN("No golden brush hashes for " << platform.toStdString() << " yet; add to kGolden:\n" << lines.toStdString());
    else
        REQUIRE(checked == static_cast<int>(results.size()));
}

TEST_CASE("Stage profiler: counts the paint stages and leaves the painted pixels unchanged", "[BrushReplay]")
{
    const std::vector<Stroke> strokes = goldenStrokes();
    const ReplayCase c{ PENCIL, "pencil", 24 };
    const bool wasOn = StageProfiler::enabled();

    StageProfiler::enabled() = false;
    StageProfiler::reset();
    const ReplayResult off = replay(c, strokes);
    REQUIRE(StageProfiler::calls(StageProfiler::InputEvent) == 0);

    StageProfiler::enabled() = true;
    const ReplayResult on = replay(c, strokes);
    StageProfiler::enabled() = wasOn;

    CHECK(StageProfiler::calls(StageProfiler::InputEvent) > 0);
    CHECK(StageProfiler::calls(StageProfiler::DabMask) > 0);
    CHECK(StageProfiler::calls(StageProfiler::BufferPaste) > 0);
    CHECK(on.layerSha1 == off.layerSha1);
    CHECK(on.frameSha1 == off.frameSha1);
    StageProfiler::reset();
}

// Hidden: timing run for `compare_replay.py`. Prints one line per tool and size:
// "replay <tool> <size> <median ms> <layer sha1> <frame sha1>". Use a Release build.
TEST_CASE("Brush replay: time every tool and size, print hashes", "[.replay]")
{
    const std::vector<Stroke> strokes = goldenStrokes();
    for (const ReplayCase& c : kReplayCases)
    {
        std::vector<double> ms;
        ReplayResult r;
        for (int rep = 0; rep < 3; ++rep)
        {
            StageProfiler::reset();
            r = replay(c, strokes);
            ms.push_back(r.ms);
        }
        std::sort(ms.begin(), ms.end());
        std::printf("replay %s %d %.1f %s %s\n", c.name, c.size, ms[1], r.layerSha1.constData(), r.frameSha1.constData());
        if (StageProfiler::enabled())
            StageProfiler::report(); // last repetition only
    }
    std::fflush(stdout);
}
