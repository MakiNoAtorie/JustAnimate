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

#include <QCheckBox>
#include <QFormLayout>

#include "editor.h"
#include "keyframe.h"
#include "layer.h"
#include "layermanager.h"
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
    mKeyTransformCheckBox = new QCheckBox(tr("Transform Key"), this);
    mKeyTransformCheckBox->setToolTip(tr("Drag to move the current Drawing's key, Ctrl+drag to rotate it, Shift+drag to scale it. The tween eases between key transforms."));
    mKeyFields = new QWidget(this);
    mKeyFields->setToolTip(mKeyTransformCheckBox->toolTip());
    QFormLayout* form = new QFormLayout(mKeyFields);
    form->setContentsMargins(0, 0, 0, 0);
    const struct { const char* name; QString label; qreal min, max; QString suffix; } fields[] = {
        { "keyMoveXBox", tr("Move X"), -100000, 100000, QString() },
        { "keyMoveYBox", tr("Move Y"), -100000, 100000, QString() },
        { "keyRotationBox", tr("Rotation"), -3600, 3600, QString(QChar(0x00B0)) },
        { "keyScaleBox", tr("Scale"), 1, 10000, QStringLiteral("%") },
        { "keyOpacityBox", tr("Opacity"), 0, 100, QStringLiteral("%") },
    };
    for (int i = 0; i < 5; ++i) {
        QDoubleSpinBox* box = new QDoubleSpinBox(mKeyFields);
        box->setObjectName(fields[i].name);
        box->setRange(fields[i].min, fields[i].max);
        box->setSuffix(fields[i].suffix);
        form->addRow(fields[i].label, box);
        connect(box, &QDoubleSpinBox::editingFinished, this, &TransformOptionsWidget::applyKeyTransform);
        mKeyBoxes[i] = box;
    }
    ui->verticalLayout_2->insertWidget(2, mKeyTransformCheckBox);
    ui->verticalLayout_2->insertWidget(3, mKeyFields);
    mKeyTransformCheckBox->setVisible(false);
    mKeyFields->setVisible(false);
    connect(mKeyTransformCheckBox, &QCheckBox::clicked, this, [=](bool enabled) {
        mTransformTool->setKeyTransformEnabled(enabled);
    });
    connect(mEditor, &Editor::frameModified, this, &TransformOptionsWidget::updateKeyTransform);
    connect(mEditor, &Editor::scrubbed, this, &TransformOptionsWidget::updateKeyTransform);
    connect(mEditor->layers(), &LayerManager::currentLayerChanged, this, &TransformOptionsWidget::updateKeyTransform);

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

    const bool keyMode = currentTool->isPropertyEnabled(TransformToolProperties::KEYTRANSFORM_ENABLED);
    const bool keyOn = keyMode && currentTool->transformSettings().keyTransformEnabled();
    mKeyTransformCheckBox->setVisible(keyMode);
    {
        QSignalBlocker b(mKeyTransformCheckBox);
        mKeyTransformCheckBox->setChecked(keyOn);
    }
    mKeyFields->setVisible(keyOn);
    // The selection fields have nothing to show while drags edit the key.
    for (int i = 0; i < ui->numericLayout->count(); ++i) {
        if (QWidget* w = ui->numericLayout->itemAt(i)->widget()) { w->setVisible(!keyOn); }
    }
    updateKeyTransform();
}

void TransformOptionsWidget::updateKeyTransform()
{
    const Layer* layer = mEditor->layers()->currentLayer();
    const bool drawing = layer && (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR);
    const KeyFrame* key = drawing ? layer->getLastKeyFrameAtPosition(mEditor->currentFrame()) : nullptr;
    const TweenState s = key ? key->tweenState() : TweenState();
    const qreal values[] = { s.tx, s.ty, s.rot, s.scale * 100, s.opacity * 100 };
    for (int i = 0; i < 5; ++i) {
        QSignalBlocker b(mKeyBoxes[i]);
        mKeyBoxes[i]->setEnabled(key != nullptr);
        mKeyBoxes[i]->setValue(values[i]);
        mKeyShown[i] = mKeyBoxes[i]->value();
    }
}

void TransformOptionsWidget::applyKeyTransform()
{
    const Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR)) { return; }
    const KeyFrame* key = layer->getLastKeyFrameAtPosition(mEditor->currentFrame());
    if (key == nullptr) { return; }

    bool changed = false;
    for (int i = 0; i < 5; ++i) { changed |= mKeyBoxes[i]->value() != mKeyShown[i]; }
    if (!changed) { return; }

    // A box the user did not change keeps the exact value, not the rounded one it shows.
    const TweenState s = key->tweenState();
    auto pick = [&](int i, qreal current, qreal unit) {
        return mKeyBoxes[i]->value() == mKeyShown[i] ? current : mKeyBoxes[i]->value() / unit;
    };
    const TweenState next{ pick(0, s.tx, 1), pick(1, s.ty, 1), pick(2, s.rot, 1), pick(3, s.scale, 100), pick(4, s.opacity, 100) };
    mEditor->layers()->setKeyTween(key->pos(), key->tween(), key->tweenEasing(), next);
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
    connect(transformTool, &TransformTool::keyTransformChanged, this, &TransformOptionsWidget::updatePropertyVisibility);
}

std::array<QDoubleSpinBox*, 5> TransformOptionsWidget::numericBoxes() const
{
    return { ui->positionXBox, ui->positionYBox, ui->scaleXBox, ui->scaleYBox, ui->rotationBox };
}

void TransformOptionsWidget::updateNumericTransform()
{
    const SelectionManager* select = mEditor->select();
    const bool selected = select->somethingSelected();
    const QPointF centre = selected ? select->selectionCentre() : QPointF();
    const qreal values[] = { centre.x(), centre.y(), select->myScaleX() * 100, select->myScaleY() * 100, select->myRotation() };
    const auto boxes = numericBoxes();
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

    const auto boxes = numericBoxes();
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
