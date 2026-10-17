/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef TRANSFORMOPTIONSWIDGET_H
#define TRANSFORMOPTIONSWIDGET_H

#include <array>

#include "basewidget.h"

#include "toolproperties.h"

class TransformTool;
class Editor;
class BaseTool;
class QDoubleSpinBox;
class QCheckBox;

namespace Ui {
class TransformOptionsWidget;
}

class TransformOptionsWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit TransformOptionsWidget(Editor* editor, QWidget *parent = nullptr);
    ~TransformOptionsWidget();

    void initUI() override;
    void updateUI() override;

    void makeConnectionsFromUIToModel();
    void makeConnectionFromModelToUI(TransformTool* transformTool);

private:

    void updatePropertyVisibility();

    void updateToolConnections(BaseTool* tool);
    void setShowSelectionInfo(bool enabled);
    void setAntiAliasingEnabled(bool enabled);
    void updateNumericTransform();
    void applyNumericTransform();
    // Position X, position Y, scale X, scale Y, rotation: the order of mShownValues.
    std::array<QDoubleSpinBox*, 5> numericBoxes() const;
    void updateKeyTransform();
    void applyKeyTransform();

    Ui::TransformOptionsWidget *ui;
    QCheckBox* mKeyTransformCheckBox = nullptr;
    QWidget* mKeyFields = nullptr;
    // Move X, move Y, rotation, scale (%), opacity (%) of the current Drawing's key.
    std::array<QDoubleSpinBox*, 5> mKeyBoxes = {};
    qreal mKeyShown[5] = {};

    TransformTool* mTransformTool = nullptr;
    Editor* mEditor = nullptr;
    qreal mShownValues[5] = {}; // Box values after the last update: an unchanged box keeps the exact value.
};

#endif // TRANSFORMOPTIONSWIDGET_H
