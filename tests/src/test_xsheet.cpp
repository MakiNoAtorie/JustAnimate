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

#include <algorithm>
#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QDomDocument>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include "editor.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "mainwindow2.h"
#include "object.h"
#include "pagewriter.h"
#include "paperpainter.h"
#include "pencildef.h"
#include "preferencemanager.h"
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

// Pixels that differ between two images, as a bounding box and a count.
QRect diffBox(const QImage& a, const QImage& b, int* count)
{
    QRect box;
    *count = 0;
    for (int y = 0; y < a.height(); ++y)
    {
        for (int x = 0; x < a.width(); ++x)
        {
            if (a.pixel(x, y) != b.pixel(x, y)) { box |= QRect(x, y, 1, 1); ++*count; }
        }
    }
    return box;
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

void menuTexts(const QMenu* menu, QStringList* texts)
{
    for (const QAction* action : menu->actions())
    {
        *texts << action->text();
        if (action->menu()) { menuTexts(action->menu(), texts); }
    }
}

// The texts of the menu a right-click at pos opens, submenus included; empty when none opens.
QStringList rightClickMenu(QWidget* target, QPoint pos)
{
    QStringList texts;
    QTimer::singleShot(0, [&texts] {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
        {
            menuTexts(menu, &texts);
            menu->close();
        }
    });
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(pos), QPointF(target->mapToGlobal(pos)), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(target, &press); // no release: a bare TimeLineCells needs its TimeLine there
    QApplication::processEvents(); // runs the timer when no menu opened
    return texts;
}

} // namespace

TEST_CASE("X-sheet right-click opens the timeline's cell and layer menus", "[XSheet]")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    REQUIRE((bitmap->keyExists(1) || bitmap->addNewKeyFrameAt(1)));
    s.object->addNewSoundLayer();
    LayerCamera* camera = s.object->addNewCameraLayer();
    REQUIRE((camera->keyExists(1) || camera->addNewKeyFrameAt(1)));
    REQUIRE(s.start());
    Editor* editor = s.editor.get();

    XSheet xsheet(nullptr);
    xsheet.setEditor(editor);
    xsheet.initUI();
    xsheet.resize(800, 400);
    xsheet.show();
    QApplication::processEvents();
    QWidget* sheet = xsheet.findChild<QAbstractScrollArea*>()->viewport();
    TimeLineCells tracks(nullptr, editor, TIMELINE_CELL_TYPE::Tracks);
    TimeLineCells labels(nullptr, editor, TIMELINE_CELL_TYPE::Layers);

    const int count = s.object->getLayerCount();
    for (int i = 0; i < count; ++i)
    {
        INFO("layer " << i);
        const int rowY = TimeLineCells::getOffsetY() + (count - 1 - i) * labels.getLayerHeight() + labels.getLayerHeight() / 2;
        const QPoint sheetCell = xsheet.cellRect(i, 1).center();

        editor->layers()->setCurrentLayer(i);
        const QStringList timelineCellMenu = rightClickMenu(&tracks, QPoint(tracks.getFrameSize() / 2, rowY));
        editor->layers()->setCurrentLayer(i == 0 ? 1 : 0);
        CHECK(!timelineCellMenu.isEmpty());
        CHECK(rightClickMenu(sheet, sheetCell) == timelineCellMenu);
        CHECK(editor->layers()->currentLayerIndex() == i);

        const QStringList timelineLayerMenu = rightClickMenu(&labels, QPoint(200, rowY));
        CHECK(!timelineLayerMenu.isEmpty());
        CHECK(rightClickMenu(sheet, QPoint(sheetCell.x(), 5)) == timelineLayerMenu);
    }
}

TEST_CASE("X-sheet runs number each Drawing and end where the next one starts", "[XSheet]")
{
    Object object;
    object.init();
    LayerBitmap* layer = object.addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(4);
    layer->getKeyFrameAt(4)->setMark(Mark::Breakdown);

    const QList<Layer::Run> runs = layer->runs(10);
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

TEST_CASE("Timesheet columns default from layer order and sound fills Dialogue", "[XSheet]")
{
    Object object;
    object.init();
    LayerCamera* camera = object.addNewCameraLayer();
    object.addNewSoundLayer();
    for (int i = 0; i < 13; ++i)
    {
        if (i % 2) { object.addNewVectorLayer(); }
        else { object.addNewBitmapLayer(); }
    }
    QList<TimesheetColumn> columns;
    for (int i = 0; i < object.getLayerCount(); ++i) { columns << effectiveTimesheetColumn(object, i); }
    using C = TimesheetColumn;
    CHECK(columns == QList<C>({ C::Camera, C::Dialogue, C::KeyA, C::KeyB, C::KeyC, C::KeyD, C::KeyE, C::KeyF,
                                C::InbetweenA, C::InbetweenB, C::InbetweenC, C::InbetweenD, C::InbetweenE, C::InbetweenF, C::InbetweenF }));

    // A chosen column wins; a sound layer stays on Dialogue.
    object.getLayer(2)->setTimesheetColumn(C::Camera);
    camera->setTimesheetColumn(C::KeyA);
    object.getLayer(1)->setTimesheetColumn(C::KeyB);
    CHECK(effectiveTimesheetColumn(object, 2) == C::Camera);
    CHECK(effectiveTimesheetColumn(object, 0) == C::KeyA);
    CHECK(effectiveTimesheetColumn(object, 1) == C::Dialogue);
    CHECK(effectiveTimesheetColumn(object, 3) == C::KeyB);
    CHECK(effectiveTimesheetColumn(object, 99) == C::Default);
}

TEST_CASE("The Timesheet column is saved on the layer", "[XSheet]")
{
    QDomDocument doc;
    LayerBitmap saved(1);
    REQUIRE_FALSE(saved.createBaseDomElement(doc).hasAttribute("timesheet"));

    const QList<QPair<TimesheetColumn, QString>> codes = { { TimesheetColumn::KeyA, "KA" }, { TimesheetColumn::KeyF, "KF" },
        { TimesheetColumn::InbetweenA, "IA" }, { TimesheetColumn::InbetweenF, "IF" }, { TimesheetColumn::Camera, "CAM" } };
    for (const auto& code : codes)
    {
        saved.setTimesheetColumn(code.first);
        const QDomElement elem = saved.createBaseDomElement(doc);
        CHECK(elem.attribute("timesheet") == code.second);
        LayerBitmap loaded(2);
        loaded.loadBaseDomElement(elem);
        CHECK(loaded.timesheetColumn() == code.first);
    }

    // Old files and unknown codes load as Default.
    LayerBitmap loaded(3);
    loaded.setTimesheetColumn(TimesheetColumn::KeyC);
    loaded.loadBaseDomElement(LayerBitmap(4).createBaseDomElement(doc));
    CHECK(loaded.timesheetColumn() == TimesheetColumn::Default);
    QDomElement bad = saved.createBaseDomElement(doc);
    bad.setAttribute("timesheet", "ZZ");
    loaded.loadBaseDomElement(bad);
    CHECK(loaded.timesheetColumn() == TimesheetColumn::Default);
}

TEST_CASE("Timesheet column changes undo and duplicates keep them", "[XSheet]")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    editor->layers()->setTimesheetColumn(layer, TimesheetColumn::InbetweenC);
    REQUIRE(layer->timesheetColumn() == TimesheetColumn::InbetweenC);
    CHECK(editor->layers()->duplicateLayer(layer, "Copy")->timesheetColumn() == TimesheetColumn::InbetweenC);
    undo->trigger(); // the duplicate
    undo->trigger();
    CHECK(layer->timesheetColumn() == TimesheetColumn::Default);
    redo->trigger();
    CHECK(layer->timesheetColumn() == TimesheetColumn::InbetweenC);
}

TEST_CASE("MainWindow2 X-sheet edits give the same project as timeline edits", "[XSheet]")
{
    QSettings& settings = pencilSettings();
    const SettingsGuard guard({SETTING_WINDOW_STATE, SETTING_WINDOW_GEOMETRY, SETTING_LAYOUT_VERSION, SETTING_WORKSPACE_CURRENT,
                               "Workspace/State/Animate", SETTING_XSHEET_LAYOUT});
    settings.remove(SETTING_WINDOW_STATE);
    settings.remove("Workspace/State/Animate");
    settings.remove(SETTING_WORKSPACE_CURRENT);
    const bool timesheet = GENERATE(false, true);
    settings.setValue(SETTING_XSHEET_LAYOUT, timesheet ? 1 : 0);

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
    // The Timesheet layout runs Keyframe A-F, Dialogue, In-between A-F, Camera left to right; Western follows the timeline.
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        object->getLayer(i)->setTimesheetColumn(i == index ? TimesheetColumn::Camera : TimesheetColumn::KeyA);
    }
    QList<int> timelineOrder;
    for (int i = object->getLayerCount() - 1; i >= 0; --i) { timelineOrder << i; }
    // Layer indices left to right on the X-sheet.
    QList<int> columns;
    for (int i = 0; i < object->getLayerCount(); ++i) { columns << i; }
    std::sort(columns.begin(), columns.end(), [xsheet](int a, int b) { return xsheet->cellRect(a, 1).left() < xsheet->cellRect(b, 1).left(); });
    QList<TimesheetColumn> order;
    for (int i : columns) { order << effectiveTimesheetColumn(*object, i); }
    if (timesheet)
    {
        CHECK(std::is_sorted(order.cbegin(), order.cend()));
        CHECK(order.first() == TimesheetColumn::KeyA);
        CHECK(columns != timelineOrder);
    }
    else { CHECK(columns == timelineOrder); }
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
    QAction* layoutToggle = action("actionXSheetTimesheet");
    REQUIRE(layoutToggle);
    CHECK(layoutToggle->isChecked() == timesheet);
    layoutToggle->toggle();
    CHECK(settings.value(SETTING_XSHEET_LAYOUT).toInt() == (timesheet ? 0 : 1));
    layoutToggle->toggle();
    CHECK(settings.value(SETTING_XSHEET_LAYOUT).toInt() == (timesheet ? 1 : 0));
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

TEST_CASE("The Timesheet layout shows every fixed column and an empty one takes no edits", "[XSheet]")
{
    const SettingsGuard guard({ SETTING_XSHEET_LAYOUT });
    TestSession s;
    LayerBitmap* key = s.object->addNewBitmapLayer();
    s.object->addNewCameraLayer();
    key->setTimesheetColumn(TimesheetColumn::KeyA);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    editor->preference()->set(SETTING::XSHEET_LAYOUT, 1);
    XSheet xsheet(nullptr);
    xsheet.setEditor(editor);
    xsheet.initUI();
    xsheet.resize(1600, 400);
    xsheet.show();
    QApplication::processEvents();

    const int keyIndex = 0;
    const int cameraIndex = 1;
    const QRect first = xsheet.cellRect(keyIndex, 3);
    // Key A-F, Dialogue, In-between A-F, Camera: Camera is 13 columns right of Key A.
    CHECK(xsheet.cellRect(cameraIndex, 3).left() - first.left() == 13 * first.width());

    editor->layers()->setCurrentLayer(cameraIndex);
    editor->scrubTo(1);
    QWidget* cells = xsheet.findChild<QAbstractScrollArea*>()->viewport();
    const QPoint keyC = first.center() + QPoint(2 * first.width(), 0);
    mouse(cells, QEvent::MouseButtonPress, keyC, Qt::LeftButton);
    mouse(cells, QEvent::MouseButtonRelease, keyC, Qt::NoButton);
    CHECK(editor->currentFrame() == 1);
    CHECK(editor->layers()->currentLayerIndex() == cameraIndex);
}

TEST_CASE("Layers that share a Timesheet column each get a sub-column bottom-up", "[XSheet]")
{
    const SettingsGuard guard({ SETTING_XSHEET_LAYOUT });
    TestSession s;
    Layer* bottom = s.object->addNewBitmapLayer();
    Layer* top = s.object->addNewBitmapLayer();
    for (Layer* layer : { bottom, top })
    {
        REQUIRE((layer->keyExists(1) || layer->addNewKeyFrameAt(1)));
        layer->setTimesheetColumn(TimesheetColumn::KeyA);
    }

    // Export: a Drawing on either layer shows, the bottom layer in the left half of Key A.
    PageSpec spec;
    spec.orientation = QPageLayout::Portrait;
    spec.dpi = 100;
    const QRectF page(QPointF(), QSizeF(spec.pixelSize()));
    TimesheetInfo info;
    info.length = 4;
    info.fps = 24;
    const QImage one = renderPage(spec, timesheetPages(*s.object, info), 0);
    const QRectF keyA = timesheetCell(page, 0, 0) | timesheetCell(page, 0, 3);
    const QRect left = QRectF(keyA.left(), keyA.top(), keyA.width() / 2, keyA.height()).toAlignedRect();
    const QRect right = QRectF(keyA.center().x(), keyA.top(), keyA.width() / 2, keyA.height()).toAlignedRect();
    for (Layer* layer : { bottom, top })
    {
        REQUIRE(layer->addNewKeyFrameAt(3));
        int count = 0;
        const QRect box = diffBox(one, renderPage(spec, timesheetPages(*s.object, info), 0), &count);
        CHECK(count > 5);
        CHECK((layer == bottom ? left : right).contains(box));
        REQUIRE(layer->removeKeyFrame(3));
    }

    // Panel: both layers show, side by side in Key A, bottom first.
    REQUIRE(s.start());
    s.editor->preference()->set(SETTING::XSHEET_LAYOUT, 1);
    XSheet xsheet(nullptr);
    xsheet.setEditor(s.editor.get());
    xsheet.initUI();
    const QRect b = xsheet.cellRect(0, 1);
    CHECK(xsheet.cellRect(1, 1).left() == b.left() + b.width());
}

TEST_CASE("X-sheet visibility has a command with no default key", "[XSheet]")
{
    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    REQUIRE(preset.contains(CMD_TOGGLE_XSHEET));
    CHECK(preset.value(CMD_TOGGLE_XSHEET).toString().isEmpty());
}

TEST_CASE("A Timesheet export has one sheet per 144 frames", "[XSheet]")
{
    CHECK(timesheetSheetCount(0) == 1);
    CHECK(timesheetSheetCount(1) == 1);
    CHECK(timesheetSheetCount(144) == 1);
    CHECK(timesheetSheetCount(145) == 2);
    CHECK(timesheetSheetCount(288) == 2);
    CHECK(timesheetSheetCount(289) == 3);
}

TEST_CASE("A rendered Timesheet sheet has its columns and frame numbers in known cells", "[XSheet]")
{
    Object object;
    object.init();
    PageSpec spec;
    spec.orientation = QPageLayout::Portrait;
    spec.dpi = 100;
    const QRectF page(QPointF(), QSizeF(spec.pixelSize()));
    TimesheetInfo info;
    info.length = 200;
    info.fps = 1; // marks 1+0 ... 288+0: digit counts differ between the sheets even where a font draws only boxes
    const QImage blank = renderPage(spec, timesheetPages(object, info), 0);
    const QImage second = renderPage(spec, timesheetPages(object, info), 1);
    CHECK(blank.height() > blank.width()); // portrait

    // Drawings at frames 1 and 3, length 4: ink in rows 0-3 of the layer's column only.
    Layer* layer = object.addNewBitmapLayer();
    REQUIRE((layer->keyExists(1) || layer->addNewKeyFrameAt(1)));
    REQUIRE(layer->addNewKeyFrameAt(3));
    info.length = 4;
    info.fps = 24;
    Object bare;
    bare.init();
    const QImage empty = renderPage(spec, timesheetPages(bare, info), 0);
    for (int c = static_cast<int>(TimesheetColumn::KeyA); c <= static_cast<int>(TimesheetColumn::Camera); ++c)
    {
        if (static_cast<TimesheetColumn>(c) == TimesheetColumn::Dialogue) { continue; } // sound layers only
        layer->setTimesheetColumn(static_cast<TimesheetColumn>(c));
        const QRectF cells = timesheetCell(page, c - 1, 0) | timesheetCell(page, c - 1, 3);
        int count = 0;
        const QRect box = diffBox(empty, renderPage(spec, timesheetPages(object, info), 0), &count);
        CHECK(count > 5);
        CHECK(cells.toAlignedRect().contains(box));
    }

    // Sheet 2 of 2 differs from sheet 1 only in its frame numbers and the header.
    const qreal headerBottom = timesheetCell(page, -1, 0).top() - timesheetCell(page, -1, 0).height() * 5;
    int inFirstBlock = 0, inSecondBlock = 0, elsewhere = 0;
    const QRect frames1 = (timesheetCell(page, -1, 0) | timesheetCell(page, -1, 71)).toAlignedRect();
    const QRect frames2 = (timesheetCell(page, -1, 72) | timesheetCell(page, -1, 143)).toAlignedRect();
    for (int y = 0; y < blank.height(); ++y)
    {
        for (int x = 0; x < blank.width(); ++x)
        {
            if (blank.pixel(x, y) == second.pixel(x, y)) { continue; }
            if (frames1.contains(x, y)) { ++inFirstBlock; }
            else if (frames2.contains(x, y)) { ++inSecondBlock; }
            else if (y >= headerBottom) { ++elsewhere; }
        }
    }
    CHECK(inFirstBlock > 20);
    CHECK(inSecondBlock > 20);
    CHECK(elsewhere == 0);
    CHECK(frames2.left() > frames1.right()); // the second 72 frames sit right of the first
}
