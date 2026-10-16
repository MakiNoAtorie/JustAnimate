#include "catch.hpp"

#include <QApplication>
#include <QSet>

#include "pencildef.h"
#include "pencilsettings.h"
#include "testfixture.h"
#include "theme.h"

TEST_CASE("Theme choice applies and persists", "[Theme]")
{
    const SettingsGuard guard({ SETTING_THEME });

    Theme::save(Theme::Dark);
    CHECK(Theme::saved() == Theme::Dark);
    Theme::apply(Theme::Dark);
    CHECK(Theme::isDark());

    Theme::save(Theme::Light);
    CHECK(Theme::saved() == Theme::Light);
    Theme::apply(Theme::Light);
    CHECK_FALSE(Theme::isDark());

    Theme::apply(Theme::System);
}

TEST_CASE("Accent colour persists and becomes the highlight", "[Theme]")
{
    const SettingsGuard guard({ SETTING_THEME, SETTING_ACCENT });
    pencilSettings().remove(SETTING_ACCENT);
    CHECK(Theme::savedAccent() == Theme::Blue);

    Theme::saveAccent(Theme::Pink);
    CHECK(Theme::savedAccent() == Theme::Pink);
    Theme::apply(Theme::Dark);
    CHECK(QApplication::palette().color(QPalette::Highlight) == Theme::palette(Theme::Dark, Theme::Pink).color(QPalette::Highlight));

    // Every choice gives its own highlight.
    QSet<QRgb> seen;
    for (int a = 0; a < Theme::ACCENT_COUNT; ++a)
        seen << Theme::palette(Theme::Light, static_cast<Theme::Accent>(a)).color(QPalette::Highlight).rgb();
    CHECK(seen.size() == Theme::ACCENT_COUNT);
    CHECK(Theme::ACCENT_COUNT == 8);

    Theme::saveAccent(Theme::Blue);
    Theme::apply(Theme::System);
}
