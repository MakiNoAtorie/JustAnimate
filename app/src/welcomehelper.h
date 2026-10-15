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
#ifndef WELCOMEHELPER_H
#define WELCOMEHELPER_H

#include <QList>
#include <QString>
#include <QStringList>

class Object;

namespace Welcome
{
struct RecentEntry
{
    QString path;
    QString name;
    QString thumbnail; // empty when no cached thumbnail exists
    bool missing = false;
};

QString presetPath(int index);
QString thumbnailPath(const QString& projectPath);
QList<RecentEntry> recentEntries(const QStringList& recentFiles);
bool saveThumbnail(const Object* object, const QString& projectPath, int frame);
}

#endif // WELCOMEHELPER_H
