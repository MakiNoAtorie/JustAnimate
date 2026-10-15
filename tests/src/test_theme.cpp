#include "catch.hpp"

#include <QSettings>

#include "pencildef.h"
#include "theme.h"

TEST_CASE("Theme choice applies and persists", "[Theme]")
{
    const Theme::Mode before = Theme::saved();

    Theme::save(Theme::Dark);
    CHECK(Theme::saved() == Theme::Dark);
    Theme::apply(Theme::Dark);
    CHECK(Theme::isDark());

    Theme::save(Theme::Light);
    CHECK(Theme::saved() == Theme::Light);
    Theme::apply(Theme::Light);
    CHECK_FALSE(Theme::isDark());

    Theme::apply(Theme::System);
    Theme::save(before);
    CHECK(Theme::saved() == before);
}
