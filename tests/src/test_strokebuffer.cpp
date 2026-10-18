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

#include <QPixmap>

#include "bitmapimage.h"
#include "canvaspainter.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "object.h"
#include "strokebuffer.h"

namespace
{
struct Canvas
{
    std::unique_ptr<Object> object { new Object };
    LayerBitmap* layer = nullptr;
    BitmapImage* image = nullptr;

    explicit Canvas(QColor fill = Qt::transparent)
    {
        object->init();
        layer = object->addNewBitmapLayer();
        layer->addNewKeyFrameAt(1);
        image = layer->getBitmapImageAtFrame(1);
        if (fill.alpha() > 0)
        {
            QImage pixels(40, 40, QImage::Format_ARGB32_Premultiplied);
            pixels.fill(fill);
            BitmapImage source(QPoint(0, 0), pixels);
            image->paste(&source);
        }
    }
};
}

TEST_CASE("StrokeBuffer commits a normal dab to a bitmap Drawing")
{
    Canvas c;
    StrokeBuffer buffer;
    buffer.begin(c.layer, 1, StrokeBuffer::Mode::Paint);
    buffer.dab(QPointF(20, 20), 10, 0, Qt::red, 1.0, false);
    CHECK(buffer.compositionMode() == QPainter::CompositionMode_SourceOver);

    const QRect changed = buffer.commit();
    CHECK(changed.contains(QPoint(20, 20)));
    CHECK(c.image->pixel(QPoint(20, 20)) == qRgba(255, 0, 0, 255));
    CHECK(qAlpha(c.image->pixel(QPoint(2, 2))) == 0);
    CHECK(buffer.tiles().bounds().isEmpty()); // commit clears the buffer
}

TEST_CASE("StrokeBuffer erases on commit and keeps pixels under lock alpha")
{
    Canvas c(Qt::blue);
    StrokeBuffer buffer;
    buffer.begin(c.layer, 1, StrokeBuffer::Mode::Erase);
    buffer.dab(QPointF(20, 20), 10, 0, Qt::white, 1.0, false);
    CHECK(buffer.compositionMode() == QPainter::CompositionMode_DestinationOut);
    buffer.commit();
    CHECK(qAlpha(c.image->pixel(QPoint(20, 20))) == 0);
    CHECK(c.image->pixel(QPoint(5, 5)) == qRgba(0, 0, 255, 255));

    Canvas locked(Qt::blue);
    locked.layer->setLockAlpha(true);
    buffer.begin(locked.layer, 1, StrokeBuffer::Mode::Erase);
    buffer.dab(QPointF(20, 20), 10, 0, Qt::white, 1.0, false);
    CHECK(buffer.compositionMode() == QPainter::CompositionMode_Destination);
    buffer.commit();
    CHECK(locked.image->pixel(QPoint(20, 20)) == qRgba(0, 0, 255, 255));
}

TEST_CASE("StrokeBuffer smudge commits the layer pixels it moved")
{
    Canvas c(Qt::blue);
    QImage red(10, 40, QImage::Format_ARGB32_Premultiplied);
    red.fill(Qt::red);
    BitmapImage redSource(QPoint(0, 0), red);
    c.image->paste(&redSource); // x 0..9 red, 10..39 blue

    StrokeBuffer buffer;
    buffer.begin(c.layer, 1, StrokeBuffer::Mode::Smudge);
    SmudgeSettings smudge;
    smudge.length = 1.0f;
    smudge.radius = 1.0f;
    smudge.colorRate = 0.0f;
    for (int x = 6; x <= 16; x += 2)
    {
        buffer.dab(QPointF(x, 20), 8, 0, Qt::green, 1.0, false, BrushTip(), smudge);
    }
    CHECK(buffer.compositionMode() == QPainter::CompositionMode_Source);
    buffer.commit();

    const QRgb smudged = c.image->pixel(QPoint(14, 20));
    CHECK(qRed(smudged) > 0);            // red pulled into the blue
    CHECK(qGreen(smudged) == 0);         // colour rate 0: no front colour
    CHECK(qAlpha(smudged) == 255);
    CHECK(c.image->pixel(QPoint(30, 5)) == qRgba(0, 0, 255, 255)); // outside the stroke
}

TEST_CASE("CanvasPainter paints a vector layer without a stroke buffer")
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    LayerVector* layer = object->addNewVectorLayer();
    layer->addNewKeyFrameAt(1);

    QPixmap canvas(20, 20);
    CanvasPainter painter(canvas);
    painter.setViewTransform(QTransform(), QTransform());
    painter.setOptions(CanvasPainterOptions());
    painter.setPaintSettings(object.get(), 0, 1, nullptr);
    painter.paint(QRect(0, 0, 20, 20));
    SUCCEED();
}
