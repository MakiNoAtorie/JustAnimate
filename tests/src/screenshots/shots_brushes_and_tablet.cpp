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

#include "screenshots.h"

#include <algorithm>
#include <cmath>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QFontDatabase>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainterPath>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QtMath>

#include "bitmapimage.h"
#include "brushpreseteditor.h"
#include "brushpresets.h"
#include "colormanager.h"
#include "dabtool.h"
#include "editor.h"
#include "layerbitmap.h"
#include "scribblearea.h"
#include "stroketool.h"
#include "toolmanager.h"

namespace
{
struct Pt
{
    QPointF pos;
    qreal pressure = 1;
    int tiltX = 0;
    int tiltY = 0;
    ulong time = 0;
};
using Path = QList<Pt>;

// A horizontal wave; `pressure` maps 0..1 along the stroke to the pen pressure.
Path wave(qreal amplitude, qreal (*pressure)(qreal), int stepMs = 8, int tiltX = 0, int tiltY = 0, int steps = 48)
{
    Path path;
    for (int i = 0; i <= steps; ++i)
    {
        const qreal t = qreal(i) / steps;
        path.append({ QPointF(40 + 320 * t, 50 - amplitude * std::sin(t * 2 * M_PI)), pressure(t), tiltX, tiltY, ulong(1000 + i * stepMs) });
    }
    return path;
}
qreal flat(qreal) { return 0.8; }
qreal swell(qreal t) { return 0.1 + 0.9 * std::sin(t * M_PI); }

Path line(QPointF from, QPointF to, int steps = 24)
{
    Path path;
    for (int i = 0; i <= steps; ++i)
    {
        const qreal t = qreal(i) / steps;
        path.append({ from + (to - from) * t, 1.0, 0, 0, ulong(1000 + i * 8) });
    }
    return path;
}

void send(PresetSampler& s, QEvent::Type type, const Pt& p)
{
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), p.pos, s.area->mapToGlobal(p.pos), p.pressure, p.tiltX, p.tiltY,
                   0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    e.setTimestamp(p.time);
    QApplication::sendEvent(s.area.get(), &e);
}

void stroke(PresetSampler& s, const Path& path)
{
    send(s, QEvent::TabletPress, path.first());
    for (int i = 1; i < path.size() - 1; ++i) { send(s, QEvent::TabletMove, path[i]); }
    send(s, QEvent::TabletRelease, path.last());
}

StrokeTool* useTool(PresetSampler& s, ToolType type, const QJsonObject& settings, const QString& tip = QString())
{
    auto* tool = static_cast<DabTool*>(s.editor->tools()->getTool(type));
    s.editor->tools()->setCurrentTool(type);
    QTemporaryDir unused;
    BrushPresets(unused.path()).apply(QJsonObject{ { "settings", settings }, { "tipImage", tip } }, *tool);
    tool->seedRandom(1);
    return tool;
}

// The strokes of one settings set on a clear layer.
QImage render(PresetSampler& s, const QJsonObject& settings, const QList<Path>& paths, const QString& tip = QString())
{
    s.editor->color()->setFrontColor(QColor(35, 35, 45));
    s.layer->getBitmapImageAtFrame(1)->clear();
    useTool(s, BRUSH, settings, tip);
    for (const Path& path : paths) { stroke(s, path); }
    BitmapImage* image = s.layer->getBitmapImageAtFrame(1);
    QImage out(380, 110, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::white);
    QPainter p(&out);
    const QSize size = image->bounds().size().boundedTo(out.size());
    p.drawImage(QRect(QPoint((out.width() - size.width()) / 2, (out.height() - size.height()) / 2), size), *image->image());
    p.setPen(QColor(200, 200, 205));
    p.drawRect(out.rect().adjusted(0, 0, -1, -1));
    return out;
}

struct Cell { QString label; QImage image; };

// A labelled grid of strips, saved as a shot through a QLabel.
void saveFigure(const QList<Cell>& cells, int columns, const QString& name)
{
    const int pad = 10, labelHeight = 20;
    const QSize cell = cells.first().image.size();
    const int rows = (cells.size() + columns - 1) / columns;
    QPixmap pixmap(columns * (cell.width() + pad) + pad, rows * (cell.height() + labelHeight + pad) + pad);
    pixmap.fill(Qt::white);
    QPainter p(&pixmap);
    p.setPen(QColor(40, 40, 50));
    for (int i = 0; i < cells.size(); ++i)
    {
        const int x = pad + (i % columns) * (cell.width() + pad);
        const int y = pad + (i / columns) * (cell.height() + labelHeight + pad);
        p.drawText(QRect(x, y, cell.width(), labelHeight), Qt::AlignLeft | Qt::AlignVCenter, cells[i].label);
        p.drawImage(x, y + labelHeight, cells[i].image);
    }
    p.end();
    QLabel label;
    label.setPixmap(pixmap);
    label.resize(pixmap.size());
    saveShot(&label, name);
}

QJsonObject base(QJsonObject extra = {})
{
    QJsonObject settings{ { "Width", 22 }, { "PressureEnabled", false }, { "Feather", 1 } };
    for (auto it = extra.begin(); it != extra.end(); ++it) { settings.insert(it.key(), it.value()); }
    return settings;
}

QString makeStarTip(const QString& dir)
{
    QImage star(64, 64, QImage::Format_ARGB32);
    star.fill(Qt::transparent);
    QPainterPath path;
    for (int i = 0; i < 10; ++i)
    {
        const qreal radius = (i % 2 == 0) ? 30 : 12;
        const QPointF point(32 + radius * std::sin(i * M_PI / 5), 33 - radius * std::cos(i * M_PI / 5));
        if (i == 0) { path.moveTo(point); } else { path.lineTo(point); }
    }
    path.closeSubpath();
    QPainter p(&star);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillPath(path, Qt::black);
    p.end();
    const QString file = dir + "/star.png";
    REQUIRE(star.save(file));
    return file;
}

// The offscreen platform has no fonts of its own: without one, every glyph is a box.
struct ShotFont
{
    QFont previous = QApplication::font();
    ShotFont()
    {
        for (const char* file : { "C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf" })
        {
            const int id = QFile::exists(file) ? QFontDatabase::addApplicationFont(file) : -1;
            if (id >= 0)
            {
                QApplication::setFont(QFont(QFontDatabase::applicationFontFamilies(id).value(0), 9));
                return;
            }
        }
    }
    ~ShotFont() { QApplication::setFont(previous); }
};

QWidget* panel(ShotWindow& shot, const char* name)
{
    auto* widget = shot.window.findChild<QWidget*>(name);
    REQUIRE(widget);
    return widget;
}

QDialog* openPreferences(ShotWindow& shot, int page)
{
    auto* action = shot.window.findChild<QAction*>("actionPreference");
    REQUIRE(action);
    action->trigger();
    QApplication::processEvents();
    QDialog* dialog = nullptr;
    for (QDialog* d : shot.window.findChildren<QDialog*>())
    {
        if (d->windowTitle() == "Preferences") { dialog = d; }
    }
    REQUIRE(dialog);
    dialog->findChild<QListWidget*>("contentsWidget")->setCurrentRow(page);
    return dialog;
}
} // namespace

// Shots for docs/manual/*-brushes-and-tablet.md. Each SECTION saves images/brushes-and-tablet-<shot>.png.
TEST_CASE("Manual screenshots: Brushes and tablet", "[.screenshots]")
{
    ShotFont font;
    BrushPresets().addDefaults();
    ShotWindow shot;
    PresetSampler sampler(QColor(35, 35, 45));
    QTemporaryDir files;
    REQUIRE(files.isValid());

    SECTION("tool-settings")
    {
        shot.editor->tools()->setCurrentTool(BRUSH);
        auto* dock = static_cast<QDockWidget*>(panel(shot, "ToolOption"));
        dock->setFloating(true);
        dock->resize(300, 780);
        saveShot(dock, "brushes-and-tablet-tool-settings");
    }

    SECTION("spacing")
    {
        QList<Cell> cells;
        for (int spacing : { 5, 12, 40, 100, 250 })
        {
            cells << Cell{ QString("Spacing %1%").arg(spacing), render(sampler, base({ { "Spacing", spacing } }), { wave(18, flat) }) };
        }
        saveFigure(cells, 2, "brushes-and-tablet-spacing");
    }

    SECTION("flow-opacity")
    {
        const Path down = line({ 60, 20 }, { 300, 80 });
        const Path up = line({ 60, 80 }, { 300, 20 });
        QList<Cell> cells;
        cells << Cell{ "Opacity 100%, Flow 100%", render(sampler, base(), { down, up }) };
        cells << Cell{ "Opacity 50%, Flow 100%", render(sampler, base({ { "Opacity", 50 } }), { down, up }) };
        cells << Cell{ "Opacity 100%, Flow 30%", render(sampler, base({ { "Flow", 30 } }), { down, up }) };
        cells << Cell{ "Opacity 50%, Flow 30%", render(sampler, base({ { "Opacity", 50 }, { "Flow", 30 } }), { down, up }) };
        saveFigure(cells, 2, "brushes-and-tablet-flow-opacity");
    }

    SECTION("wash")
    {
        // One stroke that crosses itself: a figure eight.
        Path eight;
        for (int i = 0; i <= 100; ++i)
        {
            const qreal t = 0.8 + qreal(i) / 100 * 2.3 * M_PI;
            eight.append({ QPointF(200 + 110 * std::sin(t), 50 + 30 * std::sin(2 * t)), 1.0, 0, 0, ulong(1000 + i * 8) });
        }
        const QJsonObject soft{ { "Opacity", 50 }, { "Spacing", 12 }, { "Width", 22 } };
        QJsonObject washed = soft;
        washed.insert("WashEnabled", true);
        QList<Cell> cells;
        cells << Cell{ "Wash off", render(sampler, base(soft), { eight }) };
        cells << Cell{ "Wash on", render(sampler, base(washed), { eight }) };
        saveFigure(cells, 1, "brushes-and-tablet-wash");
    }

    SECTION("pressure-curve")
    {
        saveShot(openPreferences(shot, 3), "brushes-and-tablet-pressure-curve");
    }

    SECTION("pressure")
    {
        QList<Cell> cells;
        cells << Cell{ "Pressure off", render(sampler, base(), { wave(18, swell) }) };
        cells << Cell{ "Pressure on", render(sampler, base({ { "PressureEnabled", true } }), { wave(18, swell) }) };
        saveFigure(cells, 1, "brushes-and-tablet-pressure");
    }

    SECTION("taper")
    {
        QList<Cell> cells;
        cells << Cell{ "No taper", render(sampler, base({ { "Width", 30 } }), { wave(15, flat) }) };
        cells << Cell{ "Start taper 80 px", render(sampler, base({ { "Width", 30 }, { "TaperStart", 80 } }), { wave(15, flat) }) };
        cells << Cell{ "End taper 80 px", render(sampler, base({ { "Width", 30 }, { "TaperEnd", 80 } }), { wave(15, flat) }) };
        cells << Cell{ "Start and end taper 120 px", render(sampler, base({ { "Width", 30 }, { "TaperStart", 120 }, { "TaperEnd", 120 } }), { wave(15, flat) }) };
        saveFigure(cells, 2, "brushes-and-tablet-taper");
    }

    SECTION("dynamics")
    {
        const QJsonObject speed = base({ { "SpeedToSize", -80 } });
        const QJsonObject tilt = base({ { "TiltToSize", 80 } });
        QList<Cell> cells;
        cells << Cell{ "Speed to size -80, slow stroke", render(sampler, speed, { wave(15, flat, 40) }) };
        cells << Cell{ "Speed to size -80, fast stroke", render(sampler, speed, { wave(15, flat, 2) }) };
        cells << Cell{ "Tilt to size 80, pen upright", render(sampler, tilt, { wave(15, flat, 8, 0, 0) }) };
        cells << Cell{ "Tilt to size 80, pen tilted 55 degrees", render(sampler, tilt, { wave(15, flat, 8, 0, 55) }) };
        saveFigure(cells, 2, "brushes-and-tablet-dynamics");
    }

    SECTION("tips")
    {
        const QString star = makeStarTip(files.path());
        QList<Cell> cells;
        cells << Cell{ "Round tip, Feather 1", render(sampler, base(), { wave(15, flat) }) };
        cells << Cell{ "Round tip, Feather 70", render(sampler, base({ { "Feather", 70 } }), { wave(15, flat) }) };
        cells << Cell{ "Square Tip", render(sampler, base({ { "TipSquare", true }, { "Spacing", 25 } }), { wave(15, flat) }) };
        cells << Cell{ "Square Tip, Tip Aspect 30%, Tip Angle 45", render(sampler, base({ { "TipSquare", true }, { "TipAspect", 30 }, { "TipAngle", 45 }, { "Spacing", 15 } }), { wave(15, flat) }) };
        cells << Cell{ "Square Tip, Tip Aspect 30%, Follow Direction", render(sampler, base({ { "TipSquare", true }, { "TipAspect", 30 }, { "TipFollowDirection", true }, { "Spacing", 15 } }), { wave(15, flat) }) };
        cells << Cell{ "Image tip (a star in a PNG file)", render(sampler, base({ { "Width", 26 }, { "Spacing", 45 } }), { wave(15, flat) }, star) };
        saveFigure(cells, 2, "brushes-and-tablet-tips");
    }

    SECTION("scatter")
    {
        const QString star = makeStarTip(files.path());
        QList<Cell> cells;
        cells << Cell{ "No jitter or scatter", render(sampler, base({ { "Spacing", 30 } }), { wave(15, flat) }) };
        cells << Cell{ "Size jitter 60%", render(sampler, base({ { "Spacing", 30 }, { "SizeJitter", 60 } }), { wave(15, flat) }) };
        cells << Cell{ "Scatter 80%", render(sampler, base({ { "Spacing", 30 }, { "Scatter", 80 } }), { wave(15, flat) }) };
        cells << Cell{ "Scatter 150%, 4 dabs per step", render(sampler, base({ { "Width", 12 }, { "Spacing", 60 }, { "Scatter", 150 }, { "DabCount", 4 } }), { wave(15, flat) }) };
        cells << Cell{ "Star tip, no Angle jitter", render(sampler, base({ { "Width", 24 }, { "Spacing", 70 } }), { wave(15, flat) }, star) };
        cells << Cell{ "Star tip, Angle jitter 180", render(sampler, base({ { "Width", 24 }, { "Spacing", 70 }, { "AngleJitter", 180 } }), { wave(15, flat) }, star) };
        saveFigure(cells, 2, "brushes-and-tablet-scatter");
    }

    SECTION("grain")
    {
        const QJsonObject wide{ { "Width", 44 }, { "Spacing", 10 } };
        const auto grain = [&wide](int mode, int texture) {
            QJsonObject s = base(wide);
            s.insert("GrainMode", mode);
            s.insert("GrainTexture", texture);
            s.insert("GrainContrast", 200);
            return s;
        };
        QList<Cell> cells;
        cells << Cell{ "Grain: Off", render(sampler, base(wide), { wave(10, flat) }) };
        cells << Cell{ "Fixed to paper, texture Generated", render(sampler, grain(1, 0), { wave(10, flat) }) };
        cells << Cell{ "Fixed to paper, texture Fine paper", render(sampler, grain(1, 1), { wave(10, flat) }) };
        cells << Cell{ "Fixed to paper, texture Crumpled", render(sampler, grain(1, 2), { wave(10, flat) }) };
        cells << Cell{ "Moves with stroke, texture Laid paper", render(sampler, grain(2, 3), { wave(10, flat) }) };
        cells << Cell{ "Fixed to paper, texture Mottled", render(sampler, grain(1, 5), { wave(10, flat) }) };
        saveFigure(cells, 2, "brushes-and-tablet-grain");
    }

    SECTION("smoothing")
    {
        // A shaky hand: a slow wave with a small, fast tremor on top.
        Path shaky = wave(0, flat, 40, 0, 0, 160);
        for (int i = 0; i < shaky.size(); ++i)
        {
            shaky[i].pos.setY(50 + 10 * std::sin(i * 0.12) + 3 * std::sin(i * 2.9) + 2 * std::cos(i * 1.3));
        }
        const QStringList names{ "None", "1-euro filter", "Weighted", "Stabilizer" };
        QList<Cell> cells;
        cells << Cell{ "Smoothing: None", render(sampler, base({ { "Width", 5 } }), { shaky }) };
        for (int mode = 1; mode <= 3; ++mode)
        {
            cells << Cell{ "Smoothing: " + names[mode] + QString(", strength %1").arg(mode == 1 ? 100 : 70),
                           render(sampler, base({ { "Width", 5 }, { "SmoothingMode", mode }, { "SmoothingStrength", mode == 1 ? 100 : 70 } }), { shaky }) };
        }
        saveFigure(cells, 2, "brushes-and-tablet-smoothing");
    }

    SECTION("smudge")
    {
        const auto smudged = [&sampler](int mix) {
            sampler.layer->getBitmapImageAtFrame(1)->clear();
            const QJsonObject paintSettings = base({ { "Width", 36 }, { "Spacing", 10 } });
            sampler.editor->color()->setFrontColor(QColor(30, 90, 220));
            useTool(sampler, BRUSH, paintSettings);
            stroke(sampler, line({ 30, 38 }, { 370, 38 }, 40));
            sampler.editor->color()->setFrontColor(QColor(245, 215, 20));
            useTool(sampler, BRUSH, paintSettings);
            stroke(sampler, line({ 30, 62 }, { 370, 62 }, 40));
            useTool(sampler, SMUDGE, { { "Width", 30 }, { "SmudgeLength", 75 }, { "SmudgeMix", mix } });
            for (int x : { 90, 190, 290 }) { stroke(sampler, line({ double(x), 14 }, { double(x + 20), 88 }, 30)); }
            BitmapImage* image = sampler.layer->getBitmapImageAtFrame(1);
            QImage out(380, 100, QImage::Format_ARGB32_Premultiplied);
            out.fill(Qt::white);
            QPainter p(&out);
            p.drawImage(QPoint((out.width() - image->bounds().width()) / 2, (out.height() - image->bounds().height()) / 2), *image->image());
            return out;
        };
        saveFigure({ Cell{ "Smudge mixing: Light", smudged(0) }, Cell{ "Smudge mixing: Pigment", smudged(1) } }, 1,
                   "brushes-and-tablet-smudge");
    }

    SECTION("presets-panel")
    {
        shot.editor->tools()->setCurrentTool(BRUSH);
        saveShot(panel(shot, "BrushPresets"), "brushes-and-tablet-presets-panel");
    }

    SECTION("preset-editor")
    {
        BrushPresetEditor dialog(BrushPresets(), "Marker", "Marker", "Marker", "brush", &shot.window);
        dialog.resize(460, 820);
        dialog.show();
        saveShot(&dialog, "brushes-and-tablet-preset-editor");
    }

    SECTION("radial-menu")
    {
        shot.editor->tools()->setCurrentTool(BRUSH);
        auto* area = shot.window.findChild<ScribbleArea*>();
        REQUIRE(area);
        area->openToolMenu(QPointF(area->width() / 2.0, area->height() / 2.0));
        saveShot(&shot.window, "brushes-and-tablet-radial-menu");
    }

    SECTION("preset-picker")
    {
        shot.editor->tools()->setCurrentTool(BRUSH);
        auto* area = shot.window.findChild<ScribbleArea*>();
        REQUIRE(area);
        area->openPresetPicker(QPointF(area->width() / 2.0, area->height() / 2.0));
        saveShot(&shot.window, "brushes-and-tablet-preset-picker");
    }

    SECTION("tablet")
    {
        QDialog* dialog = openPreferences(shot, 0);
        dialog->resize(800, 760);
        QApplication::processEvents();
        // The Editing group is at the end of the General page.
        for (QScrollArea* area : dialog->findChildren<QScrollArea*>()) { area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum()); }
        saveShot(dialog, "brushes-and-tablet-tablet");
    }
}
