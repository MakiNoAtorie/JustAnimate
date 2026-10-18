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
#ifndef STROKEBUFFER_H
#define STROKEBUFFER_H

#include <QPainter>
#include <QRect>

#include "tiledbuffer.h"

class BezierCurve;
class Layer;
class VectorImage;

/// The pixels of one stroke in canvas space, owned by a Stroke tool. The canvas draws them with
/// compositionMode(); commit() writes them, or a vector curve, into the Drawing the stroke began on.
class StrokeBuffer
{
public:
    enum class Mode { Paint, Erase, Smudge };

    /// Clears the buffer and starts a stroke on layer at frame.
    void begin(Layer* layer, int frame, Mode mode);
    /// One dab in canvas space; a Smudge stroke moves the layer pixels under it by smudge.
    void dab(QPointF center, qreal width, qreal feather, QColor color, qreal opacity, bool antialiasing,
             const BrushTip& tip = {}, const SmudgeSettings& smudge = {});
    /// How the buffer lands on its layer, on screen and on commit.
    QPainter::CompositionMode compositionMode() const;
    /// Writes the buffer into the bitmap Drawing at the stroke's frame and clears it.
    /// Returns the canvas rect it changed; empty when nothing was written.
    QRect commit();
    /// Adds curve, given in canvas space, to the vector Drawing at the stroke's frame and clears the buffer.
    /// interacts joins the curve's ends to the curves near them. Returns the Drawing; nullptr when there is none.
    VectorImage* commit(BezierCurve curve, qreal scaling, bool interacts);
    /// Drops the stroke without writing it.
    void clear();

    /// Paths and previews go straight to the tiles; read tiles only through TiledBuffer::tiles().
    TiledBuffer& tiles() { return mTiles; }

private:
    TiledBuffer mTiles;
    Layer* mLayer = nullptr;
    int mFrame = 1;
    Mode mMode = Mode::Paint;
};

#endif // STROKEBUFFER_H
