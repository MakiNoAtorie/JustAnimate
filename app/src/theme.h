#ifndef THEME_H
#define THEME_H

#include <QIcon>
#include <QPalette>

// One central palette per theme. The choice persists in QSettings.
namespace Theme
{
enum Mode { System, Light, Dark };
// Append only: the index is the saved value.
enum Accent { Blue, Teal, Green, Amber, Orange, Red, Pink, Violet, ACCENT_COUNT };

Mode saved();
void save(Mode mode);
Accent savedAccent();
void saveAccent(Accent accent);
// The accent is the Highlight role, toned for contrast against the mode's panels.
QPalette palette(Mode mode, Accent accent = savedAccent());
// Applies the mode and the saved accent to the running application now.
void apply(Mode mode);
// True when the active palette is dark.
bool isDark();
// A one-colour SVG (stroke="currentColor") drawn in the application palette's
// WindowText at paint time, so it follows theme changes. A checked button
// draws it in the `on` role. Null when the file is missing or invalid.
QIcon icon(const QString& svgPath, QPalette::ColorRole on = QPalette::Highlight);
}

#endif // THEME_H
