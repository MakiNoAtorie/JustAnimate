#include "theme.h"

#include <QApplication>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>

#include "pencildef.h"

namespace
{
const char* const kKey = "Theme";

QPalette darkPalette()
{
    QPalette p;
    const QColor window(53, 53, 53), base(35, 35, 35), text(Qt::white), disabled(127, 127, 127);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, window);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::PlaceholderText, disabled);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, disabled);
    return p;
}

// Captured on first use, before any change, so System restores the platform look.
struct Original
{
    QString style = QApplication::style()->objectName();
    QPalette palette = QApplication::palette();
};

const Original& original()
{
    static const Original o;
    return o;
}
}

namespace Theme
{
Mode saved()
{
    QSettings s(PENCIL2D, PENCIL2D);
    const int v = s.value(kKey, System).toInt();
    return (v == Light || v == Dark) ? static_cast<Mode>(v) : System;
}

void save(Mode mode)
{
    QSettings(PENCIL2D, PENCIL2D).setValue(kKey, static_cast<int>(mode));
}

QPalette palette(Mode mode)
{
    switch (mode)
    {
    case Dark: return darkPalette();
    case Light: return QPalette(QColor(240, 240, 240));
    default: return original().palette;
    }
}

void apply(Mode mode)
{
    const QString style = (mode == System) ? original().style : QStringLiteral("Fusion");
    if (QStyleFactory::keys().contains(style, Qt::CaseInsensitive))
        QApplication::setStyle(style);
    QApplication::setPalette(palette(mode));
}

bool isDark()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}
}
