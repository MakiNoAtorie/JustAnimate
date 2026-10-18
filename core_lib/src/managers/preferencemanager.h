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

#ifndef PREFERENCEMANAGER_H
#define PREFERENCEMANAGER_H

#include <array>
#include <bitset>
#include <utility>
#include "basemanager.h"
#include "pencildef.h"
#include "preferencesdef.h"
#include "keyframe.h"

class PreferenceManager : public BaseManager
{
    Q_OBJECT

public:
    explicit PreferenceManager(Editor* editor);
    ~PreferenceManager() override;

    virtual bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    void loadPrefs();
    /// Typed access: a value of the wrong type for row S does not compile.
    template<SETTING S> PrefType<S> get() const { return std::get<PrefType<S>>(mValues[static_cast<int>(S)]); }
    template<SETTING S, typename V, typename = std::enable_if_t<kPrefAccepts<S, V>>>
    void set(V&& value) { store(S, PrefType<S>(std::forward<V>(value))); }

    /// The Mark filter onion skin and Flip share: the saved Marks in Marked drawings mode, else one that passes every Drawing.
    MarkFilter markFilter();
    /// True when onion skin and Flip step by Drawing (absolute or Marked drawings mode), false when by frame.
    bool isOnionAbsolute();

signals:
    void optionChanged(SETTING e);

private:
    template<typename T> void store(SETTING option, T value);

    std::array<PrefValue, static_cast<size_t>(SETTING::COUNT)> mValues;
};

#endif // PREFERENCEMANAGER_H
