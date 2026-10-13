#include "catch.hpp"

#include "commandlineparser.h"

TEST_CASE("CommandLineParser bad arguments fall back to defaults")
{
    CommandLineParser parser;
    parser.process({ "app", "in.pclx", "--width", "abc", "--height", "x1",
                     "--start", "0", "--end", "nope" });

    REQUIRE(parser.inputPath() == "in.pclx");
    REQUIRE(parser.width() == -1);
    REQUIRE(parser.height() == -1);
    REQUIRE(parser.startFrame() == 1);
    REQUIRE(parser.endFrame() == -1);
}

TEST_CASE("CommandLineParser end before start is clamped to start")
{
    CommandLineParser parser;
    parser.process({ "app", "in.pclx", "--start", "5", "--end", "2" });

    REQUIRE(parser.startFrame() == 5);
    REQUIRE(parser.endFrame() == 5);
}

TEST_CASE("CommandLineParser accepts valid arguments")
{
    CommandLineParser parser;
    parser.process({ "app", "in.pclx", "-o", "out.png", "--width", "640", "--end", "last-sound" });

    REQUIRE(parser.outputPaths() == QStringList({ "out.png" }));
    REQUIRE(parser.width() == 640);
    REQUIRE(parser.endFrame() == -2);
}
