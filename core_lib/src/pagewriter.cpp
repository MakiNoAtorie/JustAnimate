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
#include "pagewriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainter>
#include <QPdfWriter>

QSize PageSpec::pixelSize() const
{
    const QSize s = pageSize().sizePixels(dpi);
    return orientation == QPageLayout::Landscape ? s.transposed() : s;
}

QImage renderPage(const PageSpec& spec, const PagePaintFn& paint, int page)
{
    QImage image(spec.pixelSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    paint(painter, QRectF(QPointF(), QSizeF(image.size())), page);
    return image;
}

static Status pageError(const QString& path, const QString& error)
{
    DebugDetails dd;
    dd << "writePages" << QString("&nbsp;&nbsp;filePath: ").append(path) << QString("&nbsp;&nbsp;Error: ").append(error);
    return Status(Status::FAIL, dd, QCoreApplication::translate("PageWriter", "Export on paper failed"),
                  QCoreApplication::translate("PageWriter", "Could not write %1.").arg(QDir::toNativeSeparators(path)));
}

Status writePages(const QString& path, const PageSpec& spec, int pageCount, const PagePaintFn& paint)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix == "png")
    {
        const QString base = info.dir().filePath(info.completeBaseName());
        for (int i = 0; i < pageCount; ++i)
        {
            const QString file = QString("%1_%2.png").arg(base).arg(i + 1, 4, 10, QChar('0'));
            QImageWriter writer(file, "PNG");
            if (!writer.write(renderPage(spec, paint, i))) { return pageError(file, writer.errorString()); }
        }
        return Status::OK;
    }
    if (suffix != "pdf") { return pageError(path, "the file name must end in .pdf or .png"); }

    QPdfWriter writer(path);
    writer.setResolution(spec.dpi);
    writer.setPageLayout(QPageLayout(spec.pageSize(), spec.orientation, QMarginsF()));
    QPainter painter;
    if (!painter.begin(&writer)) { return pageError(path, "cannot open the file"); }
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF page(0, 0, writer.width(), writer.height());
    for (int i = 0; i < pageCount; ++i)
    {
        if (i > 0) { writer.newPage(); }
        paint(painter, page, i);
    }
    return painter.end() ? Status::OK : pageError(path, "cannot finish the file");
}
