/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "catch.hpp"

#include <QTemporaryDir>
#include <QFile>
#include <QAction>
#include <QIcon>
#include "importlayersdialog.h"
#include "editor.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"


TEST_CASE("Import Layers from a corrupt project", "[ImportLayersDialog]")
{
    Object* object = new Object;
    object->init();
    object->addNewBitmapLayer();
    Editor editor;
    editor.setObject(object);

    ImportLayersDialog dialog(nullptr);
    dialog.setCore(&editor);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SECTION("A file that is not a project reports an error")
    {
        const QString path = dir.filePath("corrupt.pclx");
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("this is not a zip archive");
        file.close();

        Status st = dialog.loadFile(path);

        REQUIRE_FALSE(st.ok());
        REQUIRE(editor.object() == object);
        REQUIRE(object->getLayerCount() == 1);
    }

    SECTION("A file that cannot be read reports an error")
    {
        Status st = dialog.loadFile(dir.filePath("missing.pclx"));

        REQUIRE_FALSE(st.ok());
        REQUIRE(editor.object() == object);
        REQUIRE(object->getLayerCount() == 1);
    }
}

TEST_CASE("Import Layers is one undo step", "[ImportLayersDialog]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("source.pclx");
    QList<int> sourceIds;
    {
        Object source;
        source.init();
        source.addNewBitmapLayer()->setName("Imported bitmap");
        source.addNewVectorLayer()->setName("Imported vector");
        for (int i = 0; i < source.getLayerCount(); ++i) { sourceIds.append(source.getLayer(i)->id()); }
        FileManager fm;
        REQUIRE(fm.save(&source, path).ok());
    }

    TestSession session;
    session.object->addNewBitmapLayer()->setName("Own");
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    Object* object = editor->object();

    ImportLayersDialog dialog(nullptr);
    dialog.setCore(editor);
    REQUIRE(dialog.loadFile(path).ok());

    auto names = [&]
    {
        QStringList list;
        for (int i = 0; i < object->getLayerCount(); ++i) { list.append(object->getLayer(i)->name()); }
        return list;
    };
    int edits = 0;
    QObject::connect(editor->undoRedo(), &UndoRedoManager::didRecordEdit, [&edits] { ++edits; });

    dialog.importLayersById(sourceIds);
    const QStringList after{ "Own", "Imported bitmap", "Imported vector" };
    REQUIRE(names() == after);
    CHECK(edits == 1);

    QObject parent;
    editor->undoRedo()->createUndoAction(&parent, QIcon())->trigger();
    CHECK(names() == QStringList{ "Own" });
    editor->undoRedo()->createRedoAction(&parent, QIcon())->trigger();
    CHECK(names() == after);
}
