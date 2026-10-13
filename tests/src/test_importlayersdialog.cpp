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
#include "importlayersdialog.h"
#include "editor.h"
#include "object.h"


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
