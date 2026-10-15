#ifndef THEME_H
#define THEME_H

#include <QPalette>

// One central palette per theme. The choice persists in QSettings.
namespace Theme
{
enum Mode { System, Light, Dark };

Mode saved();
void save(Mode mode);
QPalette palette(Mode mode);
// Applies the mode to the running application now.
void apply(Mode mode);
// True when the active palette is dark. Icons pick their variant from this.
bool isDark();
}

#endif // THEME_H
