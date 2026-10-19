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

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "log.h"

static QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

TEST_CASE("A warning goes to the session log", "[Log]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = startSessionLog(dir.path(), { "Version: test" });
    REQUIRE_FALSE(path.isEmpty());
    REQUIRE(sessionLogPath() == path);

    qWarning("log test warning %d", 42);
    const QByteArray text = readFile(path);
    stopSessionLog();

    REQUIRE(text.startsWith("Version: test\n"));
    REQUIRE(text.contains("warning default: log test warning 42"));
    REQUIRE(sessionLogPath().isEmpty());
}

TEST_CASE("Old session logs beyond the cap are deleted", "[Log]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    for (int i = 0; i < 12; ++i)
    {
        QFile old(QDir(dir.path()).filePath(QString("session-20000101-000000-%1-1.log").arg(i, 3, 10, QChar('0'))));
        REQUIRE(old.open(QIODevice::WriteOnly));
    }
    const QString path = startSessionLog(dir.path(), {}, 10);
    stopSessionLog();

    const QStringList logs = QDir(dir.path()).entryList({ "session-*.log" }, QDir::Files, QDir::Name);
    REQUIRE(logs.size() == 10);
    REQUIRE(logs.first() == "session-20000101-000000-003-1.log");
    REQUIRE(logs.last() == QFileInfo(path).fileName());
}

TEST_CASE("Export Log writes this session and the previous one", "[Log]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    {
        QFile previous(QDir(dir.path()).filePath("session-20000101-000000-000-1.log"));
        REQUIRE(previous.open(QIODevice::WriteOnly));
        previous.write("previous session line\n");
    }
    REQUIRE_FALSE(startSessionLog(dir.path(), {}).isEmpty());
    qWarning("export test warning");
    const QString target = QDir(dir.path()).filePath("export.txt");
    const bool exported = exportSessionLogs(target);
    stopSessionLog();

    REQUIRE(exported);
    const QByteArray text = readFile(target);
    REQUIRE(text.contains("export test warning"));
    REQUIRE(text.contains("previous session line"));
    REQUIRE(text.indexOf("export test warning") < text.indexOf("previous session line"));
    REQUIRE_FALSE(exportSessionLogs(target)); // no session open
}
