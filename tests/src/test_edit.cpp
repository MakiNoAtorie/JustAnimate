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
#include <QIcon>

#include "edit.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"

namespace
{
QList<int> keys(const Layer* layer)
{
    QList<int> positions;
    layer->foreachKeyFrame([&](KeyFrame* key) { positions.append(key->pos()); });
    std::sort(positions.begin(), positions.end());
    return positions;
}

struct EditFixture
{
    TestSession session;
    Layer* ink = session.object->addNewBitmapLayer();
    Layer* paint = session.object->addNewVectorLayer();
    Editor* editor = nullptr;
    QObject parent;
    QAction* undo = nullptr;
    int steps = 0;

    EditFixture()
    {
        REQUIRE(session.start());
        editor = session.editor.get();
        undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
        QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [this] { steps++; });
    }
};
}

TEST_CASE("Edit: commit adds one undo step")
{
    EditFixture f;
    {
        Edit edit = f.editor->edit(f.ink, "Move Frames");
        REQUIRE(f.ink->moveKeyFrame(1, 2));
        edit.commit();
    }
    CHECK(f.steps == 1);
    CHECK(f.undo->text().contains("Move Frames"));
    CHECK(f.editor->undoRedo()->pendingStateCount() == 0);

    f.undo->trigger();
    CHECK(keys(f.ink) == QList<int>{ 1 });
}

TEST_CASE("Edit: scope exit without commit adds no undo step")
{
    EditFixture f;
    {
        Edit edit = f.editor->edit(f.ink, "Move Frames");
        REQUIRE(f.ink->moveKeyFrame(1, 2));
    }
    CHECK(f.steps == 0);
    CHECK_FALSE(f.undo->isEnabled());
    CHECK(f.editor->undoRedo()->pendingStateCount() == 0);
}

TEST_CASE("Edit: nested Edits add one undo step")
{
    EditFixture f;
    {
        Edit outer = f.editor->edit(f.ink, "Insert Frame");
        {
            Edit add = f.editor->edit(f.ink, 3, "New drawing");
            REQUIRE(f.ink->addNewKeyFrameAt(3));
            add.commit();
        }
        {
            Edit move = f.editor->edit(f.ink, "Insert Frame");
            REQUIRE(f.ink->moveKeyFrame(3, 2));
            move.commit();
        }
        outer.commit();
    }
    CHECK(f.steps == 1);
    CHECK(f.undo->text().contains("Insert Frame"));

    f.undo->trigger();
    CHECK(keys(f.ink) == QList<int>{ 1 });
    CHECK_FALSE(f.undo->isEnabled());
}

TEST_CASE("Edit: a multi-layer Edit adds one undo step")
{
    EditFixture f;
    {
        Edit outer = f.editor->edit("Move Frames");
        for (Layer* layer : { f.ink, f.paint })
        {
            Edit edit = f.editor->edit(layer, "Move Frames");
            REQUIRE(layer->addNewKeyFrameAt(4));
            REQUIRE(layer->moveKeyFrame(1, 2));
            edit.commit();
        }
        outer.commit();
    }
    CHECK(f.steps == 1);

    f.undo->trigger();
    CHECK(keys(f.ink) == QList<int>{ 1 });
    CHECK(keys(f.paint) == QList<int>{ 1 });
    CHECK_FALSE(f.undo->isEnabled());
}

TEST_CASE("Edit: a removed key comes back on undo")
{
    EditFixture f;
    REQUIRE(f.ink->addNewKeyFrameAt(5));
    {
        Edit edit = f.editor->edit(f.ink, "Remove drawing");
        REQUIRE(f.ink->removeKeyFrame(5));
        edit.commit();
    }
    CHECK(f.steps == 1);
    CHECK(keys(f.ink) == QList<int>{ 1 });

    f.undo->trigger();
    CHECK(keys(f.ink) == QList<int>{ 1, 5 });
}
