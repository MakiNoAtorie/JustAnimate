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
#include "welcomedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "app_util.h"
#include "welcomehelper.h"

WelcomeDialog::WelcomeDialog(int defaultPreset, const QStringList& recentFiles, QWidget* parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Welcome to JustAnimate"));
    setMinimumWidth(640);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h1>Welcome to JustAnimate!</h1>")));

    layout->addWidget(new QLabel(tr("Recent projects")));
    mRecent = new QListWidget;
    mRecent->setObjectName("welcomeRecent");
    mRecent->setViewMode(QListView::IconMode);
    mRecent->setIconSize(QSize(160, 90));
    mRecent->setGridSize(QSize(180, 130));
    mRecent->setResizeMode(QListView::Adjust);
    mRecent->setMovement(QListView::Static);
    mRecent->setWordWrap(true);
    mRecent->setMinimumHeight(280);
    for (const Welcome::RecentEntry& e : Welcome::recentEntries(recentFiles))
    {
        auto* item = new QListWidgetItem(mRecent);
        item->setData(Qt::UserRole, e.path);
        item->setToolTip(e.path);
        if (e.missing)
        {
            item->setText(tr("%1 (missing)").arg(e.name));
            item->setFlags(Qt::NoItemFlags);
        }
        else
        {
            item->setText(e.name);
            if (!e.thumbnail.isEmpty())
            {
                item->setIcon(QIcon(e.thumbnail));
            }
        }
    }
    layout->addWidget(mRecent);
    connect(mRecent, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        mOpenPath = item->data(Qt::UserRole).toString();
        accept();
    });

    auto* newRow = new QHBoxLayout;
    newRow->addWidget(new QLabel(tr("New project from preset:")));
    mPresets = new QComboBox;
    mPresets->setObjectName("welcomePresets");
    newRow->addWidget(mPresets, 1);
    auto* newButton = new QPushButton(tr("New"));
    newRow->addWidget(newButton);
    layout->addLayout(newRow);
    connect(newButton, &QPushButton::clicked, this, &QDialog::accept);

    auto* templateRow = new QHBoxLayout;
    templateRow->addWidget(new QLabel(tr("New project from template:")));
    auto* templates = new QComboBox;
    templates->setObjectName("welcomeTemplates");
    for (const Welcome::Template& t : Welcome::templates())
    {
        templates->addItem(tr("%1 (%2 x %3, %4 fps)").arg(t.name).arg(t.size.width()).arg(t.size.height()).arg(t.fps));
    }
    templateRow->addWidget(templates, 1);
    auto* templateButton = new QPushButton(tr("New"));
    templateRow->addWidget(templateButton);
    layout->addLayout(templateRow);
    connect(templateButton, &QPushButton::clicked, this, [this, templates] {
        mTemplate = templates->currentIndex();
        accept();
    });

    mAlwaysUse = new QCheckBox(tr("Always use this preset"));
    layout->addWidget(mAlwaysUse);

    auto* bottom = new QHBoxLayout;
    auto* browse = new QPushButton(tr("Open..."));
    bottom->addWidget(browse);
    bottom->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"));
    bottom->addWidget(cancel);
    layout->addLayout(bottom);
    connect(browse, &QPushButton::clicked, this, [this] {
        mBrowse = true;
        accept();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    fillPresets(defaultPreset);
    hideQuestionMark(*this);
}

int WelcomeDialog::presetIndex() const
{
    return mPresets->currentData().toInt();
}

bool WelcomeDialog::shouldAlwaysUse() const
{
    return mAlwaysUse->isChecked();
}

void WelcomeDialog::fillPresets(int defaultPreset)
{
    mPresets->addItem(tr("Default"), 0);

    QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (!dataDir.cd("presets") || !dataDir.exists("presets.ini"))
    {
        return;
    }
    QSettings presets(dataDir.filePath("presets.ini"), QSettings::IniFormat);
    for (const QString& key : presets.allKeys())
    {
        bool ok = false;
        const int index = key.toInt(&ok);
        const QString name = presets.value(key).toString();
        if (!ok || index == 0 || name.isEmpty() || !dataDir.exists(QString("%1.pclx").arg(index))) continue;
        mPresets->addItem(name, index);
    }
    mPresets->model()->sort(0);
    const int at = mPresets->findData(defaultPreset);
    mPresets->setCurrentIndex(at < 0 ? 0 : at);
}
