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
#include "pencilsettings.h"

#include <memory>

#include <QStringList>
#include <QDebug>


// ==== Singleton ====

static QString gIniPath;

void usePencilSettingsIniFile(const QString& path)
{
    gIniPath = path;
}

QSettings& pencilSettings()
{
    // MSVC with Qt 6.5 rejects a ternary of QSettings prvalues: copy constructor is deleted.
    static const std::unique_ptr<QSettings> instance(gIniPath.isEmpty()
        ? new QSettings(PENCIL2D, PENCIL2D)
        : new QSettings(gIniPath, QSettings::IniFormat));
    QSettings& settings = *instance;

    if ( !settings.contains("InitPencilSetting") )
    {
        settings.setValue("InitPencilSetting", true);
        restoreToDefaultSetting();
    }
    return settings;
}

void restoreToDefaultSetting() // TODO: finish reset list
{
    QSettings& s = pencilSettings();

    s.setValue(SETTING_AUTO_SAVE_NUMBER, 15);
    s.setValue(SETTING_TOOL_CURSOR, true);

    s.sync();
    qDebug("restored default tools");
}
void checkExistingShortcuts()
{
    QSettings defaultKey(":resources/kb.ini", QSettings::IniFormat);

    QSettings& curSetting = pencilSettings();
    foreach (QString pShortcutsKey, defaultKey.allKeys())
    {
        if ( ! curSetting.contains( pShortcutsKey ) )
        {
            curSetting.setValue(pShortcutsKey, defaultKey.value(pShortcutsKey));
        }
    }

    curSetting.beginGroup(SHORTCUTS_GROUP);
    defaultKey.beginGroup(SHORTCUTS_GROUP);
    foreach (QString pKey, curSetting.allKeys())
    {
        if ( !defaultKey.contains(pKey) )
        {
            curSetting.remove(pKey);
        }
    }
    defaultKey.endGroup();
    curSetting.endGroup();
    curSetting.sync();
}

void restoreShortcutsToDefault()
{
    QSettings defaultKey(":resources/kb.ini", QSettings::IniFormat);

    QSettings& curSetting = pencilSettings();
    curSetting.remove("shortcuts");

    foreach (QString pShortcutsKey, defaultKey.allKeys())
    {
        curSetting.setValue(pShortcutsKey, defaultKey.value(pShortcutsKey));
    }
}
