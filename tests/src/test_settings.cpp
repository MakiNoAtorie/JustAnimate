#include "catch.hpp"

#include <type_traits>
#include <variant>
#include <vector>
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

TEST_CASE("Preferences - an unknown startup choice loads as the Welcome screen", "[Settings]")
{
    const SettingsGuard guard({ SETTING_STARTUP_ACTION });
    for (int stored : { -1, 3, 42 })
    {
        CAPTURE(stored);
        pencilSettings().setValue(SETTING_STARTUP_ACTION, stored);
        Editor editor;
        PreferenceManager prefs(&editor);
        prefs.loadPrefs();
        CHECK(prefs.getInt(SETTING::STARTUP_ACTION) == STARTUP_WELCOME);
    }
}

namespace
{
std::vector<const char*> prefKeys()
{
    std::vector<const char*> keys;
    for (const PrefRow& row : kPrefRows)
    {
        keys.push_back(row.key);
        if (row.migratedFrom) { keys.push_back(row.migratedFrom); }
    }
    return keys;
}

template<SETTING S, typename V, typename = void> struct CanSet : std::false_type {};
template<SETTING S, typename V>
struct CanSet<S, V, std::void_t<decltype(std::declval<PreferenceManager&>().set<S>(std::declval<V>()))>> : std::true_type {};

static_assert(CanSet<SETTING::GRID, bool>::value, "a bool row takes a bool");
static_assert(!CanSet<SETTING::GRID, int>::value, "a bool row refuses an int");
static_assert(!CanSet<SETTING::ONION_FALLOFF, bool>::value, "an int row refuses a bool");
static_assert(CanSet<SETTING::CURSOR_STYLE, CursorStyle>::value, "an int row takes its enum");
static_assert(!CanSet<SETTING::LAYER_VISIBILITY_THRESHOLD, double>::value, "a float row refuses a double");
static_assert(CanSet<SETTING::ONION_TYPE, const char*>::value, "a string row takes text");
static_assert(!CanSet<SETTING::ONION_TYPE, int>::value, "a string row refuses an int");
static_assert(std::is_same_v<decltype(std::declval<PreferenceManager&>().get<SETTING::ONION_TYPE>()), QString>, "a string row reads a QString");
}

TEST_CASE("Preferences - every table row loads its default and round-trips a new value", "[Settings]")
{
    const std::vector<const char*> keys = prefKeys();
    const SettingsGuard guard(keys);
    QSettings& store = pencilSettings();
    for (const char* key : keys) { store.remove(key); }

    Editor editor;
    PreferenceManager prefs(&editor);
    prefs.loadPrefs();
    CHECK(prefs.get<SETTING::ONION_MARKS>() == MarkFilter({ Mark::Key, Mark::Breakdown }).toInt());
    for (const PrefRow& row : kPrefRows)
    {
        CAPTURE(row.key);
        std::visit([&](auto def) {
            using T = decltype(def);
            if constexpr (std::is_same_v<T, bool>) { CHECK(prefs.isOn(row.id) == def); prefs.set(row.id, !def); }
            else if constexpr (std::is_same_v<T, int>) { CHECK(prefs.getInt(row.id) == def); prefs.set(row.id, def + 1); }
            else if constexpr (std::is_same_v<T, float>) { CHECK(prefs.getFloat(row.id) == def); prefs.set(row.id, def + 0.25f); }
            else { CHECK(prefs.getString(row.id) == QString(def)); prefs.set(row.id, QString(def) + "x"); }
        }, row.def);
    }

    PreferenceManager loaded(&editor);
    loaded.loadPrefs();
    for (const PrefRow& row : kPrefRows)
    {
        CAPTURE(row.key);
        std::visit([&](auto def) {
            using T = decltype(def);
            if constexpr (std::is_same_v<T, bool>) { CHECK(loaded.isOn(row.id) == !def); CHECK(store.value(row.key).toBool() == !def); }
            else if constexpr (std::is_same_v<T, int>) { CHECK(loaded.getInt(row.id) == def + 1); CHECK(store.value(row.key).toInt() == def + 1); }
            else if constexpr (std::is_same_v<T, float>) { CHECK(loaded.getFloat(row.id) == def + 0.25f); CHECK(store.value(row.key).toFloat() == def + 0.25f); }
            else { CHECK(loaded.getString(row.id) == QString(def) + "x"); CHECK(store.value(row.key).toString() == QString(def) + "x"); }
        }, row.def);
    }
}

TEST_CASE("Preferences - each migrated-from key moves once while the new key is absent", "[Settings]")
{
    const SettingsGuard guard(prefKeys());
    QSettings& store = pencilSettings();
    int migrated = 0;
    for (const PrefRow& row : kPrefRows)
    {
        if (!row.migratedFrom) { continue; }
        CAPTURE(row.key);
        ++migrated;
        for (bool old : { false, true })
        {
            store.remove(row.key);
            store.setValue(row.migratedFrom, old);
            const QVariant expected = row.migrate ? row.migrate(QVariant(old)) : QVariant(old);

            Editor editor;
            PreferenceManager prefs(&editor);
            prefs.loadPrefs();
            CHECK_FALSE(store.contains(row.migratedFrom));
            CHECK(store.value(row.key).toString() == expected.toString());

            store.setValue(row.migratedFrom, !old); // a stale old key no longer wins
            PreferenceManager again(&editor);
            again.loadPrefs();
            CHECK_FALSE(store.contains(row.migratedFrom));
            CHECK(store.value(row.key).toString() == expected.toString());
        }
    }
    CHECK(migrated > 0);
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
