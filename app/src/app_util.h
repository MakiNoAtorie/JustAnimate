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
#ifndef APP_UTIL_H
#define APP_UTIL_H

#include <QAction>
#include <QSignalBlocker>
#include "preferencemanager.h"

class QDialog;
class QKeySequence;
class QSettings;
class QString;

void hideQuestionMark(QDialog& dlg);
void setMenuActionChecked(QAction* action, bool checked);

/** Two-way binds a checkable action to the bool preference S. */
template<SETTING S>
void bindPreferenceSetting(QAction* action, PreferenceManager* prefs)
{
    Q_ASSERT(action->isCheckable());
    Q_ASSERT(prefs);

    QSignalBlocker b(action);
    action->setChecked(prefs->get<S>()); // set initial state

    QObject::connect(action, &QAction::triggered, [=](bool on) { prefs->set<S>(on); });
    QObject::connect(prefs, &PreferenceManager::optionChanged, [=](SETTING s)
    {
        if (s == S) { action->setChecked(prefs->get<S>()); }
    });
}

/** True when a frame step must be dropped: a held key repeats faster than the playback fps, so a repeat
 *  that comes less than one frame after the last step is skipped. A single tap always steps.
 *  `msSinceStep` is -1 before the first step. */
bool skipFrameStep(bool autoRepeat, long long msSinceStep, int fps);

/** True when every word of query occurs in label (case-insensitive, '&' mnemonics ignored). */
bool prefLabelMatches(const QString& label, const QString& query);

/** Returns the command that already uses `seq`, or an empty string.
 *  `settings` is positioned in the "shortcuts" group. An empty `seq` never conflicts. */
QString findShortcutConflict(const QSettings& settings, const QString& cmd, const QKeySequence& seq);

#endif // APP_UTIL_H