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
#ifndef PLATFORMHANDLER_H
#define PLATFORMHANDLER_H

#include <functional>

#include "preferencemanager.h"

namespace PlatformHandler
{

void configurePlatformSpecificSettings();
bool isDarkMode();
void initialise();

/// Turns the WinTab tablet API on or off (Windows only). Returns true if WinTab is in use afterwards.
bool setWinTabEnabled(bool enabled);

/// Applies the stored tablet API setting. Returns true if WinTab is in use, false if Windows Ink is.
inline bool applyTabletApi(PreferenceManager* prefs, const std::function<bool(bool)>& switchWinTab = setWinTabEnabled)
{
    return switchWinTab(prefs->isOn(SETTING::TABLET_WINTAB));
}

}

#endif // PLATFORMHANDLER_H
