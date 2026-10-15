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

#ifndef TILE_H
#define TILE_H

#include <QPoint>
#include <QImage>
#include <vector>

class Tile
{
public:

    explicit Tile (const QPoint& pos, QSize size);
    ~Tile();

    const QImage& image() const { return mTileImage; }
    QImage& image() { return mTileImage; }

    const QPoint& pos() const { return mPos; }
    const QPointF& posF() const { return mPosF; }
    const QRect& bounds() const { return mBounds; }
    const QSize& size() const { return mSize; }

    /** Loads the input image into the tile */
    void load(const QImage& image, const QPoint& topLeft);
    void clear();
    /** Moves the tile to pos; keeps the pixels. */
    void moveTo(const QPoint& pos);

    /** Stroke alpha (0..255), one byte per pixel, when TiledBuffer accumulates coverage. */
    std::vector<quint8>& coverage() { return mCoverage; }
    /** True when coverage changed after the image was last colourized. */
    bool coverageDirty = false;

private:
    std::vector<quint8> mCoverage;
    QImage mTileImage;
    QPointF mPosF;
    QPoint mPos;
    QRect mBounds;
    QSize mSize;
};

#endif // TILE_H
