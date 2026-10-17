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

#include <QMessageBox>

extern bool gModalWatchdogQuiet; // main.cpp
extern int gModalWatchdogHits;
extern QString gModalWatchdogLast;

TEST_CASE("Modal watchdog closes a modal left open", "[harness]")
{
    const int hits = gModalWatchdogHits;
    gModalWatchdogQuiet = true;
    QMessageBox box(QMessageBox::Information, "Left open", "No closer");
    box.exec();
    gModalWatchdogQuiet = false;

    CHECK(gModalWatchdogHits == hits + 1);
    CHECK(gModalWatchdogLast.contains("QMessageBox"));
    CHECK(gModalWatchdogLast.contains("\"Left open\""));
}
