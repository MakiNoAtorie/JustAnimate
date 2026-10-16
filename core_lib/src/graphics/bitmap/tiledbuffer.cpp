/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2018 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "tiledbuffer.h"

#include <QElapsedTimer>
#include <QFile>
#include <QImageReader>
#include <QPainterPath>
#include <QThreadPool>
#include <QtEndian>
#include <QtMath>
#include <algorithm>
#include <atomic>
#include <cmath>

#include "tile.h"
#include "stageprofiler.h"
#include "util.h"

TiledBuffer::TiledBuffer(QObject* parent) : QObject(parent)
{
}

TiledBuffer::~TiledBuffer()
{
    clear();
    setPoolCapBytes(0);
}

qsizetype TiledBuffer::tileBytes(Tile* tile)
{
    return tile->image().sizeInBytes() + qsizetype(tile->coverage().capacity());
}

void TiledBuffer::setPoolCapBytes(qsizetype cap)
{
    mPoolCap = cap;
    while (mPoolBytes > mPoolCap)
    {
        mPoolBytes -= tileBytes(mPool.back());
        delete mPool.back();
        mPool.pop_back();
    }
}

Tile* TiledBuffer::getTileFromIndex(const TileIndex& tileIndex)
{
    Tile* selectedTile = mTiles.value(tileIndex, nullptr);

    if (!selectedTile) {
        // Time to allocate it, update table:
        const QPoint& tilePos (getTilePos(tileIndex));
        if (mPool.empty())
        {
            selectedTile = new Tile(tilePos, QSize(UNIFORM_TILE_SIZE, UNIFORM_TILE_SIZE));
        }
        else
        {
            selectedTile = mPool.back();
            mPool.pop_back();
            mPoolBytes -= tileBytes(selectedTile);
            selectedTile->moveTo(tilePos);
            // Coverage mode colourizes the whole image later, so only the image path clears it.
            if (!mCoverageMode) selectedTile->clear();
        }
        if (mSmudging) selectedTile->load(mSmudgeSource, mSmudgeTopLeft);
        if (mCoverageMode)
        {
            selectedTile->coverage().assign(UNIFORM_TILE_SIZE * UNIFORM_TILE_SIZE, 0);
            selectedTile->coverageDirty = true;
        }
        mTiles.insert(tileIndex, selectedTile);

        emit this->tileCreated(this, selectedTile);
    } else {
        emit this->tileUpdated(this, selectedTile);
    }

    return selectedTile;
}

void TiledBuffer::drawBrush(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing) {
    StageScope stage(StageProfiler::DabMask);
    endCoverage();
    const float tileSize = UNIFORM_TILE_SIZE;

    // Gather the tiles the dab touches; one extra pixel covers antialiasing and the single-pixel fallback.
    const qreal reach = 0.5 * brushWidth + 1;
    const int xLeft = qFloor(qFloor(point.x() - reach) / tileSize);
    const int xRight = qFloor(qFloor(point.x() + reach) / tileSize);
    const int yTop = qFloor(qFloor(point.y() - reach) / tileSize);
    const int yBottom = qFloor(qFloor(point.y() + reach) / tileSize);

    // If we are not using antialiasing, make sure at least one pixel is within the brush's circle
    bool drawPoint = false;
    if (!antialiasing && brushWidth < 1.42) { // Overestimated approximation of 2*sqrt(2), which is the maximum distance a point can be from the center of a pixel
        // Measure the actual distance to the center of the nearest pixel
        const QPointF nearestPixelCenter(qRound(point.x()+0.5)-0.5, qRound(point.y()+0.5)-0.5);
        const qreal distanceToNearest = QLineF(point, nearestPixelCenter).length();
        if (distanceToNearest >= brushWidth/2) {
            // Nothing will be drawn with drawEllipse, so prepare to draw the nearest pixel with drawPoint
            drawPoint = true;
            point = QPointF(nearestPixelCenter.x() - 0.5, nearestPixelCenter.y() - 0.5);
            pen = QPen(brush, 1);
        }
    }
    const QRectF brushRect(point.x() - 0.5 * brushWidth, point.y() - 0.5 * brushWidth, brushWidth, brushWidth);

    for (int tileY = yTop; tileY <= yBottom; tileY++) {
        for (int tileX = xLeft; tileX <= xRight; tileX++) {

            Tile* tile = getTileFromIndex({tileX, tileY});

            QPainter painter(&tile->image());

            painter.translate(-tile->pos());
            painter.setRenderHint(QPainter::Antialiasing, antialiasing);
            painter.setPen(pen);
            painter.setBrush(brush);
            painter.setCompositionMode(cm);
            if (drawPoint) {
                painter.drawPoint(point);
            } else {
                painter.drawEllipse(brushRect);
            }
            painter.end();

            mTileBounds.extend(tile->bounds());
        }
    }
}

namespace
{
// Profile entries over u = r^2 / R^2, so a mask build needs no sqrt per pixel.
constexpr int kProfileSize = 1024;

// x * a / 255 on each premultiplied channel, rounded like Qt's BYTE_MUL.
inline quint32 byteMul(quint32 x, quint32 a)
{
    quint32 t = (x & 0xff00ff) * a;
    t = ((t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8) & 0xff00ff;
    x = ((x >> 8) & 0xff00ff) * a;
    x = (x + ((x >> 8) & 0xff00ff) + 0x800080) & 0xff00ff00;
    return x | t;
}

// x * a / 255 on one channel, rounded like byteMul.
inline quint32 mul255(quint32 x, quint32 a)
{
    const quint32 t = x * a;
    return (t + (t >> 8) + 0x80) >> 8;
}

constexpr int kGrainSize = 256; // power of two, so wrapping is a bit mask

// Tileable paper grain, 0..255: hashed white noise, blurred over a wrapped 3x3 box and
// stretched back to full contrast. Integer math only, so every platform gets the same texture.
const std::vector<quint8>& grainTexture()
{
    static const std::vector<quint8> texture = [] {
        constexpr int n = kGrainSize;
        std::vector<int> noise(n * n);
        for (int i = 0; i < n * n; ++i)
        {
            quint32 h = quint32(i) * 2654435761u;
            h ^= h >> 15;
            h *= 2246822519u;
            h ^= h >> 13;
            noise[i] = int(h & 255);
        }
        std::vector<quint8> out(n * n);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                int sum = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        sum += noise[((y + dy) & (n - 1)) * n + ((x + dx) & (n - 1))];
                out[y * n + x] = quint8(qBound(0, (sum - 9 * 128) / 3 + 128, 255));
            }
        return out;
    }();
    return texture;
}

// The colour-mix step of smudge: t = 0 gives a, 1 gives b, linear in premultiplied RGBA.
std::array<float, 4> mixSmudgeColor(const std::array<float, 4>& a, const std::array<float, 4>& b, float t)
{
    std::array<float, 4> out;
    for (int i = 0; i < 4; ++i) out[i] = a[i] + (b[i] - a[i]) * t;
    return out;
}
}

void TiledBuffer::blendDab(Tile* tile, const QueuedDab& dab) const
{
    const QRect& rect = dab.rect;
    const QRect area = rect & tile->bounds();
    const QPoint tilePos = tile->pos();
    const quint8* mask = dab.mask->alpha.data();
    if (mCoverageMode)
    {
        // Same alpha as the image path below: byteMul's alpha lane is mul255.
        tile->coverageDirty = true;
        if (dab.washWeight)
        {
            for (int y = area.top(); y <= area.bottom(); ++y)
            {
                quint8* d = tile->coverage().data() + (y - tilePos.y()) * UNIFORM_TILE_SIZE + (area.left() - tilePos.x());
                const quint8* m = mask + (y - rect.top()) * rect.width() + (area.left() - rect.left());
                for (int k = 0, w = area.width(); k < w; ++k)
                {
                    const quint32 c = d[k];
                    d[k] = static_cast<quint8>(c + mul255(qMax(dab.srcAlpha, c) - c, mul255(dab.washWeight, m[k])));
                }
            }
            return;
        }
        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            quint8* d = tile->coverage().data() + (y - tilePos.y()) * UNIFORM_TILE_SIZE + (area.left() - tilePos.x());
            const quint8* m = mask + (y - rect.top()) * rect.width() + (area.left() - rect.left());
            for (int k = 0, w = area.width(); k < w; ++k)
            {
                const quint32 s = mul255(dab.srcAlpha, m[k]);
                d[k] = static_cast<quint8>(s + mul255(d[k], 255 - s));
            }
        }
    }
    else if (dab.washWeight)
    {
        QImage& image = tile->image();
        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            QRgb* d = reinterpret_cast<QRgb*>(image.scanLine(y - tilePos.y())) + (area.left() - tilePos.x());
            const quint8* m = mask + (y - rect.top()) * rect.width() + (area.left() - rect.left());
            for (int k = 0, w = area.width(); k < w; ++k)
            {
                // Each channel moves toward src and stops there, so no pixel goes past the wash alpha.
                const quint32 a = mul255(dab.washWeight, m[k]);
                quint32 out = 0;
                for (int shift = 0; shift < 32; shift += 8)
                {
                    const quint32 dc = (d[k] >> shift) & 0xff;
                    const quint32 sc = (dab.src >> shift) & 0xff;
                    out |= (dc + mul255(qMax(sc, dc) - dc, a)) << shift;
                }
                d[k] = out;
            }
        }
    }
    else
    {
        QImage& image = tile->image();
        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            QRgb* d = reinterpret_cast<QRgb*>(image.scanLine(y - tilePos.y())) + (area.left() - tilePos.x());
            const quint8* m = mask + (y - rect.top()) * rect.width() + (area.left() - rect.left());
            // Branchless: a zero mask gives s = 0, and byteMul(d, 255) == d.
            for (int k = 0, w = area.width(); k < w; ++k)
            {
                const quint32 s = byteMul(dab.src, m[k]);
                d[k] = s + byteMul(d[k], 255 - (s >> 24));
            }
        }
    }
}

void TiledBuffer::flushDabs()
{
    if (mQueuedDabs.empty()) return;
    // Each task owns whole tiles, so no two threads write the same pixels.
    const int count = int(mQueuedTiles.size());
    std::atomic<int> next{0};
    auto work = [&] {
        for (int i = next++; i < count; i = next++)
        {
            Tile* tile = mQueuedTiles[i];
            for (int d : tile->pendingDabs)
                blendDab(tile, mQueuedDabs[d]);
            tile->pendingDabs.clear();
        }
    };
    // shortcut: pool threads expire after 30 s idle, so the first big flush after a pause pays ~3 ms
    // to start them, keep them alive if that hitch shows.
    QThreadPool* pool = QThreadPool::globalInstance();
    int helpers = 0;
    // A batch under ~50 us runs here: waking threads costs more. tryStart: a busy pool leaves
    // the work to this thread instead of making it wait.
    const int maxHelpers = mQueuedNs < 50000 ? 0 : qMin(pool->maxThreadCount(), count) - 1;
    while (helpers < maxHelpers && pool->tryStart([&] { work(); mDabsDone.release(); }))
        ++helpers;
    work();
    mDabsDone.acquire(helpers);
    mLastDabWorkers = helpers + 1;
    mQueuedTiles.clear();
    mQueuedDabs.clear();
    mQueuedNs = 0;
}

QHash<TileIndex, Tile*> TiledBuffer::tiles()
{
    flushDabs();
    if (!mCoverageMode) return mTiles;
    const quint32 color = mCoverageColor | 0xff000000;
    for (Tile* tile : mTiles)
    {
        if (!tile->coverageDirty) continue;
        tile->coverageDirty = false;
        QImage& image = tile->image();
        const quint8* c = tile->coverage().data();
        for (int y = 0; y < UNIFORM_TILE_SIZE; ++y)
        {
            QRgb* d = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < UNIFORM_TILE_SIZE; ++x)
                d[x] = byteMul(color, *c++);
        }
    }
    return mTiles;
}

void TiledBuffer::endCoverage()
{
    flushDabs();
    if (!mCoverageMode) return;
    tiles();
    mCoverageMode = false;
}

Status loadBrushTipImage(const QString& fileName, QImage& mask)
{
    constexpr quint32 kMaxSide = 10000; // GIMP's own brush size limit
    const Status bad(Status::ERROR_LOAD_IMAGE_FAIL, QObject::tr("This file is not a valid brush tip: %1").arg(fileName));
    if (!fileName.endsWith(".gbr", Qt::CaseInsensitive))
    {
        // Check the size from the header before the decoder allocates the image.
        QImageReader reader(fileName);
        const QSize size = reader.size();
        if (!size.isValid() || size.width() > int(kMaxSide) || size.height() > int(kMaxSide)) return bad;
        QImage image = reader.read();
        if (image.isNull() || image.width() > int(kMaxSide) || image.height() > int(kMaxSide)) return bad;
        const bool hasAlpha = image.hasAlphaChannel();
        image = image.convertToFormat(QImage::Format_ARGB32);
        QImage out(image.size(), QImage::Format_Alpha8);
        for (int y = 0; y < image.height(); ++y)
        {
            const QRgb* src = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            uchar* dst = out.scanLine(y);
            for (int x = 0; x < image.width(); ++x)
            {
                const int dark = 255 - qGray(src[x]);
                dst[x] = uchar(hasAlpha ? (qAlpha(src[x]) * dark + 127) / 255 : dark);
            }
        }
        mask = out;
        return Status::OK;
    }

    // GIMP brush: big-endian header size, version, width, height, bytes per pixel, then in version 2
    // "GIMP" and spacing; the name fills the rest of the header, then the pixels follow.
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return Status(Status::ERROR_FILE_CANNOT_OPEN, fileName);
    const QByteArray header = file.read(28);
    if (header.size() < 20) return bad;
    auto field = [&](int i) { return qFromBigEndian<quint32>(header.constData() + 4 * i); };
    const quint32 headerSize = field(0), version = field(1), width = field(2), height = field(3), depth = field(4);
    const quint32 minHeader = (version == 1) ? 20 : 28;
    if ((version != 1 && version != 2) || headerSize < minHeader || quint32(header.size()) < minHeader) return bad;
    if (version == 2 && header.mid(20, 4) != "GIMP") return bad;
    if (width == 0 || height == 0 || width > kMaxSide || height > kMaxSide || (depth != 1 && depth != 4)) return bad;
    const qint64 pixelBytes = qint64(width) * height * depth;
    if (qint64(headerSize) + pixelBytes > file.size() || !file.seek(headerSize)) return bad;
    const QByteArray pixels = file.read(pixelBytes);
    if (pixels.size() != pixelBytes) return bad;

    QImage out(int(width), int(height), QImage::Format_Alpha8);
    const uchar* src = reinterpret_cast<const uchar*>(pixels.constData());
    for (int y = 0; y < out.height(); ++y)
    {
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x, src += depth)
            dst[x] = (depth == 1) ? src[0] : src[3]; // 255 paints; RGBA tips use their alpha
    }
    mask = out;
    return Status::OK;
}

TiledBuffer::DabMask TiledBuffer::grainMask(const DabMask& mask, QPoint origin, const BrushTip& tip)
{
    const std::vector<quint8>& texture = grainTexture();
    constexpr int wrap = kGrainSize - 1;
    // Paper grain samples canvas pixels; moving grain samples pixels relative to the dab.
    const QPoint anchor = (tip.grainMode == 1) ? origin : QPoint();
    const float step = 1.0f / qMax(tip.grainScale, 0.01f);
    // Level = ((t - 0.5) * contrast + 0.5 + brightness), clamped to 0..1, as a factor on alpha.
    const float gain = tip.grainContrast / 255.0f;
    const float offset = 0.5f - 0.5f * tip.grainContrast + tip.grainBrightness;
    DabMask out = mask;
    const QRect& r = mask.rect;
    quint8* a = out.alpha.data();
    for (int y = r.top(); y <= r.bottom(); ++y)
    {
        const float fy = (y + anchor.y() + 0.5f) * step - 0.5f;
        const int py = qFloor(fy);
        const float ty = fy - py;
        const quint8* ra = texture.data() + (py & wrap) * kGrainSize;
        const quint8* rb = texture.data() + ((py + 1) & wrap) * kGrainSize;
        for (int x = r.left(); x <= r.right(); ++x, ++a)
        {
            if (*a == 0) continue;
            // Bilinear between texel centres, wrapped at the texture edge.
            const float fx = (x + anchor.x() + 0.5f) * step - 0.5f;
            const int px = qFloor(fx);
            const float tx = fx - px;
            const int xa = px & wrap, xb = (px + 1) & wrap;
            const float top = ra[xa] + (ra[xb] - ra[xa]) * tx;
            const float bottom = rb[xa] + (rb[xb] - rb[xa]) * tx;
            const float level = qBound(0.0f, (top + (bottom - top) * ty) * gain + offset, 1.0f);
            *a = static_cast<quint8>(*a * level + 0.5f);
        }
    }
    return out;
}

TiledBuffer::DabMask TiledBuffer::imageDabMask(const QImage& image, float width, float aspect, int angle, float cx, float cy)
{
    DabMask mask;
    QImage src = image.convertToFormat(QImage::Format_Alpha8);
    if (width <= 0 || src.isNull()) return mask;
    // Box-halve large images first, so bilinear sampling does not skip pixels.
    // ponytail: halves on every new mask, keep a mip chain per image if big tips build masks slowly.
    while (src.width() >= 2 * width && src.width() > 1 && src.height() > 1)
    {
        QImage half(src.width() / 2, src.height() / 2, QImage::Format_Alpha8);
        for (int y = 0; y < half.height(); ++y)
        {
            const uchar* a = src.constScanLine(2 * y);
            const uchar* b = src.constScanLine(2 * y + 1);
            uchar* dst = half.scanLine(y);
            for (int x = 0; x < half.width(); ++x)
                dst[x] = uchar((a[2 * x] + a[2 * x + 1] + b[2 * x] + b[2 * x + 1] + 2) / 4);
        }
        src = half;
    }

    const int w = src.width();
    const int h = src.height();
    const float sx = w / width;   // image pixels per dab pixel along the tip width
    const float sy = sx / aspect; // and across it
    const float halfW = width / 2;
    const float halfH = halfW * h / w * aspect;
    const float rad = qDegreesToRadians(float(angle));
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    const float reachX = std::abs(cosA) * halfW + std::abs(sinA) * halfH;
    const float reachY = std::abs(sinA) * halfW + std::abs(cosA) * halfH;
    const int x0 = qFloor(cx - reachX);
    const int x1 = qCeil(cx + reachX);
    const int y0 = qFloor(cy - reachY);
    const int y1 = qCeil(cy + reachY);
    mask.rect = QRect(QPoint(x0, y0), QPoint(x1, y1));
    const int mw = mask.rect.width();
    mask.alpha.assign(mw * mask.rect.height(), 0);
    for (int y = y0; y <= y1; ++y)
    {
        const float dy = y + 0.5f - cy;
        quint8* row = mask.alpha.data() + (y - y0) * mw - x0;
        for (int x = x0; x <= x1; ++x)
        {
            // Same tip frame as the generated tips: u along the width, v across it.
            const float dx = x + 0.5f - cx;
            const float ix = (dx * cosA - dy * sinA) * sx + w / 2.0f;
            const float iy = (dx * sinA + dy * cosA) * sy + h / 2.0f;
            if (ix < 0 || iy < 0 || ix >= w || iy >= h) continue;
            // Bilinear between pixel centres, clamped at the image edge.
            const float fx = ix - 0.5f;
            const float fy = iy - 0.5f;
            const int px = qFloor(fx);
            const int py = qFloor(fy);
            const float tx = fx - px;
            const float ty = fy - py;
            const int xa = qMax(px, 0), xb = qMin(px + 1, w - 1);
            const uchar* ra = src.constScanLine(qMax(py, 0));
            const uchar* rb = src.constScanLine(qMin(py + 1, h - 1));
            const float top = ra[xa] + (ra[xb] - ra[xa]) * tx;
            const float bottom = rb[xa] + (rb[xb] - rb[xa]) * tx;
            row[x] = static_cast<quint8>(top + (bottom - top) * ty + 0.5f);
        }
    }
    return mask;
}

const TiledBuffer::DabMask& TiledBuffer::dabMask(qreal diameter, qreal feather, int phaseX, int phaseY, bool antialiasing, const BrushTip& tip)
{
    // Key: diameter in 1/4 px, tip angle in degrees, aspect in %, feather in 1/16 %, sub-pixel phase in 1/4 px.
    const int dq = qMax(0, qRound(diameter * 4));
    const bool image = !tip.image.isNull();
    // Feather does not shape an image mask.
    const int fq = image ? 0 : qBound(0, qRound(feather * 16), 1600);
    const int aq = qBound(1, qRound(tip.aspect * 100), 100);
    const bool square = tip.square && !image;
    const bool circle = !image && !square && aq == 100;
    // The shape repeats every 90 degrees for a square, every 180 degrees for other generated tips.
    const int period = image ? 360 : (square && aq == 100) ? 90 : 180;
    const int angleQ = circle ? 0 : ((qRound(tip.angle) % period) + period) % period;
    const quint64 key = (quint64(image) << 63) | (quint64(dq) << 40) | (quint64(angleQ) << 27) | (quint64(aq) << 20)
                        | (quint64(square) << 19) | (quint64(fq) << 8) | (phaseX << 4) | (phaseY << 1) | (antialiasing ? 1 : 0);
    // Cached image masks belong to one image.
    if (image && tip.image.cacheKey() != mTipImageKey)
    {
        flushDabs();
        mDabMasks.clear();
        mDabMaskBytes = 0;
        mTipImageKey = tip.image.cacheKey();
    }
    auto it = mDabMasks.constFind(key);
    if (it != mDabMasks.constEnd()) return *it;
    flushDabs(); // queued dabs point into the cache

    // shortcut: drops the whole cache past 16 MB, use LRU if pressure strokes thrash it.
    if (mDabMaskBytes > 16 * 1024 * 1024)
    {
        mDabMasks.clear();
        mDabMaskBytes = 0;
    }

    if (image)
    {
        DabMask mask = imageDabMask(tip.image, dq / 4.0f, aq / 100.0f, angleQ, phaseX / 4.0f, phaseY / 4.0f);
        mDabMaskBytes += qsizetype(mask.alpha.size());
        return *mDabMasks.insert(key, std::move(mask));
    }

    const float r = dq / 8.0f;
    const float r2 = r * r;
    const float f = fq / 1600.0f;
    const float cx = phaseX / 4.0f;
    const float cy = phaseY / 4.0f;

    if (mProfileFeather != fq)
    {
        mProfile.resize(kProfileSize + 1);
        for (int i = 0; i <= kProfileSize; ++i)
        {
            const float t = std::sqrt(float(i) / kProfileSize);
            mProfile[i] = (t <= 1 - f) ? 1.0f : (1 - t) / f;
        }
        mProfileFeather = fq;
    }

    // Coverage is 0 at d >= reach and 1 at d <= inner; the profile is 1 at d^2 <= solid2.
    const float reach = antialiasing ? r + 0.5f : r;
    const float reach2 = reach * reach;
    const float inner = antialiasing ? qMax(0.0f, r - 0.5f) : r;
    const float inner2 = inner * inner;
    const float solid2 = (1 - f) * (1 - f) * r2;

    // Tip frame: u along the tip width, v across it, scaled by 1 / aspect so the shape has radius r.
    // shortcut: anti-aliasing runs in the scaled frame, so the edge across a thin tip is sharper.
    const float rad = qDegreesToRadians(float(angleQ));
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    const float aspect = aq / 100.0f;
    const float invAspect = 1.0f / aspect;
    float halfW = reach;
    float halfH = reach;
    if (tip.square)
    {
        halfW = reach * (std::abs(cosA) + aspect * std::abs(sinA));
        halfH = reach * (std::abs(sinA) + aspect * std::abs(cosA));
    }
    else if (!circle)
    {
        halfW = reach * std::sqrt(cosA * cosA + aspect * aspect * sinA * sinA);
        halfH = reach * std::sqrt(sinA * sinA + aspect * aspect * cosA * cosA);
    }

    // Pixel x is inside the box when |x + 0.5 - cx| < halfW.
    const int x0 = qFloor(cx - halfW - 0.5f) + 1;
    const int x1 = qCeil(cx + halfW - 0.5f) - 1;
    const int y0 = qFloor(cy - halfH - 0.5f) + 1;
    const int y1 = qCeil(cy + halfH - 0.5f) - 1;
    DabMask mask;
    if (x1 >= x0 && y1 >= y0)
    {
        mask.rect = QRect(QPoint(x0, y0), QPoint(x1, y1));
        const int w = mask.rect.width();
        mask.alpha.assign(w * mask.rect.height(), 0);
        const float toIndex = r2 > 0 ? kProfileSize / r2 : 0.0f;
        for (int y = y0; y <= y1; ++y)
        {
            const float dy = y + 0.5f - cy;
            const float dy2 = dy * dy;
            const float half = std::sqrt(qMax(0.0f, reach2 - dy2));
            quint8* row = mask.alpha.data() + (y - y0) * w - x0;
            const int rowLo = circle ? qMax(x0, qFloor(cx - half - 0.5f)) : x0;
            const int rowHi = circle ? qMin(x1, qCeil(cx + half - 0.5f)) : x1;
            for (int x = rowLo; x <= rowHi; ++x)
            {
                const float dx = x + 0.5f - cx;
                float d2 = dx * dx + dy2;
                if (!circle)
                {
                    // Screen y points down, so a positive angle turns the tip counter-clockwise on screen.
                    const float u = dx * cosA - dy * sinA;
                    const float v = (dx * sinA + dy * cosA) * invAspect;
                    d2 = tip.square ? qMax(u * u, v * v) : u * u + v * v;
                }
                if (d2 >= reach2) continue;
                float a = 255.0f;
                if (d2 > solid2) a *= mProfile[qMin(int(d2 * toIndex + 0.5f), kProfileSize)];
                if (d2 > inner2) a *= qBound(0.0f, r - std::sqrt(d2) + 0.5f, 1.0f);
                row[x] = static_cast<quint8>(a + 0.5f);
            }
        }
    }
    // A solid aliased dab that misses every pixel centre still marks the pixel under it.
    if (!antialiasing && fq == 0 && std::all_of(mask.alpha.begin(), mask.alpha.end(), [](quint8 a) { return a == 0; }))
    {
        mask.rect = QRect(0, 0, 1, 1);
        mask.alpha.assign(1, 255);
    }
    mDabMaskBytes += qsizetype(mask.alpha.size());
    return *mDabMasks.insert(key, std::move(mask));
}

void TiledBuffer::drawDab(QPointF center, qreal diameter, qreal feather, QColor color, qreal opacity, bool antialiasing,
                          const BrushTip& tip)
{
    StageScope stage(StageProfiler::DabMask);
    feather = qBound(0.0, feather, 100.0);
    // Feather lowers the centre alpha by its percentage.
    const bool wash = mWashOpacity < 1;
    const quint32 washWeight = wash ? quint32(qBound(0, qRound(255 * opacity), 255)) : 0;
    if (wash && washWeight == 0) return;
    const int mainAlpha = qRound(color.alphaF() * 255 * (wash ? mWashOpacity : opacity));
    const int alpha = mainAlpha - qRound(mainAlpha * feather / 100);
    if (alpha <= 0) return;
    const quint32 srcAlpha = qMin(alpha, 255);
    const quint32 src = qPremultiply(qRgba(color.red(), color.green(), color.blue(), srcAlpha));
    const QRgb rgb = qRgb(color.red(), color.green(), color.blue()) & 0xffffff;
    if (mCoverageMode && rgb != mCoverageColor) endCoverage();
    if (!mCoverageMode && mTiles.isEmpty())
    {
        mCoverageMode = true;
        mCoverageColor = rgb;
    }

    const int qx = qRound(center.x() * 4);
    const int qy = qRound(center.y() * 4);
    const QPoint origin(qFloor(qx / 4.0), qFloor(qy / 4.0));
    const DabMask* mask = &dabMask(diameter, feather, qx - origin.x() * 4, qy - origin.y() * 4, antialiasing, tip);
    const QRect rect = mask->rect.translated(origin);
    if (rect.isEmpty()) return;
    if (tip.grainMode != 0)
    {
        // Masks of blended dabs are no longer needed; queued ones stay until the next flush.
        if (mQueuedDabs.empty()) mGrainMasks.clear();
        mGrainMasks.push_back(grainMask(*mask, origin, tip));
        mask = &mGrainMasks.back();
    }

    // Small dabs blend here; larger ones queue for flushDabs. Once one is queued, the rest queue
    // behind it to keep dab order.
    const QueuedDab dab{ mask, rect, src, srcAlpha, washWeight };
    const double pixels = double(rect.width()) * rect.height();
    const double predictedNs = mDabNsPerPixel * pixels;
    const bool inlineDab = mQueuedDabs.empty() && predictedNs < 5000;
    if (!inlineDab) mQueuedNs += predictedNs;
    // Timing costs as much as a tiny dab, so only dabs near the limit update the mean.
    const bool timed = inlineDab && predictedNs > 500;
    QElapsedTimer timer;
    if (timed) timer.start();

    const qreal tileSize = UNIFORM_TILE_SIZE;
    for (int tileY = qFloor(rect.top() / tileSize); tileY <= qFloor(rect.bottom() / tileSize); tileY++) {
        for (int tileX = qFloor(rect.left() / tileSize); tileX <= qFloor(rect.right() / tileSize); tileX++) {
            Tile* tile = getTileFromIndex({tileX, tileY});
            if (inlineDab)
            {
                blendDab(tile, dab);
            }
            else
            {
                if (tile->pendingDabs.empty()) mQueuedTiles.push_back(tile);
                tile->pendingDabs.push_back(int(mQueuedDabs.size()));
            }
            mTileBounds.extend(tile->bounds());
        }
    }
    if (timed) mDabNsPerPixel = 0.9 * mDabNsPerPixel + 0.1 * (timer.nsecsElapsed() / pixels);
    if (!inlineDab) mQueuedDabs.push_back(dab);
}

void TiledBuffer::setSmudgeSource(const QImage& image, QPoint topLeft)
{
    endCoverage();
    mSmudging = true;
    mSmudgeSource = image;
    mSmudgeTopLeft = topLeft;
    mSmudgeBucketEmpty = true;
}

std::array<float, 4> TiledBuffer::sampleSmudge(QPointF center, qreal radius)
{
    const QRect box = QRectF(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius).toAlignedRect();
    const float cx = float(center.x());
    const float cy = float(center.y());
    const float r2 = float(radius * radius);
    quint64 sum[4] = {};
    quint64 count = 0;
    const qreal tileSize = UNIFORM_TILE_SIZE;
    for (int tileY = qFloor(box.top() / tileSize); tileY <= qFloor(box.bottom() / tileSize); tileY++) {
        for (int tileX = qFloor(box.left() / tileSize); tileX <= qFloor(box.right() / tileSize); tileX++) {
            const Tile* tile = mTiles.value({tileX, tileY}, nullptr);
            if (!tile) continue;
            const QRect area = box & tile->bounds();
            for (int y = area.top(); y <= area.bottom(); ++y)
            {
                const QRgb* d = reinterpret_cast<const QRgb*>(tile->image().constScanLine(y - tile->pos().y())) - tile->pos().x();
                const float dy = y + 0.5f - cy;
                for (int x = area.left(); x <= area.right(); ++x)
                {
                    const float dx = x + 0.5f - cx;
                    if (dx * dx + dy * dy > r2) continue;
                    sum[0] += qRed(d[x]);
                    sum[1] += qGreen(d[x]);
                    sum[2] += qBlue(d[x]);
                    sum[3] += qAlpha(d[x]);
                    ++count;
                }
            }
        }
    }
    std::array<float, 4> mean{};
    if (count == 0) return mean;
    for (int i = 0; i < 4; ++i) mean[i] = float(sum[i]) / (255.0f * float(count));
    return mean;
}

void TiledBuffer::compositeSmudge(const DabMask& mask, const QRect& rect, quint32 src, quint32 weight)
{
    // Tile pixels are fetched here: QImage::bits() detaches, which is not safe on worker threads.
    struct Part { QRect area; uchar* bits; qsizetype stride; QPoint pos; };
    std::vector<Part> parts;
    const qreal tileSize = UNIFORM_TILE_SIZE;
    for (int tileY = qFloor(rect.top() / tileSize); tileY <= qFloor(rect.bottom() / tileSize); tileY++) {
        for (int tileX = qFloor(rect.left() / tileSize); tileX <= qFloor(rect.right() / tileSize); tileX++) {
            Tile* tile = mTiles.value({tileX, tileY});
            QImage& image = tile->image();
            parts.push_back({ rect & tile->bounds(), image.bits(), image.bytesPerLine(), tile->pos() });
        }
    }
    auto rows = [&](int top, int bottom) {
        for (const Part& p : parts)
        {
            for (int y = qMax(top, p.area.top()), last = qMin(bottom, p.area.bottom()); y <= last; ++y)
            {
                QRgb* d = reinterpret_cast<QRgb*>(p.bits + (y - p.pos.y()) * p.stride) + (p.area.left() - p.pos.x());
                const quint8* m = mask.alpha.data() + (y - rect.top()) * rect.width() + (p.area.left() - rect.left());
                for (int k = 0, w = p.area.width(); k < w; ++k)
                {
                    const int a = int(mul255(weight, m[k]));
                    if (a == 0) continue;
                    // Rounds up toward src, so light repeated dabs still reach it.
                    int c[4];
                    for (int i = 0; i < 4; ++i)
                    {
                        const int dc = int(d[k] >> (8 * i)) & 0xff;
                        const int delta = (int(src >> (8 * i)) & 0xff) - dc;
                        c[i] = dc + (delta * a + (delta > 0 ? 254 : -254)) / 255;
                    }
                    // Keep premultiplied: no colour channel above alpha.
                    d[k] = (quint32(c[3]) << 24) | (quint32(qMin(c[2], c[3])) << 16) |
                           (quint32(qMin(c[1], c[3])) << 8) | quint32(qMin(c[0], c[3]));
                }
            }
        }
    };
    // Bands of at least 64 rows; each pixel depends only on itself, so bands match the serial result.
    const qint64 pixels = qint64(rect.width()) * rect.height();
    const int bands = pixels >= mParallelSmudgeMinPixels ? qMin(parallelWorkerCount(), rect.height() / 64) : 1;
    mLastSmudgeWorkers = qMax(bands, 1);
    if (bands <= 1)
    {
        rows(rect.top(), rect.bottom());
        return;
    }
    parallelFor(bands, [&](int band) {
        rows(rect.top() + rect.height() * band / bands, rect.top() + rect.height() * (band + 1) / bands - 1);
    });
}

void TiledBuffer::smudgeDab(QPointF center, qreal diameter, qreal feather, QColor color, qreal opacity, bool antialiasing,
                            const BrushTip& tip, const SmudgeSettings& smudge)
{
    StageScope stage(StageProfiler::DabMask);
    endCoverage();
    feather = qBound(0.0, feather, 100.0);
    // Same centre alpha as drawDab.
    const int mainAlpha = qRound(255 * qBound(0.0, opacity, 1.0));
    const int weight = mainAlpha - qRound(mainAlpha * feather / 100);
    if (weight <= 0) return;

    const int qx = qRound(center.x() * 4);
    const int qy = qRound(center.y() * 4);
    const QPoint origin(qFloor(qx / 4.0), qFloor(qy / 4.0));
    const DabMask* mask = &dabMask(diameter, feather, qx - origin.x() * 4, qy - origin.y() * 4, antialiasing, tip);
    DabMask grained;
    if (tip.grainMode != 0)
    {
        grained = grainMask(*mask, origin, tip);
        mask = &grained;
    }
    const QRect rect = mask->rect.translated(origin);
    if (rect.isEmpty()) return;

    // Every point is within 0.71 px of a pixel centre, so the sample is never empty.
    const qreal radius = qMax(0.75, 0.5 * diameter * smudge.radius);
    const QRect area = rect | QRectF(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius).toAlignedRect();
    const qreal tileSize = UNIFORM_TILE_SIZE;
    for (int tileY = qFloor(area.top() / tileSize); tileY <= qFloor(area.bottom() / tileSize); tileY++) {
        for (int tileX = qFloor(area.left() / tileSize); tileX <= qFloor(area.right() / tileSize); tileX++) {
            mTileBounds.extend(getTileFromIndex({tileX, tileY})->bounds());
        }
    }

    // Pick up what lies under the dab, then paint what the bucket holds.
    const std::array<float, 4> sample = sampleSmudge(center, radius);
    mSmudgeBucket = mSmudgeBucketEmpty ? sample : mixSmudgeColor(sample, mSmudgeBucket, smudge.length);
    mSmudgeBucketEmpty = false;
    const float alpha = float(color.alphaF());
    const std::array<float, 4> paint = mixSmudgeColor(
        mSmudgeBucket, { float(color.redF()) * alpha, float(color.greenF()) * alpha, float(color.blueF()) * alpha, alpha },
        smudge.colorRate);
    auto to8 = [](float v) { return quint32(qBound(0, qRound(v * 255), 255)); };
    const quint32 a = to8(paint[3]);
    const quint32 src = (a << 24) | (qMin(to8(paint[0]), a) << 16) | (qMin(to8(paint[1]), a) << 8) | qMin(to8(paint[2]), a);
    compositeSmudge(*mask, rect, src, quint32(weight));
}

void TiledBuffer::drawImage(const QImage& image, const QRect& imageBounds, QPainter::CompositionMode cm, bool antialiasing) {
    StageScope stage(StageProfiler::DabMask);
    endCoverage();
    const float tileSize = UNIFORM_TILE_SIZE;
    const float imageXRad = image.width();
    const float imageYRad = image.height();
    // Gather the number of tiles that fits the size of the brush width
    const int xLeft = qFloor((qFloor(imageBounds.left() - imageXRad)) / tileSize);
    const int xRight = qFloor((qFloor(imageBounds.right() + imageXRad)) / tileSize);
    const int yTop = qFloor(qFloor(imageBounds.top() - imageYRad) / tileSize);
    const int yBottom = qFloor(qFloor(imageBounds.bottom() + imageYRad) / tileSize);

    for (int tileY = yTop; tileY <= yBottom; tileY++) {
        for (int tileX = xLeft; tileX <= xRight; tileX++) {

            Tile* tile = getTileFromIndex({tileX, tileY});

            QPainter painter(&tile->image());

            painter.translate(-tile->pos());
            painter.setRenderHint(QPainter::Antialiasing, antialiasing);
            painter.setCompositionMode(cm);
            painter.drawImage(imageBounds.topLeft(), image);
            painter.end();

            mTileBounds.extend(tile->bounds());
        }
    }
}


void TiledBuffer::drawPath(QPainterPath path, QPen pen, QBrush brush,
                           QPainter::CompositionMode cm, bool antialiasing)
{
    StageScope stage(StageProfiler::DabMask);
    endCoverage();
    const qreal width = pen.widthF();
    const float tileSize = UNIFORM_TILE_SIZE;
    const QRectF pathRect = path.boundingRect();

    // Gather the number of tiles that fits the size of the brush width
    const int xLeft = qFloor((qFloor(pathRect.left() - width)) / tileSize);
    const int xRight = qFloor((qFloor(pathRect.right() + width)) / tileSize);
    const int yTop = qFloor(qFloor(pathRect.top() - width) / tileSize);
    const int yBottom = qFloor(qFloor(pathRect.bottom() + width) / tileSize);

    for (int tileY = yTop; tileY <= yBottom; tileY++) {
        for (int tileX = xLeft; tileX <= xRight; tileX++) {

            Tile* tile = getTileFromIndex({tileX, tileY});

            QPainter painter(&tile->image());

            painter.translate(-tile->pos());
            painter.setRenderHint(QPainter::Antialiasing, antialiasing);
            painter.setPen(pen);
            painter.setBrush(brush);
            painter.setCompositionMode(cm);
            painter.drawPath(path);
            painter.end();

            mTileBounds.extend(tile->bounds());
        }
    }
}

void TiledBuffer::clear()
{
    for (Tile* tile : mQueuedTiles) tile->pendingDabs.clear();
    mQueuedTiles.clear();
    mQueuedDabs.clear();
    mQueuedNs = 0;

    QHashIterator<TileIndex, Tile*> i(mTiles);

    while (i.hasNext()) {
        i.next();
        Tile* tile = i.value();
        if (tile)
        {
            mTiles.remove(i.key());
            const qsizetype bytes = tileBytes(tile);
            if (mPoolBytes + bytes <= mPoolCap)
            {
                mPool.push_back(tile);
                mPoolBytes += bytes;
            }
            else
            {
                delete tile;
            }
        }
    }

    mTileBounds = BlitRect();
    mCoverageMode = false;
    mSmudging = false;
    mSmudgeSource = QImage();
    mSmudgeBucketEmpty = true;
}

QPoint TiledBuffer::getTilePos(const TileIndex& index) const
{
    return QPoint { qRound(UNIFORM_TILE_SIZE*static_cast<qreal>(index.x)),
                    qRound(UNIFORM_TILE_SIZE*static_cast<qreal>(index.y)) };
}
