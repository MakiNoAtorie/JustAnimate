#ifndef SHORTCUTCONFLICT_H
#define SHORTCUTCONFLICT_H

#include <QKeySequence>
#include <QString>

class QSettings;

/// Returns the command that already uses `seq`, or an empty string.
/// `settings` is positioned in the "shortcuts" group. An empty `seq` never conflicts.
QString findShortcutConflict(const QSettings& settings, const QString& cmd, const QKeySequence& seq);

#endif // SHORTCUTCONFLICT_H
