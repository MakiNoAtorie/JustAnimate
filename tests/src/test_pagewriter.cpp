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
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "commandlineexporter.h"
#include "commandlineparser.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "object.h"
#include "objectdata.h"
#include "pagewriter.h"
#include "paperpainter.h"
#include "scribblearea.h"

QString QtResourceToFile(QString rscPath, QString filename, QTemporaryDir& tempDir); // test_filemanager.cpp

namespace
{
int pdfPageCount(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { return -1; }
    return QString::fromLatin1(file.readAll()).count(QRegularExpression("/Type\\s*/Page[^s]"));
}

// Pixels that differ between two images, as a bounding box and a count.
QRect diffBox(const QImage& a, const QImage& b, int* count)
{
    QRect box;
    *count = 0;
    for (int y = 0; y < a.height(); ++y)
    {
        for (int x = 0; x < a.width(); ++x)
        {
            if (a.pixel(x, y) != b.pixel(x, y)) { box |= QRect(x, y, 1, 1); ++*count; }
        }
    }
    return box;
}
} // namespace

TEST_CASE("A rendered page has each Title block value in its cell", "[PageWriter]")
{
    Object object;
    object.init();
    LayerCamera* camera = object.addNewCameraLayer();
    PageSpec spec;
    spec.dpi = 100; // NOTES text is small: three lines fit its cell
    const PagePaintFn paint = paperFramePages(object, *camera, 1);
    const QImage blank = renderPage(spec, paint, 0);
    REQUIRE(blank.size() == spec.pixelSize());
    CHECK(blank.width() > blank.height()); // landscape

    // The Title block cells, as paintPaperTemplate lays them out above the camera frame.
    const QRectF f = paperFrameOnPage(QRectF(QPointF(), QSizeF(blank.size())), camera->getViewSize());
    const QRectF block(f.left(), f.top() - f.height() * 0.22, f.width(), f.height() * 0.16);
    const qreal unit = block.width() / 6, top = block.height() * 0.35;
    struct Field { QString PaperTemplate::*value; QRectF cell; };
    const Field fields[] = {
        { &PaperTemplate::title, QRectF(block.left(), block.top(), 2 * unit, top) },
        { &PaperTemplate::id, QRectF(block.left() + 2 * unit, block.top(), unit, top) },
        { &PaperTemplate::name, QRectF(block.left() + 3 * unit, block.top(), unit, top) },
        { &PaperTemplate::state, QRectF(block.left() + 4 * unit, block.top(), unit, top) },
        { &PaperTemplate::date, QRectF(block.left() + 5 * unit, block.top(), unit, top) },
        { &PaperTemplate::notes, QRectF(block.left(), block.top() + top, block.width(), block.height() - top) },
    };
    for (const Field& field : fields)
    {
        PaperTemplate paper; // off: the export draws the sheet anyway
        paper.*field.value = "WWW";
        object.data()->setPaperTemplate(paper);
        int count = 0;
        const QRect box = diffBox(blank, renderPage(spec, paint, 0), &count);
        CHECK(count > 20);
        CHECK(field.cell.toAlignedRect().contains(box));
    }
}

TEST_CASE("A page shows its frame inside the sheet frame at true aspect", "[PageWriter]")
{
    Object object;
    object.init();
    LayerCamera* camera = object.addNewCameraLayer();
    LayerBitmap* layer = object.addNewBitmapLayer();
    layer->addNewKeyFrameAt(2);
    layer->getBitmapImageAtFrame(2)->drawRect(camera->getViewRect(), Qt::NoPen, Qt::black, QPainter::CompositionMode_SourceOver, false);
    PageSpec spec;
    spec.size = PageSpec::B4;
    spec.dpi = 50;
    const QImage page = renderPage(spec, paperFramePages(object, *camera, 1), 1); // page 1 is frame 2
    const QRectF f = paperFrameOnPage(QRectF(QPointF(), QSizeF(page.size())), camera->getViewSize());

    const QSizeF view = camera->getViewSize();
    CHECK(f.width() / f.height() == Approx(view.width() / view.height()).epsilon(0.001));
    CHECK(QRectF(QPointF(), QSizeF(page.size())).contains(f));
    CHECK(page.pixel(f.toRect().center()) == qRgb(0, 0, 0));
    CHECK(page.pixel(f.toAlignedRect().adjusted(2, 2, -2, -2).topLeft()) == qRgb(0, 0, 0));
    CHECK(page.pixel(qRound(f.center().x()), qRound(f.bottom() + 3)) == qRgb(255, 255, 255));
}

TEST_CASE("writePages writes one PDF page or PNG file per page", "[PageWriter]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    PageSpec spec;
    spec.dpi = 30;
    const PagePaintFn paint = [](QPainter& p, const QRectF& page, int) { p.drawRect(page.adjusted(10, 10, -10, -10)); };

    REQUIRE(writePages(dir.filePath("sheets.pdf"), spec, 3, paint).ok());
    CHECK(pdfPageCount(dir.filePath("sheets.pdf")) == 3);

    REQUIRE(writePages(dir.filePath("sheets.png"), spec, 2, paint).ok());
    CHECK(QFileInfo::exists(dir.filePath("sheets_0001.png")));
    CHECK(QFileInfo::exists(dir.filePath("sheets_0002.png")));
    CHECK_FALSE(QFileInfo::exists(dir.filePath("sheets_0003.png")));
    CHECK(QImage(dir.filePath("sheets_0001.png")).size() == spec.pixelSize());

    CHECK_FALSE(writePages(dir.filePath("missing/sheets.pdf"), spec, 1, paint).ok());
    CHECK_FALSE(writePages(dir.filePath("sheets.jpg"), spec, 1, paint).ok());
}

TEST_CASE("A failed writePages keeps the old files and leaves no partial file", "[PageWriter]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    PageSpec spec;
    spec.dpi = 30;
    const PagePaintFn paint = [](QPainter& p, const QRectF& page, int) { p.drawRect(page.adjusted(10, 10, -10, -10)); };
    const QByteArray old = "old sheet";
    for (const QString& name : { QString("sheets_0001.png"), QString("sheets.pdf") })
    {
        QFile file(dir.filePath(name));
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(old);
    }
    auto content = [&dir](const QString& name) {
        QFile file(dir.filePath(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };

    // A folder where page 2 is written makes that write fail.
    REQUIRE(QDir(dir.path()).mkdir("sheets_0002.png.tmp"));
    CHECK_FALSE(writePages(dir.filePath("sheets.png"), spec, 2, paint).ok());
    CHECK(content("sheets_0001.png") == old);
    CHECK_FALSE(QFileInfo::exists(dir.filePath("sheets_0001.png.tmp")));
    CHECK_FALSE(QFileInfo::exists(dir.filePath("sheets_0002.png")));

    REQUIRE(QDir(dir.path()).mkdir("sheets.pdf.tmp"));
    CHECK_FALSE(writePages(dir.filePath("sheets.pdf"), spec, 2, paint).ok());
    CHECK(content("sheets.pdf") == old);
}

TEST_CASE("Cancelling writePages after the first page leaves no output files", "[PageWriter]")
{
    PageSpec spec;
    spec.dpi = 30;
    const PagePaintFn paint = [](QPainter& p, const QRectF& page, int) { p.drawRect(page.adjusted(10, 10, -10, -10)); };
    for (const QString& name : { QString("sheets.png"), QString("sheets.pdf") })
    {
        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        QList<int> done;
        const Status st = writePages(dir.filePath(name), spec, 3, paint, [&done](int pages) { done << pages; return pages < 1; });
        CHECK(st == Status::CANCELED);
        CHECK(done == QList<int>{ 1 });
        CHECK(QDir(dir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }
}

TEST_CASE("The command line exports a frame range on paper", "[PageWriter]")
{
    CommandLineParser parser;
    parser.process({ "app", "in.pclx", "-o", "out.pdf", "--paper", "b4" });
    CHECK(parser.paper() == PageSpec::B4);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString project = QtResourceToFile(":/empty.pclx", "empty.pclx", dir);
    Object* object = new Object;
    object->init();
    object->addNewCameraLayer();
    Editor editor;
    ScribbleArea scribbleArea(nullptr);
    editor.setScribbleArea(&scribbleArea);
    editor.setObject(object);
    editor.init();
    CommandLineExporter exporter(&editor);

    REQUIRE(exporter.process(project, { dir.filePath("out.pdf") }, "", -1, -1, 1, 4, false, PageSpec::A4).ok());
    CHECK(pdfPageCount(dir.filePath("out.pdf")) == 4);
}

TEST_CASE("B4 pages are JIS B4, 257 x 364 mm", "[PageWriter]")
{
    PageSpec spec;
    spec.size = PageSpec::B4;
    const QSizeF mm = spec.pageSize().size(QPageSize::Millimeter);
    CHECK(mm.width() == Approx(257).margin(0.5));
    CHECK(mm.height() == Approx(364).margin(0.5));
}