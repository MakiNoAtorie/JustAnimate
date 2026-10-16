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

#ifndef PENCILSETTINGS_H
#define PENCILSETTINGS_H

#include <QObject>
#include <QSettings>
#include "pencildef.h"

QSettings& pencilSettings();
// Tests only: call before the first pencilSettings() to use an INI file instead of the native store.
void usePencilSettingsIniFile(const QString& path);
void restoreToDefaultSetting();

void restoreShortcutsToDefault();
void checkExistingShortcuts();

#endif // PENCILSETTINGS_H
