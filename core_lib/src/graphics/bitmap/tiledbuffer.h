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
#ifndef TILEDBUFFER_H
#define TILEDBUFFER_H

#include <QObject>
#include <QPainter>
#include <QHash>
#include <QSemaphore>
#include <vector>

#include "blitrect.h"

class QImage;
class QRect;
class Tile;

struct TileIndex {
    int x;
    int y;
};

inline size_t qHash(const TileIndex &key, size_t seed)
{
    return qHash(key.x, seed) ^ key.y;
}

inline bool operator==(const TileIndex &e1, const TileIndex &e2)
{
    return e1.x == e2.x
           && e1.y == e2.y;
}

class TiledBuffer: public QObject
{
    Q_OBJECT
public:
    TiledBuffer(QObject* parent = nullptr);
    ~TiledBuffer();

    /** Clears the content of the tiled buffer */
    void clear();

    /** Returns true if there are any tiles, otherwise false */
    bool isValid() const { return !mTiles.isEmpty(); }

    /** Draws a brush with the specified parameters to the tiled buffer */
    void drawBrush(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing);
    /** Draws a round dab with SourceOver: full alpha inside 1 - feather/100 of the radius, then a linear fade.
     *  feather is 0..100 and also lowers the centre alpha, like ScribbleArea::setGaussianGradient. */
    void drawDab(QPointF center, qreal diameter, qreal feather, QColor color, qreal opacity, bool antialiasing);
    /** Draws a path with the specified parameters to the tiled buffer */
    void drawPath(QPainterPath path, QPen pen, QBrush brush,
                  QPainter::CompositionMode cm, bool antialiasing);
    /** Draws a image with the specified parameters to the tiled buffer */
    void drawImage(const QImage& image, const QRect& imageBounds, QPainter::CompositionMode cm, bool antialiasing);

    /** The tiles, colourized from the stroke coverage first. */
    QHash<TileIndex, Tile*> tiles() const;

    const QRect& bounds() const { return mTileBounds; }

    /** Bytes held by cleared tiles that wait for reuse. */
    qsizetype poolBytes() const { return mPoolBytes; }
    /** Sets the most bytes the pool keeps, and frees pooled tiles above it. */
    void setPoolCapBytes(qsizetype cap);

    /** Threads that rendered the last batch of queued dabs; 0 if none was queued. */
    int lastDabWorkers() const { return mLastDabWorkers; }

signals:
    void tileUpdated(TiledBuffer* tiledBuffer, Tile* tile);
    void tileCreated(TiledBuffer* tiledBuffer, Tile* tile);

private:

    Tile* getTileFromIndex(const TileIndex& tileIndex);

    inline QPoint getTilePos(const TileIndex& index) const;

    const int UNIFORM_TILE_SIZE = 64;

    BlitRect mTileBounds;

    QHash<TileIndex, Tile*> mTiles;

    // Dabs of one colour on an empty buffer add to Tile::coverage(); tiles() colourizes it once.
    bool mCoverageMode = false;
    QRgb mCoverageColor = 0;
    void endCoverage();

    // Tiles are one size, so the pool is a plain free list.
    std::vector<Tile*> mPool;
    qsizetype mPoolBytes = 0;
    qsizetype mPoolCap = 128 * 1024 * 1024;
    static qsizetype tileBytes(Tile* tile);

    /** Dab alpha (0..255) on the pixels in rect, relative to the dab's integer origin. */
    struct DabMask
    {
        QRect rect;
        std::vector<quint8> alpha;
    };
    const DabMask& dabMask(qreal diameter, qreal feather, int phaseX, int phaseY, bool antialiasing);

    QHash<quint64, DabMask> mDabMasks;
    qsizetype mDabMaskBytes = 0;
    std::vector<float> mProfile; // alpha over r^2 / R^2 for mProfileFeather
    int mProfileFeather = -1;

    struct QueuedDab
    {
        const DabMask* mask; // valid until the next cache insert, which flushes first
        QRect rect;
        quint32 src;
        quint32 srcAlpha;
    };
    void blendDab(Tile* tile, const QueuedDab& dab) const;
    /** Renders queued dabs on the thread pool, one tile per task, each tile in dab order. */
    void flushDabs() const;
    mutable std::vector<QueuedDab> mQueuedDabs;
    mutable std::vector<Tile*> mQueuedTiles;
    mutable double mQueuedNs = 0; // predicted inline time of the queued dabs
    // A member, not a local: a helper may still be inside release() when acquire() returns.
    mutable QSemaphore mDabsDone;
    mutable int mLastDabWorkers = 0;
    double mDabNsPerPixel = 1.0; // rolling mean of inline dab time
};

#endif // TILEDBUFFER_H
