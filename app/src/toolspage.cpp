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

#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QtMath>

#include "preferencemanager.h"
#include "pressurecurvewidget.h"

#include "ui_toolspage.h"

ToolsPage::ToolsPage() : ui(new Ui::ToolsPage)
{
    ui->setupUi(this);

    mPressureCurve = new PressureCurveWidget(this);
    auto* resetCurve = new QPushButton(tr("Reset Pressure Curve"), this);
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

    // Taper also gives mouse strokes thin ends, like simulated pressure.
    auto taperBox = [this](const QString& label, SETTING setting) {
        auto* box = new QSpinBox(this);
        box->setRange(0, 1000);
        box->setSuffix(tr(" px"));
        box->setSpecialValueText(tr("Off"));
        ui->verticalLayout_2->addWidget(new QLabel(label, this));
        ui->verticalLayout_2->addWidget(box, 0, Qt::AlignLeft);
        connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, setting](int v) { mManager->set(setting, v); });
        return box;
    };
    mTaperStart = taperBox(tr("Stroke start taper (Pencil, Brush, Eraser):"), SETTING::TAPER_START);
    mTaperEnd = taperBox(tr("Stroke end taper (Pencil, Brush, Eraser):"), SETTING::TAPER_END);
    auto* dynamics = new QFormLayout;
    struct Row { SETTING setting; QString label; int min; int max; QString suffix; };
    const Row rows[] = {
        { SETTING::SPEED_TO_SIZE, tr("Speed to size:"), -100, 100, "%" },
        { SETTING::TILT_TO_SIZE, tr("Tilt to size:"), -100, 100, "%" },
        { SETTING::TILT_TO_ANGLE, tr("Tilt to tip angle:"), -100, 100, "%" },
        { SETTING::SIZE_JITTER, tr("Size jitter:"), 0, 100, "%" },
        { SETTING::SCATTER, tr("Scatter:"), 0, 500, "%" },
        { SETTING::ANGLE_JITTER, tr("Angle jitter:"), 0, 180, QString(QChar(0x00B0)) },
        { SETTING::DAB_COUNT, tr("Dabs per step:"), 1, 16, "" },
    };
    for (const auto& [setting, label, min, max, suffix] : rows)
    {
        auto* box = new QSpinBox(this);
        box->setRange(min, max);
        box->setSuffix(suffix);
        dynamics->addRow(label, box);
        mDynamicsBoxes.append({ setting, box });
        connect(box, &QSpinBox::valueChanged, this, [this, setting = setting](int value) { mManager->set(setting, value); });
    }
    ui->verticalLayout_2->addLayout(dynamics);

    connect(ui->useQuickSizingBox, &QCheckBox::stateChanged, this, &ToolsPage::quickSizingChange);
    connect(ui->rotationIncrementSlider, &QSlider::valueChanged, this, &ToolsPage::rotationIncrementChange);
    connect(ui->invertZoomDirectionBox, &QCheckBox::stateChanged, this, &ToolsPage::invertZoomDirectionChange);
}

ToolsPage::~ToolsPage()
{
    delete ui;
}

void ToolsPage::updateValues()
{
    ui->useQuickSizingBox->setChecked(mManager->isOn(SETTING::QUICK_SIZING));
    ui->invertZoomDirectionBox->setChecked(mManager->isOn(SETTING::INVERT_DRAG_ZOOM_DIRECTION));
    setRotationIncrement(mManager->getInt(SETTING::ROTATION_INCREMENT));
    mPressureCurve->setCurve(PressureCurve::fromString(mManager->getString(SETTING::PRESSURE_CURVE)));
    mTaperStart->setValue(mManager->getInt(SETTING::TAPER_START));
    mTaperEnd->setValue(mManager->getInt(SETTING::TAPER_END));
    for (const auto& [setting, box] : mDynamicsBoxes)
    {
        QSignalBlocker blocker(box);
        box->setValue(mManager->getInt(setting));
    }
}

void ToolsPage::quickSizingChange(int b)
{
    mManager->set(SETTING::QUICK_SIZING, b != Qt::Unchecked);
}

void ToolsPage::invertZoomDirectionChange(int b)
{
    mManager->set(SETTING::INVERT_DRAG_ZOOM_DIRECTION, b != Qt::Unchecked);
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
