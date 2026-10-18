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

#ifndef PREFERENCESDEF_H
#define PREFERENCESDEF_H

#include <type_traits>
#include <variant>
#include <QString>
#include <QVariant>
#include "pencildef.h"

enum class SETTING
{
    ANTIALIAS,
    GRID,
    SHADOW,
    PREV_ONION,
    NEXT_ONION,
    INVISIBLE_LINES,
    OUTLINES,
    ONION_BLUE,
    ONION_RED,
    CURSOR_STYLE,
    HIGH_RESOLUTION,
    TABLET_WINTAB,
    WINDOW_OPACITY,
    SHOW_STATUS_BAR,
    CURVE_SMOOTHING,
    PRESSURE_CURVE,
    BACKGROUND_STYLE,
    AUTO_SAVE,
    AUTO_SAVE_NUMBER,
    AUTO_SAVE_BY_TIME,
    AUTO_SAVE_BY_TIME_TIMER,
    AUTO_SAVE_REMINDER,
    SHORT_SCRUB,
    FPS,
    FIELD_W,
    FIELD_H,
    FRAME_SIZE,
    TIMELINE_SIZE,
    LABEL_FONT_SIZE,
    DRAW_LABEL,
    ONION_MAX_OPACITY,
    ONION_MIN_OPACITY,
    ONION_PREV_FRAMES_NUM,
    ONION_NEXT_FRAMES_NUM,
    ONION_WHILE_PLAYBACK,
    ONION_MUTLIPLE_LAYERS,
    ONION_TYPE,
    ONION_MARKS,
    ONION_FALLOFF,
    ONION_PREV_COLOR,
    ONION_NEXT_COLOR,
    FLIP_ROLL_MSEC,
    FLIP_ROLL_DRAWINGS,
    FLIP_INBETWEEN_MSEC,
    SOUND_SCRUB_ACTIVE,
    SOUND_SCRUB_MSEC,
    LAYER_VISIBILITY,
    LAYER_VISIBILITY_THRESHOLD,
    GRID_SIZE_W,
    GRID_SIZE_H,
    OVERLAY_CENTER,
    OVERLAY_THIRDS,
    OVERLAY_GOLDEN,
    OVERLAY_SAFE,
    OVERLAY_PERSPECTIVE1,
    OVERLAY_PERSPECTIVE2,
    OVERLAY_PERSPECTIVE3,
    OVERLAY_ANGLE,
    OVERLAY_SAFE_HELPER_TEXT_ON,
    ACTION_SAFE_ON,
    ACTION_SAFE,
    TIMECODE_TEXT,
    TITLE_SAFE_ON,
    TITLE_SAFE,
    QUICK_SIZING,
    INVERT_DRAG_ZOOM_DIRECTION,
    INVERT_SCROLL_ZOOM_DIRECTION,
    LANGUAGE,
    LAYOUT_LOCK,
    DRAW_ON_EMPTY_FRAME_ACTION,
    FRAME_POOL_SIZE,
    UNDO_REDO_MAX_STEPS,
    ROTATION_INCREMENT,
    SHOW_SELECTION_INFO,
    STARTUP_ACTION,
    DEFAULT_PRESET,
    FAST_RENDER_LAYERS_ABOVE,
    XSHEET_LAYOUT,
    LAST_TEMPLATE,
    INBETWEEN_EASING,
    COUNT, // COUNT must always be the last one.
};

// Pointer over the canvas for Stroke tools (SETTING::CURSOR_STYLE).
enum CursorStyle
{
    CURSOR_BRUSH_OUTLINE,
    CURSOR_CROSS_ONLY
};

// What the app opens at startup without a file (SETTING::STARTUP_ACTION).
enum StartupAction
{
    STARTUP_WELCOME,
    STARTUP_LAST_PROJECT,
    STARTUP_DEFAULT_PRESET
};

// Actions for drawing on an empty frame.
enum DrawOnEmptyFrameAction
{
    CREATE_NEW_KEY,
    DUPLICATE_PREVIOUS_KEY,
    KEEP_DRAWING_ON_PREVIOUS_KEY
};

/// Converts the value of a migrated-from key to the value of the new key.
using PrefMigration = QVariant (*)(const QVariant& old);

/// One preference: its QSettings key, its type and default (bool, int, float or text), and an old key it replaces.
struct PrefRow
{
    SETTING id;
    const char* key;
    std::variant<bool, int, float, const char*> def;
    /// Read once while key is absent; removed on every load.
    const char* migratedFrom = nullptr;
    /// Null copies the old value as it is.
    PrefMigration migrate = nullptr;
};

/// Every preference, in SETTING order. A new preference is an enum value plus one row here.
inline constexpr PrefRow kPrefRows[] = {
    { SETTING::ANTIALIAS,                   SETTING_ANTIALIAS,                   true },
    { SETTING::GRID,                        SETTING_SHOW_GRID,                   false },
    { SETTING::SHADOW,                      SETTING_SHADOW,                      false },
    { SETTING::PREV_ONION,                  SETTING_PREV_ONION,                  false },
    { SETTING::NEXT_ONION,                  SETTING_NEXT_ONION,                  false },
    { SETTING::INVISIBLE_LINES,             SETTING_INVISIBLE_LINES,             false },
    { SETTING::OUTLINES,                    SETTING_OUTLINES,                    false },
    { SETTING::ONION_BLUE,                  SETTING_ONION_BLUE,                  false },
    { SETTING::ONION_RED,                   SETTING_ONION_RED,                   false },
    // The tool icon cursor is gone: only the dotted-cursor key decides.
    { SETTING::CURSOR_STYLE,                SETTING_CURSOR_STYLE,                int(CURSOR_BRUSH_OUTLINE), SETTING_CANVAS_CURSOR,
      [](const QVariant& old) { return QVariant(int(old.toBool() ? CURSOR_BRUSH_OUTLINE : CURSOR_CROSS_ONLY)); } },
    { SETTING::HIGH_RESOLUTION,             SETTING_HIGH_RESOLUTION,             true },
    { SETTING::TABLET_WINTAB,               SETTING_TABLET_WINTAB,               true },
    { SETTING::WINDOW_OPACITY,              SETTING_WINDOW_OPACITY,              0 },
    { SETTING::SHOW_STATUS_BAR,             SETTING_SHOW_STATUS_BAR,             true },
    { SETTING::CURVE_SMOOTHING,             SETTING_CURVE_SMOOTHING,             20 },
    { SETTING::PRESSURE_CURVE,              SETTING_PRESSURE_CURVE,              "" },
    { SETTING::BACKGROUND_STYLE,            SETTING_BACKGROUND_STYLE,            "white" },
    { SETTING::AUTO_SAVE,                   SETTING_AUTO_SAVE,                   false },
    { SETTING::AUTO_SAVE_NUMBER,            SETTING_AUTO_SAVE_NUMBER,            256 },
    { SETTING::AUTO_SAVE_BY_TIME,           SETTING_AUTO_SAVE_BY_TIME,           true },
    { SETTING::AUTO_SAVE_BY_TIME_TIMER,     SETTING_AUTO_SAVE_BY_TIME_TIMER,     5 },
    { SETTING::AUTO_SAVE_REMINDER,          SETTING_AUTO_SAVE_REMINDER,          true },
    { SETTING::SHORT_SCRUB,                 SETTING_SHORT_SCRUB,                 false },
    { SETTING::FPS,                         SETTING_FPS,                         12 },
    { SETTING::FIELD_W,                     SETTING_FIELD_W,                     800 },
    { SETTING::FIELD_H,                     SETTING_FIELD_H,                     600 },
    { SETTING::FRAME_SIZE,                  SETTING_FRAME_SIZE,                  12 },
    { SETTING::TIMELINE_SIZE,               SETTING_TIMELINE_SIZE,               240 },
    { SETTING::LABEL_FONT_SIZE,             SETTING_LABEL_FONT_SIZE,             12 },
    { SETTING::DRAW_LABEL,                  SETTING_DRAW_LABEL,                  false },
    { SETTING::ONION_MAX_OPACITY,           SETTING_ONION_MAX_OPACITY,           50 },
    { SETTING::ONION_MIN_OPACITY,           SETTING_ONION_MIN_OPACITY,           20 },
    { SETTING::ONION_PREV_FRAMES_NUM,       SETTING_ONION_PREV_FRAMES_NUM,       5 },
    { SETTING::ONION_NEXT_FRAMES_NUM,       SETTING_ONION_NEXT_FRAMES_NUM,       5 },
    { SETTING::ONION_WHILE_PLAYBACK,        SETTING_ONION_WHILE_PLAYBACK,        0 },
    { SETTING::ONION_MUTLIPLE_LAYERS,       SETTING_MULTILAYER_ONION,            false },
    { SETTING::ONION_TYPE,                  SETTING_ONION_TYPE,                  "relative" },
    // MarkFilter({ Mark::Key, Mark::Breakdown }).toInt()
    { SETTING::ONION_MARKS,                 SETTING_ONION_MARKS,                 6 },
    { SETTING::ONION_FALLOFF,               SETTING_ONION_FALLOFF,               0 },
    { SETTING::ONION_PREV_COLOR,            SETTING_ONION_PREV_COLOR,            "#ff0000" }, // palette-exempt: onion skin tint is drawing data
    { SETTING::ONION_NEXT_COLOR,            SETTING_ONION_NEXT_COLOR,            "#0000ff" }, // palette-exempt: onion skin tint is drawing data
    { SETTING::FLIP_ROLL_MSEC,              SETTING_FLIP_ROLL_MSEC,              100 },
    { SETTING::FLIP_ROLL_DRAWINGS,          SETTING_FLIP_ROLL_DRAWINGS,          5 },
    { SETTING::FLIP_INBETWEEN_MSEC,         SETTING_FLIP_INBETWEEN_MSEC,         100 },
    { SETTING::SOUND_SCRUB_ACTIVE,          SETTING_SOUND_SCRUB_ACTIVE,          false },
    { SETTING::SOUND_SCRUB_MSEC,            SETTING_SOUND_SCRUB_MSEC,            100 },
    { SETTING::LAYER_VISIBILITY,            SETTING_LAYER_VISIBILITY,            2 },
    { SETTING::LAYER_VISIBILITY_THRESHOLD,  SETTING_LAYER_VISIBILITY_THRESHOLD,  0.5f },
    { SETTING::GRID_SIZE_W,                 SETTING_GRID_SIZE_W,                 100 },
    { SETTING::GRID_SIZE_H,                 SETTING_GRID_SIZE_H,                 100 },
    { SETTING::OVERLAY_CENTER,              SETTING_OVERLAY_CENTER,              false },
    { SETTING::OVERLAY_THIRDS,              SETTING_OVERLAY_THIRDS,              false },
    { SETTING::OVERLAY_GOLDEN,              SETTING_OVERLAY_GOLDEN,              false },
    { SETTING::OVERLAY_SAFE,                SETTING_OVERLAY_SAFE,                false },
    { SETTING::OVERLAY_PERSPECTIVE1,        SETTING_OVERLAY_PERSPECTIVE1,        false },
    { SETTING::OVERLAY_PERSPECTIVE2,        SETTING_OVERLAY_PERSPECTIVE2,        false },
    { SETTING::OVERLAY_PERSPECTIVE3,        SETTING_OVERLAY_PERSPECTIVE3,        false },
    { SETTING::OVERLAY_ANGLE,               SETTING_OVERLAY_ANGLE,               15 },
    { SETTING::OVERLAY_SAFE_HELPER_TEXT_ON, SETTING_OVERLAY_SAFE_HELPER_TEXT_ON, true },
    { SETTING::ACTION_SAFE_ON,              SETTING_ACTION_SAFE_ON,              true },
    { SETTING::ACTION_SAFE,                 SETTING_ACTION_SAFE,                 5 },
    { SETTING::TIMECODE_TEXT,               SETTING_TIMECODE_TEXT,               1 },
    { SETTING::TITLE_SAFE_ON,               SETTING_TITLE_SAFE_ON,               true },
    { SETTING::TITLE_SAFE,                  SETTING_TITLE_SAFE,                  10 },
    { SETTING::QUICK_SIZING,                SETTING_QUICK_SIZING,                true },
    { SETTING::INVERT_DRAG_ZOOM_DIRECTION,  SETTING_INVERT_DRAG_ZOOM_DIRECTION,  false },
    { SETTING::INVERT_SCROLL_ZOOM_DIRECTION, SETTING_INVERT_SCROLL_ZOOM_DIRECTION, false },
    { SETTING::LANGUAGE,                    SETTING_LANGUAGE,                    "" },
    { SETTING::LAYOUT_LOCK,                 SETTING_LAYOUT_LOCK,                 false },
    { SETTING::DRAW_ON_EMPTY_FRAME_ACTION,  SETTING_DRAW_ON_EMPTY_FRAME_ACTION,  int(CREATE_NEW_KEY) },
    { SETTING::FRAME_POOL_SIZE,             SETTING_FRAME_POOL_SIZE,             1024 },
    { SETTING::UNDO_REDO_MAX_STEPS,         SETTING_UNDO_REDO_MAX_STEPS,         100 },
    { SETTING::ROTATION_INCREMENT,          SETTING_ROTATION_INCREMENT,          15 },
    { SETTING::SHOW_SELECTION_INFO,         SETTING_SHOW_SELECTION_INFO,         false },
    { SETTING::STARTUP_ACTION,              SETTING_STARTUP_ACTION,              int(STARTUP_WELCOME) },
    { SETTING::DEFAULT_PRESET,              SETTING_DEFAULT_PRESET,              0 },
    { SETTING::FAST_RENDER_LAYERS_ABOVE,    SETTING_FAST_RENDER_LAYERS_ABOVE,    false },
    { SETTING::XSHEET_LAYOUT,               SETTING_XSHEET_LAYOUT,               0 },
    { SETTING::LAST_TEMPLATE,               SETTING_LAST_TEMPLATE,               "" },
    { SETTING::INBETWEEN_EASING,            SETTING_INBETWEEN_EASING,            0 },
};

constexpr bool prefRowsInOrder()
{
    int i = 0;
    for (const PrefRow& row : kPrefRows) { if (static_cast<int>(row.id) != i++) { return false; } }
    return i == static_cast<int>(SETTING::COUNT);
}
static_assert(prefRowsInOrder(), "kPrefRows holds one row per SETTING, in enum order");

/// A stored preference value; the alternatives follow PrefRow::def, with text as QString.
using PrefValue = std::variant<bool, int, float, QString>;

/// The value type of preference S: bool, int, float or QString.
template<SETTING S>
using PrefType = std::variant_alternative_t<kPrefRows[static_cast<int>(S)].def.index(), PrefValue>;

/// True when V is the type of preference S. An int row also takes an unscoped enum; a text row takes anything QString takes.
template<SETTING S, typename V>
inline constexpr bool kPrefAccepts =
    std::is_same_v<std::decay_t<V>, PrefType<S>> ||
    (std::is_same_v<PrefType<S>, int> && std::is_enum_v<std::decay_t<V>> && std::is_convertible_v<V, int>) ||
    (std::is_same_v<PrefType<S>, QString> && std::is_convertible_v<V, QString>);

#endif // PREFERENCESDEF_H
