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

/// Number of blend modes. A file value outside [0, kBlendModeCount) loads as Normal.
constexpr int kBlendModeCount = 5;
static_assert(static_cast<int>(BlendMode::Add) + 1 == kBlendModeCount, "kBlendModeCount follows the last mode");

/// Menu names by BlendMode value. Translate with QCoreApplication::translate("BlendMode", name).
constexpr const char* kBlendModeNames[kBlendModeCount] = { QT_TRANSLATE_NOOP("BlendMode", "Normal"),
                                                           QT_TRANSLATE_NOOP("BlendMode", "Multiply"),
                                                           QT_TRANSLATE_NOOP("BlendMode", "Screen"),
                                                           QT_TRANSLATE_NOOP("BlendMode", "Overlay"),
                                                           QT_TRANSLATE_NOOP("BlendMode", "Add") };

/** Blends `count` premultiplied ARGB32 source pixels onto `dst` in place.
 *  Uses SSE2 on x86-64 and gives the same bytes as blendPixelsScalar. */
void blendPixels(BlendMode mode, quint32* dst, const quint32* src, int count);

/// Reference for blendPixels: integer maths with exact rounding of x / 255.
void blendPixelsScalar(BlendMode mode, quint32* dst, const quint32* src, int count);

/** Blends `layer` (premultiplied, in device pixels) onto the painter's device at device position `pos`,
 *  with the painter's opacity. The device must be a QImage or a QPixmap. An unclipped premultiplied
 *  QImage is blended in place; other devices go through a copy. */
void drawBlended(QPainter& painter, const QImage& layer, const QPoint& pos, BlendMode mode);

/** Paints a layer with `paint` into a device-size buffer with the painter's transform, then blends it
 *  onto the painter's device with drawBlended. */
void drawBlendedLayer(QPainter& painter, BlendMode mode, const std::function<void(QPainter&)>& paint);

/** Paints with `paint` onto a copy of the painter's device, with the painter's transform, then draws the copy back
 *  as backdrop + opacity * (copy - backdrop). Group opacity: overlapping members do not blend twice, and member
 *  blend modes still blend with the layers below the group. */
void drawWithOpacity(QPainter& painter, qreal opacity, const std::function<void(QPainter&)>& paint);

#endif // BLENDMODE_H
