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

#include <filesystem>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
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

static Status writePdf(const QString& path, const PageSpec& spec, int pageCount, const PagePaintFn& paint)
{
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

Status writePages(const QString& path, const PageSpec& spec, int pageCount, const PagePaintFn& paint)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix != "png" && suffix != "pdf") { return pageError(path, "the file name must end in .pdf or .png"); }

    // Every file is written as <name>.tmp first and renamed only when all are complete, so a failure keeps the old files.
    QStringList files;
    if (suffix == "pdf") { files << path; }
    const QString base = info.dir().filePath(info.completeBaseName());
    for (int i = 0; suffix == "png" && i < pageCount; ++i) { files << QString("%1_%2.png").arg(base).arg(i + 1, 4, 10, QChar('0')); }
    auto removeTemporaries = [&files] { for (const QString& file : files) { QFile::remove(file + ".tmp"); } };

    for (int i = 0; i < files.size(); ++i)
    {
        const QString tmp = files[i] + ".tmp";
        QFile::remove(tmp);
        Status st = Status::OK;
        if (suffix == "pdf") { st = writePdf(tmp, spec, pageCount, paint); }
        else
        {
            QImageWriter writer(tmp, "PNG");
            if (!writer.write(renderPage(spec, paint, i))) { st = pageError(files[i], writer.errorString()); }
        }
        if (!st.ok())
        {
            removeTemporaries();
            return st;
        }
    }
    for (const QString& file : files)
    {
        std::error_code renameError;
        std::filesystem::rename(QFileInfo(file + ".tmp").filesystemFilePath(), QFileInfo(file).filesystemFilePath(), renameError);
        if (renameError)
        {
            removeTemporaries();
            return pageError(file, QString::fromStdString(renameError.message()));
        }
    }
    return Status::OK;
}
