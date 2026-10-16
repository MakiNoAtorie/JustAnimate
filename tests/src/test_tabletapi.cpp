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
#include "catch.hpp"

#include <QSettings>
#include <QVector>

#include "platformhandler.h"
#include "preferencemanager.h"
#include "testfixture.h"
#include "pencilsettings.h"

TEST_CASE("Tablet API: WinTab is the default", "[TabletApi]")
{
    const SettingsGuard guard({ SETTING_TABLET_WINTAB });
    pencilSettings().remove(SETTING_TABLET_WINTAB);

    TestSession session;
    REQUIRE(session.start());

    QVector<bool> asked;
    const bool winTabInUse = PlatformHandler::applyTabletApi(session.editor->preference(),
                                                             [&asked](bool on) { asked.append(on); return on; });

    REQUIRE(asked == QVector<bool>{ true });
    REQUIRE(winTabInUse);
}

TEST_CASE("Tablet API: the stored setting is applied at startup", "[TabletApi]")
{
    const SettingsGuard guard({ SETTING_TABLET_WINTAB });
    pencilSettings().setValue(SETTING_TABLET_WINTAB, false);

    TestSession session;
    REQUIRE(session.start());

    QVector<bool> asked;
    const bool winTabInUse = PlatformHandler::applyTabletApi(session.editor->preference(),
                                                             [&asked](bool on) { asked.append(on); return on; });

    REQUIRE(asked == QVector<bool>{ false });
    REQUIRE_FALSE(winTabInUse);
}

TEST_CASE("Tablet API: Windows Ink stays in use when the switch to WinTab fails", "[TabletApi]")
{
    const SettingsGuard guard({ SETTING_TABLET_WINTAB });
    pencilSettings().setValue(SETTING_TABLET_WINTAB, true);

    TestSession session;
    REQUIRE(session.start());
    PreferenceManager* prefs = session.editor->preference();

    const bool winTabInUse = PlatformHandler::applyTabletApi(prefs, [](bool) { return false; });

    REQUIRE_FALSE(winTabInUse);
    // The request is kept: WinTab is tried again at the next start.
    REQUIRE(prefs->isOn(SETTING::TABLET_WINTAB));
}
