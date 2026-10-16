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

#include <memory>
#include <QAction>
#include <QDomDocument>
#include <QIcon>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextStream>

#include "filemanager.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"

namespace
{

/// Layer names bottom first, with "*" after a group member.
QString arrangement(const Object* object)
{
    QStringList names;
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        const Layer* layer = object->getLayer(i);
        names << layer->name() + (layer->groupId() ? "*" : "");
    }
    return names.join(" ");
}

/// Camera, then bitmap layers A B C D bottom to top.
void addLayers(Object* object)
{
    object->addNewCameraLayer()->setName("Cam");
    for (const char* name : { "A", "B", "C", "D" }) { object->addNewBitmapLayer()->setName(name); }
}

} // namespace

TEST_CASE("Layer groups survive save and load", "[LayerGroup]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("groups.pclx");
    std::unique_ptr<Object> object(new Object);
    object->init();
    addLayers(object.get());
    object->addLayerGroup(LayerGroup{ 7, "Hair", true, false, 0.5 });
    object->getLayer(2)->setGroupId(7);
    object->getLayer(3)->setGroupId(7);

    FileManager fm;
    REQUIRE(fm.save(object.get(), path).ok());
    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);

    REQUIRE(loaded->layerGroups().size() == 1);
    const LayerGroup group = loaded->layerGroups().first();
    CHECK(group.id == 7);
    CHECK(group.name == "Hair");
    CHECK(group.collapsed);
    CHECK_FALSE(group.visible);
    CHECK(group.opacity == Approx(0.5));
    CHECK(arrangement(loaded.get()) == "Cam A B* C* D");
}

TEST_CASE("A file without layer groups loads and saves unchanged", "[LayerGroup]")
{
    QTemporaryFile file;
    REQUIRE(file.open());
    QTextStream(&file) << "<!DOCTYPE PencilDocument><document><object>"
                          "<layer name='A' id='1' visibility='1' type='1'></layer>"
                          "<layer name='B' id='2' visibility='1' type='1' group='9'></layer>"
                          "</object></document>";
    file.close();

    FileManager fm;
    std::unique_ptr<Object> object(fm.load(file.fileName()));
    REQUIRE(object != nullptr);
    CHECK(object->layerGroups().isEmpty());
    CHECK(arrangement(object.get()) == "A B Camera Layer"); // a member of a missing group loads ungrouped

    QDomDocument doc;
    const QDomElement saved = object->saveXML(doc);
    CHECK(saved.elementsByTagName("layerGroup").isEmpty());
    const QDomNodeList layers = saved.elementsByTagName("layer");
    for (int i = 0; i < layers.size(); ++i) { CHECK_FALSE(layers.at(i).toElement().hasAttribute("group")); }
}

TEST_CASE("Group edits and reorders undo and redo", "[LayerGroup]")
{
    TestSession session;
    addLayers(session.object);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());
    const Object* object = editor->object();
    auto layer = [&](const char* name) { return object->findLayerByName(name); };

    const int group = layers->createLayerGroup(layer("B"));
    REQUIRE(group != 0);
    CHECK(arrangement(object) == "Cam A B* C D");
    REQUIRE(editor->undoRedo()->hasUnsavedChanges());

    layers->addLayerToGroup(layer("D"), group); // moves next to the group
    CHECK(arrangement(object) == "Cam A B* D* C");

    undo->trigger();
    CHECK(arrangement(object) == "Cam A B* C D");
    redo->trigger();
    CHECK(arrangement(object) == "Cam A B* D* C");

    layers->removeLayerFromGroup(layer("B")); // leaves the group on the outside
    CHECK(arrangement(object) == "Cam A B D* C");
    undo->trigger();
    CHECK(arrangement(object) == "Cam A B* D* C");

    undo->trigger();
    undo->trigger();
    CHECK(arrangement(object) == "Cam A B C D");
    CHECK(object->layerGroups().isEmpty());
    redo->trigger();
    CHECK(object->layerGroups().size() == 1);
    CHECK(arrangement(object) == "Cam A B* C D");
}

TEST_CASE("Reordering keeps group members together", "[LayerGroup]")
{
    TestSession session;
    addLayers(session.object);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    const Object* object = editor->object();
    auto layer = [&](const char* name) { return object->findLayerByName(name); };

    const int group = layers->createLayerGroup(layer("B"));
    layers->addLayerToGroup(layer("C"), group);
    REQUIRE(arrangement(object) == "Cam A B* C* D");

    SECTION("A member moves only inside its group")
    {
        REQUIRE(layers->moveLayer(2, 4));
        CHECK(arrangement(object) == "Cam A C* B* D");
        undo->trigger();
        CHECK(arrangement(object) == "Cam A B* C* D");
    }
    SECTION("A layer moves over the group, not into it")
    {
        REQUIRE(layers->moveLayer(1, 2)); // A above B
        CHECK(arrangement(object) == "Cam B* C* A D");
        REQUIRE(layers->moveLayer(4, 2)); // D below C
        CHECK(arrangement(object) == "Cam D B* C* A");
    }
    SECTION("A collapsed group moves as a block")
    {
        layers->setLayerGroupCollapsed(group, true);
        CHECK(object->isLayerFolded(2));
        CHECK_FALSE(object->isLayerFolded(3)); // the top member keeps its row
        REQUIRE(layers->moveLayer(3, 4)); // the group above D
        CHECK(arrangement(object) == "Cam A D B* C*");
        undo->trigger();
        CHECK(arrangement(object) == "Cam A B* C* D");
    }
    SECTION("The bottom camera layer stays at the bottom")
    {
        CHECK_FALSE(layers->moveLayer(0, 2));
        REQUIRE(layers->moveLayer(4, 0));
        CHECK(arrangement(object) == "Cam D A B* C*");
    }
}
