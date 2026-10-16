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

#include <memory>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "brushpresets.h"
#include "brushtool.h"
#include "miniz.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "tiledbuffer.h"
#include "toolmanager.h"

namespace
{

void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

// The Brush tool of a started editor; its settings and tip are put back at scope exit.
struct BrushSession
{
    TestSession s;
    BrushTool* brush = nullptr;
    std::unique_ptr<ToolSettingsGuard> guard;

    BrushSession()
    {
        s.start();
        brush = static_cast<BrushTool*>(s.editor->tools()->getTool(BRUSH));
        guard = std::make_unique<ToolSettingsGuard>(brush);
    }
    ~BrushSession()
    {
        brush->setTipImage(QString());
        brush->setPressureCurve(QString());
    }
};

// Writes a zip with the given entry names and contents.
void writeZip(const QString& path, const QList<QPair<QByteArray, QByteArray>>& entries)
{
    mz_zip_archive mz;
    mz_zip_zero_struct(&mz);
    REQUIRE(mz_zip_writer_init_file(&mz, path.toUtf8().constData(), 0));
    for (const auto& e : entries)
        REQUIRE(mz_zip_writer_add_mem(&mz, e.first.constData(), e.second.constData(), size_t(e.second.size()), MZ_BEST_SPEED));
    REQUIRE(mz_zip_writer_finalize_archive(&mz));
    REQUIRE(mz_zip_writer_end(&mz));
}

}

TEST_CASE("Brush presets survive a restart", "[BrushPresets]")
{
    QTemporaryDir dir;
    QImage tip(4, 4, QImage::Format_RGB32);
    tip.fill(Qt::black);
    REQUIRE(tip.save(dir.filePath("tip.png")));
    BrushSession b;
    b.brush->setWidth(50);
    b.brush->setSmoothingMode(static_cast<int>(SmoothingMode::None));
    b.brush->setWashEnabled(true);
    b.brush->setFlow(30);
    REQUIRE(b.brush->setTipImage(dir.filePath("tip.png")).ok());
    REQUIRE(BrushPresets(dir.path()).save("Ink", *b.brush));

    b.brush->resetSettings();
    b.brush->setTipImage(QString());
    BrushPresets presets(dir.path()); // a new store reads the folder as after a restart
    REQUIRE(presets.names() == QStringList{ "Ink" });
    REQUIRE(presets.apply("Ink", *b.brush));
    const StrokeToolProperties& p = b.brush->strokeToolProperties();
    CHECK(p.width() == 50.0);
    CHECK(p.smoothingMode() == SmoothingMode::None);
    CHECK(p.washEnabled());
    CHECK(p.flow() == 30.0);
    CHECK(b.brush->tipImagePath() == dir.filePath("tip.png"));

    // A tip file that is gone gives the generated tip.
    REQUIRE(QFile::remove(dir.filePath("tip.png")));
    REQUIRE(presets.apply("Ink", *b.brush));
    CHECK(b.brush->tipImagePath().isEmpty());
    CHECK(p.width() == 50.0);
}

TEST_CASE("Brush presets keep dynamics and scatter", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushSession b;
    b.brush->setDynamic(StrokeToolProperties::SCATTER_VALUE, 250);
    b.brush->setDynamic(StrokeToolProperties::SIZE_JITTER_VALUE, 40);
    b.brush->setDynamic(StrokeToolProperties::TAPER_START_VALUE, 80);
    b.brush->setDynamic(StrokeToolProperties::TAPER_END_VALUE, 120);
    REQUIRE(BrushPresets(dir.path()).save("Chalk", *b.brush));

    b.brush->resetSettings();
    REQUIRE(b.brush->strokeToolProperties().scatter() == 0);
    REQUIRE(b.brush->strokeToolProperties().taperStart() == 0);
    REQUIRE(BrushPresets(dir.path()).apply("Chalk", *b.brush));
    CHECK(b.brush->strokeToolProperties().scatter() == 250);
    CHECK(b.brush->strokeToolProperties().sizeJitter() == 40);
    CHECK(b.brush->strokeToolProperties().taperStart() == 80);
    CHECK(b.brush->strokeToolProperties().taperEnd() == 120);
}

TEST_CASE("Brush presets can be renamed and deleted", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushSession b;
    BrushPresets presets(dir.path());
    REQUIRE(presets.save("Ink", *b.brush));
    REQUIRE(presets.save("Wash", *b.brush));

    REQUIRE(presets.rename("Ink", "Pen"));
    REQUIRE_FALSE(presets.rename("Pen", "Wash")); // never overwrites another preset
    REQUIRE(BrushPresets(dir.path()).names() == QStringList{ "Pen", "Wash" });

    REQUIRE(presets.remove("Pen"));
    REQUIRE(BrushPresets(dir.path()).names() == QStringList{ "Wash" });
}

TEST_CASE("Deleting a preset deletes its imported tip and import never overwrites a tip", "[BrushPresets]")
{
    QTemporaryDir dir;
    QImage tip(4, 4, QImage::Format_RGB32);
    tip.fill(Qt::black);
    REQUIRE(tip.save(dir.filePath("tip.png")));
    BrushSession b;
    REQUIRE(b.brush->setTipImage(dir.filePath("tip.png")).ok());
    const BrushPresets source(dir.filePath("source"));
    REQUIRE(source.save("Ink", *b.brush));
    REQUIRE(source.exportPreset("Ink", dir.filePath("Ink.jabrush")).ok());

    const BrushPresets presets(dir.filePath("presets"));
    const QDir tips(dir.filePath("presets/tips"));
    REQUIRE(presets.importPreset(dir.filePath("Ink.jabrush")).ok());
    REQUIRE(presets.rename("Ink", "Pen"));
    REQUIRE(presets.importPreset(dir.filePath("Ink.jabrush")).ok());
    CHECK(tips.entryList(QDir::Files).size() == 2);
    REQUIRE(presets.apply("Ink", *b.brush));
    const QString inkTip = b.brush->tipImagePath();
    REQUIRE(presets.apply("Pen", *b.brush));
    const QString penTip = b.brush->tipImagePath();
    CHECK(inkTip != penTip);

    // A tip stays while another preset uses it.
    REQUIRE(presets.save("Pen copy", *b.brush));
    REQUIRE(presets.remove("Pen"));
    CHECK(QFile::exists(penTip));
    REQUIRE(presets.remove("Pen copy"));
    CHECK_FALSE(QFile::exists(penTip));
    REQUIRE(presets.remove("Ink"));
    CHECK(tips.entryList(QDir::Files).isEmpty());

    // A tip outside the tips folder is the user's file.
    REQUIRE(source.remove("Ink"));
    CHECK(QFile::exists(dir.filePath("tip.png")));
}

TEST_CASE("Brush preset names cannot leave the preset folder", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushSession b;
    BrushPresets presets(dir.path());
    for (const QString& bad : { "", " ", "../x", "a/b", "a\\b", ".hidden", "c:x" })
        REQUIRE_FALSE(presets.save(bad, *b.brush));
    REQUIRE(presets.save("Ink", *b.brush));
    REQUIRE_FALSE(presets.rename("Ink", "../Ink"));
    REQUIRE(presets.names() == QStringList{ "Ink" });
}

TEST_CASE("Corrupt brush preset files are skipped", "[BrushPresets]")
{
    QTemporaryDir dir;
    writeFile(dir.filePath("Broken.json"), "{ not json");
    writeFile(dir.filePath("List.json"), "[1, 2]");
    writeFile(dir.filePath("Good.json"), "{}");
    BrushPresets presets(dir.path());
    REQUIRE(presets.names() == QStringList{ "Good" });

    BrushSession b;
    b.brush->setWidth(70);
    REQUIRE_FALSE(presets.apply("Broken", *b.brush));
    REQUIRE_FALSE(presets.apply("Missing", *b.brush));
    CHECK(b.brush->strokeToolProperties().width() == 70.0); // a failed apply changes nothing
}

TEST_CASE("Bad brush preset values fall back to defaults", "[BrushPresets]")
{
    QTemporaryDir dir;
    writeFile(dir.filePath("Odd.json"),
              R"({ "settings": { "Width": "big", "Opacity": 5000, "SmoothingMode": 1e300,
                                 "WashEnabled": 1, "NoSuchSetting": true },
                   "tipImage": 7 })");
    BrushSession b;
    b.brush->setWidth(70);
    b.brush->setWashEnabled(true);

    REQUIRE(BrushPresets(dir.path()).apply("Odd", *b.brush));
    const StrokeToolProperties& p = b.brush->strokeToolProperties();
    CHECK(p.width() == 24.0);    // Brush default
    CHECK(p.opacity() == 100.0); // clamped to the maximum
    CHECK(p.smoothingMode() == SmoothingMode::Stabilizer); // clamped to the last mode
    CHECK_FALSE(p.washEnabled());
    CHECK(b.brush->tipImagePath().isEmpty());
}

namespace
{

const QStringList DEFAULT_PRESETS{ "2B Pencil", "Chalk", "G-Pen", "HB Pencil", "Hard Eraser", "Marker", "Soft Airbrush", "Soft Eraser", "Watercolour" };

void tabletStroke(ScribbleArea* area, qreal y)
{
    auto send = [&](QEvent::Type type, qreal x) {
        const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
        const QPointF pos(x, y);
        QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                       0.8, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
        QApplication::sendEvent(area, &e);
    };
    send(QEvent::TabletPress, 100);
    for (int i = 1; i <= 40; ++i)
        send(QEvent::TabletMove, 100 + 10 * i);
    send(QEvent::TabletRelease, 500);
}

long long inkedPixels(BitmapImage* bitmap)
{
    const QImage* img = bitmap->image();
    long long n = 0;
    for (int y = 0; y < img->height(); ++y)
        for (int x = 0; x < img->width(); ++x)
            n += qAlpha(img->pixel(x, y));
    return n;
}

}

TEST_CASE("Default brush pack is added and keeps user edits", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushPresets presets(dir.path());
    presets.addDefaults();
    QStringList installed = presets.names();
    installed.sort();
    REQUIRE(installed == DEFAULT_PRESETS);

    // An edited default stays as the user left it.
    writeFile(dir.filePath("Marker.json"), R"({"settings":{"Width":77}})");
    presets.addDefaults();
    QFile marker(dir.filePath("Marker.json"));
    REQUIRE(marker.open(QIODevice::ReadOnly));
    CHECK(marker.readAll().contains("77"));
}

TEST_CASE("A deleted built-in preset stays deleted after a restart", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushPresets presets(dir.path());
    presets.addDefaults();
    REQUIRE(presets.remove("Marker"));
    BrushPresets(dir.path()).addDefaults(); // the next start
    CHECK_FALSE(presets.names().contains("Marker"));
    CHECK(presets.names().contains("Chalk"));

    // A built-in that was never added is added, once.
    REQUIRE(presets.addClassic());
    CHECK(presets.names().contains("Classic"));
    REQUIRE(presets.remove("Classic"));
    REQUIRE(presets.addClassic());
    CHECK_FALSE(presets.names().contains("Classic"));
}

TEST_CASE("Every default brush preset paints a stroke", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushPresets presets(dir.path());
    presets.addDefaults();

    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    for (int f = 2; f <= 20; ++f)
        layer->addNewKeyFrameAt(f);
    s.start();
    s.area->init();
    s.area->resize(800, 600);
    s.editor->layers()->setCurrentLayer(layer);

    std::vector<std::unique_ptr<ToolSettingsGuard>> guards;
    for (ToolType t : { PENCIL, BRUSH, ERASER })
        guards.push_back(std::make_unique<ToolSettingsGuard>(s.editor->tools()->getTool(t)));

    int frame = 1;
    for (const QString& name : DEFAULT_PRESETS)
    {
        INFO(name.toStdString());
        const bool eraser = name.contains("Eraser");
        const ToolType type = eraser ? ERASER : (name.contains("Pencil") ? PENCIL : BRUSH);
        StrokeTool* tool = static_cast<StrokeTool*>(s.editor->tools()->getTool(type));

        s.editor->scrubTo(frame);
        if (eraser)
        {
            // Ink to erase.
            s.editor->tools()->setCurrentTool(PENCIL);
            static_cast<StrokeTool*>(s.editor->tools()->getTool(PENCIL))->setWidth(60);
            tabletStroke(s.area.get(), 300);
        }
        const long long before = inkedPixels(layer->getBitmapImageAtFrame(frame));
        REQUIRE(presets.apply(name, *tool));
        s.editor->tools()->setCurrentTool(type);
        tabletStroke(s.area.get(), 300);
        const long long after = inkedPixels(layer->getBitmapImageAtFrame(frame));
        if (eraser)
            CHECK(after < before);
        else
            CHECK(after > 0);
        ++frame;
    }
}

TEST_CASE("Brush presets export and import as jabrush files", "[BrushPresets]")
{
    QTemporaryDir from, to, tips;
    QImage tip(4, 4, QImage::Format_RGB32);
    tip.fill(Qt::black);
    REQUIRE(tip.save(tips.filePath("tip.png")));
    BrushSession b;
    b.brush->setWidth(50);
    b.brush->setFlow(30);
    REQUIRE(b.brush->setTipImage(tips.filePath("tip.png")).ok());
    REQUIRE(BrushPresets(from.path()).save("Ink", *b.brush));
    REQUIRE(BrushPresets(from.path()).exportPreset("Ink", to.filePath("Ink.jabrush")).ok());

    // The archive carries the tip, so the original tip file is not needed on import.
    REQUIRE(QFile::remove(tips.filePath("tip.png")));
    BrushPresets other(to.filePath("presets"));
    REQUIRE(other.importPreset(to.filePath("Ink.jabrush")).ok());
    REQUIRE(other.names() == QStringList{ "Ink" });
    REQUIRE_FALSE(other.importPreset(to.filePath("Ink.jabrush")).ok()); // never overwrites a preset

    b.brush->resetSettings();
    b.brush->setTipImage(QString());
    REQUIRE(other.apply("Ink", *b.brush));
    CHECK(b.brush->strokeToolProperties().width() == 50.0);
    CHECK(b.brush->strokeToolProperties().flow() == 30.0);
    CHECK(b.brush->tipImagePath().startsWith(other.folder()));
    QImage mask;
    REQUIRE(loadBrushTipImage(b.brush->tipImagePath(), mask).ok());
    CHECK(mask.size() == QSize(4, 4));
}

TEST_CASE("A preset keeps its description and pressure curve", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushSession b;
    const BrushPresets presets(dir.filePath("presets"));
    b.brush->setPressureCurve("0,0.9 1,1");
    REQUIRE(presets.save("Firm", *b.brush, "Bold lines"));
    b.brush->setPressureCurve(QString());
    REQUIRE(presets.save("Plain", *b.brush));

    REQUIRE(presets.apply("Firm", *b.brush));
    CHECK(b.brush->pressureCurve() == "0,0.9 1,1");
    CHECK(presets.description("Firm") == "Bold lines");
    REQUIRE(presets.apply("Plain", *b.brush));
    CHECK(b.brush->pressureCurve().isEmpty()); // empty = the global curve
    CHECK(presets.description("Plain").isEmpty());

    // Both travel in a .jabrush.
    REQUIRE(presets.exportPreset("Firm", dir.filePath("Firm.jabrush")).ok());
    const BrushPresets other(dir.filePath("other"));
    REQUIRE(other.importPreset(dir.filePath("Firm.jabrush")).ok());
    REQUIRE(other.apply("Firm", *b.brush));
    CHECK(b.brush->pressureCurve() == "0,0.9 1,1");
    CHECK(other.description("Firm") == "Bold lines");
}

TEST_CASE("A preset records the tool it is for", "[BrushPresets]")
{
    QTemporaryDir dir;
    BrushSession b;
    const BrushPresets presets(dir.filePath("presets"));
    StrokeTool* eraser = static_cast<StrokeTool*>(b.s.editor->tools()->getTool(ERASER));
    ToolSettingsGuard eraserGuard(eraser);
    REQUIRE(presets.save("Rub", *eraser));
    REQUIRE(presets.save("Ink", *b.brush));
    REQUIRE(QDir().mkpath(presets.folder()));
    writeFile(presets.filePath("Old"), R"({"settings":{"Width":5}})"); // before the tool field

    CHECK(presets.tool("Rub") == "eraser");
    CHECK(presets.tool("Ink") == "brush");
    CHECK(presets.tool("Old") == "brush");
    CHECK(presets.names("eraser") == QStringList{ "Rub" });
    CHECK(presets.names("brush") == QStringList{ "Ink", "Old" });
    CHECK(presets.names().size() == 3);
    CHECK(BrushPresets::toolKey(ERASER) == "eraser");
    CHECK(BrushPresets::toolKey(PENCIL) == "brush");
    CHECK(BrushPresets::toolKey(PEN) == "brush");

    // The tool travels in a .jabrush.
    REQUIRE(presets.exportPreset("Rub", dir.filePath("Rub.jabrush")).ok());
    const BrushPresets other(dir.filePath("other"));
    REQUIRE(other.importPreset(dir.filePath("Rub.jabrush")).ok());
    CHECK(other.tool("Rub") == "eraser");

    // The defaults include a hard and a soft eraser, for the eraser only.
    const BrushPresets defaults(dir.filePath("defaults"));
    defaults.addDefaults();
    CHECK(defaults.names("eraser") == QStringList{ "Hard Eraser", "Soft Eraser" });
    CHECK_FALSE(defaults.names("brush").contains("Hard Eraser"));
}

TEST_CASE("Malformed jabrush files are rejected before writing", "[BrushPresets]")
{
    QTemporaryDir dir;
    const QByteArray preset = R"({ "name": "Ink", "settings": {}, "tipImage": "" })";
    const QString file = dir.filePath("bad.jabrush");
    const QString folder = dir.filePath("presets");

    SECTION("not a zip") { writeFile(file, "PK not really"); }
    SECTION("path escape") { writeZip(file, { { "preset.json", preset }, { "../evil.png", "x" } }); }
    SECTION("unknown entry") { writeZip(file, { { "preset.json", preset }, { "notes.txt", "x" } }); }    SECTION("no preset") { writeZip(file, { { "tip.png", "x" } }); }
    SECTION("corrupt json") { writeZip(file, { { "preset.json", "{ nope" } }); }
    SECTION("bad name") { writeZip(file, { { "preset.json", R"({ "name": "../Ink", "settings": {} })" } }); }
    SECTION("bad tip")
    {
        writeZip(file, { { "preset.json", R"({ "name": "Ink", "settings": {}, "tipImage": "tip.png" })" },
                         { "tip.png", "not an image" } });
    }
    SECTION("missing tip") { writeZip(file, { { "preset.json", R"({ "name": "Ink", "tipImage": "tip.png" })" } }); }
    SECTION("oversized preset") { writeZip(file, { { "preset.json", QByteArray(2 << 20, ' ') } }); }

    const Status st = BrushPresets(folder).importPreset(file);
    CHECK_FALSE(st.ok());
    CHECK_FALSE(QDir(folder).exists());
    CHECK_FALSE(QFile::exists(dir.filePath("evil.png")));
}
