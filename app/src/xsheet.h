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
#ifndef XSHEET_H
#define XSHEET_H

#include <QList>
#include <QRect>
#include "basedockwidget.h"
#include "pagewriter.h"

class Layer;
class Object;
class QAction;
class QToolBar;
class XSheetCells;
enum class Mark;
enum class TimesheetColumn;

/// The X-sheet Panel: the timeline as a table, one column per layer and one row per frame.
/// It has no data of its own; it reads the Editor and edits through the timeline's operations.
class XSheet : public BaseDockWidget
{
    Q_OBJECT

public:
    /// One Drawing's cells: its first and last frame, its number on the layer, its Mark and whether it is linked.
    struct Run
    {
        int first;
        int last;
        int number;
        Mark mark;
        bool linked;
    };
    /// The Drawings of `layer`; the last one runs to `sceneLength`, a sound clip to its own end.
    static QList<Run> runs(const Layer* layer, int sceneLength);
    /// The user-visible name of a Timesheet column, such as "Key A".
    static QString columnName(TimesheetColumn column);

    /// Timesheet export: the header fields the Title block does not hold, the scene length and its fps.
    struct TimesheetInfo
    {
        QString scene, shot, animator;
        int length = 1;
        int fps = 24;
    };
    /// Sheets for a scene of `length` frames: 144 frames each, at least one.
    static int timesheetSheetCount(int length);
    /// On a page, the cell of a Timesheet column (0 Key A ... 13 Camera, -1 the frame marks) at row 0-143 of a sheet.
    /// Rows 0-71 are the left half, rows 72-143 the right.
    static QRectF timesheetCell(const QRectF& page, int column, int row);
    /// Page i is sheet i + 1. Each column shows the topmost layer in it. object must outlive the function.
    static PagePaintFn timesheetPages(const Object& object, const TimesheetInfo& info);

    explicit XSheet(QWidget* parent);

    void initUI() override;
    void updateUI() override;

    /// Adds the timeline's edit actions (+ / −, ripple insert and delete, New drawing) to the X-sheet's toolbar.
    void setEditActions(const QList<QAction*>& actions);

    /// The cell of a layer at a frame, in the cell area's viewport coordinates.
    QRect cellRect(int layerIndex, int frame) const;
    /// Layer indices left to right in the current layout (preference XSHEET_LAYOUT).
    QList<int> columns() const;

private:
    QToolBar* mToolBar = nullptr;
    XSheetCells* mCells = nullptr;
};

#endif // XSHEET_H
