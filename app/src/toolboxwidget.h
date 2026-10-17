/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang
Copyright (C) 2024-2099 Oliver S. Larsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef TOOLBOXWIDGET_H
#define TOOLBOXWIDGET_H

#include <QObject>
#include <QToolButton>
#include <QWidget>

#include "flowlayout.h"
#include "pencildef.h"
#include "toolboxlayout.h"

class Editor;
class QMenu;
class QTimer;

namespace Ui {
class ToolBoxWidget;
}

class ToolBoxWidget : public QWidget
{
    Q_OBJECT
public:
    ToolBoxWidget(QWidget* parent = nullptr);
    ~ToolBoxWidget() override;

    void setEditor(Editor* editor) { mEditor = editor; }
    void initUI();
    void updateUI();

public slots:
    void setActiveTool(ToolType toolType);

public:

    void pencilOn();
    void eraserOn();
    void selectOn();
    void moveOn();
    void penOn();
    void handOn();
    void polylineOn();
    void shapeOn();
    void lassoFillOn();
    void bucketOn();
    void eyedropperOn();
    void brushOn();
    void smudgeOn();

    void updateLayoutAlignment();
    void deselectAllTools();
    void toolOn(ToolType toolType, QToolButton* toolButton);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    int getMinHeightForWidth(int width) const;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Member { ToolType type; QToolButton* source; QIcon icon; QString toolTip; };
    struct Group { QString key; QToolButton* button; QList<Member> members; QMenu* menu; ToolType last; };

    void initGroups();
    void showGroupMenu(Group& group);
    void setGroupLast(Group& group, ToolType type);
    Group* groupOf(ToolType type);

    QList<Group> mGroups;
    QTimer* mPressTimer = nullptr;
    Group* mPressed = nullptr;
    FlowLayout* mFlowlayout = nullptr;

    Ui::ToolBoxWidget* ui = nullptr;
    Editor* mEditor = nullptr;
};

#endif // TOOLBOXWIDGET_H
