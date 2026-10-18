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
#include <QImage>
#include <QSemaphore>
#include <array>
#include <deque>
#include <vector>

#include "blitrect.h"
#include "pencilerror.h"

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

/** The shape of a dab. The default is the round tip. */
struct BrushTip
{
    bool square = false;
    qreal aspect = 1.0; // height / width, 0.01..1
    qreal angle = 0.0;  // degrees, counter-clockwise on screen
    QImage image;       // Format_Alpha8 coverage; when set, it replaces the generated shape and square
    // Grain multiplies a paper texture into each dab after the mask cache.
    int grainTexture = 0;       // 0 generated, 1.. bundled (TiledBuffer::grainTextureCount)
    int grainMode = 0;         // 0 off, 1 fixed to the canvas (paper), 2 moves with each dab
    float grainScale = 1.0f;    // texture size factor
    float grainContrast = 1.0f; // 0 flattens the grain to its mid level
    float grainBrightness = 0;  // -1..1, added to the grain level
};

/** Smudge dab settings, each 0..1. */
struct SmudgeSettings
{
    float length = 0.5f; // part of the colour bucket kept at each dab
    float radius = 0.5f; // sample radius as a part of the dab radius
    float colorRate = 0; // part of the paint colour in each dab
    bool wet = false;    // mix colours as pigments (Kubelka-Munk), not as light
};

/** Loads a brush tip mask from a GIMP .gbr file or an image file such as PNG.
 *  An image with alpha paints its opaque dark parts; one without alpha paints its dark parts. */
Status loadBrushTipImage(const QString& fileName, QImage& mask);

class TiledBuffer: public QObject
{
    Q_OBJECT
public:
    TiledBuffer(QObject* parent = nullptr);
    ~TiledBuffer();

    /** Grain textures: 0 is the generated one, 1.. are the bundled paper images (core_lib/data/textures). */
    static constexpr int grainTextureCount = 6;
    /** Untranslated name, context "GrainTexture". */
    static const char* grainTextureName(int index);
    /** False when a bundled image is missing or malformed, and the generated texture stands in. */
    static bool grainTextureLoaded(int index);

    /** Clears the content of the tiled buffer */
    void clear();

    /** Returns true if there are any tiles, otherwise false */
    bool isValid() const { return !mTiles.isEmpty(); }

    /** Draws a brush with the specified parameters to the tiled buffer */
    void drawBrush(QPointF point, qreal brushWidth, QPen pen, QBrush brush, QPainter::CompositionMode cm, bool antialiasing);
    /** Draws a dab with SourceOver: full alpha inside 1 - feather/100 of the radius, then a linear fade.
     *  feather is 0..100 and also lowers the centre alpha by that percentage.
     *  diameter is the tip width; tip sets the shape. */
    void drawDab(QPointF center, qreal diameter, qreal feather, QColor color, qreal opacity, bool antialiasing,
                 const BrushTip& tip = {});
    /** Below 1, drawDab moves pixels toward this alpha by its opacity and never past it (wash). */
    void setWashOpacity(qreal opacity) { mWashOpacity = opacity; }
    /** Smudge stroke: new tiles start as a copy of image at topLeft, so the tiles replace the layer
     *  pixels (CompositionMode_Source). clear() ends it and empties the colour bucket. */
    void setSmudgeSource(const QImage& image, QPoint topLeft);
    bool isSmudging() const { return mSmudging; }
    /** One smudge dab: mixes the pixels under the dab into the colour bucket, then paints the
     *  bucket mixed with color by colorRate. Call setSmudgeSource first. */
    void smudgeDab(QPointF center, qreal diameter, qreal feather, QColor color, qreal opacity, bool antialiasing,
                   const BrushTip& tip, const SmudgeSettings& smudge);
    /** Draws a path with the specified parameters to the tiled buffer */
    void drawPath(QPainterPath path, QPen pen, QBrush brush,
                  QPainter::CompositionMode cm, bool antialiasing);
    /** Draws a image with the specified parameters to the tiled buffer */
    void drawImage(const QImage& image, const QRect& imageBounds, QPainter::CompositionMode cm, bool antialiasing);

    /** The tiles, colourized from the stroke coverage first. */
    QHash<TileIndex, Tile*> tiles();

    const QRect& bounds() const { return mTileBounds; }

    /** Bytes held by cleared tiles that wait for reuse. */
    qsizetype poolBytes() const { return mPoolBytes; }
    /** Sets the most bytes the pool keeps, and frees pooled tiles above it. */
    void setPoolCapBytes(qsizetype cap);
    /** Bytes held by cached dab masks. */
    qsizetype dabMaskBytes() const { return mDabMaskBytes; }

    /** Threads that rendered the last batch of queued dabs; 0 if none was queued. */
    int lastDabWorkers() const { return mLastDabWorkers; }
    /** Threads that composited the last smudge dab. */
    int lastSmudgeWorkers() const { return mLastSmudgeWorkers; }
    /** Smudge dabs with at least this many pixels in their box composite in row bands on all cores. */
    void setParallelSmudgeMinPixels(int pixels) { mParallelSmudgeMinPixels = pixels; }

    /** Emits updated once for the tiles touched since the last call; drawPath and drawImage call it themselves. */
    void emitUpdated();

signals:
    /** The tiles in canvasRect changed. */
    void updated(const QRect& canvasRect);

private:

    Tile* getTileFromIndex(const TileIndex& tileIndex);

    inline QPoint getTilePos(const TileIndex& index) const;

    const int UNIFORM_TILE_SIZE = 64;

    BlitRect mTileBounds;
    QRect mDirty; // tiles touched since the last emitUpdated

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
    const DabMask& dabMask(qreal diameter, qreal feather, int phaseX, int phaseY, bool antialiasing, const BrushTip& tip);
    /** The mask times the grain; origin is the dab's integer canvas origin. */
    static DabMask grainMask(const DabMask& mask, QPoint origin, const BrushTip& tip);
    static DabMask imageDabMask(const QImage& image, float width, float aspect, int angle, float cx, float cy);

    QHash<quint64, DabMask> mDabMasks;
    qsizetype mDabMaskBytes = 0;
    qint64 mTipImageKey = 0; // QImage::cacheKey of the image tip the cached masks were made from
    std::vector<float> mProfile; // alpha over r^2 / R^2 for mProfileFeather
    int mProfileFeather = -1;

    struct QueuedDab
    {
        const DabMask* mask; // valid until the next cache insert, which flushes first
        QRect rect;
        quint32 src;
        quint32 srcAlpha;
        quint32 washWeight; // 0: SourceOver; else moves pixels toward src by this / 255
    };
    void blendDab(Tile* tile, const QueuedDab& dab) const;
    /** Renders queued dabs on the thread pool, one tile per task, each tile in dab order. */
    void flushDabs();
    std::vector<QueuedDab> mQueuedDabs;
    std::deque<DabMask> mGrainMasks; // per-dab masks that queued dabs point into
    std::vector<Tile*> mQueuedTiles;
    double mQueuedNs = 0; // predicted inline time of the queued dabs
    // A member, not a local: a helper may still be inside release() when acquire() returns.
    QSemaphore mDabsDone;
    int mLastDabWorkers = 0;
    double mDabNsPerPixel = 1.0; // rolling mean of inline dab time
    qreal mWashOpacity = 1.0;

    /** Moves the pixels under mask toward src by weight / 255 times the mask, alpha too. */
    void compositeSmudge(const DabMask& mask, const QRect& rect, quint32 src, quint32 weight);
    /** Mean premultiplied colour in a circle, 0..1 per channel; tiles must cover the circle. */
    std::array<float, 4> sampleSmudge(QPointF center, qreal radius);
    bool mSmudging = false;
    QImage mSmudgeSource;
    QPoint mSmudgeTopLeft;
    std::array<float, 4> mSmudgeBucket{}; // premultiplied RGBA, 0..1
    bool mSmudgeBucketEmpty = true;
    int mLastSmudgeWorkers = 0;
    int mParallelSmudgeMinPixels = 256 * 1024; // smaller dabs lose to thread start-up (JustPaint, ROADMAP B7)
};

#endif // TILEDBUFFER_H
