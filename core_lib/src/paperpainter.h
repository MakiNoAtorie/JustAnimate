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

#include <QList>
#include <QPair>
#include <QRectF>
#include "pagewriter.h"

class QPainter;
class Layer;
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

/// Storyboard export: pages of five panels, so ceil(panels / 5) pages.
int storyboardPageCount(int panels);
/// The cell of a panel row (0-4) and column (0 S, 1 C, 2 PICTURE, 3 NOTES, 4 AUDIO, 5 TIME) on a storyboard page.
QRectF storyboardCell(const QRectF& page, int row, int column);
/// The picture frame in a PICTURE cell, at the camera's aspect.
QRectF storyboardPicture(const QRectF& cell, const QSizeF& cameraSize);
/// Page i shows panels 5i to 5i + 4; a panel is its first and last frame on `board`.
/// TIME is its length at fps; PRODUCTION is the Paper template TITLE. object, board and camera must outlive the function.
PagePaintFn storyboardPages(const Object& object, const Layer& board, const LayerCamera& camera,
                            const QList<QPair<int, int>>& panels, int fps);

#endif // PAPERPAINTER_H
