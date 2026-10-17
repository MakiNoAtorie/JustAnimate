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

#include <algorithm>

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

WelcomeDialog::WelcomeDialog(int defaultPreset, const QStringList& recentFiles, const QString& lastTemplate, QWidget* parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Welcome to JustAnimate"));
    setMinimumWidth(640);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h1>Welcome to JustAnimate!</h1>"), this));

    mRecentLabel = new QLabel(tr("Recent projects"), this);
    layout->addWidget(mRecentLabel);
    mRecent = new QListWidget(this);
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
    mRecentLabel->setHidden(mRecent->count() == 0);
    mRecent->setHidden(mRecent->count() == 0);
    connect(mRecent, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        mOpenPath = item->data(Qt::UserRole).toString();
        accept();
    });

    auto* newRow = new QHBoxLayout;
    newRow->addWidget(new QLabel(tr("New project:"), this));
    mNew = new QComboBox(this);
    mNew->setObjectName("welcomeNew");
    newRow->addWidget(mNew, 1);
    auto* newButton = new QPushButton(tr("New"), this);
    newRow->addWidget(newButton);
    layout->addLayout(newRow);
    connect(newButton, &QPushButton::clicked, this, [this] {
        const int code = mNew->currentData().toInt();
        mTemplate = code < 0 ? -code - 1 : -1;
        accept();
    });

    mHide = new QCheckBox(tr("Don't show this screen again"), this);
    mHide->setObjectName("welcomeHide");
    mHide->setToolTip(tr("Opens the last project at startup, or a new project from the default preset when there is none. Change it in Preferences."));
    layout->addWidget(mHide);
    auto* bottom = new QHBoxLayout;
    auto* browse = new QPushButton(tr("Open..."), this);
    bottom->addWidget(browse);
    bottom->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"), this);
    bottom->addWidget(cancel);
    layout->addLayout(bottom);
    connect(browse, &QPushButton::clicked, this, [this] {
        mBrowse = true;
        accept();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    fillNew(defaultPreset, lastTemplate);
    hideQuestionMark(*this);
}

int WelcomeDialog::presetIndex() const
{
    const int code = mNew->currentData().toInt();
    return code < 0 ? 0 : code;
}

bool WelcomeDialog::shouldHide() const
{
    return mHide->isChecked();
}

void WelcomeDialog::fillNew(int defaultPreset, const QString& lastTemplate)
{
    // Item data: a template i is -(i + 1); a preset is its index (0 is Default).
    const auto& templates = Welcome::templates();
    for (int i = 0; i < templates.size(); ++i)
    {
        const Welcome::Template& t = templates.at(i);
        mNew->addItem(tr("%1 (%2 x %3, %4 fps)").arg(t.name).arg(t.size.width()).arg(t.size.height()).arg(t.fps), -(i + 1));
    }
    mNew->insertSeparator(mNew->count());
    mNew->addItem(tr("Default"), 0);

    QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (dataDir.cd("presets") && dataDir.exists("presets.ini"))
    {
        QSettings presets(dataDir.filePath("presets.ini"), QSettings::IniFormat);
        QList<QPair<QString, int>> found;
        for (const QString& key : presets.allKeys())
        {
            bool ok = false;
            const int index = key.toInt(&ok);
            const QString name = presets.value(key).toString();
            if (!ok || index == 0 || name.isEmpty() || !dataDir.exists(QString("%1.pclx").arg(index))) continue;
            found.append({ name, index });
        }
        std::sort(found.begin(), found.end());
        for (const auto& p : found) { mNew->addItem(p.first, p.second); }
    }

    int at = Welcome::templateIndex(lastTemplate) >= 0 ? mNew->findData(-(Welcome::templateIndex(lastTemplate) + 1)) : -1;
    if (at < 0 && defaultPreset > 0 && lastTemplate.isEmpty())
    {
        at = mNew->findData(defaultPreset);
    }
    mNew->setCurrentIndex(at < 0 ? 0 : at);
}