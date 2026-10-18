#ifndef STROKEOPTIONSWIDGET_H
#define STROKEOPTIONSWIDGET_H

#include "basewidget.h"
#include "pencildef.h"

class Editor;
class BaseTool;

class StrokeTool;

namespace Ui {
class StrokeOptionsWidget;
}

class StrokeOptionsWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit StrokeOptionsWidget(Editor* editor, QWidget *parent = nullptr);
    ~StrokeOptionsWidget();

    void updateUI() override;
    void initUI() override;

private:
    void makeConnectionFromUIToModel();
    void updateToolConnections(BaseTool* tool);

    /// Makes a control the row of a property: its range, label and value come from the descriptor.
    void bind(int id, QWidget* control);
    void updateRow(int id);

    void setVisibility(BaseTool*);

private:
    Ui::StrokeOptionsWidget *ui;

    StrokeTool* mCurrentTool = nullptr;
    QWidget* mDynamicsBox = nullptr;
    QList<std::pair<int, QWidget*>> mRows; // property id, control

    Editor* mEditor = nullptr;

};

#endif // STROKEOPTIONSWIDGET_H
