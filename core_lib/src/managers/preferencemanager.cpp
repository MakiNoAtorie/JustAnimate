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

#include "preferencemanager.h"

#include <QSettings>
#include <QDebug>
#include "pencilsettings.h"

namespace
{
const PrefRow& row(SETTING option) { return kPrefRows[static_cast<int>(option)]; }

QVariant defaultValue(const PrefRow& r)
{
    return std::visit([](auto def) {
        if constexpr (std::is_same_v<decltype(def), const char*>) { return QVariant(QString::fromUtf8(def)); }
        else { return QVariant(def); }
    }, r.def);
}

// Ranges the stored value must keep; other int rows take any value.
template<typename D>
using PrefValueType = std::conditional_t<std::is_same_v<D, const char*>, QString, D>;

int clamped(SETTING option, int value)
{
    switch (option)
    {
    case SETTING::FRAME_SIZE: return qBound(4, value, 40);
    case SETTING::TIMELINE_SIZE: return qMax(2, value);
    case SETTING::LABEL_FONT_SIZE: return qMax(12, value);
    case SETTING::STARTUP_ACTION: return value >= STARTUP_WELCOME && value <= STARTUP_DEFAULT_PRESET ? value : int(STARTUP_WELCOME);
    default: return value;
    }
}
}

PreferenceManager::PreferenceManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
    for (const PrefRow& r : kPrefRows)
    {
        std::visit([&](auto def) { mValues[static_cast<int>(r.id)] = PrefValueType<decltype(def)>(); }, r.def);
    }
}

PreferenceManager::~PreferenceManager()
{
}

bool PreferenceManager::init()
{
    loadPrefs();
    return true;
}

Status PreferenceManager::load(Object*)
{
    return Status::OK;
}

Status PreferenceManager::save(Object*)
{
    return Status::OK;
}

void PreferenceManager::loadPrefs()
{
    QSettings& settings = pencilSettings();

    settings.remove(SETTING_TOOL_CURSOR);
    if (!settings.contains(SETTING_STARTUP_ACTION))
    {
        // The old flags showed the Welcome screen when either flag was on; LoadDefaultPreset never changed startup.
        const bool welcome = settings.value(SETTING_SHOW_WELCOME_SCREEN, true).toBool() || settings.value(SETTING_ASK_FOR_PRESET, false).toBool();
        const bool last = settings.value(SETTING_LOAD_MOST_RECENT, false).toBool();
        settings.setValue(SETTING_STARTUP_ACTION, welcome ? STARTUP_WELCOME : last ? STARTUP_LAST_PROJECT : STARTUP_DEFAULT_PRESET);
    }
    settings.remove(SETTING_ASK_FOR_PRESET);
    settings.remove(SETTING_SHOW_WELCOME_SCREEN);
    settings.remove(SETTING_LOAD_MOST_RECENT);
    settings.remove(SETTING_LOAD_DEFAULT_PRESET);

    for (const PrefRow& r : kPrefRows)
    {
        if (r.migratedFrom)
        {
            if (!settings.contains(r.key) && settings.contains(r.migratedFrom))
            {
                const QVariant old = settings.value(r.migratedFrom);
                settings.setValue(r.key, r.migrate ? r.migrate(old) : old);
            }
            settings.remove(r.migratedFrom);
        }
        const QVariant value = settings.value(r.key, defaultValue(r));
        std::visit([&](auto def) { store(r.id, value.value<PrefValueType<decltype(def)>>()); }, r.def);
    }
}

MarkFilter PreferenceManager::markFilter()
{
    if (get<SETTING::ONION_TYPE>() != "marked") { return MarkFilter(); }
    return MarkFilter::fromInt(get<SETTING::ONION_MARKS>());
}

bool PreferenceManager::isOnionAbsolute()
{
    return get<SETTING::ONION_TYPE>() != "relative";
}

template<typename T>
void PreferenceManager::store(SETTING option, T value)
{
    PrefValue& slot = mValues[static_cast<int>(option)];
    if constexpr (std::is_same_v<T, int>) { value = clamped(option, value); }
    pencilSettings().setValue(row(option).key, value);

    bool changed;
    if constexpr (std::is_same_v<T, float>) { changed = !qFuzzyCompare(std::get<float>(slot), value); }
    else { changed = std::get<T>(slot) != value; }
    if (changed)
    {
        slot = value;
        emit optionChanged(option);
    }
}

template void PreferenceManager::store<bool>(SETTING, bool);
template void PreferenceManager::store<int>(SETTING, int);
template void PreferenceManager::store<float>(SETTING, float);
template void PreferenceManager::store<QString>(SETTING, QString);
