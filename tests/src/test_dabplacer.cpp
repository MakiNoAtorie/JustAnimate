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

#include <QRandomGenerator>

#include "dabplacer.h"
#include "pencilsettings.h"
#include "pressurecurve.h"
#include "toolproperties.h"

namespace
{
/// Brush defaults: the test store has no DabPlacerTest group.
ToolProperties brushProperties()
{
    ToolProperties p;
    p.loadTool(BRUSH, "DabPlacerTest", pencilSettings());
    return p;
}

bool sameDabs(const QList<Dab>& a, const QList<Dab>& b)
{
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); ++i)
        if (a[i].pos != b[i].pos || a[i].width != b[i].width || a[i].opacity != b[i].opacity || a[i].angle != b[i].angle)
            return false;
    return true;
}
} // namespace

TEST_CASE("DabPlacer - samples in and dabs out with a seeded random source")
{
    ToolProperties p = brushProperties();
    p.set(ToolProperty::Width, 10.0);
    p.set(ToolProperty::Spacing, 50.0);
    p.set(ToolProperty::PressureEnabled, true);
    const PressureCurve curve;
    QRandomGenerator random(1);
    const QList<DabSample> line{ { QPointF(0, 0), 0.5 }, { QPointF(50, 0), 0.5 }, { QPointF(100, 0), 0.5 } };

    SECTION("Spacing and pressure")
    {
        const QList<Dab> dabs = DabPlacer::place(line, p, curve, random);
        // Width 10 at pressure 0.5 is 5; 50% spacing puts a dab every 2.5 px.
        REQUIRE(dabs.size() == 40);
        CHECK(dabs.first().pos == QPointF(2.5, 0));
        CHECK(dabs.last().pos == QPointF(100, 0));
        for (const Dab& d : dabs)
            CHECK(d.width == 5);
    }
    SECTION("A pressure ramp widens the dabs")
    {
        const QList<Dab> dabs = DabPlacer::place({ { QPointF(0, 0), 0.1 }, { QPointF(100, 0), 1.0 } }, p, curve, random);
        REQUIRE(dabs.size() > 10);
        for (int i = 1; i < dabs.size(); ++i)
            CHECK(dabs[i].width >= dabs[i - 1].width);
        CHECK(dabs.first().width < 2);
        CHECK(dabs.last().width > 9.5);
    }
    SECTION("The end taper waits for the end of the stroke")
    {
        p.set(ToolProperty::TaperStart, 20);
        p.set(ToolProperty::TaperEnd, 20);
        DabPlacer placer;
        placer.begin(p, curve, line[0], random);
        const QList<Dab> held = placer.add(line[2]);
        REQUIRE_FALSE(held.isEmpty());
        CHECK(held.last().pos.x() <= 80);
        CHECK(held.first().width < held.last().width);
        CHECK(placer.hasPending());
        const QList<Dab> rest = placer.add(line[2], true);
        REQUIRE_FALSE(rest.isEmpty());
        CHECK(rest.last().width < rest.first().width);
        CHECK_FALSE(placer.hasPending());
    }
    SECTION("A tap is one dab at the press")
    {
        const QList<Dab> dabs = DabPlacer::place({ { QPointF(5, 5), 0.5 }, { QPointF(5.5, 5), 0.5 } }, p, curve, random);
        REQUIRE(dabs.size() == 1);
        CHECK(dabs[0].pos == QPointF(5, 5));
        CHECK(dabs[0].width == 5);
    }
    SECTION("Jitter, scatter and angle jitter replay from a seed")
    {
        p.set(ToolProperty::SizeJitter, 100);
        p.set(ToolProperty::Scatter, 100);
        p.set(ToolProperty::AngleJitter, 90);
        p.set(ToolProperty::DabCount, 3);
        QRandomGenerator a(7), b(7), c(8);
        const QList<Dab> first = DabPlacer::place(line, p, curve, a);
        CHECK(sameDabs(first, DabPlacer::place(line, p, curve, b)));
        CHECK_FALSE(sameDabs(first, DabPlacer::place(line, p, curve, c)));
        for (const Dab& d : first)
        {
            CHECK(d.width <= 5);
            CHECK(d.width >= 0.5);
            CHECK(std::abs(d.pos.y()) <= 5);
        }
    }
}

TEST_CASE("DabPlacer - scatter draws y before x so every compiler places the same dabs")
{
    ToolProperties p = brushProperties();
    p.set(ToolProperty::Width, 10.0);
    p.set(ToolProperty::PressureEnabled, false);
    p.set(ToolProperty::Scatter, 100);
    const PressureCurve curve;
    QRandomGenerator random(3);
    const QList<Dab> dabs = DabPlacer::place({ { QPointF(0, 0), 1 }, { QPointF(100, 0), 1 } }, p, curve, random);
    REQUIRE(!dabs.isEmpty());

    // Per dab: one size draw, then y, then x; the order MSVC and GCC gave before the draws were named.
    QRandomGenerator mirror(3);
    mirror.generate();
    const qreal y = (mirror.generate() % 20001) / 10000.0 - 1.0;
    // The line runs along x, so the dab's y is its scatter alone.
    CHECK(dabs[0].pos.y() == Approx(y * 10));
}

TEST_CASE("DabPlacer - a recorded pen trace from #175 keeps every dab inside its pressure width")
{
    ToolProperties p = brushProperties();
    p.set(ToolProperty::Width, 40.0);
    p.set(ToolProperty::PressureEnabled, true);
    p.set(ToolProperty::SpeedToSize, 100);
    p.set(ToolProperty::TiltToSize, 100);
    p.set(ToolProperty::SizeJitter, 100);
    p.set(ToolProperty::TaperEnd, 30);
    QRandomGenerator random(175);

    // A light tablet stroke with the #175 suspects: repeated and backward samples, zero and huge
    // speeds, tilt past its range, a dip to zero and pressure outside 0..1. Columns: x, y,
    // pressure, speed (viewport px/ms), tilt x and y (degrees).
    const qreal trace[][6] = {
        { 100, 300, 0.20, 0.0, 0, 0 },      { 104, 300, 0.21, 0.5, 10, 5 },    { 104, 300, 0.21, 0.0, 10, 5 },
        { 110, 301, 0.19, 1e6, 90, -90 },   { 118, 302, 0.22, 3.0, -90, 90 },  { 115, 302, 0.20, 2.0, 30, 30 },
        { 130, 303, 0.00, 0.0, 0, 0 },      { 145, 303, 0.18, 8.0, 45, 45 },   { 160, 304, 1.40, 4.0, 60, 0 },
        { 175, 304, -0.3, 4.0, 0, 60 },     { 190, 305, 0.20, -5.0, 120, 120 }, { 205, 305, 0.20, 2.0, 0, 0 },
        { 205.2, 305, 0.21, 0.1, 0, 0 },    { 240, 306, 0.20, 50.0, 5, 5 },    { 300, 306, 0.20, 3.0, 5, 5 },
        { 300, 306, 0.20, 0.0, 5, 5 },
    };
    QList<DabSample> samples;
    for (const auto& row : trace)
        samples.append({ QPointF(row[0], row[1]), row[2], row[3], QPointF(row[4], row[5]) });

    const QList<Dab> dabs = DabPlacer::place(samples, p, PressureCurve(), random);
    REQUIRE(dabs.size() > 50);
    // The widest trace pressure in 0..1 is 1, times at most 2 for speed and 2 for tilt.
    for (const Dab& d : dabs)
    {
        INFO("dab at " << d.pos.x() << " pressure " << d.pressure << " width " << d.width);
        CHECK(d.width >= 0);
        CHECK(d.width <= 40 * 2 * 2);
        CHECK(d.taper >= 0);
        CHECK(d.taper <= 1);
        CHECK(d.sizeFactor <= 4);
        CHECK(d.opacity >= 0);
        CHECK(d.opacity <= 1);
    }
    // Outside the pressure-1.4 segment the trace stays at pressure 0.22 or less.
    for (const Dab& d : dabs)
        if (d.pos.x() < 145 || d.pos.x() > 190)
            CHECK(d.width <= 40 * 0.22 * 4 + 1e-9);
}
