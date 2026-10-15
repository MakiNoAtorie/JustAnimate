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
#ifndef INPUTSMOOTHER_H
#define INPUTSMOOTHER_H

#include <QList>
#include <QPointF>

/// Stored as an int in tool settings: append new modes, do not reorder.
enum class SmoothingMode
{
    None = 0,
    OneEuro = 1,
    Weighted = 2,
    Stabilizer = 3,
};

struct SmoothingPreset
{
    SmoothingMode mode;
    int strength; // 0-100
};

/// Smooths pointer positions (viewport pixels) by their input timestamps.
class InputSmoother
{
public:
    static constexpr SmoothingMode LAST_MODE = SmoothingMode::Stabilizer;

    void setMode(SmoothingMode mode, int strength);

    /// Starts a stroke at pos.
    void reset(QPointF pos, quint64 timestampMs);
    /// Returns the smoothed position for a new input sample.
    QPointF filter(QPointF pos, quint64 timestampMs);
    /// Ends the stroke when the pen lifts at pos. Every mode catches up: returns pos.
    QPointF release(QPointF pos, quint64 timestampMs);

    /// The preset for an old StabilizationLevel (NONE, SIMPLE, STRONG).
    static SmoothingPreset presetForLegacyLevel(int level);

private:
    QPointF weighted(QPointF pos);
    QPointF stabilized(QPointF pos);

    SmoothingMode mMode = SmoothingMode::None;
    int mStrength = 50;

    QPointF mPos;      // last output
    QPointF mVelocity; // filtered, px/s
    quint64 mTime = 0;
    QList<QPointF> mHistory; // Weighted: raw samples, newest last
};

#endif // INPUTSMOOTHER_H
