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
#include "transformoptionswidget.h"
#include "ui_transformoptionswidget.h"

#include "editor.h"
#include "selectionmanager.h"
#include "toolmanager.h"
#include "transformtool.h"

TransformOptionsWidget::TransformOptionsWidget(Editor* editor, QWidget *parent) :
    BaseWidget(parent),
    ui(new Ui::TransformOptionsWidget), mEditor(editor)
{
    ui->setupUi(this);
    initUI();
}

TransformOptionsWidget::~TransformOptionsWidget()
{
    delete ui;
}

void TransformOptionsWidget::initUI()
{
    makeConnectionsFromUIToModel();
    connect(mEditor->select(), &SelectionManager::selectionChanged, this, &TransformOptionsWidget::updateNumericTransform);
    connect(mEditor, &Editor::frameModified, this, &TransformOptionsWidget::updateNumericTransform);
    for (QDoubleSpinBox* box : { ui->positionXBox, ui->positionYBox, ui->scaleXBox, ui->scaleYBox, ui->rotationBox }) {
        connect(box, &QDoubleSpinBox::editingFinished, this, &TransformOptionsWidget::applyNumericTransform);
    }
    updateNumericTransform();
}

void TransformOptionsWidget::updateUI()
{
    TransformTool* currentTool = mEditor->tools()->currentTransformTool();
    if (currentTool == nullptr) { return; }

    updatePropertyVisibility();
    updateToolConnections(currentTool);
    const TransformToolProperties selectP = currentTool->transformSettings();

    if (currentTool->isPropertyEnabled(TransformToolProperties::SHOWSELECTIONINFO_ENABLED)) {
        setShowSelectionInfo(selectP.showSelectionInfoEnabled());
    }

    if (currentTool->isPropertyEnabled(TransformToolProperties::ANTI_ALIASING_ENABLED)) {
        setAntiAliasingEnabled(selectP.antiAliasingEnabled());
    }
}

void TransformOptionsWidget::updatePropertyVisibility()
{
    TransformTool* currentTool = mEditor->tools()->currentTransformTool();
    if (mEditor->tools()->currentTransformTool() == nullptr) { return; }

    ui->antiAliasingCheckBox->setVisible(currentTool->isPropertyEnabled(TransformToolProperties::ANTI_ALIASING_ENABLED));
}

void TransformOptionsWidget::updateToolConnections(BaseTool* tool)
{
    if (mTransformTool) {
        disconnect(mTransformTool, nullptr, this, nullptr);
    }

    mTransformTool = static_cast<TransformTool*>(tool);

    makeConnectionFromModelToUI(mTransformTool);
}

void TransformOptionsWidget::makeConnectionsFromUIToModel()
{
    connect(ui->showSelectionInfoCheckBox, &QCheckBox::clicked, this, [=](bool enabled) {
       mTransformTool->setShowSelectionInfo(enabled);
    });

    connect(ui->antiAliasingCheckBox, &QCheckBox::clicked, this, [=](bool enabled) {
       mTransformTool->setAntiAliasingEnabled(enabled);
    });
}

void TransformOptionsWidget::makeConnectionFromModelToUI(TransformTool* transformTool)
{
    connect(transformTool, &TransformTool::showSelectionInfoChanged, this, &TransformOptionsWidget::setShowSelectionInfo);
    connect(transformTool, &TransformTool::antiAliasingChanged, this, &TransformOptionsWidget::setAntiAliasingEnabled);
}

void TransformOptionsWidget::updateNumericTransform()
{
    const SelectionManager* select = mEditor->select();
    const bool selected = select->somethingSelected();
    const QPointF centre = selected ? select->selectionCentre() : QPointF();
    const qreal values[] = { centre.x(), centre.y(), select->myScaleX() * 100, select->myScaleY() * 100, select->myRotation() };
    QDoubleSpinBox* boxes[] = { ui->positionXBox, ui->positionYBox, ui->scaleXBox, ui->scaleYBox, ui->rotationBox };
    for (int i = 0; i < 5; ++i) {
        QSignalBlocker b(boxes[i]);
        boxes[i]->setEnabled(selected);
        boxes[i]->setValue(selected ? values[i] : (i == 2 || i == 3 ? 100 : 0));
        mShownValues[i] = boxes[i]->value();
    }
}

void TransformOptionsWidget::applyNumericTransform()
{
    SelectionManager* select = mEditor->select();
    if (!select->somethingSelected()) { return; }

    QDoubleSpinBox* boxes[] = { ui->positionXBox, ui->positionYBox, ui->scaleXBox, ui->scaleYBox, ui->rotationBox };
    bool changed = false;
    for (int i = 0; i < 5; ++i) { changed |= boxes[i]->value() != mShownValues[i]; }
    if (!changed) { return; }

    // A box the user did not change keeps the exact value, not the rounded one it shows.
    auto pick = [&](int i, qreal current, qreal unit) {
        return boxes[i]->value() == mShownValues[i] ? current : boxes[i]->value() / unit;
    };
    const QPointF centre = select->selectionCentre();
    select->setNumericTransform(QPointF(pick(0, centre.x(), 1), pick(1, centre.y(), 1)),
                                pick(4, select->myRotation(), 1),
                                pick(2, select->myScaleX(), 100),
                                pick(3, select->myScaleY(), 100));
}

void TransformOptionsWidget::setShowSelectionInfo(bool enabled)
{
    QSignalBlocker b(ui->showSelectionInfoCheckBox);
    ui->showSelectionInfoCheckBox->setChecked(enabled);
}

void TransformOptionsWidget::setAntiAliasingEnabled(bool enabled)
{
    QSignalBlocker b(ui->antiAliasingCheckBox);
    ui->antiAliasingCheckBox->setChecked(enabled);
}
