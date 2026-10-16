#ifndef THEME_H
#define THEME_H

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
// True when the active palette is dark. Icons pick their variant from this.
bool isDark();
}

#endif // THEME_H
