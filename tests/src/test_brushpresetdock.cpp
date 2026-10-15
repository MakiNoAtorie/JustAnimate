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

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QTemporaryDir>

#include "brushpresetdock.h"
#include "brushpresets.h"
#include "brushtool.h"
#include "testfixture.h"
#include "toolmanager.h"

namespace
{

// Number of pixels with any ink.
int inkPixels(const QIcon& icon)
{
    const QImage img = icon.pixmap(BrushPresetDock::sampleSize()).toImage();
    int count = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) != 0)
                ++count;
    return count;
}

struct DockSession
{
    QTemporaryDir dir;
    TestSession s;
    BrushTool* brush = nullptr;
    std::unique_ptr<ToolSettingsGuard> guard;
    std::unique_ptr<BrushPresetDock> dock;

    DockSession()
    {
        s.start();
        s.editor->tools()->setCurrentTool(BRUSH);
        brush = static_cast<BrushTool*>(s.editor->tools()->getTool(BRUSH));
        guard = std::make_unique<ToolSettingsGuard>(brush);
    }
    void openDock()
    {
        dock = std::make_unique<BrushPresetDock>(nullptr, dir.path());
        dock->setEditor(s.editor.get());
        dock->initUI();
    }
    QListWidget* list() const { return dock->findChild<QListWidget*>(); }
};

}

TEST_CASE("Selecting a preset row applies it and refreshes tool options", "[BrushPresetDock]")
{
    DockSession d;
    d.brush->setWidth(7);
    REQUIRE(BrushPresets(d.dir.path()).save("Fine", *d.brush));
    d.brush->setWidth(60);
    REQUIRE(BrushPresets(d.dir.path()).save("Wide", *d.brush));
    d.brush->setWidth(20);

    d.openDock();
    int resets = 0;
    QObject::connect(d.s.editor->tools(), &ToolManager::toolsReset, [&resets] { ++resets; });
    REQUIRE(d.list()->count() == 2);
    CHECK(d.brush->strokeToolProperties().width() == 20); // listing does not apply

    d.list()->setCurrentRow(1);
    CHECK(d.brush->strokeToolProperties().width() == 60);
    CHECK(resets == 1);

    d.list()->setCurrentRow(0);
    CHECK(d.brush->strokeToolProperties().width() == 7);
}

TEST_CASE("Preset rows show a rendered sample stroke and the description", "[BrushPresetDock]")
{
    DockSession d;
    d.brush->setWidth(4);
    REQUIRE(BrushPresets(d.dir.path()).save("Fine", *d.brush));
    QFile f(d.dir.filePath("Fine.json"));
    REQUIRE(f.open(QIODevice::ReadOnly));
    QJsonObject preset = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    preset.insert("description", "Thin lines");
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(QJsonDocument(preset).toJson());
    f.close();

    d.openDock();
    QListWidgetItem* item = d.list()->item(0);
    CHECK(item->text().contains("Fine"));
    CHECK(item->text().contains("Thin lines"));
    CHECK(inkPixels(item->icon()) > 0);
}

TEST_CASE("Sample strokes update when a preset file changes", "[BrushPresetDock]")
{
    DockSession d;
    d.brush->setWidth(3);
    REQUIRE(BrushPresets(d.dir.path()).save("Pen", *d.brush));
    d.openDock();
    const int before = inkPixels(d.list()->item(0)->icon());
    REQUIRE(before > 0);

    d.brush->setWidth(30);
    REQUIRE(BrushPresets(d.dir.path()).save("Pen", *d.brush));
    int after = before;
    const QDeadlineTimer deadline(5000);
    while (after == before && !deadline.hasExpired())
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        after = inkPixels(d.list()->item(0)->icon());
    }
    CHECK(after > before);
}
