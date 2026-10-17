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

#include <QImage>
#include <QSize>

class LayerCamera;
class Object;
class QPainter;
class QTransform;

namespace FrameRenderer
{
struct Options
{
    bool antialiasing = true;
    bool background = false; // white under the layers
};

/// The layer rules, written once: visibility, Rough, tween, blend modes, groups and group opacity.
/// Paints every layer of frame through the painter's current transform.
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
