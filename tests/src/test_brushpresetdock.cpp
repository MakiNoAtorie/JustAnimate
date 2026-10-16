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

#include <QCheckBox>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QTemporaryDir>

#include "brushpresetdock.h"
#include "brushpreseteditor.h"
#include "brushpresets.h"
#include "brushtool.h"
#include "pressurecurvewidget.h"
#include "testfixture.h"
#include "toolmanager.h"

namespace
{

// Number of pixels with any ink.
int inkPixels(const QImage& img)
{
    int count = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) != 0)
                ++count;
    return count;
}

int inkPixels(const QIcon& icon)
{
    return inkPixels(icon.pixmap(BrushPresetDock::sampleSize()).toImage());
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

TEST_CASE("The preset editor previews edits live and saves every part of the preset", "[BrushPresetDock]")
{
    DockSession d;
    d.brush->setWidth(4);
    const BrushPresets presets(d.dir.path());
    REQUIRE(presets.save("Fine", *d.brush));
    QImage tip(8, 8, QImage::Format_RGB32);
    tip.fill(Qt::black);
    REQUIRE(tip.save(d.dir.filePath("tip.png")));

    BrushPresetEditor editor(presets, "Fine", "Fine", "Fine");
    const int before = inkPixels(editor.preview());
    REQUIRE(before > 0);
    auto width = editor.findChild<QDoubleSpinBox*>("Width");
    REQUIRE(width);
    CHECK(width->value() == 4);
    width->setValue(40);
    CHECK(inkPixels(editor.preview()) > before);

    editor.findChild<QLineEdit*>("description")->setText("Wide lines");
    auto tipEdit = editor.findChild<QLineEdit*>("tipImage");
    tipEdit->setText(d.dir.filePath("tip.png"));
    emit tipEdit->editingFinished();
    editor.findChild<QCheckBox*>("globalCurve")->setChecked(false);
    editor.findChild<PressureCurveWidget*>()->setCurve(PressureCurve::fromString("0,0.9 1,1"));
    REQUIRE(editor.save());

    REQUIRE(presets.apply("Fine", *d.brush));
    CHECK(d.brush->strokeToolProperties().width() == 40);
    CHECK(d.brush->tipImagePath() == d.dir.filePath("tip.png"));
    CHECK(d.brush->pressureCurve() == PressureCurve::fromString("0,0.9 1,1").toString());
    CHECK(presets.description("Fine") == "Wide lines");
    d.brush->setTipImage(QString());
    d.brush->setPressureCurve(QString());
}

TEST_CASE("The preset editor labels every setting in words", "[BrushPresetDock]")
{
    DockSession d;
    const BrushPresets presets(d.dir.path());
    BrushPresetEditor editor(presets, QString(), "New");
    auto form = editor.findChild<QFormLayout*>();
    REQUIRE(form);
    int rows = 0;
    const ToolProperties& props = d.brush->toolProperties();
    for (const auto& range : props.typeRanges())
    {
        for (int key = range.first; key < range.second; ++key)
        {
            if (props.getInfo(key).type() == PropertyInfo::INVALID)
                continue;
            const QString id = props.identifier(key);
            CAPTURE(id.toStdString());
            QWidget* control = editor.findChild<QWidget*>(id);
            REQUIRE(control);
            auto label = qobject_cast<QLabel*>(form->labelForField(control));
            REQUIRE(label);
            CHECK_FALSE(label->text().contains(QRegularExpression("[a-z][A-Z]"))); // not an identifier
            ++rows;
        }
    }
    CHECK(rows > 20);
}

TEST_CASE("The preset editor never overwrites another preset", "[BrushPresetDock]")
{
    DockSession d;
    d.brush->setWidth(4);
    const BrushPresets presets(d.dir.path());
    REQUIRE(presets.save("Fine", *d.brush));

    BrushPresetEditor duplicate(presets, "Fine", "Fine");
    CHECK_FALSE(duplicate.save()); // a new or duplicated preset needs a free name
    duplicate.findChild<QLineEdit*>("name")->setText("Fine copy");
    duplicate.findChild<QDoubleSpinBox*>("Width")->setValue(30);
    REQUIRE(duplicate.save());
    CHECK(presets.names() == QStringList{ "Fine copy", "Fine" });

    BrushPresetEditor rename(presets, "Fine", "Fine", "Fine");
    rename.findChild<QLineEdit*>("name")->setText("Fine copy");
    CHECK_FALSE(rename.save());
    rename.findChild<QLineEdit*>("name")->setText("Thin");
    REQUIRE(rename.save());
    CHECK(presets.names() == QStringList{ "Fine copy", "Thin" });
    REQUIRE(presets.apply("Fine copy", *d.brush));
    CHECK(d.brush->strokeToolProperties().width() == 30);
}

TEST_CASE("The dock lists only the active tool's presets", "[BrushPresetDock]")
{
    DockSession d;
    StrokeTool* eraser = static_cast<StrokeTool*>(d.s.editor->tools()->getTool(ERASER));
    ToolSettingsGuard eraserGuard(eraser);
    const BrushPresets presets(d.dir.path());
    d.brush->setWidth(9);
    REQUIRE(presets.save("Ink", *d.brush));
    eraser->setWidth(70);
    REQUIRE(presets.save("Rub", *eraser));
    eraser->setWidth(20);

    d.openDock();
    REQUIRE(d.list()->count() == 1);
    CHECK(d.list()->item(0)->data(Qt::UserRole).toString() == "Ink");

    d.s.editor->tools()->setCurrentTool(ERASER);
    REQUIRE(d.list()->count() == 1);
    CHECK(d.list()->item(0)->data(Qt::UserRole).toString() == "Rub");

    d.brush->setWidth(30);
    d.list()->setCurrentRow(0);
    CHECK(eraser->strokeToolProperties().width() == 70);
    CHECK(d.brush->strokeToolProperties().width() == 30); // the brush is untouched

    // A new preset made while the eraser is active is an eraser preset.
    BrushPresetEditor editor(presets, QString(), "Wipe", QString(), "eraser");
    REQUIRE(editor.save());
    CHECK(presets.tool("Wipe") == "eraser");
    // Duplicate keeps the source's tool.
    BrushPresetEditor duplicate(presets, "Rub", "Rub copy");
    REQUIRE(duplicate.save());
    CHECK(presets.tool("Rub copy") == "eraser");
}
