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

#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>

#include "editor.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "mainwindow2.h"
#include "object.h"
#include "pencildef.h"
#include "testfixture.h"
#include "timeline.h"
#include "timelinecells.h"
#include "undoredomanager.h"
#include "xsheet.h"

namespace
{

QList<QList<int>> keyPositions(const Object* object)
{
    QList<QList<int>> all;
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        QList<int> positions;
        object->getLayer(i)->foreachKeyFrame([&](const KeyFrame* key) { positions.prepend(key->pos()); });
        all << positions;
    }
    return all;
}

void mouse(QWidget* target, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, QPointF(pos), QPointF(target->mapToGlobal(pos)), Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(target, &e);
}

void drag(QWidget* target, QPoint from, QPoint to)
{
    mouse(target, QEvent::MouseButtonPress, from, Qt::LeftButton);
    mouse(target, QEvent::MouseMove, to, Qt::LeftButton);
    mouse(target, QEvent::MouseButtonRelease, to, Qt::NoButton);
}

} // namespace

TEST_CASE("X-sheet runs number each Drawing and end where the next one starts", "[XSheet]")
{
    Object object;
    object.init();
    LayerBitmap* layer = object.addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(4);
    layer->getKeyFrameAt(4)->setMark(Mark::Breakdown);

    const QList<XSheet::Run> runs = XSheet::runs(layer, 10);
    REQUIRE(runs.size() == 2);
    CHECK(runs[0].first == 1);
    CHECK(runs[0].last == 3);
    CHECK(runs[0].number == 1);
    CHECK(runs[0].mark == Mark::None);
    // The last Drawing stays on screen to the end of the scene.
    CHECK(runs[1].first == 4);
    CHECK(runs[1].last == 10);
    CHECK(runs[1].number == 2);
    CHECK(runs[1].mark == Mark::Breakdown);
}

TEST_CASE("MainWindow2 X-sheet edits give the same project as timeline edits", "[XSheet]")
{
    QSettings& settings = pencilSettings();
    const SettingsGuard guard({SETTING_WINDOW_STATE, SETTING_WINDOW_GEOMETRY, SETTING_LAYOUT_VERSION, SETTING_WORKSPACE_CURRENT,
                               "Workspace/State/Animate"});
    settings.remove(SETTING_WINDOW_STATE);
    settings.remove("Workspace/State/Animate");
    settings.remove(SETTING_WORKSPACE_CURRENT);

    MainWindow2 window;
    window.resize(1366, 768);
    window.show();
    QApplication::processEvents();

    Editor* editor = window.findChild<Editor*>();
    REQUIRE(editor);
    auto* timeline = window.findChild<TimeLine*>("TimeLine");
    auto* xsheet = window.findChild<XSheet*>("XSheet");
    REQUIRE(timeline);
    REQUIRE(xsheet);
    CHECK(window.dockWidgetArea(xsheet) == Qt::BottomDockWidgetArea);
    CHECK(window.tabifiedDockWidgets(timeline).contains(xsheet));

    Object* object = editor->object();
    int index = -1;
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        if (object->getLayer(i)->type() == Layer::BITMAP) { index = i; }
    }
    REQUIRE(index >= 0);
    REQUIRE(object->getLayer(index)->firstKeyFramePosition() == 1);
    REQUIRE(object->getLayer(index)->getMaxKeyFramePosition() == 1);

    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    const auto action = [&window](const char* name) { return window.findChild<QAction*>(name); };
    const QList<QList<int>> initial = keyPositions(object);

    // --- X-sheet ---
    xsheet->raise();
    QApplication::processEvents();
    QWidget* cells = xsheet->findChild<QAbstractScrollArea*>()->viewport();
    QToolBar* bar = xsheet->findChild<QToolBar*>();
    const auto sheetButton = [&](const char* name) { qobject_cast<QToolButton*>(bar->widgetForAction(action(name)))->click(); };

    editor->scrubTo(1);
    mouse(cells, QEvent::MouseButtonPress, xsheet->cellRect(index, 3).center(), Qt::LeftButton);
    mouse(cells, QEvent::MouseButtonRelease, xsheet->cellRect(index, 3).center(), Qt::NoButton);
    REQUIRE(editor->currentFrame() == 3);
    REQUIRE(editor->layers()->currentLayerIndex() == index);

    sheetButton("actionAdd_Frame");
    REQUIRE(keyPositions(object)[index] == QList<int>({ 1, 3 }));
    // Drawing 1 ends at the bottom of frame 2; drop its line end on frame 4.
    const QRect end = xsheet->cellRect(index, 2);
    drag(cells, QPoint(end.center().x(), end.bottom() - 1), xsheet->cellRect(index, 4).center());
    REQUIRE(keyPositions(object)[index] == QList<int>({ 1, 5 }));
    sheetButton("actionExposure_Add");
    sheetButton("actionRipple_Insert_Frame");
    sheetButton("actionRipple_Delete_Frame");
    sheetButton("actionExposure_Subtract");
    const QList<QList<int>> sheetResult = keyPositions(object);

    int sheetUndos = 0;
    while (undo->isEnabled() && keyPositions(object) != initial) { undo->trigger(); ++sheetUndos; }
    REQUIRE(keyPositions(object) == initial);

    // The current frame follows the timeline: the X-sheet scrolls to show it.
    editor->scrubTo(300);
    QApplication::processEvents();
    CHECK(cells->rect().contains(xsheet->cellRect(index, 300)));

    // --- timeline ---
    timeline->raise();
    QApplication::processEvents();
    const QList<TimeLineCells*> timelineCells = timeline->findChildren<TimeLineCells*>();
    TimeLineCells* tracks = *std::max_element(timelineCells.cbegin(), timelineCells.cend(), [&](TimeLineCells* a, TimeLineCells* b) {
        return a->mapTo(timeline, QPoint()).x() < b->mapTo(timeline, QPoint()).x(); });
    QToolButton* newDrawing = nullptr;
    for (QToolButton* b : timeline->findChildren<QToolButton*>())
    {
        if (b->toolTip() == "New drawing") { newDrawing = b; }
    }
    REQUIRE(newDrawing);

    editor->scrubTo(1);
    editor->layers()->setCurrentLayer(0);
    const int size = tracks->getFrameSize();
    const int y = TimeLineCells::getOffsetY() + (object->getLayerCount() - 1 - index) * tracks->getLayerHeight() + tracks->getLayerHeight() / 2;
    const auto frameX = [&](int frame) { return TimeLineCells::getOffsetX() + (frame - 1) * size + size / 2; };
    // A cell click picks the layer; a ruler click picks the frame.
    for (const int at : { y, TimeLineCells::getOffsetY() / 2 })
    {
        mouse(tracks, QEvent::MouseButtonPress, QPoint(frameX(3), at), Qt::LeftButton);
        mouse(tracks, QEvent::MouseButtonRelease, QPoint(frameX(3), at), Qt::NoButton);
    }
    REQUIRE(editor->currentFrame() == 3);
    REQUIRE(editor->layers()->currentLayerIndex() == index);

    newDrawing->click();
    drag(tracks, QPoint(TimeLineCells::getOffsetX() + 2 * size - 1, y), QPoint(frameX(4), y));
    REQUIRE(keyPositions(object)[index] == QList<int>({ 1, 5 }));
    action("actionExposure_Add")->trigger();
    action("actionRipple_Insert_Frame")->trigger();
    action("actionRipple_Delete_Frame")->trigger();
    action("actionExposure_Subtract")->trigger();
    CHECK(keyPositions(object) == sheetResult);

    int timelineUndos = 0;
    while (undo->isEnabled() && keyPositions(object) != initial) { undo->trigger(); ++timelineUndos; }
    CHECK(keyPositions(object) == initial);
    CHECK(timelineUndos == sheetUndos);
}

TEST_CASE("X-sheet visibility has a command with no default key", "[XSheet]")
{
    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    REQUIRE(preset.contains(CMD_TOGGLE_XSHEET));
    CHECK(preset.value(CMD_TOGGLE_XSHEET).toString().isEmpty());
}
