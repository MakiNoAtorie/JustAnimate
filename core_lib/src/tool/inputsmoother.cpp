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
#include <QLineF>
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

// Weighted: Gaussian sigma in viewport px. Stabilizer: string length in viewport px.
constexpr qreal kWeightedSigmaPerStrength = 0.4;
constexpr qreal kStringPerStrength = 0.5;

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
    mHistory = { pos };
}

QPointF InputSmoother::filter(QPointF pos, quint64 timestampMs)
{
    // Coalesced events can share a timestamp: count them 1 ms apart.
    const qreal dt = (timestampMs > mTime ? timestampMs - mTime : 1) / 1000.0;
    mTime = qMax(mTime, timestampMs);

    switch (mMode)
    {
    case SmoothingMode::None:
        mPos = pos;
        return mPos;
    case SmoothingMode::Weighted: return weighted(pos);
    case SmoothingMode::Stabilizer: return stabilized(pos);
    case SmoothingMode::OneEuro: break;
    }

    mVelocity += alpha(kVelocityCutoff, dt) * ((pos - mPos) / dt - mVelocity);
    const qreal speed = std::hypot(mVelocity.x(), mVelocity.y());
    const qreal minCutoff = kMinCutoffWeak * std::pow(kMinCutoffStrong / kMinCutoffWeak, mStrength / 100.0);
    mPos += alpha(minCutoff + kBeta * speed, dt) * (pos - mPos);
    return mPos;
}

QPointF InputSmoother::weighted(QPointF pos)
{
    const qreal sigma = qMax(mStrength * kWeightedSigmaPerStrength, 0.01);
    // A pen held still adds no path distance: skip repeats so the history stays bounded.
    if (mHistory.isEmpty() || mHistory.last() != pos) { mHistory.append(pos); }

    // Weigh each sample by a Gaussian of its path distance back from the pen.
    QPointF sum;
    qreal total = 0;
    qreal d = 0;
    int i = mHistory.size() - 1;
    for (; i >= 0; --i)
    {
        if (i + 1 < mHistory.size()) { d += QLineF(mHistory[i], mHistory[i + 1]).length(); }
        if (d > 3 * sigma) { break; }
        const qreal w = std::exp(-d * d / (2 * sigma * sigma));
        sum += w * mHistory[i];
        total += w;
    }
    mHistory.remove(0, i + 1); // samples past 3 sigma add nothing
    mPos = sum / total;
    return mPos;
}

QPointF InputSmoother::stabilized(QPointF pos)
{
    // Pulled string: the output moves only when the pen is further than the string length.
    const qreal radius = mStrength * kStringPerStrength;
    const QPointF d = pos - mPos;
    const qreal len = std::hypot(d.x(), d.y());
    if (len > radius) { mPos += d * ((len - radius) / len); }
    return mPos;
}

QPointF InputSmoother::release(QPointF pos, quint64 timestampMs)
{
    reset(pos, qMax(mTime, timestampMs)); // catch up: the stroke ends at the pen
    return pos;
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
