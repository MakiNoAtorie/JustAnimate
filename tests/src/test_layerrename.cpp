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
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>

#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "testfixture.h"
#include "timelinecells.h"
#include "timeline.h"
#include "mainwindow2.h"
#include "pencildef.h"
#include "pencilsettings.h"

#include <QAction>

TEST_CASE("Double-click and F2 rename a layer in place", "[LayerRename]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer(); // the top row
    layer->setName("Old");
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    TimeLineCells cells(nullptr, editor, TIMELINE_CELL_TYPE::Layers);
    TimeLineCells* names = &cells;
    names->resize(300, 200);
    names->show();

    // A cancelled editor is hidden and deleted later, so only a visible one counts.
    auto edit = [&]() -> QLineEdit* {
        for (QLineEdit* e : names->findChildren<QLineEdit*>()) { if (e->isVisible()) { return e; } }
        return nullptr;
    };
    auto key = [&](QLineEdit* e, int k) {
        QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier);
        QApplication::sendEvent(e, &press);
    };
    auto doubleClick = [&] {
        const QPointF pos(120, TimeLineCells::getOffsetY() + names->getLayerHeight() / 2);
        QMouseEvent e(QEvent::MouseButtonDblClick, pos, names->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(names, &e);
    };

    REQUIRE(edit() == nullptr);
    doubleClick();
    REQUIRE(edit() != nullptr);
    REQUIRE(edit()->text() == "Old");

    // Esc cancels
    edit()->setText("Esc");
    key(edit(), Qt::Key_Escape);
    REQUIRE(layer->name() == "Old");
    REQUIRE(edit() == nullptr);

    // An empty name is refused and the editor stays open
    doubleClick();
    QLineEdit* open = edit();
    open->setText("");
    key(open, Qt::Key_Return);
    REQUIRE(layer->name() == "Old");
    REQUIRE(edit() == open);

    // Enter accepts
    open->setText("New");
    key(open, Qt::Key_Return);
    REQUIRE(layer->name() == "New");

    // F2 opens the editor on the current layer
    QKeyEvent f2(QEvent::KeyPress, Qt::Key_F2, Qt::NoModifier);
    QApplication::sendEvent(names, &f2);
    REQUIRE(edit() != nullptr);
    REQUIRE(edit()->text() == "New");
}

// Needs MainWindow2: the Rename Layer action and its key live there.
TEST_CASE("Rename Layer action has F2 and renames the current layer in place", "[LayerRename]")
{
    const SettingsGuard guard({ "shortcuts/" CMD_RENAME_LAYER });
    pencilSettings().remove("shortcuts/" CMD_RENAME_LAYER);
    checkExistingShortcuts();
    MainWindow2 window;
    Editor* editor = window.findChild<Editor*>();
    REQUIRE(editor);
    auto* action = window.findChild<QAction*>("actionRename_Layer");
    REQUIRE(action);
    CHECK(action->shortcut() == QKeySequence(Qt::Key_F2));

    auto* timeline = window.findChild<TimeLine*>();
    REQUIRE(timeline);
    auto renameEdit = [&]() -> QLineEdit* {
        for (TimeLineCells* cells : timeline->findChildren<TimeLineCells*>())
        {
            for (QLineEdit* e : cells->findChildren<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly)) { if (!e->isHidden()) { return e; } }
        }
        return nullptr;
    };
    REQUIRE(renameEdit() == nullptr);
    action->trigger();
    QLineEdit* edit = renameEdit();
    REQUIRE(edit);
    CHECK(edit->text() == editor->layers()->currentLayer()->name());
}
