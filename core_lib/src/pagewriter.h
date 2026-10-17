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
#ifndef PAGEWRITER_H
#define PAGEWRITER_H

#include <functional>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include "pencilerror.h"

class QPainter;

/// A paper page: size, orientation and the pixels per inch its painter works in.
struct PageSpec
{
    enum Size { A4, B4 }; // B4 is JIS B4 (257 x 364 mm)
    Size size = A4;
    QPageLayout::Orientation orientation = QPageLayout::Landscape;
    int dpi = 150;

    QPageSize pageSize() const { return QPageSize(size == B4 ? QPageSize::JisB4 : QPageSize::A4); }
    /// The page in pixels at dpi, oriented.
    QSize pixelSize() const;
};

/// Paints page `index` into `page` (pixels at the spec's dpi, origin top left). The page is white.
using PagePaintFn = std::function<void(QPainter& painter, const QRectF& page, int index)>;

/// One page as an image, for tests and PNG output.
QImage renderPage(const PageSpec& spec, const PagePaintFn& paint, int page);

/// Gets the number of pages done after each page; returns false to cancel.
using PageProgressFn = std::function<bool(int pagesDone)>;

/// Writes pageCount pages: a .pdf is one file, a .png path writes <base>_0001.png, <base>_0002.png, ...
/// Each file is written as <name>.tmp and renamed when all are written; a failure leaves the old files.
/// A cancel from progress returns Status::CANCELED and also leaves the old files.
Status writePages(const QString& path, const PageSpec& spec, int pageCount, const PagePaintFn& paint,
                  const PageProgressFn& progress = {});

#endif // PAGEWRITER_H
