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
#include <QPainterPath>
#include <QThreadPool>
#include <QtMath>
#include <algorithm>
#include <atomic>
#include <cmath>

#include "tile.h"
#include "stageprofiler.h"

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

const TiledBuffer::DabMask& TiledBuffer::dabMask(qreal diameter, qreal feather, int phaseX, int phaseY, bool antialiasing)
{
    // Key: diameter in 1/4 px, feather in 1/16 %, sub-pixel phase in 1/4 px.
    const int dq = qMax(0, qRound(diameter * 4));
    const int fq = qBound(0, qRound(feather * 16), 1600);
    const quint64 key = (quint64(dq) << 32) | (quint64(fq) << 8) | (phaseX << 4) | (phaseY << 1) | (antialiasing ? 1 : 0);
    auto it = mDabMasks.constFind(key);
    if (it != mDabMasks.constEnd()) return *it;
    flushDabs(); // queued dabs point into the cache

    // shortcut: drops the whole cache past 16 MB, use LRU if pressure strokes thrash it.
    if (mDabMaskBytes > 16 * 1024 * 1024)
    {
        mDabMasks.clear();
        mDabMaskBytes = 0;
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

    // Pixel x is inside when |x + 0.5 - cx| < reach.
    const int x0 = qFloor(cx - reach - 0.5f) + 1;
    const int x1 = qCeil(cx + reach - 0.5f) - 1;
    const int y0 = qFloor(cy - reach - 0.5f) + 1;
    const int y1 = qCeil(cy + reach - 0.5f) - 1;
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
            const int rowLo = qMax(x0, qFloor(cx - half - 0.5f));
            const int rowHi = qMin(x1, qCeil(cx + half - 0.5f));
            for (int x = rowLo; x <= rowHi; ++x)
            {
                const float dx = x + 0.5f - cx;
                const float d2 = dx * dx + dy2;
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

void TiledBuffer::drawDab(QPointF center, qreal diameter, qreal feather, QColor color, qreal opacity, bool antialiasing)
{
    StageScope stage(StageProfiler::DabMask);
    feather = qBound(0.0, feather, 100.0);
    // Same centre alpha as ScribbleArea::setGaussianGradient.
    const int mainAlpha = qRound(color.alphaF() * 255 * opacity);
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
    const DabMask& mask = dabMask(diameter, feather, qx - origin.x() * 4, qy - origin.y() * 4, antialiasing);
    const QRect rect = mask.rect.translated(origin);
    if (rect.isEmpty()) return;

    // Small dabs blend here; larger ones queue for flushDabs. Once one is queued, the rest queue
    // behind it to keep dab order.
    const QueuedDab dab{ &mask, rect, src, srcAlpha };
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
}

QPoint TiledBuffer::getTilePos(const TileIndex& index) const
{
    return QPoint { qRound(UNIFORM_TILE_SIZE*static_cast<qreal>(index.x)),
                    qRound(UNIFORM_TILE_SIZE*static_cast<qreal>(index.y)) };
}
