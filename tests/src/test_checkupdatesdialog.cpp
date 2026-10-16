/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "catch.hpp"

#include <memory>
#include <QProgressBar>
#include "appinfo.h"
#include "checkupdatesdialog.h"

TEST_CASE("CheckUpdatesDialog dev build finishes without a network manager", "[CheckUpdatesDialog]")
{
    // Other versions would start a real network request
    if (QString(APP_VERSION) != "0.0.0.0") {
        return;
    }

    auto dialog = std::make_unique<CheckUpdatesDialog>();
    dialog->startChecking();

    QProgressBar* bar = dialog->findChild<QProgressBar*>();
    REQUIRE(bar != nullptr);
    REQUIRE(bar->maximum() > bar->minimum());
    REQUIRE(bar->value() == bar->maximum());

    dialog.reset();
}
