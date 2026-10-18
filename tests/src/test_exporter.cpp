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
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "exporter.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "object.h"

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
