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
#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "blendmode.h"
#include "camera.h"
#include "framerenderer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layervector.h"
#include "movieexporter.h"
#include "object.h"
#include "pagewriter.h"
#include "paperpainter.h"
#include "objectdata.h"
#include "vectorimage.h"
#include "welcomehelper.h"

namespace
{
LayerBitmap* square(Object* object, const QRectF& rect, QColor color)
{
    LayerBitmap* layer = object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->getBitmapImageAtFrame(1)->drawRect(rect, Qt::NoPen, QBrush(color), QPainter::CompositionMode_Source, false);
    return layer;
}

// One layer per rule: hidden, Rough, tween, blend mode, a translucent group, a vector stroke; the camera pans and zooms.
std::unique_ptr<Object> everyRule()
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    square(object.get(), QRectF(-40, -30, 80, 60), QColor(90, 160, 220));
    square(object.get(), QRectF(-10, -10, 20, 20), Qt::green)->setVisible(false);
    square(object.get(), QRectF(0, 0, 30, 30), Qt::black)->setRough(true);

    LayerBitmap* tweened = square(object.get(), QRectF(-25, -5, 10, 10), Qt::red);
    tweened->addNewKeyFrameAt(5);
    tweened->getBitmapImageAtFrame(5)->drawRect(QRectF(-25, -5, 10, 10), Qt::NoPen, QBrush(Qt::red),
                                                QPainter::CompositionMode_Source, false);
    tweened->getKeyFrameAt(1)->setTween(true);
    TweenState end;
    end.tx = 30;
    end.rot = 45;
    end.opacity = 0.5;
    tweened->getKeyFrameAt(5)->setTweenState(end);

    square(object.get(), QRectF(-20, -20, 25, 25), QColor(240, 200, 40))->setBlendMode(BlendMode::Multiply);

    square(object.get(), QRectF(5, -25, 20, 20), Qt::magenta)->setGroupId(1);
    square(object.get(), QRectF(12, -18, 20, 20), Qt::cyan)->setGroupId(1);
    object->addLayerGroup(LayerGroup{ 1, "G", false, true, 0.5 });

    LayerVector* vector = object->addNewVectorLayer();
    vector->addNewKeyFrameAt(1);
    BezierCurve curve(QList<QPointF>{ QPointF(-35, 20), QPointF(0, 5), QPointF(35, 25) }, false);
    vector->getVectorImageAtFrame(1)->addCurve(curve, 1.0, false);

    LayerCamera* camera = object->addNewCameraLayer();
    camera->setViewRect(QRect(-40, -30, 80, 60));
    if (!camera->keyExists(1)) { camera->addNewKeyFrameAt(1); }
    camera->getCameraAtFrame(1)->translate(5, -3);
    camera->getCameraAtFrame(1)->scale(1.25);
    return object;
}

QByteArray sha1(const QImage& image)
{
    return QCryptographicHash::hash(QByteArrayView(image.constBits(), image.sizeInBytes()), QCryptographicHash::Sha1).toHex();
}
} // namespace

TEST_CASE("The export adapter keeps the golden pixels of every layer rule", "[FrameRenderer]")
{
    auto object = everyRule();
    const LayerCamera* camera = object->getLayersByType<LayerCamera>().front();

    // macOS rasterises antialiased and rotated paths differently, so it has its own hashes (CI-confirmed).
    struct Case { int frame; QSize size; bool transparency; bool antialiasing; const char* hash; const char* macHash; };
    const Case cases[] = {
        { 1, QSize(80, 60), true, true, "6d7e1b7eb7d7621ada6287ac77180398e9efad19", "00a97949409e8c3e374fd604365d7646f95304e6" },
        { 3, QSize(160, 120), true, true, "2c91323f7b76a7bb2b39b83b1c1a902c53c86329", "a4db93156c840e5eb1e2aed35c6202c38642c043" },
        { 3, QSize(160, 120), false, false, "0e76b500c721a885bef540e5a39f65e791162c44", "d5c056bdd4bc99ff1356c7ae5a150485c474f42d" },
    };
    for (const Case& c : cases)
    {
        FrameRenderer::ExportOptions options;
        options.size = c.size;
        options.transparency = c.transparency;
        options.antialiasing = c.antialiasing;
        const QImage image = FrameRenderer::renderForExport(*object, c.frame, *camera, options);
        CAPTURE(c.frame, c.size.width(), c.transparency, c.antialiasing);
#ifdef Q_OS_MACOS
        CHECK(sha1(image).toStdString() == std::string(c.macHash));
#else
        CHECK(sha1(image).toStdString() == std::string(c.hash));
#endif
    }
}

TEST_CASE("Image sequence export writes the export adapter pixels", "[FrameRenderer]")
{
    auto object = everyRule();
    const LayerCamera* camera = object->getLayersByType<LayerCamera>().front();
    QTemporaryDir dir;
    REQUIRE(object->exportFrames(1, 3, camera, QSize(160, 120), dir.filePath("seq"), "PNG", true, false, "", true, nullptr, 0).ok());

    FrameRenderer::ExportOptions options;
    options.size = QSize(160, 120);
    options.transparency = true;
    for (int frame = 1; frame <= 3; ++frame)
    {
        const QImage written = QImageReader(dir.filePath(QString("seq%1.png").arg(frame, 4, 10, QChar('0')))).read()
                                   .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        CHECK(written == FrameRenderer::renderForExport(*object, frame, *camera, options));
    }
}

TEST_CASE("Movie and GIF frames are the export adapter pixels", "[FrameRenderer]")
{
    auto object = everyRule();
    const LayerCamera* camera = object->getLayersByType<LayerCamera>().front();
    ExportMovieDesc desc;
    desc.exportSize = QSize(160, 120);
    desc.alpha = true;
    FrameRenderer::ExportOptions options;
    options.size = desc.exportSize;
    for (const QString name : { QString("out.mp4"), QString("out.gif") })
    {
        desc.strFileName = name;
        options.transparency = !name.endsWith(".gif"); // GIF frames are opaque
        for (int frame = 1; frame <= 3; ++frame)
        {
            CAPTURE(name.toStdString(), frame);
            CHECK(MovieExporter::frameImage(*object, desc, frame) == FrameRenderer::renderForExport(*object, frame, *camera, options));
        }
    }
}

TEST_CASE("Paper pages draw the export adapter pixels in the camera frame", "[FrameRenderer]")
{
    auto object = everyRule();
    const LayerCamera* camera = object->getLayersByType<LayerCamera>().front();
    PageSpec spec;
    spec.dpi = 50;
    const PagePaintFn expected = [&](QPainter& painter, const QRectF& page, int index) {
        const QRectF f = paperFrameOnPage(page, camera->getViewSize());
        PaperTemplate paper = object->data()->paperTemplate();
        paper.on = true;
        paintPaperTemplate(painter, f, paper);
        FrameRenderer::ExportOptions options;
        options.size = f.toRect().size();
        options.transparency = true;
        painter.drawImage(f.toRect(), FrameRenderer::renderForExport(*object, 1 + index, *camera, options));
    };
    for (int index = 0; index < 3; ++index)
    {
        CAPTURE(index);
        CHECK(renderPage(spec, paperFramePages(*object, *camera, 1), index) == renderPage(spec, expected, index));
    }
}

TEST_CASE("The Welcome thumbnail is the export adapter fit to the thumbnail", "[FrameRenderer]")
{
    auto object = everyRule();
    const LayerCamera* camera = object->getLayersByType<LayerCamera>().front();
    QTemporaryDir dir;
    const QString project = dir.filePath("thumb.pclx");
    REQUIRE(Welcome::saveThumbnail(object.get(), project, 3));
    const QImage written = QImageReader(Welcome::thumbnailPath(project)).read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QFile::remove(Welcome::thumbnailPath(project));
    REQUIRE_FALSE(written.isNull());

    const QSize cam = camera->getViewSize();
    const qreal s = qMin(qreal(written.width()) / cam.width(), qreal(written.height()) / cam.height());
    FrameRenderer::ExportOptions options;
    options.size = written.size();
    CHECK(written == FrameRenderer::renderForExport(*object, 3, camera->getViewAtFrame(3) * QTransform::fromScale(s, s), written.size(), options));
}
