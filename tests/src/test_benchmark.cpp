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
#include "brushpresets.h"
#include "colormanager.h"
#include "dabtool.h"
#include "editor.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "stageprofiler.h"
#include "stroketool.h"
#include "testfixture.h"
#include "tiledbuffer.h"
#include "toolmanager.h"
#include "viewmanager.h"

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace
{

// Highest resident set seen by sampleRam(); 0 on platforms without a reading.
quint64 peakRamBytes = 0;

void sampleRam()
{
    quint64 rss = 0;
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        rss = pmc.WorkingSetSize;
#elif defined(Q_OS_LINUX)
    if (std::FILE* f = std::fopen("/proc/self/statm", "r"))
    {
        unsigned long size = 0, resident = 0;
        if (std::fscanf(f, "%lu %lu", &size, &resident) == 2)
            rss = quint64(resident) * 4096;
        std::fclose(f);
    }
#endif
    peakRamBytes = std::max(peakRamBytes, rss);
}

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

// Builds the project in memory (not timed) and saves it; saveMs gets the time of the save.
QString createProject(const QString& dir, const BenchConfig& cfg, double& saveMs)
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
    QElapsedTimer t;
    t.start();
    const Status st = fm.save(object.get(), path);
    saveMs = t.nsecsElapsed() / 1e6;
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
    std::vector<Scenario> sc = {
        { "save",         "file",   1 },
        { "open",         "file",   1 },
        { "scrub",        "frame",  cfg.frames },
        { "play",         "frame",  cfg.frames },
        { "draw pencil",  "stroke", cfg.strokes },
        { "draw brush",   "stroke", cfg.strokes },
        { "draw top",     "stroke", cfg.strokes },
        { "scrub onion",  "frame",  cfg.frames },
    };
    QString path;
    for (int rep = 0; rep < cfg.reps; ++rep)
    {
        double ms = 0;
        path = createProject(dir.path(), cfg, ms);
        sc[0].ms.push_back(ms);
    }

    for (int rep = 0; rep < cfg.reps; ++rep)
    {
        Session s;
        QElapsedTimer t;

        t.start();
        const Status st = s.editor->openObject(path, [](int) {}, [](int) {});
        sc[1].ms.push_back(t.nsecsElapsed() / 1e6);
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
            sampleRam();
        }
        sc[2].ms.push_back(t.nsecsElapsed() / 1e6);
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
            sampleRam();
        }
        sc[3].ms.push_back(t.nsecsElapsed() / 1e6);
        REQUIRE(s.counter.paints == cfg.frames);
        s.editor->playback()->stop();
        REQUIRE(s.editor->currentFrame() == cfg.frames + 1);

        t.start();
        drawStrokes(s, PENCIL, cfg.strokes);
        sc[4].ms.push_back(t.nsecsElapsed() / 1e6);
        auto* layer = static_cast<LayerBitmap*>(s.editor->layers()->currentLayer());
        REQUIRE(layer->getBitmapImageAtFrame(1)->bounds().width() > 500); // the strokes landed

        t.start();
        drawStrokes(s, BRUSH, cfg.strokes);
        sc[5].ms.push_back(t.nsecsElapsed() / 1e6);

        // Pencil on the top layer: every other bitmap layer is below it.
        s.editor->layers()->setCurrentLayer(cfg.layers);
        t.start();
        drawStrokes(s, PENCIL, cfg.strokes);
        sc[6].ms.push_back(t.nsecsElapsed() / 1e6);

        // Scrub again with tinted onion skin on both sides (default 5 frames each).
        const SettingsGuard guard({ SETTING_PREV_ONION, SETTING_NEXT_ONION, SETTING_ONION_RED, SETTING_ONION_BLUE });
        for (SETTING o : { SETTING::PREV_ONION, SETTING::NEXT_ONION, SETTING::ONION_RED, SETTING::ONION_BLUE })
            s.editor->preference()->set(o, true);
        s.area->onObjectLoaded();
        s.counter.paints = 0;
        t.start();
        for (int f = 1; f <= cfg.frames; ++f)
        {
            s.editor->scrubTo(f);
            s.area->repaint();
        }
        sc[7].ms.push_back(t.nsecsElapsed() / 1e6);
        REQUIRE(s.counter.paints == cfg.frames);
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
    std::printf("peak RAM during scrub and play: %.1f MB\n", peakRamBytes / (1024.0 * 1024.0));
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
// or with the Classic preset from presetFolder, on a fresh canvas: a blue layer below and a half-transparent yellow block on the painted layer
// (so the eraser has something to erase). Events carry no timestamp, so smoothing takes them as
// 1 ms apart and the result only depends on the input.
ReplayResult replay(const ReplayCase& c, const std::vector<Stroke>& strokes, const QString& presetFolder = {})
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
    if (!presetFolder.isEmpty())
        REQUIRE(BrushPresets(presetFolder).apply("Classic", *static_cast<StrokeTool*>(tool)));
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

// The look of Pencil, Brush and Eraser; B0 hashes are in git history.
// Change these only in a commit that means to change brush output, and say so.
const Golden kGolden[] = {
    // All platforms (CI-confirmed on winnt, linux, darwin), after even dab spacing (B1), 1-euro smoothing (B4), cached dab masks and coverage-only strokes (B2), 1-euro catch-up on pen lift, flow and opacity (B3).
    { "", "pencil", 4, "713fdc3434ba9484f246d80b89702eb1c2634322", "b25a85edd25798658b8a7dc4c065fe84dd5a2525" },
    { "", "pencil", 24, "9659fc79338e00e3ce9d24b3a192d69e521beb7f", "6dfafbb19e2fca40e27765f6de9e8405f15870cc" },
    { "", "pencil", 96, "dd3c52d9adac7516444c7e2fb0adc7ec6ac7618a", "1dabe840fc3c19c4f1ab0b70aafd9626fb5ebd33" },
    { "", "pencil", 200, "47f58eeb57648e798323ce792d9f066f63656c8c", "7e2552d7087ba8dccc35dad7e0abdde13f3c2ae9" },
    { "", "brush", 4, "628936218a10f642d977063d9bf3d44f9a5df567", "d002196d458ebe9e96f90df3ff876ba1086c7c5d" },
    { "", "brush", 24, "89b011c71012f4c90ccfbf53a560f2ca5ca4cf38", "428759e78c0d74c04aea6440704cdd0b3cfbe2f2" },
    { "", "brush", 96, "335d45b13e2cf20961323bf240c9e8cf601dcfb4", "8f68afbad6cd227d4d5eb48ba49a511f52a339c4" },
    { "", "brush", 200, "bdcf96926b2d1add49ef36a73de1df3b6effc7e1", "817c30c2efe4f05906e590a3e7a6bd6a3815a487" },
    { "", "eraser", 4, "825f29b9d899ec6be9c742d5b20ab705d4411e7a", "e6c0bdb403bcc6d69a69c0ff040f79bfbfb6eeab" },
    { "", "eraser", 24, "2eb5d32d53bdd057fc48c586ca109325b1223e41", "d6189f4b7d07d74532235c06742a2e509420faf7" },
    { "", "eraser", 96, "7a7f3c09b90828603a4cfcdfaa1f3b00aa8e9a44", "678910db446768ae1dcf7a2c28365b483df35928" },
    { "", "eraser", 200, "578233e5f4786b044186df2f7b1f0347018c5db2", "75dbd2674f0b43f1a46f9b83c3a75d2610059678" },
};

// Pencil, Brush and Eraser at their defaults on master before v0.3 (git 042a65c); the Classic
// preset must keep drawing exactly this. These rows matched on winnt, linux and darwin.
const Golden kClassicGolden[] = {
    { "", "pencil", 4, "70e0944a3098804b8acead5d0d6cfb3c8099cad2", "c9660a81966d70e76615ef094ad6a48078e4c1c0" },
    { "", "pencil", 24, "4c2cee44f4d356e5d9625b0ca68c10c213156dc1", "5866b97d3919072dafaea551411b48dfd7d36df8" },
    { "", "pencil", 96, "c9e416bc6d2810adb19b98ff9bfbb99ae7032ea9", "a797264ccade2bb6503b11b8149c6a958e6c8860" },
    { "", "pencil", 200, "b1d8db552adac8a0e58a0e288c500aba848b1868", "e082b6fe6f36108b89b93171a5afea5f654249be" },
    { "", "brush", 4, "f3b5888ffdf1700367ed51862bb23d54ea87fc15", "2fa829005dd1999bc697278016beb56e84589224" },
    { "", "brush", 24, "d568cb5847668c80f99e20e05c01da6eb50d2f5e", "6bbab1562be890b1037577d093afc966d4b07f3c" },
    { "", "brush", 96, "3fbe9bcae98fc4ed0e2084599a27b78354aeed37", "c632ee22c20ced7de612ed941bde749b7c568028" },
    { "", "brush", 200, "e75f6cde6bcca74a7383b00284225ebd67957a4a", "198bddb5c44b3eca0b9ae91a8fec5c5b7b80777e" },
    { "", "eraser", 4, "cfd6c0efae85efd99c789b25db505567c29b76c7", "3acba7710c67ce85e9f8864f441979b186e3ccd5" },
    { "", "eraser", 24, "6c0492b047714ddd69a8345a3789b91aca77795b", "702c29f563e46162a07a1521ac1885a998438cd5" },
    { "", "eraser", 96, "5d979832b8865cb689329bc4df90273b23be0a61", "0d26739015c54c352be1a52bf493964e217edb46" },
    { "", "eraser", 200, "abb0f5275ea5b65d82c6be40ce10fb713ca3e547", "8de24c2284f4e574eafcc25ca871b353e038e696" },
};

// Replays every case and checks it against the table's rows for this platform ("" = any platform).
template <size_t N>
void checkGolden(const Golden (&table)[N], const QString& presetFolder)
{
    const QString platform = replayPlatform();
    const std::vector<Stroke> strokes = goldenStrokes();
    std::vector<ReplayResult> results;
    QString lines; // ready to paste into the table
    for (const ReplayCase& c : kReplayCases)
    {
        results.push_back(replay(c, strokes, presetFolder));
        lines += QString("    { \"%1\", \"%2\", %3, \"%4\", \"%5\" },\n")
                     .arg(platform, c.name).arg(c.size).arg(results.back().layerSha1, results.back().frameSha1);
    }
    INFO("Hashes on this platform:\n" << lines.toStdString());

    int checked = 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        const ReplayCase& c = kReplayCases[i];
        for (const Golden& g : table)
        {
            if ((*g.platform && platform != g.platform) || qstrcmp(c.name, g.tool) != 0 || c.size != g.size) continue;
            CAPTURE(c.name, c.size);
            CHECK(results[i].layerSha1 == g.layerSha1);
            CHECK(results[i].frameSha1 == g.frameSha1);
            ++checked;
        }
    }
    if (checked == 0)
        WARN("No golden brush hashes for " << platform.toStdString() << " yet; add to the table:\n" << lines.toStdString());
    else
        REQUIRE(checked == static_cast<int>(results.size()));
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

    REQUIRE(sc.size() == 8);
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
    checkGolden(kGolden, {});
}

TEST_CASE("Brush replay: the Classic preset draws like Pencil, Brush and Eraser before v0.3", "[BrushReplay]")
{
    QTemporaryDir dir;
    REQUIRE(BrushPresets(dir.path()).addClassic());
    checkGolden(kClassicGolden, dir.path());
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

namespace
{

struct PredictionRun
{
    QImage midStroke; // the canvas while the pen is down
    QImage afterRelease;
    QByteArray layerSha1;
};

// One mouse stroke to the right with the given tool, prediction on or off.
PredictionRun predictionRun(ToolType toolType, bool predict)
{
    Session s;
    s.editor->layers()->setCurrentLayer(1);
    s.editor->color()->setFrontColor(QColor(200, 30, 90));
    s.editor->tools()->setCurrentTool(toolType);
    BaseTool* tool = s.editor->tools()->currentTool();
    const ToolSettingsGuard guard(tool);
    tool->resetSettings();
    auto* dabTool = static_cast<DabTool*>(tool);
    dabTool->setWidth(24);
    dabTool->setPredictionEnabled(predict);

    PredictionRun r;
    mouse(s.area, QEvent::MouseButtonPress, QPointF(300, 360), Qt::LeftButton);
    for (int x = 310; x <= 600; x += 10)
    {
        mouse(s.area, QEvent::MouseMove, QPointF(x, 360), Qt::LeftButton);
        QApplication::processEvents();
    }
    r.midStroke = s.area->grab().toImage();
    mouse(s.area, QEvent::MouseButtonRelease, QPointF(600, 360), Qt::NoButton);
    QApplication::processEvents();
    r.afterRelease = s.area->grab().toImage();
    r.layerSha1 = layerHash(static_cast<LayerBitmap*>(s.editor->layers()->currentLayer())->getBitmapImageAtFrame(1));
    dabTool->setPredictionEnabled(true);
    return r;
}

} // namespace

TEST_CASE("Pen prediction: the preview runs ahead of the pen and never reaches the layer", "[PenPrediction]")
{
    for (ToolType t : { PENCIL, BRUSH })
    {
        CAPTURE(t);
        const PredictionRun on = predictionRun(t, true);
        const PredictionRun off = predictionRun(t, false);
        const QColor background = off.midStroke.pixelColor(640, 600);

        // The stroke ends at x 600 with a 12 px radius, so x 625 is only painted by the prediction.
        CHECK(off.midStroke.pixelColor(625, 360) == background);
        CHECK(on.midStroke.pixelColor(625, 360) != background);
        CHECK(on.afterRelease.pixelColor(625, 360) == background);

        CHECK(on.layerSha1 == off.layerSha1);
        CHECK(on.afterRelease == off.afterRelease);
    }
}

// Hidden: input-to-pixel time for a 256 screen px brush on a 4K canvas: one tablet move, then the
// repaint it causes. Prints median and max ms per move. Use a Release build.
TEST_CASE("Pen prediction: input-to-pixel latency of a 256 px brush on a 4K canvas", "[.latency]")
{
    Session s;
    s.editor->layers()->setCurrentLayer(1);
    s.area->resize(3840, 2160);
    QApplication::processEvents();
    s.editor->tools()->setCurrentTool(BRUSH);
    BaseTool* tool = s.editor->tools()->currentTool();
    const ToolSettingsGuard guard(tool);
    tool->resetSettings();
    // Width tops out at 200 canvas px: zoom 128 % so the dab covers 256 screen px.
    static_cast<StrokeTool*>(tool)->setWidth(200);
    s.editor->view()->scale(1.28);
    REQUIRE(s.area->size() == QSize(3840, 2160));

    const QPointingDevice pen("latency pen", 2, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
                              QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    StageProfiler::reset();
    std::vector<double> ms;
    QElapsedTimer t;
    for (int stroke = 0; stroke < 5; ++stroke)
    {
        const qreal y = 400 + stroke * 300;
        tablet(s.area, &pen, QEvent::TabletPress, { QPointF(200, y), 0.8 }, Qt::LeftButton, Qt::LeftButton);
        QApplication::processEvents();
        for (int i = 1; i <= 200; ++i)
        {
            t.start();
            tablet(s.area, &pen, QEvent::TabletMove, { QPointF(200 + i * 17, y + (i % 20 - 10) * 4), 0.8 },
                   Qt::NoButton, Qt::LeftButton);
            QApplication::processEvents();
            ms.push_back(t.nsecsElapsed() / 1e6);
        }
        tablet(s.area, &pen, QEvent::TabletRelease, { QPointF(3600, y), 0.8 }, Qt::LeftButton, Qt::NoButton);
        QApplication::processEvents();
    }
    std::sort(ms.begin(), ms.end());
    std::printf("latency 256px brush 3840x2160: median %.2f ms, p95 %.2f ms, max %.2f ms over %zu moves\n",
                ms[ms.size() / 2], ms[ms.size() * 95 / 100], ms.back(), ms.size());
    if (StageProfiler::enabled())
        StageProfiler::report();
    std::fflush(stdout);
    CHECK(s.counter.paints > 0);
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

// Hidden: Brush dab throughput with pressure off, to compare dab engines. Width tops out at 200
// canvas px, so 128 % zoom makes the large case 256 screen px. Prints dabs/s per size. Use a Release build.
TEST_CASE("Brush replay: dabs per second at 256 and 4 screen px", "[.dabrate]")
{
    const std::vector<Stroke> strokes = seededStrokes(2, 4, 1500); // long strokes: dabs outweigh the paste per stroke
    const QPointingDevice pen("dabrate pen", 3, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
                              QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    const bool wasOn = StageProfiler::enabled();
    StageProfiler::enabled() = true; // counts the dabs
    for (int screenPx : { 256, 4 })
    {
        std::vector<double> rates;
        unsigned long long dabs = 0;
        for (int rep = 0; rep < 5; ++rep)
        {
            Session s;
            s.editor->layers()->setCurrentLayer(1);
            s.editor->tools()->setCurrentTool(BRUSH);
            BaseTool* tool = s.editor->tools()->currentTool();
            const ToolSettingsGuard guard(tool);
            tool->resetSettings();
            auto* strokeTool = static_cast<StrokeTool*>(tool);
            strokeTool->setWidth(screenPx / 1.28);
            strokeTool->setPressureEnabled(false);
            s.editor->view()->scale(1.28);
            StageProfiler::reset();
            QElapsedTimer t;
            t.start();
            for (const Stroke& stroke : strokes)
            {
                tablet(s.area, &pen, QEvent::TabletPress, stroke.front(), Qt::LeftButton, Qt::LeftButton);
                for (size_t i = 1; i < stroke.size(); ++i)
                    tablet(s.area, &pen, QEvent::TabletMove, stroke[i], Qt::NoButton, Qt::LeftButton);
                tablet(s.area, &pen, QEvent::TabletRelease, stroke.back(), Qt::LeftButton, Qt::NoButton);
            }
            const double sec = t.nsecsElapsed() / 1e9;
            dabs = StageProfiler::calls(StageProfiler::DabMask);
            rates.push_back(dabs / sec);
        }
        if (wasOn) StageProfiler::report(); // last repetition only
        std::sort(rates.begin(), rates.end());
        std::printf("dabrate %d px: median %.0f dabs/s (%.0f-%.0f) over %llu dabs\n", screenPx, rates[2], rates.front(),
                    rates.back(), dabs);
    }
    StageProfiler::enabled() = wasOn;
    StageProfiler::reset();
    std::fflush(stdout);
}

// Smudge dabs straight on a TiledBuffer over seeded noise; 800 px dabs are above the parallel threshold.
static void benchSmudgeDabs()
{
    QImage source(2048, 2048, QImage::Format_ARGB32_Premultiplied);
    std::mt19937 rng(7);
    for (int y = 0; y < source.height(); ++y)
    {
        QRgb* row = reinterpret_cast<QRgb*>(source.scanLine(y));
        for (int x = 0; x < source.width(); ++x)
        {
            const quint32 v = rng();
            const int a = int(v >> 24);
            row[x] = qRgba(int(v & 0xff) * a / 255, int((v >> 8) & 0xff) * a / 255, int((v >> 16) & 0xff) * a / 255, a);
        }
    }
    for (int size : { 300, 800 })
    {
        const int dabs = 200;
        std::vector<double> ms;
        for (int rep = 0; rep < 5; ++rep)
        {
            TiledBuffer buffer;
            buffer.setSmudgeSource(source, QPoint(0, 0));
            QElapsedTimer t;
            t.start();
            for (int i = 0; i < dabs; ++i)
                buffer.smudgeDab(QPointF(500 + i * 4.5, 1000 + (i % 7) * 3), size, 30, QColor(200, 40, 40), 0.8, true,
                                 BrushTip(), SmudgeSettings());
            ms.push_back(t.nsecsElapsed() / 1e6);
        }
        std::sort(ms.begin(), ms.end());
        std::printf("smudge %d px: median %.1f ms per %d dabs (%.1f-%.1f)\n", size, ms[2], dabs, ms.front(), ms.back());
    }
    std::fflush(stdout);
}

TEST_CASE("Brush replay: smudge dabs at 300 and 800 px", "[.smudgerate]")
{
    benchSmudgeDabs();
}
