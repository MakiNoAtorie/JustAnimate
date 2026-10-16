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
#ifndef BLENDMODE_H
#define BLENDMODE_H

#include <functional>
#include <QtGlobal>

class QImage;
class QPainter;
class QPoint;

/// How a layer composites over the layers below it. The values are saved in the file (`<layer blend="N">`).
enum class BlendMode
{
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    Add = 4,
};

/** Blends `count` premultiplied ARGB32 source pixels onto `dst` in place.
 *  Integer maths with exact rounding of x / 255: a SIMD path must match it byte for byte. */
void blendPixels(BlendMode mode, quint32* dst, const quint32* src, int count);

/** Blends `layer` (premultiplied, in device pixels) onto the painter's device at device position `pos`,
 *  with the painter's opacity. The device must be a QImage or a QPixmap. */
void drawBlended(QPainter& painter, const QImage& layer, const QPoint& pos, BlendMode mode);

/** Paints a layer with `paint` into a device-size buffer with the painter's transform, then blends it
 *  onto the painter's device with drawBlended. */
void drawBlendedLayer(QPainter& painter, BlendMode mode, const std::function<void(QPainter&)>& paint);

#endif // BLENDMODE_H
