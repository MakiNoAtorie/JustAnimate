#include "catch.hpp"

#include "prefsearch.h"

TEST_CASE("PrefSearch finds options by name", "[PrefSearch]")
{
    REQUIRE(prefLabelMatches("Maximum Number of Undo/Redo Steps", "undo"));
    REQUIRE(prefLabelMatches("Enable &Action Safe area (%)", "action safe"));
    REQUIRE(prefLabelMatches("Invert Zoom Direction", "  DIRECTION zoom "));
    REQUIRE(prefLabelMatches("Short scrub", ""));
    REQUIRE_FALSE(prefLabelMatches("Short scrub", "autosave"));
    REQUIRE_FALSE(prefLabelMatches("Invert Zoom Direction", "zoom scroll"));
}
