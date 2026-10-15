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

#include <cmath>
#include <QSettings>

#include "brushtool.h"
#include "inputsmoother.h"
#include "testfixture.h"
#include "toolmanager.h"

namespace
{

struct Jitter
{
    qreal meanOffLine = 0; // mean distance from the true line y = 100
    qreal lag = 0;         // how far the last output trails the last input along x
};

// A pen moving right at 400 px/s, sampled every 5 ms, with +-2 px of jitter across the line.
Jitter filterJitteryLine(SmoothingMode mode, int strength)
{
    InputSmoother s;
    s.setMode(mode, strength);
    s.reset(QPointF(0, 100), 1000);
    qreal off = 0;
    int n = 0;
    QPointF in, out;
    for (int i = 1; i <= 200; ++i)
    {
        in = QPointF(i * 2.0, 100 + ((i % 2) ? 2.0 : -2.0));
        out = s.filter(in, 1000 + i * 5);
        if (i > 20) { off += std::abs(out.y() - 100); ++n; }
    }
    return { off / n, in.x() - out.x() };
}

} // namespace

TEST_CASE("InputSmoother - None passes points through", "[InputSmoother]")
{
    const Jitter j = filterJitteryLine(SmoothingMode::None, 50);
    CHECK(j.meanOffLine == Approx(2.0));
    CHECK(j.lag == Approx(0.0));
}

TEST_CASE("InputSmoother - 1-euro removes jitter with little lag, more with more strength", "[InputSmoother]")
{
    const Jitter light = filterJitteryLine(SmoothingMode::OneEuro, 10);
    const Jitter medium = filterJitteryLine(SmoothingMode::OneEuro, 50);
    const Jitter strong = filterJitteryLine(SmoothingMode::OneEuro, 100);
    INFO("off line " << light.meanOffLine << " " << medium.meanOffLine << " " << strong.meanOffLine);
    INFO("lag " << light.lag << " " << medium.lag << " " << strong.lag);

    CHECK(medium.meanOffLine < 0.5); // raw input is 2 px off the line
    CHECK(medium.lag > 0);
    CHECK(medium.lag < 10);
    CHECK(light.meanOffLine > medium.meanOffLine);
    CHECK(medium.meanOffLine > strong.meanOffLine);
    CHECK(light.lag < medium.lag);
    CHECK(medium.lag < strong.lag);
}

TEST_CASE("InputSmoother - runs on input timestamps: a longer gap lets the output follow further", "[InputSmoother]")
{
    auto stepAfter = [](quint64 gapMs) {
        InputSmoother s;
        s.setMode(SmoothingMode::OneEuro, 50);
        s.reset(QPointF(0, 0), 1000);
        return s.filter(QPointF(10, 0), 1000 + gapMs).x();
    };
    CHECK(stepAfter(1) < stepAfter(10));
    CHECK(stepAfter(10) < stepAfter(100));
    CHECK(stepAfter(100) < 10.0);
}

TEST_CASE("InputSmoother - old None/Simple/Strong stabilizer levels map to presets", "[InputSmoother]")
{
    CHECK(InputSmoother::presetForLegacyLevel(0).mode == SmoothingMode::None);
    CHECK(InputSmoother::presetForLegacyLevel(1).mode == SmoothingMode::OneEuro);
    CHECK(InputSmoother::presetForLegacyLevel(2).mode == SmoothingMode::OneEuro);
    CHECK(InputSmoother::presetForLegacyLevel(1).strength < InputSmoother::presetForLegacyLevel(2).strength);
}

TEST_CASE("InputSmoother - an existing user's stabilizer setting maps to mode and strength on first launch", "[InputSmoother]")
{
    const SettingsGuard keys({ "Brush/LineStabilization", "Brush/SmoothingMode", "Brush/SmoothingStrength" });
    {
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.setValue("Brush/LineStabilization", 1); // Simple
        settings.remove("Brush/SmoothingMode");
        settings.remove("Brush/SmoothingStrength");
    }

    TestSession s;
    s.start();
    const StrokeToolProperties& p = static_cast<BrushTool*>(s.editor->tools()->getTool(BRUSH))->strokeToolProperties();
    const SmoothingPreset simple = InputSmoother::presetForLegacyLevel(1);
    CHECK(p.smoothingMode() == simple.mode);
    CHECK(p.smoothingStrength() == simple.strength);
}
