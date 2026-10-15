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

#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "brushpresets.h"
#include "brushtool.h"
#include "editor.h"
#include "testfixture.h"
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
    ~BrushSession() { brush->setTipImage(QString()); }
};

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
