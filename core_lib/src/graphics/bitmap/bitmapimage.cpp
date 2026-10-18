/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "bitmapimage.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainterPath>
#include <QThreadPool>
#include "util.h"

#include "blitrect.h"
#include "tile.h"
#include "tiledbuffer.h"
#include "stageprofiler.h"

BitmapImage::BitmapImage()
{
}

BitmapImage::BitmapImage(const BitmapImage& a) : KeyFrame(a)
{
    mBounds = a.mBounds;
    mMinBound = a.mMinBound;
    mEnableAutoCrop = a.mEnableAutoCrop;
    mImage = a.mImage;
}

BitmapImage::BitmapImage(const QRect& rectangle, const QColor& color)
{
    mBounds = rectangle;
    mImage = QImage(mBounds.size(), QImage::Format_ARGB32_Premultiplied);
    mImage.fill(color.rgba());
    mMinBound = false;
}

BitmapImage::BitmapImage(const QPoint& topLeft, const QImage& image)
{
    mBounds = QRect(topLeft, image.size());
    mMinBound = true;
    mImage = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

BitmapImage::BitmapImage(const QPoint& topLeft, const QString& path)
{
    setFileName(path);
    mImage = QImage();

    mBounds = QRect(topLeft, QSize(-1, 0));
    mMinBound = true;
    setModified(false);
}

BitmapImage::~BitmapImage()
{
}

void BitmapImage::setImage(QImage* img)
{
    Q_ASSERT(img && img->format() == QImage::Format_ARGB32_Premultiplied);
    mImage = *img;
    mMinBound = false;

    modification();
}

BitmapImage& BitmapImage::operator=(const BitmapImage& a)
{
    if (this == &a)
    {
        return *this; // a self-assignment
    }

    KeyFrame::operator=(a);
    mBounds = a.mBounds;
    mMinBound = a.mMinBound;
    mImage = a.mImage;
    modification();
    return *this;
}

BitmapImage* BitmapImage::clone() const
{
    BitmapImage* b = new BitmapImage(*this);
    b->setFileName(""); // don't link to the file of the source bitmap image

    const bool validKeyFrame = !fileName().isEmpty();
    if (validKeyFrame && !isModified())
    {
        // This bitmapImage is temporarily unloaded.
        // since it's not in the memory, we need to copy the linked png file to prevent data loss.
        QFileInfo finfo(fileName());
        Q_ASSERT(finfo.isAbsolute());
        Q_ASSERT(QFile::exists(fileName()));

        QString newFilePath;
        do
        {
            newFilePath = QString("%1/temp-%2.%3")
                .arg(finfo.canonicalPath())
                .arg(uniqueString(12))
                .arg(finfo.suffix());
        }
        while (QFile::exists(newFilePath));

        b->setFileName(newFilePath);
        bool ok = QFile::copy(fileName(), newFilePath);
        Q_ASSERT(ok);
        qDebug() << "COPY>" << fileName();
    }
    return b;
}

void BitmapImage::copyImageFrom(KeyFrame* source)
{
    BitmapImage* other = static_cast<BitmapImage*>(source);
    if (other == this) { return; }
    other->loadFile();
    mImage = other->mImage; // implicitly shared until one copy is painted
    mBounds = other->mBounds;
    mMinBound = other->mMinBound;
    mPendingLoad = {};
    modification();
}

static QImage readImageFile(const QString& path)
{
    QImage image(path);
    image.convertTo(QImage::Format_ARGB32_Premultiplied);
    return image;
}

void BitmapImage::loadFile()
{
    if (!fileName().isEmpty() && !isLoaded())
    {
        if (mPendingLoad.valid())
        {
            mImage = mPendingLoad.get();
        }
        else
        {
            StageScope stage(StageProfiler::DrawingLoad); // a decode on the calling thread
            mImage = readImageFile(fileName());
        }
        mBounds.setSize(mImage.size());
        mMinBound = false;
        notifyLoaded();
    }
    mPendingLoad = {};
}

void BitmapImage::loadFileAsync()
{
    if (fileName().isEmpty() || isLoaded() || mPendingLoad.valid()) { return; }
    auto promise = std::make_shared<std::promise<QImage>>();
    mPendingLoad = promise->get_future().share();
    workerPool()->start([promise, path = fileName()] { promise->set_value(readImageFile(path)); });
}

void BitmapImage::unloadFile()
{
    if (isModified() == false && !fileName().isEmpty())
    {
        mImage = QImage();
        mPendingLoad = {};
    }
}

bool BitmapImage::isLoaded() const
{
    if (mImage.isNull()) { return false; }

    return mImage.width() == mBounds.width();
}

quint64 BitmapImage::memoryUsage()
{
    if (!mImage.isNull())
    {
        return imageSize(mImage);
    }
    return 0;
}

void BitmapImage::paintImage(QPainter& painter)
{
    painter.drawImage(mBounds.topLeft(), *image());
}

QImage* BitmapImage::image()
{
    loadFile();
    return &mImage;
}

BitmapImage BitmapImage::copy()
{
    return BitmapImage(mBounds.topLeft(), *image());
}

BitmapImage BitmapImage::copy(QRect rectangle)
{
    if (rectangle.isEmpty() || mBounds.isEmpty()) return BitmapImage();

    QRect intersection2 = rectangle.translated(-mBounds.topLeft());

    BitmapImage result(rectangle.topLeft(), image()->copy(intersection2));
    return result;
}

void BitmapImage::paste(BitmapImage* bitmapImage, QPainter::CompositionMode cm)
{
    if(bitmapImage->width() <= 0 || bitmapImage->height() <= 0)
    {
        return;
    }

    setCompositionModeBounds(bitmapImage, cm);

    QImage* image2 = bitmapImage->image();

    QPainter painter(image());
    painter.setCompositionMode(cm);
    painter.drawImage(bitmapImage->mBounds.topLeft() - mBounds.topLeft(), *image2);
    painter.end();

    modification();
}

void BitmapImage::paste(TiledBuffer* tiledBuffer, QPainter::CompositionMode cm, const QTransform& toImage)
{
    StageScope stage(StageProfiler::BufferPaste);
    if(tiledBuffer->bounds().width() <= 0 || tiledBuffer->bounds().height() <= 0)
    {
        return;
    }
    // Erasing cannot add pixels. Smudge tiles hold copies of the whole tile, so they keep the tile bounds.
    // Under a layer transform, smooth resampling can reach one pixel past the painted rect.
    if (cm != QPainter::CompositionMode_DestinationOut)
    {
        const QRect painted = cm == QPainter::CompositionMode_Source ? tiledBuffer->bounds() : tiledBuffer->paintBounds();
        const bool wholePixels = toImage.type() <= QTransform::TxTranslate && toImage.dx() == qRound(toImage.dx()) && toImage.dy() == qRound(toImage.dy());
        const int margin = wholePixels ? 0 : 1;
        extend(toImage.mapRect(QRectF(painted)).toAlignedRect().adjusted(-margin, -margin, margin, margin));
    }
    if (image()->isNull()) { return; } // nothing to erase
    QPainter painter(image());

    painter.setCompositionMode(cm);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setTransform(toImage * QTransform::fromTranslate(-mBounds.left(), -mBounds.top()));
    auto const tiles = tiledBuffer->tiles();
    for (const Tile* item : tiles) {
        painter.drawImage(item->pos(), item->image());
    }
    painter.end();

    modification();

    // Erase and smudge can clear pixels: shrink the bounds once per stroke.
    if (cm == QPainter::CompositionMode_DestinationOut || cm == QPainter::CompositionMode_Source)
    {
        mMinBound = false;
        autoCrop();
    }
}

BitmapPatch BitmapImage::patch(const QRect& rect)
{
    const QImage* source = image();
    BitmapPatch p{ mBounds, mMinBound, rect, QImage() };
    if (rect.isEmpty()) { return p; }
    if (source->isNull())
    {
        p.pixels = QImage(rect.size(), QImage::Format_ARGB32_Premultiplied);
        p.pixels.fill(Qt::transparent);
    }
    else
    {
        // QImage::copy fills the part outside the image with 0, which is transparent.
        p.pixels = source->copy(rect.translated(-mBounds.topLeft()));
    }
    return p;
}

BitmapPatch BitmapImage::whole()
{
    const QImage* source = image();
    return { mBounds, mMinBound, mBounds, *source };
}

void BitmapImage::applyPatch(const BitmapPatch& p)
{
    image();
    if (p.rect == p.bounds && p.pixels.size() == p.bounds.size())
    {
        // A whole patch: share its pixels.
        mImage = p.pixels;
        mBounds = p.bounds;
        mMinBound = p.minBound;
        modification();
        return;
    }
    if (mBounds != p.bounds)
    {
        QImage resized(p.bounds.size(), QImage::Format_ARGB32_Premultiplied);
        resized.fill(Qt::transparent);
        if (!resized.isNull() && !mImage.isNull())
        {
            QPainter painter(&resized);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.drawImage(mBounds.topLeft() - p.bounds.topLeft(), mImage);
        }
        mImage = resized;
        mBounds = p.bounds;
    }
    if (!p.pixels.isNull() && !mImage.isNull())
    {
        QPainter painter(&mImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(p.rect.topLeft() - mBounds.topLeft(), p.pixels);
    }
    mMinBound = p.minBound;
    modification();
}

QRect BitmapImage::changedRect(BitmapImage& a, BitmapImage& b)
{
    const QImage* imageA = a.image();
    const QImage* imageB = b.image();
    if (a.mBounds == b.mBounds && imageA->cacheKey() == imageB->cacheKey()) { return QRect(); }

    auto row = [](const QImage* img, const QRect& bounds, int y) -> const QRgb* {
        if (img->isNull() || y < bounds.top() || y > bounds.bottom()) { return nullptr; }
        return reinterpret_cast<const QRgb*>(img->constScanLine(y - bounds.top()));
    };
    auto pixelAt = [](const QRgb* line, const QRect& bounds, int x) -> QRgb {
        return (line && x >= bounds.left() && x <= bounds.right()) ? line[x - bounds.left()] : 0;
    };
    const QRect all = a.mBounds.united(b.mBounds);
    QRect changed;
    for (int y = all.top(); y <= all.bottom(); ++y)
    {
        const QRgb* lineA = row(imageA, a.mBounds, y);
        const QRgb* lineB = row(imageB, b.mBounds, y);
        if (!lineA && !lineB) { continue; }
        for (int x = all.left(); x <= all.right(); ++x)
        {
            if (pixelAt(lineA, a.mBounds, x) == pixelAt(lineB, b.mBounds, x)) { continue; }
            // Only the row ends can widen the rect: jump to the last differing pixel.
            int last = all.right();
            while (pixelAt(lineA, a.mBounds, last) == pixelAt(lineB, b.mBounds, last)) { --last; }
            changed |= QRect(x, y, last - x + 1, 1);
            break;
        }
    }
    return changed;
}

void BitmapImage::moveTopLeft(QPoint point)
{
    mBounds.moveTopLeft(point);
    // Size is unchanged so there is no need to update mBounds
    modification();
}

void BitmapImage::transform(QRect newBoundaries, bool smoothTransform)
{
    mBounds = newBoundaries;
    newBoundaries.moveTopLeft(QPoint(0, 0));
    QImage newImage(mBounds.size(), QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&newImage);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothTransform);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(newImage.rect(), QColor(0, 0, 0, 0));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(newBoundaries, *image());
    painter.end();
    mImage = newImage;

    modification();
}

BitmapImage BitmapImage::transformed(QRect selection, QTransform transform, bool smoothTransform)
{
    Q_ASSERT(!selection.isEmpty());

    BitmapImage selectedPart = copy(selection);

    // Get the transformed image
    QImage transformedImage;
    if (smoothTransform)
    {
        transformedImage = selectedPart.image()->transformed(transform, Qt::SmoothTransformation);
    }
    else
    {
        transformedImage = selectedPart.image()->transformed(transform);
    }
    return BitmapImage(transform.mapRect(selection).normalized().topLeft(), transformedImage);
}

BitmapImage BitmapImage::transformed(QRect newBoundaries, bool smoothTransform)
{
    BitmapImage transformedImage(newBoundaries, QColor(0, 0, 0, 0));
    QPainter painter(transformedImage.image());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothTransform);
    newBoundaries.moveTopLeft(QPoint(0, 0));
    painter.drawImage(newBoundaries, *image());
    painter.end();
    return transformedImage;
}

/** Update image bounds.
 *
 *  @param[in] newBoundaries the new bounds
 *
 *  Sets this image's bounds to rectangle.
 *  Modifies mBounds and crops mImage.
 */
void BitmapImage::updateBounds(QRect newBoundaries)
{
    // Check to make sure changes actually need to be made
    if (mBounds == newBoundaries) return;

    QImage newImage(newBoundaries.size(), QImage::Format_ARGB32_Premultiplied);
    newImage.fill(Qt::transparent);
    if (!newImage.isNull())
    {
        QPainter painter(&newImage);
        painter.drawImage(mBounds.topLeft() - newBoundaries.topLeft(), mImage);
        painter.end();
    }
    mImage = newImage;
    mBounds = newBoundaries;
    mMinBound = false;

    modification();
}

void BitmapImage::extend(const QPoint &p)
{
    if (!mBounds.contains(p))
    {
        extend(QRect(p, QSize(1, 1)));
    }
}

void BitmapImage::extend(QRect rectangle)
{
    if (rectangle.width() <= 0) rectangle.setWidth(1);
    if (rectangle.height() <= 0) rectangle.setHeight(1);
    if (mBounds.contains(rectangle))
    {
        // Do nothing
    }
    else
    {
        QRect newBoundaries = mBounds.united(rectangle).normalized();
        QImage newImage(newBoundaries.size(), QImage::Format_ARGB32_Premultiplied);
        newImage.fill(Qt::transparent);
        if (!newImage.isNull())
        {
            QPainter painter(&newImage);
            painter.drawImage(mBounds.topLeft() - newBoundaries.topLeft(), *image());
            painter.end();
        }
        mImage = newImage;
        mBounds = newBoundaries;

        modification();
    }
}

/** Updates the bounds after a drawImage operation with the composition mode cm.
 *
 *  @param[in] source The source image used for the drawImage call.
 *  @param[in] cm The composition mode that will be used for the draw image
 *
 *  @see BitmapImage::setCompositionModeBounds(BitmapImage, QPainter::CompositionMode)
 */
void BitmapImage::setCompositionModeBounds(BitmapImage *source, QPainter::CompositionMode cm)
{
    if (source)
    {
        setCompositionModeBounds(source->mBounds, source->mMinBound, cm);
    }
}

/** Updates the bounds after a draw operation with the composition mode cm.
 *
 * @param[in] sourceBounds The bounds of the source used for drawcall.
 * @param[in] isSourceMinBounds Is sourceBounds the minimal bounds for the source image
 * @param[in] cm The composition mode that will be used for the draw image
 *
 * For a call to draw image of a QPainter (initialized with mImage) with an argument
 * of source, this function intelligently calculates the bounds. It will attempt to
 * preserve minimum bounds based on the composition mode.
 *
 * This works baed on the principle that some minimal bounds can be determined
 * solely by the minimal bounds of this and source, depending on the value of cm.
 * Some composition modes only expand, or have no affect on the bounds.
 *
 * @warning The draw operation described by the arguments of this
 *          function needs to be called after this function is run,
 *          or the bounds will be out of sync. If mBounds is null,
 *          no draw operation needs to be performed.
 */
void BitmapImage::setCompositionModeBounds(QRect sourceBounds, bool isSourceMinBounds, QPainter::CompositionMode cm)
{
    QRect newBoundaries;
    switch(cm)
    {
    case QPainter::CompositionMode_Destination:
    case QPainter::CompositionMode_SourceAtop:
        // The Destination and SourceAtop modes
        // do not change the bounds from destination.
        newBoundaries = mBounds;
        // mMinBound remains the same
        break;
    case QPainter::CompositionMode_SourceIn:
    case QPainter::CompositionMode_DestinationIn:
    case QPainter::CompositionMode_Clear:
    case QPainter::CompositionMode_DestinationOut:
        // The bounds of the result of SourceIn, DestinationIn, Clear, and DestinationOut
        // modes are no larger than the destination bounds
        newBoundaries = mBounds;
        mMinBound = false;
        break;
    default:
        // If it's not one of the above cases, create a union of the two bounds.
        // This contains the minimum bounds, if both the destination and source
        // use their respective minimum bounds.
        newBoundaries = mBounds.united(sourceBounds);
        mMinBound = mMinBound && isSourceMinBounds;
    }

    updateBounds(newBoundaries);
}

/** Removes any transparent borders by reducing the boundaries.
 *
 *  This function reduces the bounds of an image until the top and
 *  bottom rows, and the left and right columns of pixels each
 *  contain at least one pixel with a non-zero alpha value
 *  (i.e. non-transparent pixel). Both mBounds and
 *  the size of #mImage are updated.
 *
 *  @pre mBounds.size() == mImage->size()
 *  @post Either the first and last rows and columns all contain a
 *        pixel with alpha > 0 or mBounds.isEmpty() == true
 *  @post isMinimallyBounded() == true
 */
void BitmapImage::autoCrop()
{
    if (!mEnableAutoCrop) return;
    if (mBounds.isEmpty()) return; // Exit if current bounds are null
    if (mImage.isNull()) return;

    Q_ASSERT(mBounds.size() == mImage.size());

    // Exit if already min bounded
    if (mMinBound) return;

    // Get image properties
    const int width = mImage.width();

    // Relative top and bottom row indices (inclusive)
    int relTop = 0;
    int relBottom = mBounds.height() - 1;

    // Check top row
    bool isEmpty = true; // Used to track if a non-transparent pixel has been found
    while (isEmpty && relTop <= relBottom) // Loop through rows
    {
        // Point cursor to the first pixel in the current top row
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(relTop));
        for (int col = 0; col < width; col++) // Loop through pixels in row
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if (qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in row relTop,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to the next pixel in the row
            cursor++;
        }
        if (isEmpty)
        {
            // If the row we just checked was empty, increase relTop
            // to remove the empty row from the top of the bounding box
            ++relTop;
        }
    }

    // Check bottom row
    isEmpty = true; // Reset isEmpty
    while (isEmpty && relBottom >= relTop) // Loop through rows
    {
        // Point cursor to the first pixel in the current bottom row
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(relBottom));
        for (int col = 0; col < width; col++) // Loop through pixels in row
        {
            // If the pixel is not transparent
            // (i.e. alpha channel > 0)
            if(qAlpha(*cursor) != 0)
            {
                // We've found a non-transparent pixel in row relBottom,
                // so we can stop looking for one
                isEmpty = false;
                break;
            }
            // Move cursor to point to the next pixel in the row
            ++cursor;
        }
        if (isEmpty)
        {
            // If the row we just checked was empty, decrease relBottom
            // to remove the empty row from the bottom of the bounding box
            --relBottom;
        }
    }

    // Relative left and right column indices (inclusive)
    int relLeft = 0;
    int relRight = mBounds.width()-1;

    // A pixel in the current border column ends a side at once, without a scan of every row.
    auto columnHasPixel = [&](int col) {
        for (int row = relTop; row <= relBottom; ++row)
        {
            if (qAlpha(reinterpret_cast<const QRgb*>(mImage.constScanLine(row))[col]) != 0) { return true; }
        }
        return false;
    };

    // Check left column - find minimum transparent span at start of each row
    int minLeft = mBounds.width();
    if (columnHasPixel(0)) { minLeft = 0; }
    for (int row = relTop; row <= relBottom; ++row)
    {
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(row));
        for (int col = 0; col < minLeft; ++col)
        {
            if (qAlpha(*cursor) != 0)
            {
                minLeft = col;
                break;
            }
            ++cursor;
        }
    }
    relLeft = minLeft;

    // Check right column - find minimum transparent span at end of each row
    int minRight = 0;
    if (columnHasPixel(mBounds.width() - 1)) { minRight = mBounds.width() - 1; }
    for (int row = relTop; row <= relBottom; ++row)
    {
        const QRgb* cursor = reinterpret_cast<const QRgb*>(mImage.constScanLine(row)) + mBounds.width() - 1;
        for (int col = mBounds.width() - 1; col > minRight; --col)
        {
            if (qAlpha(*cursor) != 0)
            {
                minRight = col;
                break;
            }
            --cursor;
        }
    }
    relRight = minRight;

    if (relTop > relBottom || relLeft > relRight)
    {
        clear();
        return;
    }
    //qDebug() << "Original" << mBounds;
    //qDebug() << "Autocrop" << relLeft << relTop << relRight - mBounds.width() + 1 << relBottom - mBounds.height() + 1;
    // Update mBounds and mImage if necessary
    updateBounds(mBounds.adjusted(relLeft, relTop, relRight - mBounds.width() + 1, relBottom - mBounds.height() + 1));

    //qDebug() << "New bounds" << mBounds;

    mMinBound = true;
}

QRgb BitmapImage::pixel(int x, int y)
{
    return pixel(QPoint(x, y));
}

QRgb BitmapImage::pixel(QPoint p)
{
    QRgb result = qRgba(0, 0, 0, 0); // black
    if (mBounds.contains(p))
        result = image()->pixel(p - mBounds.topLeft());
    return result;
}

void BitmapImage::setPixel(int x, int y, QRgb color)
{
    setPixel(QPoint(x, y), color);
}

void BitmapImage::setPixel(QPoint p, QRgb color)
{
    setCompositionModeBounds(QRect(p, QSize(1,1)), true, QPainter::CompositionMode_SourceOver);
    if (mBounds.contains(p))
    {
        image()->setPixel(p - mBounds.topLeft(), color);
    }
    modification();
}

void BitmapImage::fillNonAlphaPixels(const QRgb color)
{
    if (mBounds.isEmpty()) { return; }

    BitmapImage fill(bounds(), color);
    paste(&fill, QPainter::CompositionMode_SourceIn);
}

void BitmapImage::drawLine(QPointF P1, QPointF P2, QPen pen, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = 2 + pen.width();
    setCompositionModeBounds(QRect(P1.toPoint(), P2.toPoint()).normalized().adjusted(-width, -width, width, width), true, cm);
    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.drawLine(P1 - mBounds.topLeft(), P2 - mBounds.topLeft());
        painter.end();
    }
    modification();
}

void BitmapImage::drawRect(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    setCompositionModeBounds(rectangle.adjusted(-width, -width, width, width).toRect(), true, cm);
    if (brush.style() == Qt::RadialGradientPattern)
    {
        QRadialGradient* gradient = (QRadialGradient*)brush.gradient();
        gradient->setCenter(gradient->center() - mBounds.topLeft());
        gradient->setFocalPoint(gradient->focalPoint() - mBounds.topLeft());
    }
    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);

        // Adjust the brush rectangle to be bigger than the bounds itself,
        // otherwise there will be artifacts shown in some cases when smudging
        painter.drawRect(rectangle.translated(-mBounds.topLeft()).adjusted(-1, -1, 1, 1));
        painter.end();
    }
    modification();
}

void BitmapImage::drawEllipse(QRectF rectangle, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    setCompositionModeBounds(rectangle.adjusted(-width, -width, width, width).toRect(), true, cm);
    if (brush.style() == Qt::RadialGradientPattern)
    {
        QRadialGradient* gradient = (QRadialGradient*)brush.gradient();
        gradient->setCenter(gradient->center() - mBounds.topLeft());
        gradient->setFocalPoint(gradient->focalPoint() - mBounds.topLeft());
    }
    if (!image()->isNull())
    {
        QPainter painter(image());

        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setCompositionMode(cm);
        painter.drawEllipse(rectangle.translated(-mBounds.topLeft()));
        painter.end();
    }
    modification();
}

void BitmapImage::drawPath(QPainterPath path, QPen pen, QBrush brush,
                           QPainter::CompositionMode cm, bool antialiasing)
{
    int width = pen.width();
    // qreal inc = 1.0 + width / 20.0;

    setCompositionModeBounds(path.controlPointRect().adjusted(-width, -width, width, width).toRect(), true, cm);

    if (!image()->isNull())
    {
        QPainter painter(image());
        painter.setCompositionMode(cm);
        painter.setRenderHint(QPainter::Antialiasing, antialiasing);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setTransform(QTransform().translate(-mBounds.left(), -mBounds.top()));
        painter.setWorldMatrixEnabled(true);
        if (path.length() > 0)
        {
            /*
            for (int pt = 0; pt < path.elementCount() - 1; pt++)
            {
                qreal dx = path.elementAt(pt + 1).x - path.elementAt(pt).x;
                qreal dy = path.elementAt(pt + 1).y - path.elementAt(pt).y;
                qreal m = sqrt(dx*dx + dy*dy);
                qreal factorx = dx / m;
                qreal factory = dy / m;
                for (float h = 0.f; h < m; h += inc)
                {
                    qreal x = path.elementAt(pt).x + factorx * h;
                    qreal y = path.elementAt(pt).y + factory * h;
                    painter.drawPoint(QPointF(x, y));
                }
            }
            */
            painter.drawPath( path );
        }
        else
        {
            // forces drawing when points are coincident (mousedown)
            painter.drawPoint(static_cast<int>(path.elementAt(0).x), static_cast<int>(path.elementAt(0).y));
        }
        painter.end();
    }
    modification();
}

BitmapImage* BitmapImage::scanToTransparent(BitmapImage *img, const int threshold, const bool redEnabled, const bool greenEnabled, const bool blueEnabled)
{
    Q_ASSERT(img != nullptr);

    QRgb rgba = img->constScanLine(img->left(), img->top());
    if (qAlpha(rgba) == 0)
        return img;

    for (int x = img->left(); x <= img->right(); x++)
    {
        for (int y = img->top(); y <= img->bottom(); y++)
        {
            rgba = img->constScanLine(x, y);

            if (qAlpha(rgba) == 0)
                break;

            const int grayValue = qGray(rgba);
            const int redValue = qRed(rgba);
            const int greenValue = qGreen(rgba);
            const int blueValue = qBlue(rgba);
            if (grayValue >= threshold)
            {   // IF Threshold or above
                img->scanLine(x, y, transp);
            }
            else if (redValue > greenValue + COLORDIFF &&
                     redValue > blueValue + COLORDIFF &&
                     redValue > grayValue + GRAYSCALEDIFF)
            {   // IF Red line
                if (redEnabled)
                {
                    img->scanLine(x, y, redline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else if (greenValue > redValue + COLORDIFF &&
                     greenValue > blueValue + COLORDIFF &&
                     greenValue > grayValue + GRAYSCALEDIFF)
            {   // IF Green line
                if (greenEnabled)
                {
                    img->scanLine(x, y, greenline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else if (blueValue > redValue + COLORDIFF &&
                     blueValue > greenValue + COLORDIFF &&
                     blueValue > grayValue + GRAYSCALEDIFF)
            {   // IF Blue line
                if (blueEnabled)
                {
                    img->scanLine(x, y, blueline);
                }
                else
                {
                    img->scanLine(x, y, transp);
                }
            }
            else
            {   // okay, so it is in grayscale graduation area
                if (grayValue >= LOW_THRESHOLD)
                {
                    const qreal factor = static_cast<qreal>(threshold - grayValue) / static_cast<qreal>(threshold - LOW_THRESHOLD);
                    img->scanLine(x , y, qRgba(0, 0, 0, static_cast<int>(threshold * factor)));
                }
                else // grayValue < LOW_THRESHOLD
                {
                    img->scanLine(x , y, blackline);
                }
            }
        }
    }
    img->modification();
    return img;
}

Status BitmapImage::writeFile(const QString& filename)
{
    DebugDetails dd;
    dd << "BitmapImage::writeFile";
    mPendingLoad = {}; // the file changes
    dd << QString("&nbsp;&nbsp;filename = ").append(filename);

    QImageWriter writer(filename);
    if (!mImage.isNull())
    {
        bool b = writer.write(mImage);
        if (b) {
            return Status::OK;
        } else {
            dd << QString("&nbsp;&nbsp;Error: %1 (Code %2)").arg(writer.errorString()).arg(static_cast<int>(writer.error()));
            return Status(Status::FAIL, dd);
        }
    }

    if (bounds().isEmpty())
    {
        QFile f(filename);
        if(f.exists())
        {
            bool b = f.remove();
            if (!b) {
                dd << "&nbsp;&nbsp;Error: Image is empty but unable to remove file.";
                return Status::FAIL;
            }
        }

        // The frame is likely empty, act like there's no file name
        // so we don't end up writing to it later.
        setFileName("");
    }
    return Status::SAFE;
}

void BitmapImage::clear()
{
    mImage = QImage(); // null image
    mBounds = QRect(0, 0, 0, 0);
    mMinBound = true;
    modification();
}

QRgb BitmapImage::constScanLine(int x, int y) const
{
    QRgb result = QRgb();
    if (mBounds.contains(x, y)) {
        result = *(reinterpret_cast<const QRgb*>(mImage.constScanLine(y - mBounds.top())) + x - mBounds.left());
    }
    return result;
}

void BitmapImage::scanLine(int x, int y, QRgb color)
{
    if (!mBounds.contains(x, y)) {
        return;
    }
    // Make sure color is premultiplied before calling
    *(reinterpret_cast<QRgb*>(image()->scanLine(y - mBounds.top())) + x - mBounds.left()) = color;
}

void BitmapImage::clear(QRect rectangle)
{
    QRect clearRectangle = mBounds.intersected(rectangle);
    clearRectangle.moveTopLeft(clearRectangle.topLeft() - mBounds.topLeft());

    setCompositionModeBounds(clearRectangle, true, QPainter::CompositionMode_Clear);

    QPainter painter(image());
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(clearRectangle, QColor(0, 0, 0, 0));
    painter.end();

    modification();
}

bool BitmapImage::floodFill(BitmapImage** replaceImage,
                            const BitmapImage* targetImage,
                            const QRect& cameraRect,
                            const QPoint& point,
                            const QRgb& fillColor,
                            int tolerance,
                            const int expandValue,
                            const int closeGaps)
{
    // Fill region must be 1 pixel larger than the target image to fill regions on the edge connected only by transparent pixels
    const QRect& fillBounds = targetImage->mBounds.adjusted(-1, -1, 1, 1);
    QRect maxBounds = cameraRect.united(fillBounds).adjusted(-expandValue, -expandValue, expandValue, expandValue);
    const int maxWidth = maxBounds.width(), left = maxBounds.left(), top = maxBounds.top();

    // Square tolerance for use with compareColor
    tolerance = static_cast<int>(qPow(tolerance, 2));

    // Dilate everything that differs from the seed colour so gaps up to closeGaps px seal
    BitmapImage closedImage;
    if (closeGaps > 0)
    {
        closedImage = *targetImage;
        const QRgb seed = targetImage->constScanLine(point.x(), point.y());
        const int radius = (closeGaps + 1) / 2;
        QHash<QRgb, bool> cache;
        const QRect& b = targetImage->mBounds;
        for (int y = b.top(); y <= b.bottom(); y++)
        {
            for (int x = b.left(); x <= b.right(); x++)
            {
                const QRgb c = targetImage->constScanLine(x, y);
                if (compareColor(c, seed, tolerance, &cache)) continue;
                for (int dy = -radius; dy <= radius; dy++)
                    for (int dx = -radius; dx <= radius; dx++)
                        if ((dx || dy) && QPoint(x + dx, y + dy) != point &&
                            compareColor(targetImage->constScanLine(x + dx, y + dy), seed, tolerance, &cache))
                            closedImage.scanLine(x + dx, y + dy, c);
            }
        }
    }

    QRect newBounds;
    bool *filledPixels = floodFillPoints(closeGaps > 0 ? &closedImage : targetImage, maxBounds, point, tolerance, newBounds);

    QRect translatedSearchBounds = newBounds.translated(-maxBounds.topLeft());

    // The scanned bounds should take the expansion into account
    const QRect& expandRect = newBounds.adjusted(-expandValue, -expandValue, expandValue, expandValue);
    if (expandValue > 0) {
        newBounds = expandRect;
    }
    if (!maxBounds.contains(newBounds)) {
        newBounds = maxBounds;
    }
    translatedSearchBounds = newBounds.translated(-maxBounds.topLeft());

    if (expandValue > 0) {
        expandFill(filledPixels, translatedSearchBounds, maxBounds, expandValue);
    }

    *replaceImage = new BitmapImage(newBounds, Qt::transparent);

    // Fill all the found pixels
    for (int y = translatedSearchBounds.top(); y <= translatedSearchBounds.bottom(); y++)
    {
        for (int x = translatedSearchBounds.left(); x <= translatedSearchBounds.right(); x++)
        {
            const int index = y * maxWidth + x;
            if (!filledPixels[index])
            {
                continue;
            }
            (*replaceImage)->scanLine(x + left, y + top, fillColor);
        }
    }

    delete[] filledPixels;

    return true;
}

// Flood filling based on this scanline algorithm
// ----- http://lodev.org/cgtutor/floodfill.html
bool* BitmapImage::floodFillPoints(const BitmapImage* targetImage,
                                   const QRect& searchBounds,
                                   QPoint point,
                                   const int tolerance,
                                   QRect& newBounds)
{
    QRgb oldColor = targetImage->constScanLine(point.x(), point.y());
    oldColor = qRgba(qRed(oldColor), qGreen(oldColor), qBlue(oldColor), qAlpha(oldColor));

    // Preparations
    QList<QPoint> queue; // queue all the pixels of the filled area (as they are found)

    QPoint tempPoint;
    QScopedPointer< QHash<QRgb, bool> > cache(new QHash<QRgb, bool>());

    int xTemp = 0;
    bool spanLeft = false;
    bool spanRight = false;

    queue.append(point);
    // Preparations END

    bool *filledPixels = new bool[searchBounds.height()*searchBounds.width()]{};

    BlitRect blitBounds(point);
    while (!queue.empty())
    {
        tempPoint = queue.takeFirst();

        point.setX(tempPoint.x());
        point.setY(tempPoint.y());

        xTemp = point.x();

        int xCoord = xTemp - searchBounds.left();
        int yCoord = point.y() - searchBounds.top();

        if (filledPixels[yCoord*searchBounds.width()+xCoord]) continue;

        while (xTemp >= searchBounds.left() &&
               compareColor(targetImage->constScanLine(xTemp, point.y()), oldColor, tolerance, cache.data())) xTemp--;
        xTemp++;

        spanLeft = spanRight = false;
        while (xTemp <= searchBounds.right() &&
               compareColor(targetImage->constScanLine(xTemp, point.y()), oldColor, tolerance, cache.data()))
        {

            QPoint floodPoint = QPoint(xTemp, point.y());
            if (!blitBounds.contains(floodPoint)) {
                blitBounds.extend(floodPoint);
            }

            xCoord = xTemp - searchBounds.left();
            // This pixel is what we're going to fill later
            filledPixels[yCoord*searchBounds.width()+xCoord] = true;

            if (!spanLeft && (point.y() > searchBounds.top()) &&
                compareColor(targetImage->constScanLine(xTemp, point.y() - 1), oldColor, tolerance, cache.data())) {
                queue.append(QPoint(xTemp, point.y() - 1));
                spanLeft = true;
            }
            else if (spanLeft && (point.y() > searchBounds.top()) &&
                     !compareColor(targetImage->constScanLine(xTemp, point.y() - 1), oldColor, tolerance, cache.data())) {
                spanLeft = false;
            }

            if (!spanRight && point.y() < searchBounds.bottom() &&
                compareColor(targetImage->constScanLine(xTemp, point.y() + 1), oldColor, tolerance, cache.data())) {
                queue.append(QPoint(xTemp, point.y() + 1));
                spanRight = true;
            }
            else if (spanRight && point.y() < searchBounds.bottom() &&
                     !compareColor(targetImage->constScanLine(xTemp, point.y() + 1), oldColor, tolerance, cache.data())) {
                spanRight = false;
            }

            Q_ASSERT(queue.count() < (searchBounds.width() * searchBounds.height()));
            xTemp++;
        }
    }

    newBounds = blitBounds;

    return filledPixels;
}

/** Finds all pixels closest to the input color and applies the input color to the image
 *
 * An example:
 *
 * 0 is where the color was found
 * 1 is the distance from the nearest pixel of that color
 *
 * 211112
 * 100001
 * 100001
 * 211112
 *
 * @param bitmapImage: Image to search
 * @param searchColor: Color to find
 */
void BitmapImage::expandFill(bool* fillPixels, const QRect& searchBounds, const QRect& maxBounds, int expand) {

    const int maxWidth = maxBounds.width();
    const int length = maxBounds.height() * maxBounds.width();

    int* manhattanPoints = new int[length]{};

    // Fill points with max length, this is important because otherwise the filled pixels will include a border of the expanded area
    std::fill_n(manhattanPoints, length, searchBounds.width()+searchBounds.height());

    for (int y = searchBounds.top(); y <= searchBounds.bottom(); y++)
    {
        for (int x = searchBounds.left(); x <= searchBounds.right(); x++)
        {
            const int index = y*maxWidth+x;

            if (fillPixels[index]) {
                manhattanPoints[index] = 0;
                continue;
            }

            if (y > searchBounds.top()) {
                // the value will be the num of pixels away from y - 1 of the next position
                manhattanPoints[index] = qMin(manhattanPoints[index],
                                             manhattanPoints[(y - 1) * maxWidth+x] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
            if (x > searchBounds.left()) {
                // the value will be the num of pixels away from x - 1 of the next position
                manhattanPoints[index] = qMin(manhattanPoints[index],
                                             manhattanPoints[y*maxWidth+(x - 1)] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
        }
    }

    // traverse from bottom right to top left
    for (int y = searchBounds.bottom(); y >= searchBounds.top(); y--)
    {
        for (int x = searchBounds.right(); x >= searchBounds.left(); x--)
        {
            const int index = y*maxWidth+x;

            if (y + 1 < searchBounds.bottom()) {
                manhattanPoints[index] = qMin(manhattanPoints[index], manhattanPoints[(y + 1)*maxWidth+x] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
            if (x + 1 < searchBounds.right()) {
                manhattanPoints[index] = qMin(manhattanPoints[index], manhattanPoints[y*maxWidth+(x + 1)] + 1);

                int distance = manhattanPoints[index];
                if (distance <= expand) {
                    fillPixels[index] = true;
                }
            }
        }
    }

    delete[] manhattanPoints;
}

