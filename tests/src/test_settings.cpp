#include "catch.hpp"

#include <QSettings>

#include "pencildef.h"
#include "pencilsettings.h"

TEST_CASE("Test settings stay out of the native store", "[Settings]")
{
    pencilSettings().setValue("TestOnlySettingKey", 1);
    pencilSettings().sync();

    QSettings native(QSettings::NativeFormat, QSettings::UserScope, PENCIL2D, PENCIL2D);
    const bool leaked = native.contains("TestOnlySettingKey");
    native.remove("TestOnlySettingKey");
    pencilSettings().remove("TestOnlySettingKey");
    REQUIRE_FALSE(leaked);
}
