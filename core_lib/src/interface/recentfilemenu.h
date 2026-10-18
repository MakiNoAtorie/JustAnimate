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


#ifndef RECENTFILEMENU_H
#define RECENTFILEMENU_H

#include <map>
#include <QMenu>
#include <QStringList>
#include "pencildef.h"

class QAction;

/// Shows the project session's recent files.
class RecentFileMenu : public QMenu
{
    Q_OBJECT

public:
    explicit RecentFileMenu(const QString& title = tr("Open Recent"), QWidget* parent = nullptr);
    ~RecentFileMenu() override;

    void setRecentFiles(const QStringList& filenames);

signals:
    void loadRecentFile(QString filename);
    void clearRequested();

protected slots:
    void onRecentFileTriggered();

private:
    void addRecentFile(const QString& filename);
    void removeRecentFile(const QString& filename);
    void clearRecentFiles();

    QStringList mRecentFiles;
    std::map<QString, QAction*> mRecentActions;
    QAction* mClearAction = nullptr;
    QAction* mClearSeparator = nullptr;
    QAction* mEmptyAction = nullptr;
};

#endif // RECENTFILEMENU_H
