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
#include <QImage>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "exporter.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "object.h"

namespace
{
const QSize kFrame(40, 30);
const QColor kColors[] = { Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::cyan };

// Frame n (1-based) is filled with kColors[n - 1].
std::unique_ptr<Object> fiveFrames(LayerCamera*& camera)
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    camera = object->addNewCameraLayer();
    LayerBitmap* layer = object->addNewBitmapLayer();
    for (int f = 1; f <= 5; ++f)
    {
        layer->addNewKeyFrameAt(f);
        layer->getBitmapImageAtFrame(f)->drawRect(QRectF(-kFrame.width(), -kFrame.height(), kFrame.width() * 2, kFrame.height() * 2),
                                                  Qt::NoPen, QBrush(kColors[f - 1]), QPainter::CompositionMode_Source, false);
    }
    return object;
}

ExportRequest request(ExportRequest::Kind kind, int start, int end, const QString& path, bool transparency, int columns = 0)
{
    ExportRequest r;
    r.kind = kind;
    r.startFrame = start;
    r.endFrame = end;
    r.size = kFrame;
    r.path = path;
    r.transparency = transparency;
    r.columns = columns;
    return r;
}
}

TEST_CASE("Sprite sheet grid is row-major and defaults to ceil(sqrt(n)) columns", "[SpriteSheet]")
{
    SECTION("default columns")
    {
        REQUIRE(Object::spriteSheetGrid(5, 0).columns == 3);
        REQUIRE(Object::spriteSheetGrid(5, 0).rows == 2);
        REQUIRE(Object::spriteSheetGrid(4, 0).columns == 2);
        REQUIRE(Object::spriteSheetGrid(1, 0).columns == 1);
    }
    SECTION("chosen columns are clamped to the frame count")
    {
        REQUIRE(Object::spriteSheetGrid(5, 2).rows == 3);
        REQUIRE(Object::spriteSheetGrid(5, 9).columns == 5);
        REQUIRE(Object::spriteSheetGrid(5, 9).rows == 1);
    }
    SECTION("cells run left to right then top to bottom")
    {
        const auto grid = Object::spriteSheetGrid(5, 3);
        REQUIRE(grid.cell(0, kFrame) == QRect(0, 0, 40, 30));
        REQUIRE(grid.cell(2, kFrame) == QRect(80, 0, 40, 30));
        REQUIRE(grid.cell(3, kFrame) == QRect(0, 30, 40, 30));
    }
}

TEST_CASE("Sprite sheet export writes one PNG with the frames in order", "[SpriteSheet]")
{
    LayerCamera* camera = nullptr;
    auto object = fiveFrames(camera);
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("sheet.png");

    REQUIRE(Exporter::run(*object, request(ExportRequest::Kind::SpriteSheet, 1, 5, path, true, 3)).ok());

    QImage sheet(path);
    REQUIRE(sheet.size() == QSize(120, 60));
    for (int i = 0; i < 5; ++i)
    {
        const QPoint centre((i % 3) * 40 + 20, (i / 3) * 30 + 15);
        REQUIRE(sheet.pixelColor(centre) == kColors[i]);
    }
    REQUIRE(sheet.pixelColor(100, 45).alpha() == 0); // unused sixth cell
}

TEST_CASE("PNG sequence keeps alpha with a transparent background", "[SpriteSheet]")
{
    LayerCamera* camera = nullptr;
    std::unique_ptr<Object> object(new Object);
    object->init();
    camera = object->addNewCameraLayer();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    REQUIRE(Exporter::run(*object, request(ExportRequest::Kind::Sequence, 3, 3, dir.filePath("seq"), true)).ok());
    QImage clear(dir.filePath("seq0003.png"));
    REQUIRE(clear.hasAlphaChannel());
    REQUIRE(clear.pixelColor(20, 15).alpha() == 0);

    REQUIRE(Exporter::run(*object, request(ExportRequest::Kind::Sequence, 3, 3, dir.filePath("opaque"), false)).ok());
    REQUIRE(QImage(dir.filePath("opaque0003.png")).pixelColor(20, 15) == QColor(Qt::white));
}
