/*

JustAnimate - Traditional Animation Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "catch.hpp"

#include "screenshots.h"

// Shots for docs/manual/*-getting-started.md. Each SECTION saves images/getting-started-<shot>.png.
TEST_CASE("Manual screenshots: Getting started", "[.screenshots]")
{
    ShotWindow shot;

    SECTION("main-window")
    {
        saveShot(&shot.window, "getting-started-main-window");
    }
}
