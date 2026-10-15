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
#include "inputsmoother.h"

#include <cmath>
#include <QtMath>

#include "pencildef.h"

namespace
{

// 1-euro filter tuning (Casiez et al. 2012), in viewport px and Hz.
// Strength moves the minimum cutoff from kMinCutoffWeak to kMinCutoffStrong on a log scale.
constexpr qreal kMinCutoffWeak = 100.0;
constexpr qreal kMinCutoffStrong = 0.5;
constexpr qreal kBeta = 0.01;
constexpr qreal kVelocityCutoff = 1.0;

qreal alpha(qreal cutoffHz, qreal dt)
{
    const qreal tau = 1.0 / (2.0 * M_PI * cutoffHz);
    return 1.0 / (1.0 + tau / dt);
}

} // namespace

void InputSmoother::setMode(SmoothingMode mode, int strength)
{
    mMode = mode;
    mStrength = qBound(0, strength, 100);
}

void InputSmoother::reset(QPointF pos, quint64 timestampMs)
{
    mPos = pos;
    mVelocity = QPointF();
    mTime = timestampMs;
}

QPointF InputSmoother::filter(QPointF pos, quint64 timestampMs)
{
    // Coalesced events can share a timestamp: count them 1 ms apart.
    const qreal dt = (timestampMs > mTime ? timestampMs - mTime : 1) / 1000.0;
    mTime = qMax(mTime, timestampMs);

    if (mMode == SmoothingMode::None)
    {
        mPos = pos;
        return mPos;
    }

    mVelocity += alpha(kVelocityCutoff, dt) * ((pos - mPos) / dt - mVelocity);
    const qreal speed = std::hypot(mVelocity.x(), mVelocity.y());
    const qreal minCutoff = kMinCutoffWeak * std::pow(kMinCutoffStrong / kMinCutoffWeak, mStrength / 100.0);
    mPos += alpha(minCutoff + kBeta * speed, dt) * (pos - mPos);
    return mPos;
}

SmoothingPreset InputSmoother::presetForLegacyLevel(int level)
{
    switch (level)
    {
    case StabilizationLevel::SIMPLE: return { SmoothingMode::OneEuro, 25 };
    case StabilizationLevel::STRONG: return { SmoothingMode::OneEuro, 50 };
    default: return { SmoothingMode::None, 50 };
    }
}
