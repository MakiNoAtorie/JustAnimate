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

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QPainter>
#include <QScrollBar>

#include "bitmapimage.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "pencildef.h"
#include "screenshots.h"

namespace
{
// A docked Panel is too low to show all layers: float it at a set size.
QDockWidget* xsheetPanel(ShotWindow& shot) { return floatPanel(&shot.window, "XSheet", QSize(700, 420)); }

// The layer names and the first frames of the Timeline Panel, as they look now.
QPixmap timeline(ShotWindow& shot) { return floatPanel(&shot.window, "TimeLine", QSize(1000, 290))->grab().copy(0, 56, 480, 150); }

void toTop(QDockWidget* xsheet)
{
    for (QScrollBar* bar : xsheet->findChildren<QScrollBar*>())
    {
        if (bar->orientation() == Qt::Vertical) { bar->setValue(0); }
    }
}

struct Row { QString label; QPixmap pixmap; };

// Labelled Timeline pictures one under the other, such as before and after an edit.
void saveRows(const QList<Row>& rows, const QString& name)
{
    const int pad = 8, labelHeight = rows.first().label.isEmpty() ? 0 : 20;
    int width = 0, height = pad;
    for (const Row& r : rows) { width = qMax(width, r.pixmap.width()); height += labelHeight + r.pixmap.height() + pad; }
    QPixmap out(width + 2 * pad, height);
    out.fill(Qt::white);
    QPainter p(&out);
    int y = pad;
    for (const Row& r : rows)
    {
        p.setPen(QColor(40, 40, 50));
        p.drawText(QRect(pad, y, width, labelHeight), Qt::AlignLeft | Qt::AlignVCenter, r.label);
        p.drawPixmap(pad, y + labelHeight, r.pixmap);
        y += labelHeight + r.pixmap.height() + pad;
    }
    p.end();
    savePicture(out, name);
}

int roughIndex(ShotWindow& shot) { return shot.editor->currentLayerIndex(); }

void selectRough(ShotWindow& shot, int from, int to)
{
    const int i = roughIndex(shot);
    shot.editor->layers()->selectFrames(i, i, from, to);
}

// A bitmap layer "Hold" with the same image on frames 1 to 3, then a second image on frame 4.
void addRepeatedLayer(ShotWindow& shot)
{
    LayerBitmap* layer = shot.editor->object()->addNewBitmapLayer();
    layer->setName("Hold");
    shot.editor->layers()->setCurrentLayer(shot.editor->object()->getLayerCount() - 1);
    BitmapImage* first = layer->getBitmapImageAtFrame(1);
    first->drawRect(QRectF(-80, -80, 160, 160), QPen(Qt::black, 6), Qt::NoBrush, QPainter::CompositionMode_SourceOver, false);
    for (int frame : { 2, 3 }) { layer->addKeyFrame(frame, first->clone()); }
    layer->addNewKeyFrameAt(4);
    layer->getBitmapImageAtFrame(4)->drawRect(QRectF(-40, -40, 80, 80), QPen(Qt::black, 6), Qt::NoBrush, QPainter::CompositionMode_SourceOver, false);
}
} // namespace

// Shots for docs/manual/*-timeline-and-xsheet.md. Each SECTION saves images/timeline-and-xsheet-<shot>.png.
TEST_CASE("Manual screenshots: Timeline and X-sheet", "[.screenshots]")
{
    ShotWindow shot;

    SECTION("drawings")
    {
        shot.editor->scrubTo(4);
        saveRows({ { QString(), timeline(shot) } }, "timeline-and-xsheet-drawings");
    }

    SECTION("exposure-change")
    {
        shot.editor->scrubTo(4);
        const QPixmap before = timeline(shot);
        shot.editor->layers()->changeExposure(4, 1);
        shot.editor->layers()->changeExposure(4, 1);
        saveRows({ { "Before", before }, { "After Exposure + twice on the Drawing at frame 4", timeline(shot) } },
                 "timeline-and-xsheet-exposure-change");
    }

    SECTION("ripple")
    {
        shot.editor->scrubTo(4);
        const QPixmap before = timeline(shot);
        shot.editor->layers()->rippleInsertFrame();
        const QPixmap inserted = timeline(shot);
        shot.editor->scrubTo(8);
        shot.editor->layers()->rippleRemoveFrame();
        saveRows({ { "Before", before }, { "Insert Frame (Ripple) at frame 4", inserted },
                   { "Delete Frame (Ripple) at frame 8, on the Drawing that starts there", timeline(shot) } },
                 "timeline-and-xsheet-ripple");
    }

    SECTION("exposure-set")
    {
        const QPixmap before = timeline(shot);
        selectRough(shot, 1, 13);
        shot.editor->layers()->setExposureForSelectedFrames(2, true);
        saveRows({ { "Before", before }, { "After Exposure: 2s on all Drawings of the layer", timeline(shot) } },
                 "timeline-and-xsheet-exposure-set");
    }

    SECTION("clean-exposure")
    {
        addRepeatedLayer(shot);
        const QPixmap before = timeline(shot);
        selectRough(shot, 1, 4);
        shot.editor->layers()->cleanExposureOfSelectedFrames();
        saveRows({ { "Before: frames 1, 2 and 3 are three Drawings with the same image", before },
                   { "After Clean Exposure", timeline(shot) } },
                 "timeline-and-xsheet-clean-exposure");
    }

    SECTION("select")
    {
        shot.editor->layers()->selectFrames(roughIndex(shot), roughIndex(shot) + 1, 4, 10);
        saveRows({ { QString(), timeline(shot) } }, "timeline-and-xsheet-select");
    }

    SECTION("move")
    {
        selectRough(shot, 7, 13);
        const QPixmap before = timeline(shot);
        shot.editor->layers()->moveSelectedFrames(3);
        saveRows({ { "Selected Drawings on frames 7, 10 and 13", before }, { "After a drag of three frames to the right", timeline(shot) } },
                 "timeline-and-xsheet-move");
    }

    SECTION("marks")
    {
        shot.editor->scrubTo(7);
        saveRows({ { QString(), timeline(shot) } }, "timeline-and-xsheet-marks");
    }

    SECTION("markers")
    {
        shot.editor->scrubTo(13);
        saveRows({ { QString(), timeline(shot) } }, "timeline-and-xsheet-markers");
    }

    SECTION("link")
    {
        // The link bar shows well on the white cells of the X-sheet.
        QDockWidget* xsheet = xsheetPanel(shot);
        REQUIRE(shot.editor->layers()->linkDrawing(1, 11));
        shot.editor->scrubTo(14);
        toTop(xsheet);
        saveShot(xsheet, "timeline-and-xsheet-link");
    }

    SECTION("xsheet")
    {
        SettingsGuard layout({ SETTING_XSHEET_LAYOUT });
        QDockWidget* xsheet = xsheetPanel(shot);
        shot.editor->scrubTo(7);
        toTop(xsheet);
        saveShot(xsheet, "timeline-and-xsheet-xsheet");
    }

    SECTION("timesheet")
    {
        SettingsGuard layout({ SETTING_XSHEET_LAYOUT });
        QDockWidget* xsheet = xsheetPanel(shot);
        shot.editor->scrubTo(7);
        actionNamed(&shot.window, "actionXSheetTimesheet")->setChecked(true);
        toTop(xsheet);
        saveShot(xsheet, "timeline-and-xsheet-timesheet");
    }
}
