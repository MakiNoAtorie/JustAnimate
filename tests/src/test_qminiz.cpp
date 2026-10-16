/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2026-2099 Oliver S. Larsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "catch.hpp"
#include "qminiz.h"

#include <atomic>
#include <cstring>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

// https://github.com/pencil2d/pencil/issues/2009
TEST_CASE("QMiniZ::CompressFolder")
{
    SECTION("Verify robustness against paths with different roots")
    {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());

        QString subFolder = tempDir.path() + "/bar/";
        QDir().mkpath(subFolder);
        QFile file(subFolder + "biz.mp3");
        file.open(QIODevice::WriteOnly);
        file.close();

        QString zipPath = tempDir.path() + "/test.pclx";

        // Simulate root mismatch by using canonical path for file
        // but non-canonical path for the base
        QString canonicalFile = QFileInfo(subFolder + "biz.mp3").canonicalFilePath();
        QString nonCanonicalBase = tempDir.path();

        QList<QString> filesToZip = { canonicalFile };

        Status st = MiniZ::compressFolder(zipPath, nonCanonicalBase, filesToZip, "application/x-pencil2d-pclx");
        REQUIRE(st.ok());

        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        REQUIRE(mz_zip_reader_init_file(&zip, zipPath.toUtf8().data(), 0));

        int fileIndex = mz_zip_reader_locate_file(&zip, "bar/biz.mp3", nullptr, 0);
        REQUIRE(fileIndex >= 0);

        mz_zip_reader_end(&zip);
    }
}

TEST_CASE("QMiniZ::uncompressFolder rejects zip-slip entries")
{
    const QStringList evil = { "../evil.txt", "a/../../evil.txt", "/abs/evil.txt",
                               "C:/evil.txt", "C:evil.txt", "..\\evil.txt", "a\\..\\..\\evil.txt" };
    for (const QString& name : evil)
    {
        DYNAMIC_SECTION(name.toStdString())
        {
            QTemporaryDir tempDir;
            REQUIRE(tempDir.isValid());
            QString zipPath = tempDir.path() + "/evil.pclx";
            QDir().mkpath(tempDir.path() + "/out");

            mz_zip_archive zip;
            memset(&zip, 0, sizeof(zip));
            REQUIRE(mz_zip_writer_init_file(&zip, zipPath.toUtf8().data(), 0));
            REQUIRE(mz_zip_writer_add_mem(&zip, "ok.txt", "x", 1, MZ_NO_COMPRESSION));
            // The writer refuses hostile names, so patch a same-length placeholder.
            const QByteArray hostile = name.toUtf8();
            const QByteArray placeholder(hostile.size(), 'Z');
            REQUIRE(mz_zip_writer_add_mem(&zip, placeholder.constData(), "x", 1, MZ_NO_COMPRESSION));
            REQUIRE(mz_zip_writer_finalize_archive(&zip));
            mz_zip_writer_end(&zip);
            QFile zf(zipPath);
            REQUIRE(zf.open(QIODevice::ReadWrite));
            QByteArray bytes = zf.readAll();
            bytes.replace(placeholder, hostile);
            zf.seek(0);
            zf.write(bytes);
            zf.close();

            Status st = MiniZ::uncompressFolder(zipPath, tempDir.path() + "/out/");
            REQUIRE_FALSE(st.ok());
            REQUIRE_FALSE(QFileInfo::exists(tempDir.path() + "/evil.txt"));
            REQUIRE_FALSE(QFileInfo::exists(tempDir.path() + "/out/ok.txt"));
        }
    }

    SECTION("backslash entry inside the folder is accepted")
    {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        QString zipPath = tempDir.path() + "/bs.pclx";
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        REQUIRE(mz_zip_writer_init_file(&zip, zipPath.toUtf8().data(), 0));
        REQUIRE(mz_zip_writer_add_mem(&zip, "dataZ001.png", "x", 1, MZ_NO_COMPRESSION));
        REQUIRE(mz_zip_writer_finalize_archive(&zip));
        mz_zip_writer_end(&zip);
        QFile zf(zipPath);
        REQUIRE(zf.open(QIODevice::ReadWrite));
        QByteArray bytes = zf.readAll();
        bytes.replace("dataZ001.png", "data\\001.png");
        zf.seek(0);
        zf.write(bytes);
        zf.close();

        REQUIRE(MiniZ::uncompressFolder(zipPath, tempDir.path() + "/out/").ok());
        REQUIRE_FALSE(QFileInfo::exists(tempDir.path() + "/001.png"));
    }

    SECTION("a folder that cannot be made fails the extract")
    {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        QString zipPath = tempDir.path() + "/blocked.pclx";
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        REQUIRE(mz_zip_writer_init_file(&zip, zipPath.toUtf8().data(), 0));
        REQUIRE(mz_zip_writer_add_mem(&zip, "data/a.txt", "x", 1, MZ_NO_COMPRESSION));
        REQUIRE(mz_zip_writer_finalize_archive(&zip));
        mz_zip_writer_end(&zip);

        // A file where the archive wants a folder.
        QDir().mkpath(tempDir.path() + "/out");
        QFile blocker(tempDir.path() + "/out/data");
        REQUIRE(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        REQUIRE_FALSE(MiniZ::uncompressFolder(zipPath, tempDir.path() + "/out/").ok());
    }

    SECTION("normal nested entries still extract")
    {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        QString zipPath = tempDir.path() + "/good.pclx";
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        REQUIRE(mz_zip_writer_init_file(&zip, zipPath.toUtf8().data(), 0));
        REQUIRE(mz_zip_writer_add_mem(&zip, "data/a.txt", "x", 1, MZ_NO_COMPRESSION));
        REQUIRE(mz_zip_writer_finalize_archive(&zip));
        mz_zip_writer_end(&zip);

        REQUIRE(MiniZ::uncompressFolder(zipPath, tempDir.path() + "/out/").ok());
        REQUIRE(QFileInfo::exists(tempDir.path() + "/out/data/a.txt"));
    }
}

TEST_CASE("QMiniZ::uncompressFolder extracts many entries in parallel byte for byte", "[ParallelOpen]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString zipPath = tempDir.path() + "/many.pclx";
    const int count = 101; // not a multiple of the worker count
    auto nameAt = [](int i) { return QString("d%1/sub/f%2.bin").arg(i % 3).arg(i).toUtf8(); };
    auto bytesAt = [](int i) { return QByteArray(i * 37 + 1, char('a' + i % 26)); };

    // No folder entries: extraction must make the folders itself.
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    REQUIRE(mz_zip_writer_init_file(&zip, zipPath.toUtf8().data(), 0));
    for (int i = 0; i < count; ++i)
    {
        const QByteArray bytes = bytesAt(i);
        REQUIRE(mz_zip_writer_add_mem(&zip, nameAt(i).constData(), bytes.constData(), bytes.size(), MZ_DEFAULT_COMPRESSION));
    }
    REQUIRE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);

    // Progress runs on the workers, so it only counts; Catch2 asserts stay on this thread.
    std::atomic<int> calls{0};
    std::atomic<bool> wrongTotal{false};
    REQUIRE(MiniZ::uncompressFolder(zipPath, tempDir.path() + "/out/", [&](int, int total) {
        ++calls;
        if (total != count) wrongTotal = true;
    }).ok());
    REQUIRE(calls == count);
    REQUIRE_FALSE(wrongTotal);

    for (int i = 0; i < count; ++i)
    {
        CAPTURE(i);
        QFile f(tempDir.path() + "/out/" + QString::fromUtf8(nameAt(i)));
        REQUIRE(f.open(QIODevice::ReadOnly));
        const bool same = f.readAll() == bytesAt(i);
        REQUIRE(same);
    }
}
