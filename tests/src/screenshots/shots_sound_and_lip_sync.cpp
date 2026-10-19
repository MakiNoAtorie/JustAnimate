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
#include <QDockWidget>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QToolBar>

#include "cellmenus.h"
#include "editor.h"
#include "layer.h"
#include "layermanager.h"
#include "object.h"
#include "objectdata.h"
#include "preferencesdialog.h"
#include "screenshots.h"

namespace
{
// The playback controls: Play, Loop, Range, Sound on/off and Sound scrub on/off.
QToolBar* controls(QWidget* window)
{
    QDockWidget* dock = tallTimeline(window);
    for (auto* bar : dock->findChildren<QToolBar*>())
    {
        for (auto* button : bar->findChildren<QPushButton*>())
        {
            if (button->toolTip() == "Loop") { return bar; }
        }
    }
    FAIL("No playback controls");
    return nullptr;
}
} // namespace

// Shots for docs/manual/07-sound-and-lip-sync.md. Each SECTION saves images/sound-and-lip-sync-<shot>.png.
TEST_CASE("Manual screenshots: Sound and lip sync", "[.screenshots]")
{
    ShotWindow shot;
    Layer* voice = layerNamed(shot.editor, "Voice");
    shot.editor->layers()->setCurrentLayer(voice);

    SECTION("import-menu")
    {
        saveMenu(shot.window.findChild<QMenu*>("menuImport"), "sound-and-lip-sync-import-menu");
    }

    SECTION("waveform")
    {
        saveShot(tallTimeline(&shot.window), "sound-and-lip-sync-waveform");
    }

    SECTION("scrub")
    {
        shot.editor->scrubTo(9);
        saveShot(tallTimeline(&shot.window), "sound-and-lip-sync-scrub");
    }

    SECTION("scrub-controls")
    {
        saveShot(controls(&shot.window), "sound-and-lip-sync-scrub-controls");
    }

    SECTION("scrub-preference")
    {
        PreferencesDialog* prefs = openPreferences(shot.window, 2);
        for (auto* area : prefs->findChildren<QScrollArea*>())
        {
            area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
        }
        QApplication::processEvents();
        saveShot(prefs, "sound-and-lip-sync-scrub-preference");
        prefs->close();
    }

    SECTION("markers")
    {
        LayerManager* layers = shot.editor->layers();
        layers->setMarker(voice->id(), 8, FrameMarker{ "E", QColor(220, 150, 40) });
        layers->setMarker(voice->id(), 11, FrameMarker{ "O", QColor(60, 130, 220) });
        layers->setMarker(voice->id(), 15, FrameMarker{ "MBP", QColor(200, 70, 120) });
        saveShot(tallTimeline(&shot.window), "sound-and-lip-sync-markers");
    }

    SECTION("marker-menu")
    {
        tallTimeline(&shot.window);
        whenMenuOpens([](QMenu* menu) { saveShot(menu, "sound-and-lip-sync-marker-menu"); });
        CellMenus::showMarkerMenu(shot.editor, voice->id(), 5, QPoint(300, 300));
    }

    SECTION("loop")
    {
        QToolBar* bar = controls(&shot.window);
        for (auto* button : bar->findChildren<QPushButton*>())
        {
            if (button->toolTip() == "Loop") { button->setChecked(true); }
        }
        for (auto* box : bar->findChildren<QCheckBox*>())
        {
            if (box->toolTip() == "Playback range") { box->setChecked(true); }
        }
        for (auto* spin : bar->findChildren<QSpinBox*>())
        {
            if (spin->toolTip() == "Start of playback loop") { spin->setValue(5); }
            if (spin->toolTip() == "End of playback loop") { spin->setValue(17); }
        }
        QApplication::processEvents();
        saveShot(controls(&shot.window), "sound-and-lip-sync-loop");
    }

    SECTION("missing")
    {
        QMessageBox box(QMessageBox::Warning, "Missing Sound Files",
                        "These sound clips were saved without audio because their files are missing:\nVoice frame 1",
                        QMessageBox::Ok);
        box.show();
        QApplication::processEvents();
        saveShot(&box, "sound-and-lip-sync-missing");
    }
}
