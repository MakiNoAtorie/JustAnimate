/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "onionskinwidget.h"
#include "ui_onionskin.h"

#include <algorithm>
#include <QColorDialog>
#include <QComboBox>
#include <QPixmap>

#include "preferencemanager.h"
#include "editor.h"
#include "flowlayout.h"
#include "theme.h"
#include "util.h"

namespace
{
// SETTING::ONION_TYPE values in the order of the mode combo box.
const QStringList kModes = { "relative", "absolute", "marked" };
const int kMarkedMode = 2;
}

OnionSkinWidget::OnionSkinWidget(QWidget *parent) :
    BaseDockWidget(parent),
    ui(new Ui::OnionSkin)
{
    ui->setupUi(this);
    ui->onionRedButton->setIcon(Theme::icon(":/icons/tabler/stack-back.svg"));
    ui->onionBlueButton->setIcon(Theme::icon(":/icons/tabler/stack-front.svg"));

    clearFocusOnFinished(ui->onionPrevFramesNumBox);
    clearFocusOnFinished(ui->onionNextFramesNumBox);
    clearFocusOnFinished(ui->onionMinOpacityBox);
    clearFocusOnFinished(ui->onionMaxOpacityBox);
}

OnionSkinWidget::~OnionSkinWidget()
{
    delete ui;
}

void OnionSkinWidget::initUI()
{
    mPrevColorButton = addColorButton(ui->onionPrevLayout, SETTING::ONION_PREV_COLOR, tr("Previous frames tint color"));
    mNextColorButton = addColorButton(ui->onionNextlLayout, SETTING::ONION_NEXT_COLOR, tr("Next frames tint color"));
    mFalloffBox = new QComboBox(this);
    mFalloffBox->addItems({ tr("Linear"), tr("Gentle"), tr("Steep") });
    mFalloffBox->setToolTip(tr("How opacity falls from max to min with distance from the current frame"));

    updateUI();
    makeConnections();

    // Change the horizontal layout in the Distributed Opacity group box to a
    // flow layout to reduce the minimum width
    FlowLayout *opacityLayout = new FlowLayout;
    opacityLayout->setAlignment(Qt::AlignHCenter);
    opacityLayout->setContentsMargins(0, 6, 0, 6);
    ui->opacityGroup->layout()->removeWidget(ui->minOpacityGroup);
    ui->opacityGroup->layout()->removeWidget(ui->maxOpacityGroup);
    opacityLayout->addWidget(ui->minOpacityGroup);
    opacityLayout->addWidget(ui->maxOpacityGroup);
    opacityLayout->addWidget(mFalloffBox);
    delete ui->opacityGroup->layout();
    ui->opacityGroup->setLayout(opacityLayout);

#ifdef __APPLE__

    ui->scrollAreaWidgetContents->layout()->setSpacing(8);

    // Mac only style. ToolButtons are naturally borderless on Win/Linux.
    QString stylesheet =
        "QToolButton { border: 0px; } "
        "QGroupBox::title {"
            "subcontrol-origin: padding;"
            "left: 6px;"
            "padding: 2px 2px 0px 0px;"
            "background: transparent;"
        "}"
        "QGroupBox {"
            "subcontrol-origin: margin;"
            "margin-top: 8px;"
            "padding-top: 16px;"
        "}"
        "QToolButton:pressed{ border: 1px solid palette(highlight); border-radius: 2px; background-color: palette(highlight); }"
        "QToolButton:checked{ border: 1px solid palette(highlight); border-radius: 2px; background-color: palette(highlight); }";
    setStyleSheet(this->styleSheet().append(stylesheet));
#endif
}

void OnionSkinWidget::makeConnections()
{
    auto spinBoxChanged = static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged);
    connect(ui->onionMaxOpacityBox, spinBoxChanged, this, &OnionSkinWidget::onionMaxOpacityChange);
    connect(ui->onionMinOpacityBox, spinBoxChanged, this, &OnionSkinWidget::onionMinOpacityChange);
    connect(ui->onionPrevFramesNumBox, spinBoxChanged, this, &OnionSkinWidget::onionPrevFramesNumChange);
    connect(ui->onionNextFramesNumBox, spinBoxChanged, this, &OnionSkinWidget::onionNextFramesNumChange);

    connect(ui->prevFramesGroup, &QGroupBox::clicked, this, &OnionSkinWidget::prevFramesGroupClicked);
    connect(ui->nextFramesGroup, &QGroupBox::clicked, this, &OnionSkinWidget::nextFramesGroupClicked);
    connect(ui->onionBlueButton, &QToolButton::clicked, this, &OnionSkinWidget::onionBlueButtonClicked);
    connect(ui->onionRedButton, &QToolButton::clicked, this, &OnionSkinWidget::onionRedButtonClicked);

    connect(ui->onionSkinMode, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &OnionSkinWidget::onionSkinModeChange);
    for (const auto& [box, mark] : markBoxes())
    {
        connect(box, &QCheckBox::toggled, this, [this, mark = mark](bool on) {
            PreferenceManager* prefs = editor()->preference();
            MarkFilter marks = MarkFilter::fromInt(prefs->get<SETTING::ONION_MARKS>());
            marks.set(mark, on);
            prefs->set<SETTING::ONION_MARKS>(marks.toInt());
        });
    }
    connect(ui->onionWhilePlayback, &QCheckBox::stateChanged, this, &OnionSkinWidget::playbackStateChanged);
    connect(ui->onionSkinMultiLayer, &QCheckBox::stateChanged, this, &OnionSkinWidget::onionSkinMultipleLayersEnabled);
    connect(mFalloffBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
        editor()->preference()->set<SETTING::ONION_FALLOFF>(index);
    });

    PreferenceManager* prefs = editor()->preference();
    connect(prefs, &PreferenceManager::optionChanged, this, &OnionSkinWidget::updateUI);

}

void OnionSkinWidget::updateUI()
{
    PreferenceManager* prefs = editor()->preference();

    ui->prevFramesGroup->setChecked(prefs->get<SETTING::PREV_ONION>());
    ui->nextFramesGroup->setChecked(prefs->get<SETTING::NEXT_ONION>());

    QSignalBlocker b3(ui->onionBlueButton);
    ui->onionBlueButton->setChecked(prefs->get<SETTING::ONION_BLUE>());

    QSignalBlocker b4(ui->onionRedButton);
    ui->onionRedButton->setChecked(prefs->get<SETTING::ONION_RED>());

    ui->onionMaxOpacityBox->setValue(prefs->get<SETTING::ONION_MAX_OPACITY>());
    ui->onionMinOpacityBox->setValue(prefs->get<SETTING::ONION_MIN_OPACITY>());
    ui->onionPrevFramesNumBox->setValue(prefs->get<SETTING::ONION_PREV_FRAMES_NUM>());
    ui->onionNextFramesNumBox->setValue(prefs->get<SETTING::ONION_NEXT_FRAMES_NUM>());

    QSignalBlocker b5(ui->onionSkinMode);
    const int mode = std::max(0, static_cast<int>(kModes.indexOf(prefs->get<SETTING::ONION_TYPE>())));
    ui->onionSkinMode->setCurrentIndex(mode);
    const MarkFilter marks = MarkFilter::fromInt(prefs->get<SETTING::ONION_MARKS>());
    for (const auto& [box, mark] : markBoxes())
    {
        QSignalBlocker blocker(box);
        box->setChecked(marks.contains(mark));
        box->setEnabled(mode == kMarkedMode);
    }

    QSignalBlocker b6(ui->onionWhilePlayback);
    ui->onionWhilePlayback->setChecked(prefs->get<SETTING::ONION_WHILE_PLAYBACK>() != 0);

    QSignalBlocker b7(ui->onionSkinMultiLayer);
    ui->onionSkinMultiLayer->setChecked(prefs->get<SETTING::ONION_MUTLIPLE_LAYERS>());

    QSignalBlocker b8(mFalloffBox);
    mFalloffBox->setCurrentIndex(prefs->get<SETTING::ONION_FALLOFF>());

    const auto swatch = [prefs](SETTING setting) {
        QPixmap pixmap(16, 16);
        pixmap.fill(QColor(prefs->getString(setting)));
        return QIcon(pixmap);
    };
    mPrevColorButton->setIcon(swatch(SETTING::ONION_PREV_COLOR));
    mNextColorButton->setIcon(swatch(SETTING::ONION_NEXT_COLOR));
}

QToolButton* OnionSkinWidget::addColorButton(QLayout* layout, SETTING setting, const QString& toolTip)
{
    QToolButton* button = new QToolButton(this);
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    layout->addWidget(button);
    connect(button, &QToolButton::clicked, this, [this, setting] {
        PreferenceManager* prefs = editor()->preference();
        const QColor color = QColorDialog::getColor(QColor(prefs->getString(setting)), this, tr("Onion Skin Color"));
        if (color.isValid()) { prefs->set(setting, color.name()); }
    });
    return button;
}

void OnionSkinWidget::prevFramesGroupClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::PREV_ONION>(isOn);
}

void OnionSkinWidget::nextFramesGroupClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::NEXT_ONION>(isOn);
}

void OnionSkinWidget::onionRedButtonClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_RED>(isOn);
}

void OnionSkinWidget::onionBlueButtonClicked(bool isOn)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_BLUE>(isOn);
}

void OnionSkinWidget::onionMaxOpacityChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_MAX_OPACITY>(value);
}

void OnionSkinWidget::onionMinOpacityChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_MIN_OPACITY>(value);
}

void OnionSkinWidget::onionPrevFramesNumChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_PREV_FRAMES_NUM>(value);
}

void OnionSkinWidget::onionNextFramesNumChange(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_NEXT_FRAMES_NUM>(value);
}

void OnionSkinWidget::onionSkinModeChange(int index)
{
    editor()->preference()->set<SETTING::ONION_TYPE>(kModes.value(index));
}

std::array<std::pair<QCheckBox*, Mark>, 3> OnionSkinWidget::markBoxes() const
{
    return { { { ui->onionMarkKey, Mark::Key }, { ui->onionMarkBreakdown, Mark::Breakdown }, { ui->onionMarkInbetween, Mark::Inbetween } } };
}

void OnionSkinWidget::playbackStateChanged(int value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_WHILE_PLAYBACK>(value);
}

void OnionSkinWidget::onionSkinMultipleLayersEnabled(bool value)
{
    PreferenceManager* prefs = editor()->preference();
    prefs->set<SETTING::ONION_MUTLIPLE_LAYERS>(value);
}
