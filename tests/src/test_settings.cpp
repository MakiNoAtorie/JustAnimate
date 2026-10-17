#include "catch.hpp"

#include <QSettings>

#include "pencildef.h"
#include "pencilsettings.h"
#include "editor.h"
#include "preferencemanager.h"
#include "testfixture.h"

TEST_CASE("Preferences - every old startup key combination migrates once to one startup choice", "[Settings]")
{
    const SettingsGuard guard({ SETTING_STARTUP_ACTION, SETTING_ASK_FOR_PRESET, SETTING_SHOW_WELCOME_SCREEN, SETTING_LOAD_MOST_RECENT, SETTING_LOAD_DEFAULT_PRESET });
    QSettings& store = pencilSettings();
    const char* oldKeys[] = { SETTING_ASK_FOR_PRESET, SETTING_SHOW_WELCOME_SCREEN, SETTING_LOAD_MOST_RECENT, SETTING_LOAD_DEFAULT_PRESET };
    // Each old key is absent (0), false (1) or true (2). Old defaults: ask false, welcome true, last false, preset true.
    for (int combo = 0; combo < 81; ++combo)
    {
        CAPTURE(combo);
        store.remove(SETTING_STARTUP_ACTION);
        bool value[4] = { false, true, false, true };
        for (int k = 0, c = combo; k < 4; ++k, c /= 3)
        {
            if (c % 3 == 0) { store.remove(oldKeys[k]); continue; }
            value[k] = c % 3 == 2;
            store.setValue(oldKeys[k], value[k]);
        }
        const int expected = (value[0] || value[1]) ? STARTUP_WELCOME : value[2] ? STARTUP_LAST_PROJECT : STARTUP_DEFAULT_PRESET;

        Editor editor;
        PreferenceManager prefs(&editor);
        prefs.loadPrefs();
        CHECK(prefs.getInt(SETTING::STARTUP_ACTION) == expected);
        CHECK(store.value(SETTING_STARTUP_ACTION).toInt() == expected);
        for (const char* key : oldKeys) { CHECK_FALSE(store.contains(key)); }

        store.setValue(SETTING_SHOW_WELCOME_SCREEN, expected != STARTUP_WELCOME); // a stale old key no longer wins
        PreferenceManager again(&editor);
        again.loadPrefs();
        CHECK(again.getInt(SETTING::STARTUP_ACTION) == expected);
    }
}

TEST_CASE("Test settings stay out of the native store", "[Settings]")
{
    pencilSettings().setValue("TestOnlySettingKey", 1);
    pencilSettings().sync();

    QSettings native(QSettings::NativeFormat, QSettings::UserScope, PENCIL2D, PENCIL2D);
    const bool leaked = native.contains("TestOnlySettingKey");
    native.remove("TestOnlySettingKey");
    pencilSettings().remove("TestOnlySettingKey");
    REQUIRE_FALSE(leaked);
}

TEST_CASE("Preferences - a fresh profile autosaves by time every 5 minutes and not by count", "[Settings]")
{
    const SettingsGuard guard({ SETTING_AUTO_SAVE, SETTING_AUTO_SAVE_BY_TIME, SETTING_AUTO_SAVE_BY_TIME_TIMER });
    QSettings& store = pencilSettings();
    store.remove(SETTING_AUTO_SAVE);
    store.remove(SETTING_AUTO_SAVE_BY_TIME);
    store.remove(SETTING_AUTO_SAVE_BY_TIME_TIMER);

    Editor editor;
    PreferenceManager prefs(&editor);
    prefs.loadPrefs();
    CHECK(prefs.isOn(SETTING::AUTO_SAVE_BY_TIME));
    CHECK(prefs.getInt(SETTING::AUTO_SAVE_BY_TIME_TIMER) == 5);
    CHECK_FALSE(prefs.isOn(SETTING::AUTO_SAVE));

    store.setValue(SETTING_AUTO_SAVE_BY_TIME, false);
    store.setValue(SETTING_AUTO_SAVE_BY_TIME_TIMER, 15);
    PreferenceManager saved(&editor);
    saved.loadPrefs();
    CHECK_FALSE(saved.isOn(SETTING::AUTO_SAVE_BY_TIME));
    CHECK(saved.getInt(SETTING::AUTO_SAVE_BY_TIME_TIMER) == 15);
}
