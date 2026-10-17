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
#include <QString>
#include "pagewriter.h"

class QPainter;
class Layer;
class Object;
class LayerCamera;
struct PaperTemplate;

/// The white sheet around cameraFrame: the frame, its margins and the Title block above it.
QRectF paperSheetRect(const QRectF& cameraFrame);

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

/// Timesheet export: the header fields the Title block does not hold, the scene length and its fps.
struct TimesheetInfo
{
    QString scene, shot, animator;
    int length = 1;
    int fps = 24;
};
/// Export dialog values: the saved sheet* value, else Animator from NAME and Scene-Shot from an ID of the form "scene-shot".
TimesheetInfo timesheetPrefill(const PaperTemplate& paper);
/// Stores in paper's sheet* fields the typed values that differ from the Title block prefill.
void rememberTimesheet(PaperTemplate& paper, const TimesheetInfo& typed);
/// Sheets for a scene of `length` frames: 144 frames each, at least one.
int timesheetSheetCount(int length);
/// On a page, the cell of a Timesheet column (0 Key A ... 13 Camera, -1 the frame marks) at row 0-143 of a sheet.
/// Rows 0-71 are the left half, rows 72-143 the right.
QRectF timesheetCell(const QRectF& page, int column, int row);
/// Page i is sheet i + 1. Each column shows the topmost layer in it. object must outlive the function.
PagePaintFn timesheetPages(const Object& object, const TimesheetInfo& info);

#endif // PAPERPAINTER_H
