#ifndef PREFSEARCH_H
#define PREFSEARCH_H

#include <QString>
#include <QStringList>

// True when every word of query occurs in label (case-insensitive, '&' mnemonics ignored).
bool prefLabelMatches(const QString& label, const QString& query);

#endif // PREFSEARCH_H
