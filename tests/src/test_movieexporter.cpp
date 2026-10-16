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
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
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

static QByteArray readAll(const QString& path)
{
    QFile f(path);
    REQUIRE(f.open(QIODevice::ReadOnly));
    return f.readAll();
}

TEST_CASE("MovieExporter::writeThenReplace")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString target = dir.filePath("out.mp4");
    QString written;
    auto partial = [&written](Status result) {
        return [&written, result](const QString& path) {
            written = path;
            QFile f(path);
            REQUIRE(f.open(QIODevice::WriteOnly));
            f.write("cut");
            return result;
        };
    };

    SECTION("A cancelled export keeps the previous target and leaves no temporary file")
    {
        makeFile(dir);
        REQUIRE((MovieExporter::writeThenReplace(target, partial(Status::CANCELED)) == Status::CANCELED));
        REQUIRE(written != target);
        REQUIRE(written.endsWith(".mp4"));
        REQUIRE(readAll(target) == "partial");
        REQUIRE(QDir(dir.path()).entryList(QDir::Files) == QStringList("out.mp4"));
    }
    SECTION("A failed export with no target leaves no file")
    {
        REQUIRE((MovieExporter::writeThenReplace(target, partial(Status::FAIL)) == Status::FAIL));
        REQUIRE(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }
    SECTION("A successful export replaces the target with the new movie")
    {
        makeFile(dir);
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty()) { WARN("ffmpeg not installed: skipping encode check"); return; }
        auto encode = [&ffmpeg](const QString& path) {
            const QStringList args = { "-f", "lavfi", "-i", "testsrc=size=64x64:rate=6:duration=1", "-pix_fmt", "yuv420p", "-y", path };
            return MovieExporter::executeFFmpeg(ffmpeg, args, [](int) { return true; });
        };
        REQUIRE(MovieExporter::writeThenReplace(target, encode).ok());
        REQUIRE(readAll(target).mid(4, 4) == "ftyp");
        REQUIRE(QDir(dir.path()).entryList(QDir::Files) == QStringList("out.mp4"));
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

TEST_CASE("MovieExporter export presets")
{
    SECTION("quality presets map to ffmpeg args")
    {
        REQUIRE(MovieExporter::qualityArgs("a.mp4", MovieQuality::Low) == QStringList({ "-crf", "28" }));
        REQUIRE(MovieExporter::qualityArgs("a.MP4", MovieQuality::High) == QStringList({ "-crf", "18" }));
        REQUIRE(MovieExporter::qualityArgs("a.webm", MovieQuality::Medium) == QStringList({ "-crf", "32", "-b:v", "0" }));
        REQUIRE(MovieExporter::qualityArgs("a.gif", MovieQuality::High).isEmpty());
    }

    SECTION("gif palette options map to filter args")
    {
        REQUIRE(MovieExporter::gifFilter(16, false) == "[0:v]palettegen=max_colors=16 [p]; [0:v][p] paletteuse=dither=none");
        REQUIRE(MovieExporter::gifFilter(999, true).contains("max_colors=256"));
        REQUIRE(MovieExporter::gifFilter(1, true).contains("max_colors=3"));
        REQUIRE(MovieExporter::gifFilter(64, true).contains("dither=sierra2_4a"));
    }

    SECTION("ffmpeg writes a valid file per preset")
    {
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty()) { WARN("ffmpeg not installed: skipping encode check"); return; }

        QTemporaryDir dir;
        auto run = [&](const QString& name, const QStringList& extra) {
            QStringList args = { "-f", "lavfi", "-i", "testsrc=size=64x64:rate=6:duration=1" };
            args << extra << "-y" << dir.filePath(name);
            REQUIRE(MovieExporter::executeFFmpeg(ffmpeg, args, [](int) { return true; }).ok());
            return QFileInfo(dir.filePath(name)).size();
        };
        auto mp4 = [&](MovieQuality q) {
            return run(QString("q%1.mp4").arg(int(q)), QStringList({ "-pix_fmt", "yuv420p" }) + MovieExporter::qualityArgs("x.mp4", q));
        };
        REQUIRE(mp4(MovieQuality::Low) > 0);
        REQUIRE(mp4(MovieQuality::Low) < mp4(MovieQuality::High));

        auto gif = [&](int colors, bool dither) {
            return run(QString("c%1%2.gif").arg(colors).arg(dither), { "-filter_complex", MovieExporter::gifFilter(colors, dither) });
        };
        REQUIRE(gif(4, false) != gif(256, true));
    }
}

TEST_CASE("MovieExporter pipe holds few unwritten frames")
{
    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg.isEmpty()) { WARN("ffmpeg not installed: skipping pipe check"); return; }

    QImage frame(1920, 1080, QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::white);
    const qint64 frameBytes = frame.sizeInBytes();
    const QStringList input = { "-f", "rawvideo", "-pixel_format", "bgra", "-video_size", "1920x1080", "-framerate", "12" };
    const QStringList output = { "-i", "-", "-f", "null", "-" };
    MovieExporter ex;
    int written = 0;

    SECTION("a 100-frame 1080p export stays under the bound")
    {
        auto next = [&](QImage& img) { img = frame; return written++ < 100; };
        REQUIRE(ex.executeFFMpegPipe(ffmpeg, input + output, [](float) {}, next).ok());
        REQUIRE(written == 101);
        REQUIRE(ex.peakQueuedBytes() > 0);
        REQUIRE(ex.peakQueuedBytes() <= MovieExporter::MAX_QUEUED_FRAMES * frameBytes);
    }

    SECTION("cancel ends a waiting export within a second")
    {
        // -re makes ffmpeg read at 12 fps, so the renderer waits on the pipe.
        auto next = [&](QImage& img) { img = frame; ++written; return true; };
        QElapsedTimer sinceCancel;
        auto progress = [&](float) {
            if (written == 20 && !sinceCancel.isValid()) { sinceCancel.start(); ex.cancel(); }
        };
        const Status st = ex.executeFFMpegPipe(ffmpeg, input + QStringList("-re") + output, progress, next);
        REQUIRE((st == Status::CANCELED));
        REQUIRE(sinceCancel.elapsed() < 1000);
    }
}
