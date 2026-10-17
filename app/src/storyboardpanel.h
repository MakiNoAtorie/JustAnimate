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
#ifndef STORYBOARDPANEL_H
#define STORYBOARDPANEL_H

#include <QList>
#include "basedockwidget.h"

class Layer;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;

/// The Storyboard Panel: one row per storyboard panel of the current Storyboard layer (else the first one),
/// with its Panel fields and its Time. Edits go through LayerManager, so each is one undo step.
class StoryboardPanel : public BaseDockWidget
{
    Q_OBJECT

public:
    enum Column { Scene, Cut, Notes, Audio, Time, ColumnCount };

    explicit StoryboardPanel(QWidget* parent);

    void initUI() override;
    void updateUI() override;

private:
    Layer* storyboardLayer() const;
    void onItemChanged(QTableWidgetItem* item);

    QTableWidget* mTable = nullptr;
    QStackedWidget* mPages = nullptr; // empty-state page, then the table
    int mLayerId = 0;
    QList<int> mPanelFrames; // the key position of each row
    QList<int> mPanelTimes;  // the Exposure of each row
};

#endif // STORYBOARDPANEL_H
