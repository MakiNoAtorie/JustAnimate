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

#include "bitmapimage.h"
#include "tile.h"
#include "tiledbuffer.h"
#include <climits>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <QtEndian>
#include <QDebug>

TEST_CASE("BitmapImage constructors")
{
    SECTION("Init an Bitmap Image")
    {
        auto b = std::make_shared<BitmapImage>();
        REQUIRE(b->image()->isNull());

        REQUIRE(b->width() == 0);
        REQUIRE(b->height() == 0);
        REQUIRE(b->top() == 0);
        REQUIRE(b->left() == 0);
    }

    SECTION("Init with color and boundary")
    {
        auto b = std::make_shared<BitmapImage>(QRect(10, 20, 30, 40), Qt::red);

        REQUIRE(b->left() == 10);
        REQUIRE(b->top() == 20);
        REQUIRE(b->width() == 30);
        REQUIRE(b->height() == 40);

        QRgb rgb = b->image()->pixel(0, 0);
        REQUIRE(rgb == qRgb(255, 0, 0));
    }

    SECTION("Clone a BitmapImage")
    {
        auto b = std::make_shared<BitmapImage>(QRect(20, 20, 100, 100), Qt::red);
        std::unique_ptr<BitmapImage> b2(b->clone());

        REQUIRE(b->pos() == b2->pos());
        REQUIRE(b->length() == b2->length());
        
        REQUIRE(b->left() == b2->left());
        REQUIRE(b->right() == b2->right());
        REQUIRE(b->width() == b2->width());
        REQUIRE(b->height() == b2->height());
        
        QImage* img1 = b->image();
        QImage* img2 = b2->image();
        REQUIRE(img1 != img2);
        REQUIRE((*img1) == (*img2));
    }

    SECTION("#947 Initial Color")
    {
        // A new bitmap image must be fully transparent
        // otherwise a pixel dot will appear in center of canvas
        // ref: https://github.com/pencil2d/pencil/pull/947

        auto b = std::make_shared<BitmapImage>();
        for (int x = 0; x < b->width(); ++x)
        {
            for (int y = 0; y < b->height(); ++y)
            {
                QRgb color = b->pixel(x, y);
                REQUIRE(qAlpha(color) == 0);
            }
        }
    }
}

TEST_CASE("BitmapImage functions")
{
    SECTION("moveTopLeft()")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 50, 50), Qt::red);
        b->moveTopLeft(QPoint(20, 10));

        REQUIRE(b->topLeft() == QPoint(20, 10));
        REQUIRE(b->width() == 50);
        REQUIRE(b->height() == 50);
    }
}

TEST_CASE("BitmapImage autoCrop")
{
    SECTION("Empty image (all transparent)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);
        b->enableAutoCrop(true);
        b->autoCrop();

        // Empty images should have empty bounds
        REQUIRE(b->width() == 0);
        REQUIRE(b->height() == 0);
    }

    SECTION("Single pixel at origin")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);
        b->setPixel(0, 0, qRgba(255, 0, 0, 255)); // Red pixel at top-left
        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 1);
        REQUIRE(b->height() == 1);
        REQUIRE(b->left() == 0);
        REQUIRE(b->top() == 0);
    }

    SECTION("Single pixel at center")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);
        b->setPixel(50, 50, qRgba(255, 0, 0, 255)); // Red pixel at center
        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 1);
        REQUIRE(b->height() == 1);
        REQUIRE(b->left() == 50);
        REQUIRE(b->top() == 50);
    }

    SECTION("Single pixel at bottom-right corner")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);
        b->setPixel(99, 99, qRgba(255, 0, 0, 255)); // Red pixel at bottom-right
        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 1);
        REQUIRE(b->height() == 1);
        REQUIRE(b->left() == 99);
        REQUIRE(b->top() == 99);
    }

    SECTION("Centered content with transparent borders")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Draw a 20x20 red rectangle at position (40, 40)
        for (int x = 40; x < 60; ++x)
        {
            for (int y = 40; y < 60; ++y)
            {
                b->setPixel(x, y, qRgba(255, 0, 0, 255));
            }
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 20);
        REQUIRE(b->height() == 20);
        REQUIRE(b->left() == 40);
        REQUIRE(b->top() == 40);
    }

    SECTION("Content at edges (top and left)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Draw L-shape at top-left corner (50x50)
        for (int x = 0; x < 50; ++x)
        {
            b->setPixel(x, 0, qRgba(255, 0, 0, 255)); // Top edge
        }
        for (int y = 0; y < 50; ++y)
        {
            b->setPixel(0, y, qRgba(255, 0, 0, 255)); // Left edge
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 50);
        REQUIRE(b->height() == 50);
        REQUIRE(b->left() == 0);
        REQUIRE(b->top() == 0);
    }

    SECTION("Content at edges (bottom and right)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Draw L-shape at bottom-right corner (50x50)
        for (int x = 50; x < 100; ++x)
        {
            b->setPixel(x, 99, qRgba(255, 0, 0, 255)); // Bottom edge
        }
        for (int y = 50; y < 100; ++y)
        {
            b->setPixel(99, y, qRgba(255, 0, 0, 255)); // Right edge
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 50);
        REQUIRE(b->height() == 50);
        REQUIRE(b->left() == 50);
        REQUIRE(b->top() == 50);
    }

    SECTION("Inverted T shape (worst case for row-based approach)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Draw horizontal line at top
        for (int x = 0; x < 100; ++x)
        {
            b->setPixel(x, 0, qRgba(255, 0, 0, 255));
        }
        // Draw vertical line in middle
        for (int y = 0; y < 100; ++y)
        {
            b->setPixel(50, y, qRgba(255, 0, 0, 255));
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 100);
        REQUIRE(b->height() == 100);
        REQUIRE(b->left() == 0);
        REQUIRE(b->top() == 0);
    }

    SECTION("Diagonal line from corner to corner")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Draw diagonal line from (0,0) to (99,99)
        for (int i = 0; i < 100; ++i)
        {
            b->setPixel(i, i, qRgba(255, 0, 0, 255));
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 100);
        REQUIRE(b->height() == 100);
        REQUIRE(b->left() == 0);
        REQUIRE(b->top() == 0);
    }

    SECTION("Already minimally bounded (no change expected)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 50, 50), Qt::red);
        b->enableAutoCrop(true);
        b->autoCrop();

        // Image is already fully opaque, so bounds shouldn't change
        REQUIRE(b->width() == 50);
        REQUIRE(b->height() == 50);
        REQUIRE(b->left() == 0);
        REQUIRE(b->top() == 0);
    }

    SECTION("AutoCrop disabled (no change expected)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);
        b->setPixel(50, 50, qRgba(255, 0, 0, 255));
        b->enableAutoCrop(false); // Disable autoCrop
        b->autoCrop();

        // Bounds should NOT change because autoCrop is disabled
        REQUIRE(b->width() == 100);
        REQUIRE(b->height() == 100);
        REQUIRE(b->left() == 0);
        REQUIRE(b->top() == 0);
    }

    SECTION("Large image with padding consideration")
    {
        // Test with a larger image to ensure scanline padding is handled correctly
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 1000, 1000), Qt::transparent);

        // Draw a rectangle at position (100, 100) with size (800, 800)
        for (int x = 100; x < 900; ++x)
        {
            for (int y = 100; y < 900; ++y)
            {
                b->setPixel(x, y, qRgba(255, 0, 0, 255));
            }
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 800);
        REQUIRE(b->height() == 800);
        REQUIRE(b->left() == 100);
        REQUIRE(b->top() == 100);
    }

    SECTION("Multiple separate pixels")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Place pixels at various positions
        b->setPixel(10, 10, qRgba(255, 0, 0, 255));
        b->setPixel(90, 20, qRgba(0, 255, 0, 255));
        b->setPixel(30, 80, qRgba(0, 0, 255, 255));
        b->setPixel(70, 70, qRgba(255, 255, 0, 255));

        b->enableAutoCrop(true);
        b->autoCrop();

        // Bounds should be from (10,10) to (90,80)
        REQUIRE(b->width() == 81); // 90 - 10 + 1
        REQUIRE(b->height() == 71); // 80 - 10 + 1
        REQUIRE(b->left() == 10);
        REQUIRE(b->top() == 10);
    }

    SECTION("Horizontal line (single row)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Draw horizontal line from (20,50) to (79,50)
        for (int x = 20; x < 80; ++x)
        {
            b->setPixel(x, 50, qRgba(255, 0, 0, 255));
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 60);
        REQUIRE(b->height() == 1);
        REQUIRE(b->left() == 20);
        REQUIRE(b->top() == 50);
    }

    SECTION("Vertical line (single column)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 100, 100), Qt::transparent);

        // Draw vertical line from (50,20) to (50,79)
        for (int y = 20; y < 80; ++y)
        {
            b->setPixel(50, y, qRgba(255, 0, 0, 255));
        }

        b->enableAutoCrop(true);
        b->autoCrop();

        REQUIRE(b->width() == 1);
        REQUIRE(b->height() == 60);
        REQUIRE(b->left() == 50);
        REQUIRE(b->top() == 20);
    }
}

TEST_CASE("BitmapImage autoCrop performance")
{
    SECTION("Profile autoCrop on large image (4000x4000)")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 4000, 4000), Qt::transparent);
        
        // Draw some content in center
        for (int y = 800; y < 1200; ++y) {
            for (int x = 800; x < 1200; ++x) {
                b->setPixel(x, y, qRgba(255, 0, 0, 255));
            }
        }
        
        b->enableAutoCrop(true);
        
        QElapsedTimer timer;
        timer.start();
        b->autoCrop();
        qint64 elapsed = timer.nsecsElapsed();
        
        qDebug() << "Large image (4000x4000) autoCrop took:" << elapsed / 1000.0 << "microseconds";
        
        REQUIRE(b->width() == 400);
        REQUIRE(b->height() == 400);
    }
    
    SECTION("Profile autoCrop worst case - single pixel at corner")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 1000, 1000), Qt::transparent);
        
        // Single pixel at far corner
        b->setPixel(999, 999, qRgba(255, 0, 0, 255));
        
        b->enableAutoCrop(true);
        
        QElapsedTimer timer;
        timer.start();
        b->autoCrop();
        qint64 elapsed = timer.nsecsElapsed();
        
        qDebug() << "Worst case (single pixel at 999,999) autoCrop took:" << elapsed / 1000.0 << "microseconds";
        
        REQUIRE(b->width() == 1);
        REQUIRE(b->height() == 1);
        REQUIRE(b->left() == 999);
        REQUIRE(b->top() == 999);
    }
    
    SECTION("Profile autoCrop with sparse content")
    {
        auto b = std::make_shared<BitmapImage>(QRect(0, 0, 1000, 1000), Qt::transparent);
        
        // Draw sparse pixels across image
        for (int i = 0; i < 1000; i += 100) {
            b->setPixel(i, i, qRgba(255, 0, 0, 255));
        }
        
        b->enableAutoCrop(true);
        
        QElapsedTimer timer;
        timer.start();
        b->autoCrop();
        qint64 elapsed = timer.nsecsElapsed();
        
        qDebug() << "Sparse content autoCrop took:" << elapsed / 1000.0 << "microseconds";
        
        REQUIRE(b->width() == 901);
        REQUIRE(b->height() == 901);
    }
}

TEST_CASE("BitmapImage floodFill")
{
    // Closed black box (outline 3..7); fill from inside must stay inside
    BitmapImage image(QRect(0, 0, 11, 11), Qt::transparent);
    const QRgb black = qRgba(0, 0, 0, 255);
    const QRgb red = qRgba(255, 0, 0, 255);
    for (int i = 3; i <= 7; i++)
    {
        image.setPixel(i, 3, black);
        image.setPixel(i, 7, black);
        image.setPixel(3, i, black);
        image.setPixel(7, i, black);
    }

    BitmapImage* result = nullptr;
    REQUIRE(BitmapImage::floodFill(&result, &image, QRect(0, 0, 11, 11), QPoint(5, 5), red, 0, 0));
    REQUIRE(result != nullptr);

    REQUIRE(result->constScanLine(5, 5) == red);
    REQUIRE(result->constScanLine(4, 6) == red);
    REQUIRE(result->constScanLine(3, 3) == 0);
    REQUIRE(result->constScanLine(1, 1) == 0);
    REQUIRE(result->constScanLine(9, 5) == 0);
    delete result;
}

TEST_CASE("BitmapImage floodFill: close gaps seals a 3 px gap")
{
    // Box outline 3..13 with a 3 px gap (y 7..9) in the right side
    BitmapImage image(QRect(0, 0, 17, 17), Qt::transparent);
    const QRgb black = qRgba(0, 0, 0, 255);
    const QRgb red = qRgba(255, 0, 0, 255);
    for (int i = 3; i <= 13; i++)
    {
        image.setPixel(i, 3, black);
        image.setPixel(i, 13, black);
        image.setPixel(3, i, black);
        if (i < 7 || i > 9) image.setPixel(13, i, black);
    }

    BitmapImage* open = nullptr;
    REQUIRE(BitmapImage::floodFill(&open, &image, QRect(0, 0, 17, 17), QPoint(8, 8), red, 0, 0, 0));
    REQUIRE(open->constScanLine(1, 1) == red); // leaks without close gaps
    delete open;

    BitmapImage* closed = nullptr;
    REQUIRE(BitmapImage::floodFill(&closed, &image, QRect(0, 0, 17, 17), QPoint(8, 8), red, 0, 0, 4));
    REQUIRE(closed->constScanLine(8, 8) == red);
    REQUIRE(closed->constScanLine(1, 1) == 0);
    REQUIRE(closed->constScanLine(15, 8) == 0);
    delete closed;
}

TEST_CASE("BitmapImage paste: brush tiles are premultiplied images")
{
    TiledBuffer buffer;
    const QColor red(255, 0, 0, 128);
    buffer.drawBrush(QPointF(70, 32), 20, QPen(Qt::NoPen), QBrush(red), QPainter::CompositionMode_SourceOver, false);
    REQUIRE(buffer.tiles().size() == 2); // the dab crosses the tile edge at x = 64

    for (const Tile* tile : buffer.tiles())
        REQUIRE(tile->image().format() == QImage::Format_ARGB32_Premultiplied);

    BitmapImage image(QRect(0, 0, 128, 64), Qt::transparent);
    image.paste(&buffer);
    REQUIRE(image.constScanLine(62, 32) == qPremultiply(red.rgba()));
    REQUIRE(image.constScanLine(70, 32) == qPremultiply(red.rgba()));
    REQUIRE(image.constScanLine(70, 60) == 0);
}

TEST_CASE("TiledBuffer drawDab: a cached mask lands only in the tiles the dab covers")
{
    TiledBuffer buffer;
    const QColor red(255, 0, 0);
    buffer.drawDab(QPointF(40, 40), 40, 0, red, 1.0, false); // covers x, y 20..60: tile (0, 0) only
    REQUIRE(buffer.tiles().size() == 1);

    BitmapImage solid(QRect(0, 0, 64, 64), Qt::transparent);
    solid.paste(&buffer);
    CHECK(solid.constScanLine(40, 40) == qRgba(255, 0, 0, 255));
    CHECK(solid.constScanLine(40, 21) == qRgba(255, 0, 0, 255));
    CHECK(solid.constScanLine(40, 19) == 0);
    CHECK(solid.constScanLine(25, 25) == 0); // outside the circle, inside its box

    // Feather 50: the centre keeps 255 - 128 alpha, the outer half fades to zero.
    TiledBuffer soft;
    soft.drawDab(QPointF(40, 40), 40, 50, red, 1.0, false);
    BitmapImage feathered(QRect(0, 0, 64, 64), Qt::transparent);
    feathered.paste(&soft);
    CHECK(qAlpha(feathered.constScanLine(40, 40)) == 127);
    CHECK(qAlpha(feathered.constScanLine(40, 33)) == 127);
    CHECK(qAlpha(feathered.constScanLine(40, 25)) < 127);
    CHECK(qAlpha(feathered.constScanLine(40, 25)) > qAlpha(feathered.constScanLine(40, 22)));
    CHECK(qAlpha(feathered.constScanLine(40, 22)) > 0);
    CHECK(feathered.constScanLine(40, 19) == 0);
}

TEST_CASE("TiledBuffer drawDab: a generated tip is round or square with an aspect ratio and an angle")
{
    const QColor red(255, 0, 0);
    // A 40 px dab at (40, 40); the alpha at each test pixel.
    auto alphaAt = [&](const BrushTip& tip, std::initializer_list<QPoint> points) {
        TiledBuffer buffer;
        buffer.drawDab(QPointF(40, 40), 40, 0, red, 1.0, false, tip);
        BitmapImage image(QRect(0, 0, 64, 64), Qt::transparent);
        image.paste(&buffer);
        std::vector<int> alpha;
        for (QPoint p : points) alpha.push_back(qAlpha(image.constScanLine(p.x(), p.y())));
        return alpha;
    };
    const std::initializer_list<QPoint> probes = { {40, 40}, {23, 23}, {40, 25}, {25, 40}, {40, 22}, {22, 40}, {19, 40} };

    BrushTip round;
    CHECK(alphaAt(round, probes) == std::vector<int>{ 255, 0, 255, 255, 255, 255, 0 });

    BrushTip turned; // a round tip looks the same at any angle
    turned.angle = 37;
    CHECK(alphaAt(turned, probes) == alphaAt(round, probes));

    BrushTip square;
    square.square = true; // the corner of the box is inside
    CHECK(alphaAt(square, probes) == std::vector<int>{ 255, 255, 255, 255, 255, 255, 0 });

    BrushTip flat;
    flat.aspect = 0.5; // 40 px wide, 20 px tall
    CHECK(alphaAt(flat, probes) == std::vector<int>{ 255, 0, 0, 255, 0, 255, 0 });

    BrushTip upright = flat;
    upright.angle = 90; // 20 px wide, 40 px tall
    CHECK(alphaAt(upright, probes) == std::vector<int>{ 255, 0, 255, 0, 255, 0, 0 });
}

TEST_CASE("loadBrushTipImage: PNG and GIMP .gbr tips load and malformed .gbr files fail")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto writeFile = [&](const QString& name, const QByteArray& bytes) {
        QFile f(dir.filePath(name));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(bytes);
        return f.fileName();
    };
    // A version 2 .gbr: big-endian header fields, the name, then the pixels.
    auto gbr = [](quint32 width, quint32 height, quint32 bytes, const QByteArray& pixels, quint32 headerSize = 33,
                  const QByteArray& magic = "GIMP") {
        QByteArray out;
        for (quint32 v : { headerSize, 2u, width, height, bytes })
        {
            const quint32 be = qToBigEndian(v);
            out.append(reinterpret_cast<const char*>(&be), 4);
        }
        out.append(magic);
        const quint32 spacing = qToBigEndian(25u);
        out.append(reinterpret_cast<const char*>(&spacing), 4);
        out.append("tip\0\0", 5);
        return out + pixels;
    };
    const QByteArray pixels("\xff\x80\x00\x00\x40\xff", 6); // 3 x 2, 255 is full paint

    QImage mask;
    REQUIRE(loadBrushTipImage(writeFile("ok.gbr", gbr(3, 2, 1, pixels)), mask).ok());
    REQUIRE(mask.size() == QSize(3, 2));
    CHECK(mask.pixelColor(0, 0).alpha() == 255);
    CHECK(mask.pixelColor(1, 0).alpha() == 128);
    CHECK(mask.pixelColor(2, 0).alpha() == 0);
    CHECK(mask.pixelColor(2, 1).alpha() == 255);

    // An RGBA .gbr uses its alpha.
    REQUIRE(loadBrushTipImage(writeFile("rgba.gbr", gbr(1, 1, 4, QByteArray("\x10\x20\x30\x77", 4))), mask).ok());
    CHECK(mask.pixelColor(0, 0).alpha() == 0x77);

    // A PNG without alpha paints its dark parts; with alpha it paints its opaque dark parts.
    QImage png(2, 1, QImage::Format_RGB32);
    png.setPixel(0, 0, qRgb(0, 0, 0));
    png.setPixel(1, 0, qRgb(255, 255, 255));
    REQUIRE(png.save(dir.filePath("tip.png")));
    REQUIRE(loadBrushTipImage(dir.filePath("tip.png"), mask).ok());
    CHECK(mask.pixelColor(0, 0).alpha() == 255);
    CHECK(mask.pixelColor(1, 0).alpha() == 0);
    QImage pngAlpha(1, 1, QImage::Format_ARGB32);
    pngAlpha.setPixel(0, 0, qRgba(0, 0, 0, 0x60));
    REQUIRE(pngAlpha.save(dir.filePath("alpha.png")));
    REQUIRE(loadBrushTipImage(dir.filePath("alpha.png"), mask).ok());
    CHECK(mask.pixelColor(0, 0).alpha() == 0x60);

    const QByteArray good = gbr(3, 2, 1, pixels);
    const std::pair<const char*, QByteArray> bad[] = {
        { "empty", QByteArray() },
        { "short header", good.left(20) },
        { "truncated pixels", good.left(good.size() - 1) },
        { "bad magic", gbr(3, 2, 1, pixels, 33, "PMIG") },
        { "header too small", gbr(3, 2, 1, pixels, 27) },
        { "header past the end", gbr(3, 2, 1, pixels, 0x7fffffff) },
        { "zero width", gbr(0, 2, 1, pixels) },
        { "huge size", gbr(0xffffffff, 0xffffffff, 1, pixels) },
        { "bad depth", gbr(3, 2, 3, pixels + pixels + pixels) },
    };
    for (const auto& [name, bytes] : bad)
    {
        CAPTURE(name);
        mask = QImage();
        CHECK_FALSE(loadBrushTipImage(writeFile("bad.gbr", bytes), mask).ok());
        CHECK(mask.isNull());
    }
    CHECK_FALSE(loadBrushTipImage(dir.filePath("missing.gbr"), mask).ok());
    CHECK_FALSE(loadBrushTipImage(writeFile("bad.png", "not a png"), mask).ok());
}

TEST_CASE("TiledBuffer drawDab: an image tip is scaled to the dab width and turned by the tip angle")
{
    // 4 x 2: the left half paints, the right half does not.
    QImage image(4, 2, QImage::Format_Alpha8);
    image.fill(0);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            image.setPixelColor(x, y, QColor(0, 0, 0, 255));

    auto alphaAt = [&](qreal angle, std::initializer_list<QPoint> points) {
        BrushTip tip;
        tip.image = image;
        tip.angle = angle;
        TiledBuffer buffer;
        buffer.drawDab(QPointF(40, 40), 40, 0, QColor(255, 0, 0), 1.0, true, tip); // 40 x 20 px
        BitmapImage out(QRect(0, 0, 80, 80), Qt::transparent);
        out.paste(&buffer);
        std::vector<int> alpha;
        for (QPoint p : points) alpha.push_back(qAlpha(out.constScanLine(p.x(), p.y())));
        return alpha;
    };
    CHECK(alphaAt(0, { {30, 40}, {22, 35}, {50, 40}, {30, 25}, {30, 55} }) == std::vector<int>{ 255, 255, 0, 0, 0 });
    // Turned 90 degrees counter-clockwise, the left half of the image is at the bottom.
    CHECK(alphaAt(90, { {40, 50}, {35, 58}, {40, 30}, {25, 50}, {55, 50} }) == std::vector<int>{ 255, 255, 0, 0, 0 });
}

TEST_CASE("TiledBuffer drawDab: an image tip builds one mask for any feather")
{
    QImage image(8, 8, QImage::Format_Alpha8);
    image.fill(255);
    BrushTip tip;
    tip.image = image;
    TiledBuffer buffer;
    buffer.drawDab(QPointF(40, 40), 20, 10, QColor(255, 0, 0), 1.0, true, tip);
    const qsizetype bytes = buffer.dabMaskBytes();
    REQUIRE(bytes > 0);
    buffer.drawDab(QPointF(40, 40), 20, 60, QColor(255, 0, 0), 1.0, true, tip);
    CHECK(buffer.dabMaskBytes() == bytes);
}

TEST_CASE("TiledBuffer drawDab: a constant-colour stroke keeps its colour where translucent dabs overlap")
{
    TiledBuffer buffer;
    const QColor color(200, 30, 90);
    for (int i = 0; i < 10; ++i)
        buffer.drawDab(QPointF(30 + i, 30), 30, 20, color, 0.3, true);

    BitmapImage image(QRect(0, 0, 64, 64), Qt::transparent);
    image.paste(&buffer);
    int painted = 0;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
        {
            const QRgb p = image.constScanLine(x, y);
            if (qAlpha(p) == 0) continue;
            ++painted;
            CAPTURE(x, y, qAlpha(p));
            REQUIRE(p == qPremultiply(qRgba(200, 30, 90, qAlpha(p))));
        }
    REQUIRE(painted > 500);
    REQUIRE(qAlpha(image.constScanLine(34, 30)) > 230); // the overlap builds up alpha
}

TEST_CASE("TiledBuffer: a stroke that changes colour keeps the first colour's pixels")
{
    TiledBuffer buffer;
    buffer.drawDab(QPointF(20, 20), 10, 0, QColor(255, 0, 0), 1.0, false);
    buffer.drawDab(QPointF(40, 20), 10, 0, QColor(0, 0, 255), 1.0, false);

    BitmapImage image(QRect(0, 0, 64, 64), Qt::transparent);
    image.paste(&buffer);
    CHECK(image.constScanLine(20, 20) == qRgba(255, 0, 0, 255));
    CHECK(image.constScanLine(40, 20) == qRgba(0, 0, 255, 255));
    CHECK(image.constScanLine(30, 20) == 0);
}

TEST_CASE("TiledBuffer drawDab: large dabs render on worker threads with the same pixels as one dab at a time")
{
    // Translucent overlapping dabs: the result depends on dab order in every tile.
    auto draw = [](TiledBuffer& buffer, bool flushEachDab) {
        const QColor red(200, 30, 90);
        const QColor blue(20, 60, 220);
        for (int i = 0; i < 8; ++i)
        {
            // From dab 4 on the colour changes, so the second half uses the image path, not coverage.
            buffer.drawDab(QPointF(800 + i * 40, 800 + i * 25), 1500, 30, i < 4 ? red : blue, 0.3, true);
            buffer.drawDab(QPointF(700 + i * 60, 760), 4, 0, i < 4 ? red : blue, 0.5, true); // a small dab between
            if (flushEachDab) buffer.tiles();
        }
        BitmapImage image(QRect(0, 0, 1700, 1700), Qt::transparent);
        image.paste(&buffer);
        return *image.image();
    };
    TiledBuffer batched;
    const QImage threaded = draw(batched, false);
    const int workers = batched.lastDabWorkers();
    TiledBuffer oneByOne;
    const QImage serial = draw(oneByOne, true);

    REQUIRE(qAlpha(serial.pixel(800, 800)) > 0);
    CHECK(threaded == serial);
    if (QThread::idealThreadCount() > 1)
        CHECK(workers > 1);
}

TEST_CASE("TiledBuffer smudgeDab: large dabs composite in row bands with the same pixels as serial")
{
    QImage source(1100, 1100, QImage::Format_ARGB32_Premultiplied);
    quint32 seed = 1;
    for (int y = 0; y < source.height(); ++y)
        for (int x = 0; x < source.width(); ++x)
        {
            seed = seed * 1664525u + 1013904223u;
            const int a = int(seed >> 24);
            source.setPixel(x, y, qRgba(int((seed >> 4) & 0xff) * a / 255, int((seed >> 12) & 0xff) * a / 255, x % 256 * a / 255, a));
        }
    auto smudge = [&](TiledBuffer& buffer) {
        buffer.setSmudgeSource(source, QPoint(0, 0));
        for (int i = 0; i < 3; ++i) // 900 px dabs: about 810K px each, above the threshold
            buffer.smudgeDab(QPointF(530 + i * 17.25, 540 - i * 9.5), 900, 40, QColor(30, 200, 90), 0.7, true, BrushTip(),
                             SmudgeSettings());
        BitmapImage image(QRect(0, 0, 1100, 1100), Qt::transparent);
        image.paste(&buffer, QPainter::CompositionMode_Source);
        return *image.image();
    };
    TiledBuffer banded;
    const QImage threaded = smudge(banded);
    const int workers = banded.lastSmudgeWorkers();
    TiledBuffer one;
    one.setParallelSmudgeMinPixels(INT_MAX);
    const QImage serial = smudge(one);

    REQUIRE(serial != source);
    CHECK(one.lastSmudgeWorkers() == 1);
    CHECK(threaded == serial);
    if (QThread::idealThreadCount() > 1)
        CHECK(workers > 1);
}

TEST_CASE("TiledBuffer: cleared tiles go to a pool that stays under its cap and comes back clean")
{
    TiledBuffer buffer;
    const qsizetype tileBytes = 64 * 64 * 4 + 64 * 64; // image and coverage
    buffer.setPoolCapBytes(5 * tileBytes);

    // A 300 px dab covers 5 x 5 tiles, more than the pool keeps.
    buffer.drawDab(QPointF(160, 160), 300, 0, QColor(255, 0, 0), 1.0, false);
    REQUIRE(buffer.tiles().size() == 25);
    REQUIRE(buffer.poolBytes() == 0);
    buffer.clear();
    CHECK(buffer.poolBytes() > 0);
    CHECK(buffer.poolBytes() <= 5 * tileBytes);

    // Reused tiles start transparent, with both the coverage and the plain image paths.
    const qsizetype pooled = buffer.poolBytes();
    buffer.drawDab(QPointF(32, 32), 4, 0, QColor(0, 255, 0), 1.0, false);
    CHECK(buffer.poolBytes() < pooled);
    buffer.drawBrush(QPointF(96, 32), 4, QPen(Qt::NoPen), QBrush(Qt::blue), QPainter::CompositionMode_SourceOver, false);
    BitmapImage image(QRect(0, 0, 128, 64), Qt::transparent);
    image.paste(&buffer);
    CHECK(image.constScanLine(32, 32) == qRgba(0, 255, 0, 255));
    CHECK(image.constScanLine(96, 32) == qRgba(0, 0, 255, 255));
    CHECK(image.constScanLine(10, 10) == 0);
    CHECK(image.constScanLine(70, 10) == 0);

    buffer.setPoolCapBytes(0);
    CHECK(buffer.poolBytes() == 0);
}
