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

#include <QKeySequence>
#include <QPair>
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

    s.sync();
    qDebug("restored default tools");
}
void checkExistingShortcuts()
{
    QSettings defaultKey(":resources/kb.ini", QSettings::IniFormat);

    QSettings& curSetting = pencilSettings();
    const auto freeKey = [&](const QString& cmd, const QString& oldKey) {
        if (curSetting.value(cmd).toString() == oldKey) { curSetting.setValue(cmd, QString()); }
    };
    // Once, when R becomes the Rotate view tool: free R and Z from their old rotate commands.
    if (!curSetting.contains("shortcuts/" CMD_TOOL_ROTATE))
    {
        freeKey("shortcuts/" CMD_ROTATE_CLOCK, "R");
        freeKey("shortcuts/" CMD_ROTATE_ANTI_CLOCK, "Z");
    }
    // Once, when O becomes the onion skin toggle: free O and Alt+O from previous-only and next-only.
    if (!curSetting.contains("shortcuts/" CMD_ONIONSKIN))
    {
        freeKey("shortcuts/" CMD_ONIONSKIN_PREV, "O");
        freeKey("shortcuts/" CMD_ONIONSKIN_NEXT, "Alt+O");
    }
    // Once: v1.x default keys, only for a command with no key and a key no command uses.
    if (!curSetting.contains("ShortcutDefaultsV15"))
    {
        const auto keyTaken = [&](const QString& key) {
            curSetting.beginGroup(SHORTCUTS_GROUP);
            bool taken = false;
            for (const QString& k : curSetting.allKeys())
            {
                taken = taken || QKeySequence(curSetting.value(k).toString()) == QKeySequence(key);
            }
            curSetting.endGroup();
            return taken;
        };
        const QPair<const char*, const char*> defaults[] = {
            { CMD_TOGGLE_XSHEET, "Ctrl+5" }, { CMD_TOGGLE_BRUSH_PRESETS, "Ctrl+9" }, { CMD_TOGGLE_STORYBOARD, "Ctrl+0" },
            { CMD_EXPORT_MOVIE, "Ctrl+M" }, { CMD_CREATE_INBETWEENS, "Ctrl+Shift+I" } };
        for (const auto& d : defaults)
        {
            const QString cmd = QStringLiteral("shortcuts/") + d.first;
            if (curSetting.value(cmd).toString().isEmpty())
            {
                curSetting.setValue(cmd, keyTaken(d.second) ? QString() : QString(d.second));
            }
        }
        curSetting.setValue("ShortcutDefaultsV15", true);
    }
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
