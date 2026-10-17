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

class QPainter;
class QRectF;
struct PaperTemplate;

/// Draws the sheet and its Title block around cameraFrame, in the painter's coordinates. Draws nothing when off.
void paintPaperTemplate(QPainter& painter, const QRectF& cameraFrame, const PaperTemplate& paper);

#endif // PAPERPAINTER_H
