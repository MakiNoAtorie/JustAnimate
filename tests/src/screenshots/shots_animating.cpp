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
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QTabletEvent>

#include "cameracontextmenu.h"
#include "cameratool.h"
#include "cellmenus.h"
#include "editor.h"
#include "keyframe.h"
#include "layer.h"
#include "layercamera.h"
#include "layermanager.h"
#include "pegbaralignmentdialog.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "screenshots.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "transformtool.h"
#include "viewmanager.h"

namespace
{
// Everything a shot below changes in the settings store goes back at scope exit.
struct AnimatingSettings
{
    SettingsGuard guard{ SETTING_PREV_ONION, SETTING_NEXT_ONION, SETTING_ONION_RED, SETTING_ONION_BLUE,
                         SETTING_ONION_MAX_OPACITY, SETTING_ONION_MIN_OPACITY, SETTING_ONION_PREV_FRAMES_NUM,
                         SETTING_ONION_NEXT_FRAMES_NUM, SETTING_ONION_TYPE, SETTING_ONION_MARKS, SETTING_ONION_FALLOFF,
                         SETTING_ONION_WHILE_PLAYBACK, SETTING_MULTILAYER_ONION, SETTING_PLAY_AROUND_FRAMES,
                         SETTING_INBETWEEN_EASING, "shortcuts/" CMD_FLIP_HOLD };
};

QWidget* panel(ShotWindow& shot, const char* name)
{
    auto* widget = shot.window.findChild<QWidget*>(name);
    REQUIRE(widget);
    return widget;
}

// A Panel behind a tab in the default Layout: show it before a grab.
QWidget* raised(ShotWindow& shot, const char* name)
{
    QWidget* dock = panel(shot, name);
    dock->show();
    dock->raise();
    QApplication::processEvents();
    return dock;
}

// A Panel as its own window, sized so that no control hides behind a scroll bar.
QWidget* floated(ShotWindow& shot, const char* name, QSize size)
{
    auto* dock = static_cast<QDockWidget*>(panel(shot, name));
    dock->setFloating(true);
    dock->resize(size);
    dock->show();
    QApplication::processEvents();
    return dock;
}

Layer* layerNamed(ShotWindow& shot, const QString& name)
{
    Layer* layer = shot.editor->layers()->findLayerByName(name);
    REQUIRE(layer);
    return layer;
}

int indexOf(ShotWindow& shot, Layer* layer)
{
    for (int i = 0; i < shot.editor->layers()->count(); ++i)
    {
        if (shot.editor->layers()->getLayer(i) == layer) { return i; }
    }
    FAIL("layer not found");
    return -1;
}

void goTo(ShotWindow& shot, const QString& layerName, int frame)
{
    shot.editor->layers()->setCurrentLayer(layerNamed(shot, layerName));
    shot.editor->scrubTo(frame);
    QApplication::processEvents();
}

void setOnion(ShotWindow& shot, const QString& type, int before, int after)
{
    PreferenceManager* prefs = shot.editor->preference();
    prefs->set<SETTING::PREV_ONION>(true);
    prefs->set<SETTING::NEXT_ONION>(true);
    prefs->set<SETTING::ONION_RED>(true);
    prefs->set<SETTING::ONION_BLUE>(true);
    prefs->set<SETTING::ONION_TYPE>(type);
    prefs->set<SETTING::ONION_PREV_FRAMES_NUM>(before);
    prefs->set<SETTING::ONION_NEXT_FRAMES_NUM>(after);
    QApplication::processEvents();
}

void tabletAt(ShotWindow& shot, QEvent::Type type, QPointF pos)
{
    ScribbleArea* area = shot.editor->getScribbleArea();
    const bool down = type != QEvent::TabletRelease;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos), down ? 0.7 : 0, 0, 0, 0, 0, 0,
                   Qt::NoModifier, Qt::LeftButton, down ? Qt::LeftButton : Qt::NoButton);
    QApplication::sendEvent(area, &e);
}

void key(ShotWindow& shot, QEvent::Type type, int code)
{
    QKeyEvent e(type, code, Qt::NoModifier);
    QApplication::sendEvent(shot.editor->getScribbleArea(), &e);
}

void saveLabel(const QPixmap& picture, const QString& name)
{
    QLabel label;
    label.setPixmap(picture);
    label.resize(picture.size() / picture.devicePixelRatio());
    saveShot(&label, name);
}

// A popup menu is its own window: a menu, and one submenu next to it, go into one picture.
void saveMenuWithSubmenu(QMenu& menu, QMenu* submenu, const QString& name)
{
    menu.popup(QPoint(20, 20));
    QApplication::processEvents();
    submenu->popup(menu.pos() + QPoint(menu.width() - 2, 0));
    QApplication::processEvents();
    const QPixmap first = menu.grab();
    const QPixmap second = submenu->grab();
    const qreal ratio = first.devicePixelRatio();
    QPixmap picture(first.width() + second.width(), qMax(first.height(), second.height()));
    picture.setDevicePixelRatio(ratio);
    picture.fill(Qt::transparent);
    QPainter painter(&picture);
    painter.drawPixmap(0, 0, first);
    painter.drawPixmap(QPointF((first.width() - 2 * ratio) / ratio, 0), second);
    painter.end();
    submenu->hide();
    menu.hide();
    saveLabel(picture, name);
}
} // namespace

// Shots for docs/manual/*-animating.md. Each SECTION saves images/animating-<shot>.png.
TEST_CASE("Manual screenshots: Animating", "[.screenshots]")
{
    AnimatingSettings settings;

    SECTION("onion-panel")
    {
        ShotWindow shot;
        setOnion(shot, "absolute", 2, 2);
        saveShot(floated(shot, "Onion Skin", { 300, 560 }), "animating-onion-panel");
    }

    SECTION("onion-canvas")
    {
        ShotWindow shot;
        setOnion(shot, "absolute", 2, 2);
        raised(shot, "Onion Skin");
        goTo(shot, "Rough", 7);
        saveShot(&shot.window, "animating-onion-canvas");
    }

    SECTION("onion-marked")
    {
        ShotWindow shot;
        setOnion(shot, "marked", 2, 2);
        shot.editor->preference()->set<SETTING::ONION_MARKS>(MarkFilter({ Mark::Key }).toInt());
        raised(shot, "Onion Skin");
        goTo(shot, "Rough", 7);
        saveShot(&shot.window, "animating-onion-marked");
        saveShot(floated(shot, "Onion Skin", { 300, 560 }), "animating-onion-marked-panel");
    }

    SECTION("onion-layer")
    {
        ShotWindow shot;
        setOnion(shot, "absolute", 2, 2);
        shot.editor->switchOnionSkinOfLayer(indexOf(shot, layerNamed(shot, "Background")));
        goTo(shot, "Rough", 7);
        saveShot(floated(shot, "TimeLine", { 1500, 300 }), "animating-onion-layer");
    }

    SECTION("shift-trace")
    {
        ShotWindow shot;
        setOnion(shot, "absolute", 1, 1);
        goTo(shot, "Rough", 7);
        ScribbleArea* area = shot.editor->getScribbleArea();
        area->setShiftAndTrace(true);
        QApplication::processEvents();
        REQUIRE(area->shiftAndTraceFrames().contains(10));
        // Move the next Drawing up, then turn it with its upper handle.
        const QPointF handle = area->shiftAndTraceHandle(10);
        tabletAt(shot, QEvent::TabletPress, handle);
        tabletAt(shot, QEvent::TabletMove, handle + QPointF(-30, -30));
        tabletAt(shot, QEvent::TabletMove, handle + QPointF(-60, -70));
        tabletAt(shot, QEvent::TabletRelease, handle + QPointF(-60, -70));
        const QPointF turn = area->shiftAndTraceRotateHandle(10);
        tabletAt(shot, QEvent::TabletPress, turn);
        tabletAt(shot, QEvent::TabletMove, turn + QPointF(40, 10));
        tabletAt(shot, QEvent::TabletMove, turn + QPointF(80, 20));
        tabletAt(shot, QEvent::TabletRelease, turn + QPointF(80, 20));
        saveShot(&shot.window, "animating-shift-trace");
    }

    SECTION("flip")
    {
        ShotWindow shot;
        pencilSettings().setValue("shortcuts/" CMD_FLIP_HOLD, "F");
        setOnion(shot, "absolute", 1, 1);
        goTo(shot, "Rough", 4);
        shot.editor->getScribbleArea()->setFocus();
        key(shot, QEvent::KeyPress, Qt::Key_F);
        REQUIRE(shot.editor->playback()->isPenFlipping());
        const QPointF start(500, 400);
        const QPointF end = start + QPointF(2 * PlaybackManager::kPenFlipPixels + 5, 0);
        tabletAt(shot, QEvent::TabletPress, start);
        tabletAt(shot, QEvent::TabletMove, end);
        CHECK(shot.editor->currentFrame() == 10);
        saveShot(&shot.window, "animating-flip");
        tabletAt(shot, QEvent::TabletRelease, end);
        key(shot, QEvent::KeyRelease, Qt::Key_F);
        CHECK(shot.editor->currentFrame() == 4);
    }

    SECTION("playback")
    {
        ShotWindow shot;
        PlaybackManager* playback = shot.editor->playback();
        playback->setLooping(true);
        playback->setPingPong(true);
        for (auto* box : panel(shot, "TimeLine")->findChildren<QComboBox*>())
        {
            if (box->toolTip() == QCoreApplication::translate("TimeControls", "Playback speed")) { box->setCurrentIndex(2); }
        }
        playback->setSpeed(2.0);
        goTo(shot, "Rough", 7);
        saveShot(floated(shot, "TimeLine", { 1500, 300 }), "animating-playback");
    }

    SECTION("loop-range")
    {
        ShotWindow shot;
        PlaybackManager* playback = shot.editor->playback();
        playback->setLoopRange(4, 10);
        playback->setPlayAroundFrames(3);
        goTo(shot, "Rough", 7);
        saveShot(floated(shot, "TimeLine", { 1500, 300 }), "animating-loop-range");
    }

    SECTION("transform-key")
    {
        ShotWindow shot;
        goTo(shot, "Line", 7);
        shot.editor->layers()->setKeyTween(7, false, CameraEasingType::LINEAR, TweenState{ 80, -30, 20, 1.3, 1.0 });
        shot.editor->tools()->setCurrentTool(MOVE);
        static_cast<TransformTool*>(shot.editor->tools()->currentTool())->setKeyTransformEnabled(true);
        QApplication::processEvents();
        saveShot(&shot.window, "animating-transform-key");
    }

    SECTION("tween")
    {
        ShotWindow shot;
        goTo(shot, "Line", 4);
        shot.editor->layers()->setKeyTween(1, true, CameraEasingType::INOUTQUAD, TweenState{ 0, 0, 0, 1, 1 });
        shot.editor->layers()->setKeyTween(7, false, CameraEasingType::LINEAR, TweenState{ 80, -30, 20, 1.3, 1.0 });
        setOnion(shot, "absolute", 1, 1);
        goTo(shot, "Line", 4);
        saveShot(&shot.window, "animating-tween");
    }

    SECTION("tween-menu")
    {
        ShotWindow shot;
        QMenu menu;
        QAction* toggle = menu.addAction(QCoreApplication::translate("CellMenus", "Tween to Next Key"));
        toggle->setCheckable(true);
        toggle->setChecked(true);
        QMenu* easing = CellMenus::addEasingMenu(&menu, QCoreApplication::translate("CellMenus", "Tween Easing"),
                                                 CameraEasingType::INOUTQUAD, [](CameraEasingType) {});
        saveMenuWithSubmenu(menu, easing, "animating-tween-menu");
    }

    SECTION("inbetweens")
    {
        ShotWindow shot;
        Layer* line = layerNamed(shot, "Line");
        shot.editor->layers()->setCurrentLayer(line);
        LayerManager::setFrameSelected(line, 1, true);
        LayerManager::setFrameSelected(line, 7, true);
        REQUIRE(shot.editor->layers()->createInbetweens(CameraEasingType::INOUTQUAD).ok());
        setOnion(shot, "absolute", 1, 1);
        goTo(shot, "Line", 4);
        saveShot(&shot.window, "animating-inbetweens");
    }

    SECTION("inbetween-easing")
    {
        ShotWindow shot;
        QMenu menu;
        CellMenus::fillInbetweenEasingMenu(&menu, shot.editor, [](CameraEasingType) {});
        menu.popup(QPoint(20, 20));
        QApplication::processEvents();
        const QPixmap picture = menu.grab();
        menu.hide();
        saveLabel(picture, "animating-inbetween-easing");
    }

    SECTION("camera")
    {
        ShotWindow shot;
        goTo(shot, "Camera", 1);
        shot.editor->tools()->setCurrentTool(CAMERA);
        QApplication::processEvents();
        saveShot(&shot.window, "animating-camera");
    }

    SECTION("camera-path")
    {
        ShotWindow shot;
        goTo(shot, "Camera", 12);
        shot.editor->tools()->setCurrentTool(CAMERA);
        static_cast<CameraTool*>(shot.editor->tools()->currentTool())->setCameraPathEnabled(true);
        QApplication::processEvents();
        saveShot(&shot.window, "animating-camera-path");
    }

    SECTION("camera-menu")
    {
        ShotWindow shot;
        goTo(shot, "Camera", 1);
        CameraContextMenu menu(shot.editor, 1, static_cast<const LayerCamera*>(layerNamed(shot, "Camera")));
        QMenu* transform = nullptr;
        for (QAction* action : menu.actions())
        {
            if (action->menu() && action->text() == QCoreApplication::translate("CameraContextMenu", "Transform")) { transform = action->menu(); }
        }
        REQUIRE(transform);
        saveMenuWithSubmenu(menu, transform, "animating-camera-menu");
    }

    SECTION("pegbar")
    {
        ShotWindow shot;
        goTo(shot, "Rough", 7);
        PegBarAlignmentDialog dialog(shot.editor, &shot.window);
        dialog.show();
        QApplication::processEvents();
        saveShot(&dialog, "animating-pegbar");
    }
}
