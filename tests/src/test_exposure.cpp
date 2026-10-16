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
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QSettings>

#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "testfixture.h"
#include "timeline.h"
#include "timelinecells.h"
#include "undoredomanager.h"

namespace
{

QList<int> keyPositions(const Layer* layer)
{
    QList<int> positions;
    layer->foreachKeyFrame([&](const KeyFrame* key) { positions.prepend(key->pos()); });
    return positions;
}

} // namespace

TEST_CASE("Exposure + and - do nothing with no current layer", "[Exposure]")
{
    TestSession session;
    REQUIRE(session.start());
    REQUIRE(session.object->getLayerCount() == 0);
    CHECK_FALSE(session.editor->layers()->changeExposure(1, 1));
    CHECK_FALSE(session.editor->layers()->changeExposure(1, -1));
}

TEST_CASE("Exposure + and - change the current Drawing and ripple later Drawings", "[Exposure]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(3);
    layer->addNewKeyFrameAt(5);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(4); // a hold of the Drawing at 3; nothing selected
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());

    REQUIRE(editor->layers()->changeExposure(editor->currentFrame(), 1));
    REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 6 }));
    REQUIRE(editor->layers()->changeExposure(editor->currentFrame(), -1));
    REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 5 }));
    REQUIRE(editor->layers()->changeExposure(editor->currentFrame(), -1));
    REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 4 }));

    SECTION("- never shortens an Exposure below one frame")
    {
        REQUIRE_FALSE(editor->layers()->changeExposure(3, -1));
        REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 4 }));
    }
    SECTION("Each change is one undo step")
    {
        undo->trigger();
        REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 5 }));
        undo->trigger();
        REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 6 }));
        undo->trigger();
        REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 5 }));
        REQUIRE_FALSE(undo->isEnabled());
    }
}

TEST_CASE("Dragging a cell's right edge changes its Exposure with ripple", "[Exposure]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer(); // the top row
    layer->addNewKeyFrameAt(3);
    layer->addNewKeyFrameAt(5);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    TimeLine timeline(nullptr);
    timeline.setEditor(editor);
    timeline.initUI();
    timeline.resize(800, 300);
    timeline.grab(); // lays out the timeline
    // The tracks sit right of the layer names.
    const QList<TimeLineCells*> cells = timeline.findChildren<TimeLineCells*>();
    TimeLineCells* tracks = *std::max_element(cells.cbegin(), cells.cend(), [&](TimeLineCells* a, TimeLineCells* b) {
        return a->mapTo(&timeline, QPoint()).x() < b->mapTo(&timeline, QPoint()).x(); });

    const int size = tracks->getFrameSize();
    const int y = TimeLineCells::getOffsetY() + tracks->getLayerHeight() / 2;
    auto mouse = [&](QEvent::Type type, int x, Qt::MouseButtons buttons) {
        const QPointF pos(x, y);
        QMouseEvent e(type, pos, tracks->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QApplication::sendEvent(tracks, &e);
    };
    // The Drawing at 3 ends at the right edge of frame 4; drop it over frame 6.
    const int edge = TimeLineCells::getOffsetX() + 4 * size - 1;
    mouse(QEvent::MouseButtonPress, edge, Qt::LeftButton);
    mouse(QEvent::MouseMove, 6 * size - size / 2, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, 6 * size - size / 2, Qt::NoButton);
    REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 7 }));

    // Dragging left past the Drawing keeps one frame.
    mouse(QEvent::MouseButtonPress, 6 * size - 2, Qt::LeftButton);
    mouse(QEvent::MouseMove, size / 2, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, size / 2, Qt::NoButton);
    REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 4 }));

    undo->trigger();
    REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 7 }));
    undo->trigger();
    REQUIRE(keyPositions(layer) == QList<int>({ 1, 3, 5 }));
    REQUIRE_FALSE(undo->isEnabled());
}

TEST_CASE("Exposure and ripple edits have default keys", "[Exposure]")
{
    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    CHECK(QKeySequence(preset.value("CmdExposureAdd").toString()) == QKeySequence(Qt::Key_Plus));
    CHECK(QKeySequence(preset.value("CmdExposureSubtract").toString()) == QKeySequence(Qt::Key_Minus));
    CHECK(QKeySequence(preset.value("CmdRippleInsertFrame").toString()) == QKeySequence("Ctrl+F7"));
    CHECK(QKeySequence(preset.value("CmdRippleDeleteFrame").toString()) == QKeySequence("Ctrl+F5"));
}
