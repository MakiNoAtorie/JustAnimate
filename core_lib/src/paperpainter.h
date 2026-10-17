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
#ifndef PAPERPAINTER_H
#define PAPERPAINTER_H

#include <QRectF>
#include "pagewriter.h"

class QPainter;
class Object;
class LayerCamera;
struct PaperTemplate;

/// Draws the sheet and its Title block around cameraFrame, in the painter's coordinates. Draws nothing when off.
void paintPaperTemplate(QPainter& painter, const QRectF& cameraFrame, const PaperTemplate& paper);

/// Where the camera frame goes so its whole sheet fits the page, centred, at the camera's aspect.
QRectF paperFrameOnPage(const QRectF& page, const QSizeF& cameraSize);

/// Export on paper: page i is frame firstFrame + i on the project's Paper template, drawn even when the template is off.
/// object and camera must outlive the function.
PagePaintFn paperFramePages(const Object& object, const LayerCamera& camera, int firstFrame);

#endif // PAPERPAINTER_H
