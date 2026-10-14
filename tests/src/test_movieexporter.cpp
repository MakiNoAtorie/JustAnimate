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

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "movieexporter.h"
#include "pencilerror.h"

static QString makeFile(const QTemporaryDir& dir)
{
    QFile f(dir.filePath("out.mp4"));
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("partial");
    return f.fileName();
}

TEST_CASE("MovieExporter::discardPartialOutput")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = makeFile(dir);

    const QFileInfo before(path);

    SECTION("A file the failed run created is removed")
    {
        MovieExporter::discardPartialOutput(path, QDateTime(), 0);
        REQUIRE_FALSE(QFile::exists(path));
    }
    SECTION("A file the failed run began to overwrite is removed")
    {
        const QDateTime modifiedBefore = before.lastModified();
        const qint64 sizeBefore = before.size();
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write("cut");
        f.close();
        MovieExporter::discardPartialOutput(path, modifiedBefore, sizeBefore);
        REQUIRE_FALSE(QFile::exists(path));
    }
    SECTION("A file the failed run never touched is kept")
    {
        MovieExporter::discardPartialOutput(path, before.lastModified(), before.size());
        REQUIRE(QFile::exists(path));
    }
}

TEST_CASE("MovieExporter::mayOverwrite")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    int asked = 0;
    auto no = [&asked] { asked++; return false; };
    auto yes = [&asked] { asked++; return true; };

    SECTION("A new file does not ask")
    {
        REQUIRE(MovieExporter::mayOverwrite(dir.filePath("new.mp4"), no));
        REQUIRE(asked == 0);
    }
    SECTION("An existing file asks and honours a refusal")
    {
        REQUIRE_FALSE(MovieExporter::mayOverwrite(makeFile(dir), no));
        REQUIRE(asked == 1);
    }
    SECTION("An existing file asks and honours a confirmation")
    {
        REQUIRE(MovieExporter::mayOverwrite(makeFile(dir), yes));
        REQUIRE(asked == 1);
    }
}
