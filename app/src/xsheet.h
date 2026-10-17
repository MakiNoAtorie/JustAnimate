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

class Layer;
class QAction;
class QToolBar;
class XSheetCells;
enum class Mark;

/// The X-sheet Panel: the timeline as a table, one column per layer and one row per frame.
/// It has no data of its own; it reads the Editor and edits through the timeline's operations.
class XSheet : public BaseDockWidget
{
    Q_OBJECT

public:
    /// One Drawing's cells: its first and last frame, its number on the layer and its Mark.
    struct Run
    {
        int first;
        int last;
        int number;
        Mark mark;
    };
    /// The Drawings of `layer`; the last one runs to `sceneLength`, a sound clip to its own end.
    static QList<Run> runs(const Layer* layer, int sceneLength);

    explicit XSheet(QWidget* parent);

    void initUI() override;
    void updateUI() override;

    /// Adds the timeline's edit actions (+ / −, ripple insert and delete, New drawing) to the X-sheet's toolbar.
    void setEditActions(const QList<QAction*>& actions);

    /// The cell of a layer at a frame, in the cell area's viewport coordinates.
    QRect cellRect(int layerIndex, int frame) const;

private:
    QToolBar* mToolBar = nullptr;
    XSheetCells* mCells = nullptr;
};

#endif // XSHEET_H
