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
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "commandlineexporter.h"
#include "editor.h"
#include "exporter.h"
#include "filemanager.h"
#include "framerenderer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "object.h"
#include "playbackmanager.h"
#include "scribblearea.h"
#include "util.h"

namespace
{
const QSize kSize(40, 30);

// A camera and a bitmap layer with keys on frames 1 and 3; the canvas outside the rectangle stays empty.
std::unique_ptr<Object> project()
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    object->addNewCameraLayer()->setName("Cam");
    LayerBitmap* layer = object->addNewBitmapLayer();
    layer->setName("Ink");
    for (int f : { 1, 3 })
    {
        layer->addNewKeyFrameAt(f);
        layer->getBitmapImageAtFrame(f)->drawRect(QRectF(-5, -5, 10, 10), Qt::NoPen, QBrush(Qt::red), QPainter::CompositionMode_Source, false);
    }
    return object;
}

QStringList files(const QTemporaryDir& dir)
{
    return QDir(dir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

ExportRequest sequence(const QTemporaryDir& dir, const QString& format)
{
    ExportRequest r;
    r.kind = ExportRequest::Kind::Sequence;
    r.cameraName = "Cam";
    r.startFrame = 1;
    r.endFrame = 3;
    r.size = kSize;
    r.format = format;
    r.path = dir.filePath("seq");
    r.transparency = true;
    return r;
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    REQUIRE(f.open(QIODevice::ReadOnly));
    return f.readAll();
}
}

TEST_CASE("Exporter writes a sequence with the format's name size and transparency", "[Exporter]")
{
    auto object = project();
    struct Case { const char* format; const char* extension; bool alpha; };
    const Case cases[] = { { "PNG", ".png", true }, { "jpeg", ".jpg", false }, { "TIF", ".tiff", true }, { "bmp", ".bmp", false }, { "WEBP", ".webp", true } };
    for (const Case& c : cases)
    {
        if (!QImageWriter::supportedImageFormats().contains(QByteArray(c.extension + 1))) { continue; }
        CAPTURE(c.format);
        QTemporaryDir dir;
        REQUIRE(Exporter::run(*object, sequence(dir, c.format)).ok());
        REQUIRE(files(dir) == QStringList({ QString("seq0001") + c.extension, QString("seq0002") + c.extension, QString("seq0003") + c.extension }));
        const QImage image(dir.filePath(QString("seq0001") + c.extension));
        CHECK(image.size() == kSize);
        CHECK((image.pixelColor(1, 1).alpha() == 0) == c.alpha);
    }
}

TEST_CASE("Exporter writes one image of the start frame and adds the extension", "[Exporter]")
{
    auto object = project();
    QTemporaryDir dir;
    ExportRequest r = sequence(dir, "PNG");
    r.kind = ExportRequest::Kind::Image;
    r.path = dir.filePath("still");
    r.startFrame = 2;
    r.cameraName = "missing camera";
    REQUIRE(Exporter::run(*object, r).ok());
    REQUIRE(files(dir) == QStringList({ "still.png" }));
    CHECK(QImage(dir.filePath("still.png")).size() == kSize);
}

TEST_CASE("Exporter keyframes only writes the key frames of the layer", "[Exporter]")
{
    auto object = project();
    QTemporaryDir dir;
    ExportRequest r = sequence(dir, "png");
    r.path = dir.filePath("seq.png");
    r.keyframesOnly = true;
    r.keyLayerName = "Ink";
    REQUIRE(Exporter::run(*object, r).ok());
    CHECK(files(dir) == QStringList({ "seq0001.png", "seq0003.png" }));
}

TEST_CASE("Exporter sprite sheet is one PNG of the grid", "[Exporter]")
{
    auto object = project();
    QTemporaryDir dir;
    ExportRequest r = sequence(dir, "JPG");
    r.kind = ExportRequest::Kind::SpriteSheet;
    r.path = dir.filePath("sheet.png");
    REQUIRE(Exporter::run(*object, r).ok());
    REQUIRE(files(dir) == QStringList({ "sheet.png" }));
    const QImage sheet(r.path);
    CHECK(sheet.size() == QSize(80, 60));
    CHECK(sheet.pixelColor(1, 1).alpha() == 0);
}

TEST_CASE("Exporter parallel sequence writes the same files as serial encoding", "[Exporter]")
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    object->addNewCameraLayer()->setName("Cam");
    LayerBitmap* layer = object->addNewBitmapLayer();
    const int frames = parallelWorkerCount() * 2 + 3; // two full encoding chunks and a partial one
    for (int f = 1; f <= frames; ++f)
    {
        layer->addNewKeyFrameAt(f);
        layer->getBitmapImageAtFrame(f)->drawRect(QRectF(-200 + f * 20, -50, 60, 100), Qt::NoPen, QBrush(Qt::blue), QPainter::CompositionMode_Source, false);
    }
    QTemporaryDir dir;
    ExportRequest r = sequence(dir, "PNG");
    r.endFrame = frames;
    REQUIRE(Exporter::run(*object, r).ok());

    REQUIRE(files(dir).size() == frames);
    const LayerCamera* cam = Exporter::camera(*object, "Cam");
    for (int f = 1; f <= frames; ++f)
    {
        CAPTURE(f);
        QByteArray serial;
        QBuffer buffer(&serial);
        REQUIRE(buffer.open(QIODevice::WriteOnly));
        REQUIRE(QImageWriter(&buffer, "PNG").write(FrameRenderer::renderForExport(*object, f, *cam, { kSize, true, true })));
        CHECK(readFile(dir.filePath(QString("seq%1.png").arg(f, 4, 10, QChar('0')))) == serial);
    }
}

TEST_CASE("Exporter cancelled run leaves no files and keeps the old ones", "[Exporter]")
{
    auto object = project();
    QTemporaryDir dir;
    writeFile(dir.filePath("seq0001.png"), "old");
    int calls = 0;
    const Status st = Exporter::run(*object, sequence(dir, "PNG"), [&calls](int done, int total) {
        CHECK(total == 3);
        ++calls;
        return done < 2;
    });
    CHECK(st == Status::CANCELED);
    CHECK(calls == 2);
    CHECK(files(dir) == QStringList({ "seq0001.png" }));
    CHECK(readFile(dir.filePath("seq0001.png")) == "old");
}

TEST_CASE("Exporter failed run leaves no files", "[Exporter]")
{
    auto object = project();
    QTemporaryDir dir;
    ExportRequest r = sequence(dir, "PNG");
    r.path = dir.filePath("missing/seq");
    CHECK_FALSE(Exporter::run(*object, r).ok());
    CHECK(files(dir).isEmpty());

    // The last frame cannot be written: a folder holds its temporary name.
    REQUIRE(QDir(dir.path()).mkdir("seq0003.tmp.png"));
    CHECK_FALSE(Exporter::run(*object, sequence(dir, "PNG")).ok());
    CHECK(files(dir) == QStringList({ "seq0003.tmp.png" }));
}

TEST_CASE("Exporter asks once before it overwrites", "[Exporter]")
{
    auto object = project();
    QTemporaryDir dir;
    writeFile(dir.filePath("seq0001.png"), "old");
    writeFile(dir.filePath("seq0002.png"), "old");
    ExportRequest r = sequence(dir, "PNG");
    int asked = 0;
    bool answer = false;
    r.confirmOverwrite = [&](const QString&) { ++asked; return answer; };

    CHECK(Exporter::run(*object, r) == Status::CANCELED);
    CHECK(asked == 1);
    CHECK(readFile(dir.filePath("seq0002.png")) == "old");

    answer = true;
    REQUIRE(Exporter::run(*object, r).ok());
    CHECK(asked == 2);
    CHECK(QImage(dir.filePath("seq0002.png")).size() == kSize);

    QTemporaryDir fresh;
    r = sequence(fresh, "PNG");
    r.confirmOverwrite = [&](const QString&) { ++asked; return false; };
    REQUIRE(Exporter::run(*object, r).ok());
    CHECK(asked == 2);
}

TEST_CASE("Command line and dialog requests for the same range produce the same files", "[Exporter]")
{
    QTemporaryDir dir;
    const QString project = dir.filePath("p.pclx");
    {
        auto object = ::project();
        FileManager fm;
        REQUIRE(fm.save(object.get(), project).ok());
    }
    Editor editor;
    ScribbleArea scribbleArea(nullptr);
    editor.setScribbleArea(&scribbleArea);
    editor.setObject(new Object);
    editor.init();
    QDir(dir.path()).mkdir("cli");
    QDir(dir.path()).mkdir("dialog");

    for (const QString name : { "out.png", "out.gif" })
    {
        CAPTURE(name);
        const Status cli = CommandLineExporter(&editor).process(project, { dir.filePath("cli/" + name) }, "Cam", kSize.width(), kSize.height(), 1, 3, false);

        // As ActionCommands fills it from the dialog's defaults.
        ExportRequest r;
        r.kind = name.endsWith(".gif") ? ExportRequest::Kind::Movie : ExportRequest::Kind::Sequence;
        r.cameraName = "Cam";
        r.startFrame = 1;
        r.endFrame = 3;
        r.size = kSize;
        r.format = "PNG";
        r.path = dir.filePath("dialog/" + name);
        r.fps = editor.playback()->fps();
        const Status dialog = Exporter::run(*editor.object(), r);

        REQUIRE(cli.code() == dialog.code());
        if (cli == Status::ERROR_FFMPEG_NOT_FOUND) { WARN("ffmpeg not installed: skipping GIF compare"); continue; }
        REQUIRE(cli.ok());
    }
    const QStringList names = QDir(dir.filePath("cli")).entryList(QDir::Files);
    REQUIRE(names == QDir(dir.filePath("dialog")).entryList(QDir::Files));
    CHECK(names.contains("out0003.png"));
    for (const QString& n : names) { CHECK(readFile(dir.filePath("cli/" + n)) == readFile(dir.filePath("dialog/" + n))); }
}

TEST_CASE("Exporter asks before a movie overwrites its file", "[Exporter]")
{
    auto object = project();
    QTemporaryDir dir;
    ExportRequest r;
    r.kind = ExportRequest::Kind::Movie;
    r.cameraName = "Cam";
    r.size = kSize;
    r.path = dir.filePath("out.mp4");
    writeFile(r.path, "old");
    r.confirmOverwrite = [](const QString&) { return false; };
    CHECK(Exporter::run(*object, r) == Status::CANCELED);
    CHECK(readFile(r.path) == "old");
}
