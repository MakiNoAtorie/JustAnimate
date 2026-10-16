#include "prefsearch.h"

bool prefLabelMatches(const QString& label, const QString& query)
{
    QString text = label;
    text.remove('&');
    const QStringList words = query.split(' ', Qt::SkipEmptyParts);
    for (const QString& word : words)
    {
        if (!text.contains(word, Qt::CaseInsensitive))
        {
            return false;
        }
    }
    return true;
}
