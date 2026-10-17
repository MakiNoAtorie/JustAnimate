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

#include "toolspage.h"

#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QtMath>

#include "preferencemanager.h"
#include "pressurecurvewidget.h"

#include "ui_toolspage.h"

ToolsPage::ToolsPage() : ui(new Ui::ToolsPage)
{
    ui->setupUi(this);

    mPressureCurve = new PressureCurveWidget(this);
    auto* resetCurve = new QPushButton(tr("Reset Pressure Curve"), this);
    resetCurve->setObjectName("resetCurveButton");
    ui->verticalLayout_2->addWidget(new QLabel(tr("Pressure curve (pen pressure to size and opacity):"), this));
    ui->verticalLayout_2->addWidget(mPressureCurve);
    ui->verticalLayout_2->addWidget(resetCurve, 0, Qt::AlignLeft);
    connect(mPressureCurve, &PressureCurveWidget::curveChanged, this, [this](const PressureCurve& curve) {
        mManager->set(SETTING::PRESSURE_CURVE, curve.toString());
    });
    connect(resetCurve, &QPushButton::clicked, this, [this] {
        mPressureCurve->setCurve(PressureCurve());
        mManager->set(SETTING::PRESSURE_CURVE, QString());
    });

    auto* resetTools = new QPushButton(tr("Reset all tools to default"), this);
    resetTools->setObjectName("resetToolsButton");
    resetTools->setToolTip(tr("Reset every tool setting to its default"));
    ui->verticalLayout_2->addWidget(resetTools, 0, Qt::AlignLeft);
    confirmReset = [this] {
        return QMessageBox::question(this, tr("Reset all tools"), tr("Reset every tool setting to its default?")) == QMessageBox::Yes;
    };
    connect(resetTools, &QPushButton::clicked, this, [this] {
        if (confirmReset()) { emit resetToolsRequested(); }
    });

    connect(ui->useQuickSizingBox, &QCheckBox::stateChanged, this, &ToolsPage::quickSizingChange);
    connect(ui->rotationIncrementSlider, &QSlider::valueChanged, this, &ToolsPage::rotationIncrementChange);
}

ToolsPage::~ToolsPage()
{
    delete ui;
}

void ToolsPage::updateValues()
{
    ui->useQuickSizingBox->setChecked(mManager->isOn(SETTING::QUICK_SIZING));
    setRotationIncrement(mManager->getInt(SETTING::ROTATION_INCREMENT));
    mPressureCurve->setCurve(PressureCurve::fromString(mManager->getString(SETTING::PRESSURE_CURVE)));
}

void ToolsPage::quickSizingChange(int b)
{
    mManager->set(SETTING::QUICK_SIZING, b != Qt::Unchecked);
}

void ToolsPage::setRotationIncrement(int angle)
{
    int value = qSqrt((angle - 1) / 359.0) * 359;
    ui->rotationIncrementSlider->setValue(value);
}

void ToolsPage::rotationIncrementChange(int value)
{
    // Use log scale
    int angle = qPow(value / 359.0, 2) * 359 + 1;
    // Basically round up to the nearest number that is a divisor of 360
    while (360 % angle != 0) {
        angle++;
    }
    ui->rotationIncrementDisplay->setText(tr("%1 degrees").arg(angle)); // don't use tr()'s plural settings, it breaks Transifex.
    mManager->set(SETTING::ROTATION_INCREMENT, angle);
}
