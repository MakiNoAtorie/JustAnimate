#include "shortcutconflict.h"

#include <QSettings>

QString findShortcutConflict(const QSettings& settings, const QString& cmd, const QKeySequence& seq)
{
    if (seq.isEmpty()) { return QString(); }

    const QStringList keys = settings.allKeys();
    for (const QString& other : keys)
    {
        if (other != cmd && QKeySequence(settings.value(other).toString()) == seq)
        {
            return other;
        }
    }
    return QString();
}
