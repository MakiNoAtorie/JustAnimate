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

#include <QFileInfo>
#include <QTemporaryDir>

#include "commandlineexporter.h"
#include "editor.h"
#include "object.h"
#include "pencilerror.h"
#include "scribblearea.h"

QString QtResourceToFile(QString rscPath, QString filename, QTemporaryDir& tempDir); // test_filemanager.cpp

TEST_CASE("CommandLineExporter::process exit status")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString project = QtResourceToFile(":/empty.pclx", "empty.pclx", tempDir);

    Object* object = new Object;
    object->init();
    object->addNewCameraLayer();
    Editor editor;
    ScribbleArea scribbleArea(nullptr);
    editor.setScribbleArea(&scribbleArea);
    editor.setObject(object);
    editor.init();
    CommandLineExporter exporter(&editor);

    SECTION("Image sequence export succeeds")
    {
        Status st = exporter.process(project, { tempDir.filePath("out.png") }, "", 64, 48, 1, 1, false);
        REQUIRE(st.ok());
        REQUIRE(QFileInfo::exists(tempDir.filePath("out0001.png")));
    }

    SECTION("Missing input file fails")
    {
        Status st = exporter.process(tempDir.filePath("nope.pclx"), { tempDir.filePath("out.png") }, "", 64, 48, 1, 1, false);
        REQUIRE_FALSE(st.ok());
    }

    SECTION("Image sequence export into a missing folder fails")
    {
        Status st = exporter.process(project, { tempDir.filePath("missing/out.png") }, "", 64, 48, 1, 1, false);
        REQUIRE_FALSE(st.ok());
    }

    SECTION("Empty frame range fails")
    {
        Status st = exporter.process(project, { tempDir.filePath("out.png") }, "", 64, 48, 5, 2, false);
        REQUIRE_FALSE(st.ok());
        REQUIRE_FALSE(QFileInfo::exists(tempDir.filePath("out0005.png")));
    }

    SECTION("Movie export with an odd width fails")
    {
        // Fails either way: ffmpeg rejects odd H.264 dimensions, or ffmpeg is not installed.
        Status st = exporter.process(project, { tempDir.filePath("out.mp4") }, "", 63, 48, 1, 1, false);
        REQUIRE_FALSE(st.ok());
    }
}
