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
#ifndef FRAMERENDERER_H
#define FRAMERENDERER_H

#include <functional>
#include <QImage>
#include <QPair>
#include <QSize>
#include <QVector>

class Layer;
class LayerCamera;
class Object;
class QPainter;
class QTransform;
struct TweenState;

namespace FrameRenderer
{
// The layer rules, written once for the screen adapter (CanvasPainter) and the export adapter below.
// Positions are indexes into a paint order, not layer indexes.

/// Layer indexes bottom to top. With rough, Rough layers come first, under the others (the canvas shows
/// them); without, they are left out (export).
QVector<int> paintOrder(const Object& object, bool rough);

/// Paints the visible layer at index onto the painter.
using LayerPainter = std::function<void(QPainter& painter, int index)>;

/// Visibility and groups over positions first..last of order: hidden layers are skipped, a translucent group's
/// non-Rough layers fade as one image, a Rough layer in it fades on its own.
void paintLayers(const Object& object, const QVector<int>& order, int first, int last, QPainter& painter,
                 const LayerPainter& paintLayer);

/// True when a layer after pos in order blends with a mode other than Normal: it needs every layer below
/// it as backdrop, so the layers above pos cannot be cached apart from the ones below.
bool blendsAbove(const Object& object, const QVector<int>& order, int pos);

/// The positions that fade as one image with pos: its translucent group's non-Rough layers, else pos alone.
QPair<int, int> groupRange(const Object& object, const QVector<int>& order, int pos);

/// Everything the layer rules read for positions first..last on frame: equal keys paint equal pixels.
QVector<qint64> layersKey(const Object& object, int frame, const QVector<int>& order, int first, int last);

/// Sets layer's tween on frame on the painter: its opacity less fade, its transform before the painter's.
TweenState applyTween(QPainter& painter, const Layer* layer, int frame, qreal fade = 0);

struct Options
{
    bool antialiasing = true;
    bool background = false; // white under the layers
};

/// Paints every exported layer of frame through the painter's current transform.
void paint(const Object& object, int frame, QPainter& painter, const Options& options);

/// Export adapter: output size, transparency and antialias.
struct ExportOptions
{
    QSize size;
    bool transparency = false;
    bool antialiasing = true;
};

/// Renders the frame as the camera sees it: view and cameraSize map to options.size.
QImage renderForExport(const Object& object, int frame, const QTransform& view, QSize cameraSize, const ExportOptions& options);
QImage renderForExport(const Object& object, int frame, const LayerCamera& camera, const ExportOptions& options);
}

#endif // FRAMERENDERER_H
