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
#include "titlebarwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QToolButton>
#include <QResizeEvent>
#include <QStyle>
#include <QPainter>
#include <QDebug>

#include "theme.h"

TitleBarWidget::TitleBarWidget(QWidget* parent)
    : QWidget(parent)
{

    QVBoxLayout* vLayout = new QVBoxLayout();

    vLayout->setContentsMargins(3,4,3,4);
    vLayout->setSpacing(0);

    vLayout->addWidget(createCustomTitleBarWidget(this));

    setLayout(vLayout);
}

TitleBarWidget::~TitleBarWidget()
{
}

QWidget* TitleBarWidget::createCustomTitleBarWidget(QWidget* parent)
{
    QWidget* containerWidget = new QWidget(parent);

    QHBoxLayout* containerLayout = new QHBoxLayout(parent);

    mCloseButton = new QToolButton(parent);

    mCloseButton->setStyleSheet(flatButtonStylesheet());
    mCloseButton->setToolTip(tr("Close"));

    QSize iconSize = QSize(14,14);
    QSize padding = QSize(2,2);

    mCloseButton->setIcon(Theme::icon(":/icons/tabler/x.svg"));
    mCloseButton->setIconSize(iconSize);
    mCloseButton->setFixedSize(iconSize + padding);

    connect(mCloseButton, &QToolButton::clicked, this, &TitleBarWidget::closeButtonPressed);

    mDockButton = new QToolButton(parent);
    mDockButton->setIcon(Theme::icon(":/icons/tabler/app-window.svg"));
    mDockButton->setStyleSheet(flatButtonStylesheet());
    mDockButton->setToolTip(tr("Dock or undock"));

    mDockButton->setIconSize(iconSize);
    mDockButton->setFixedSize(iconSize + padding);

    connect(mDockButton, &QToolButton::clicked, this, &TitleBarWidget::undockButtonPressed);

    mTitleLabel = new QLabel(parent);
    mTitleLabel->setAlignment(Qt::AlignVCenter);

#ifdef __APPLE__
    containerLayout->addWidget(mCloseButton);
    containerLayout->addWidget(mDockButton);
    containerLayout->addWidget(mTitleLabel);
#else
    containerLayout->addWidget(mTitleLabel);
    containerLayout->addWidget(mDockButton);
    containerLayout->addWidget(mCloseButton);
#endif

    containerLayout->setSpacing(3);
    containerLayout->setContentsMargins(0,0,0,0);

    containerWidget->setLayout(containerLayout);
    containerWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    containerWidget->setMinimumSize(QSize(1,1));

    return containerWidget;
}

QString TitleBarWidget::flatButtonStylesheet() const
{
    return "QToolButton { border: 0; border-radius: 2px; } QToolButton:hover { background-color: palette(mid); }";
}

void TitleBarWidget::setTitle(const QString &title)
{
    mTitleLabel->setText(title);
}

void TitleBarWidget::hideButtons(bool hide)
{
    mCloseButton->setHidden(hide);
    mDockButton->setHidden(hide);
}

void TitleBarWidget::resizeEvent(QResizeEvent *resizeEvent)
{
    QWidget::resizeEvent(resizeEvent);

    hideButtonsIfNeeded(resizeEvent->size().width());
}

void TitleBarWidget::hideButtonsIfNeeded(int width)
{
    if (width <= mWidthOfFullLayout) {
        hideButtons(true);
    } else {
        hideButtons(false);
    }
}

void TitleBarWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // This is to ensure that after the titlebar has been hidden with buttons hidden
    // the layout width is smaller, so we enable them again briefly to get the correct width.
    hideButtons(false);

    mWidthOfFullLayout = layout()->sizeHint().width();
    hideButtonsIfNeeded(size().width());
}

void TitleBarWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.save();
    painter.setBrush(palette().color(QPalette::Midlight));
    painter.setPen(Qt::NoPen);
    painter.drawRect(this->rect());

    QPen pen(palette().color(QPalette::Mid));
    int penWidth = 1;
    pen.setWidth(penWidth);
    painter.setPen(pen);
    painter.drawLine(QPoint(this->rect().x(),
                            this->rect().height()-penWidth),
                     QPoint(this->rect().width(),
                            this->rect().height()-penWidth));
    painter.restore();
}
