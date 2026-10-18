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
#include "dabplacer.h"

#include <cmath>

#include <QLineF>
#include <QRandomGenerator>
#include <QtMath>

#include "toolproperties.h"

namespace
{
// Pen speed (viewport px/ms) at which speed to size has full effect.
constexpr qreal kFullSpeedPxPerMs = 4;
// Dynamics never shrink a dab below this part of its width; spacing stays sane.
constexpr qreal kMinSizeFactor = 0.1;
} // namespace

QList<Dab> DabPlacer::place(const QList<DabSample>& samples, const ToolProperties& properties,
                            const PressureCurve& curve, QRandomGenerator& random)
{
    if (samples.isEmpty()) return {};
    DabPlacer placer;
    const DabSample& first = samples.first();
    placer.begin(properties, curve, first, random);
    QList<Dab> dabs;
    for (int i = 1; i + 1 < samples.size(); ++i)
        dabs += placer.add(samples[i]);
    // Same rule as the release of a Dab tool stroke.
    const DabSample& last = samples.last();
    if (samples.size() == 1 || (QLineF(last.pos, first.pos).length() < 1 && !placer.hasPending()))
        dabs.append(placer.tap({ first.pos, last.pressure, last.speed, last.tilt }));
    else
        dabs += placer.add(last, true);
    return dabs;
}

void DabPlacer::begin(const ToolProperties& p, const PressureCurve& curve, const DabSample& first, QRandomGenerator& random)
{
    mCurve = curve;
    mWidth = p.get(ToolProperty::Width);
    mPressureEnabled = p.get(ToolProperty::PressureEnabled);
    mFlow = p.get(ToolProperty::Flow);
    mFlowPerDab = p.get(ToolProperty::FlowPerDab);
    mWashEnabled = p.get(ToolProperty::WashEnabled);
    mOpacity = p.get(ToolProperty::Opacity);
    mSpacing = p.get(ToolProperty::Spacing);
    mTipAngle = p.get(ToolProperty::TipAngle);
    mFollowDirection = p.get(ToolProperty::TipFollowDirection);
    mTaperStart = p.get(ToolProperty::TaperStart);
    mTaperEnd = p.get(ToolProperty::TaperEnd);
    mSpeedToSize = p.get(ToolProperty::SpeedToSize) / 100.0;
    mTiltToSize = p.get(ToolProperty::TiltToSize) / 100.0;
    mTiltToAngle = p.get(ToolProperty::TiltToAngle) / 100.0;
    mSizeJitter = p.get(ToolProperty::SizeJitter) / 100.0;
    mScatter = p.get(ToolProperty::Scatter) / 100.0;
    mAngleJitter = p.get(ToolProperty::AngleJitter);
    mDabCount = qMax(1, p.get(ToolProperty::DabCount));
    mRandom = &random;

    mLastPoint = first.pos;
    mLastPressure = first.pressure;
    mStrokeLength = 0;
    mBrushDistance = 0;
    mStrokeEnding = false;
    mPending.clear();
    mDistanceToNextDab = spacing(width(first.pressure) * taperAt(0));
}

qreal DabPlacer::spacing(qreal brushWidth) const
{
    return qMax(0.25, brushWidth * (mSpacing / 100.0));
}

qreal DabPlacer::width(qreal pressure) const
{
    return mWidth * (mPressureEnabled ? mCurve.value(pressure) : 1.0);
}

qreal DabPlacer::opacity(qreal pressure) const
{
    // Flow per dab with pressure off gives full flow, like the tools before v0.3.
    const qreal flow = mPressureEnabled ? mFlow / 100 * mCurve.value(pressure) : (mFlowPerDab ? 1.0 : mFlow / 100);
    return mWashEnabled ? flow : flow * mOpacity / 100;
}

qreal DabPlacer::spacedOpacity(qreal pressure, qreal brushWidth) const
{
    if (mFlowPerDab)
        return opacity(pressure);
    // About width / spacing dabs cover each pixel; this alpha makes them add up to opacity().
    const qreal overlap = brushWidth > 0 ? qMin(1.0, spacing(brushWidth) / brushWidth) : 1.0;
    return 1 - std::pow(1 - opacity(pressure), overlap);
}

qreal DabPlacer::taperAt(qreal distance) const
{
    qreal f = 1;
    if (mTaperStart > 0) f = qMin(f, distance / mTaperStart);
    if (mTaperEnd > 0 && mStrokeEnding) f = qMin(f, (mStrokeLength - distance) / mTaperEnd);
    return qBound(0.0, f, 1.0);
}

qreal DabPlacer::angleAt(QPointF direction, QPointF tilt) const
{
    qreal angle = mTipAngle;
    // Canvas y points down, so the angle counts counter-clockwise on screen.
    if (mFollowDirection && !direction.isNull())
        angle += qRadiansToDegrees(std::atan2(-direction.y(), direction.x()));
    // Tablet y tilt points toward the user (down), like canvas y.
    if (mTiltToAngle != 0 && !tilt.isNull())
        angle += mTiltToAngle * qRadiansToDegrees(std::atan2(-tilt.y(), tilt.x()));
    return angle;
}

qreal DabPlacer::sizeDynamics(qreal dabSpeed, QPointF tiltDeg)
{
    // Speed reaches full effect at kFullSpeedPxPerMs; tilt at 60 degrees.
    const qreal speed = qMin(dabSpeed / kFullSpeedPxPerMs, 1.0);
    const qreal tilt = qMin(std::sqrt(tiltDeg.x() * tiltDeg.x() + tiltDeg.y() * tiltDeg.y()) / 60.0, 1.0);
    // Raw generator output only: std distributions differ between platforms.
    const qreal random = (mRandom->generate() % 10001) / 10000.0;
    const qreal factor = (1 + mSpeedToSize * speed) * (1 + mTiltToSize * tilt) * (1 - mSizeJitter * random);
    return qMax(factor, kMinSizeFactor);
}

qreal DabPlacer::randomSigned()
{
    // Raw generator output only: std distributions differ between platforms.
    return (mRandom->generate() % 20001) / 10000.0 - 1.0;
}

Dab DabPlacer::tap(const DabSample& s)
{
    const qreal factor = sizeDynamics(s.speed, s.tilt);
    return { s.pos, width(s.pressure) * factor, opacity(s.pressure), angleAt(QPointF(), s.tilt), s.pressure, 1, factor };
}

void DabPlacer::segment(const DabSample& b, QList<Dab>& dabs)
{
    const qreal startPressure = mLastPressure;
    const QPointF a = mLastPoint;
    const qreal length = QLineF(a, b.pos).length();

    // Carry the leftover distance to the next sample, so spacing does not depend on the event rate.
    qreal t = mDistanceToNextDab;
    const qreal angle = angleAt(b.pos - a, b.tilt);
    while (t <= length)
    {
        const qreal f = (length > 0) ? t / length : 1.0;
        const qreal pressure = startPressure + (b.pressure - startPressure) * f;
        const qreal taper = taperAt(mBrushDistance + t);
        const qreal baseWidth = width(pressure) * taper;
        qreal brushWidth = baseWidth;
        for (int i = 0; i < mDabCount; ++i)
        {
            const qreal factor = sizeDynamics(b.speed, b.tilt);
            brushWidth = baseWidth * factor;
            QPointF offset;
            qreal dabAngle = angle;
            // Draw random numbers only for scatter that is on, so 0 scatter keeps the jitter sequence.
            if (mScatter > 0)
                offset = QPointF(randomSigned(), randomSigned()) * mScatter * baseWidth;
            if (mAngleJitter > 0)
                dabAngle += randomSigned() * mAngleJitter;
            dabs.append({ a + (b.pos - a) * f + offset, brushWidth, spacedOpacity(pressure, brushWidth), dabAngle,
                          pressure, taper, factor });
        }
        t += spacing(brushWidth);
    }
    mDistanceToNextDab = t - length;
    mBrushDistance += length;
    mLastPoint = b.pos;
    mLastPressure = b.pressure;
}

QList<Dab> DabPlacer::add(const DabSample& sample, bool ending)
{
    if (ending) mStrokeEnding = true;
    mStrokeLength += QLineF(mPending.isEmpty() ? mLastPoint : mPending.last().pos, sample.pos).length();
    mPending.append(sample);

    // Dab up to the point where the end taper can start; all of it once the stroke ends.
    QList<Dab> dabs;
    const qreal limit = mStrokeEnding ? mStrokeLength : mStrokeLength - mTaperEnd;
    while (!mPending.isEmpty())
    {
        const DabSample point = mPending.first();
        const qreal length = QLineF(mLastPoint, point.pos).length();
        if (mBrushDistance + length <= limit)
        {
            segment(point, dabs);
            mPending.removeFirst();
            continue;
        }
        if (limit > mBrushDistance)
        {
            const qreal f = (limit - mBrushDistance) / length;
            segment({ mLastPoint + (point.pos - mLastPoint) * f, mLastPressure + (point.pressure - mLastPressure) * f,
                      point.speed, point.tilt }, dabs);
        }
        break;
    }
    return dabs;
}

QList<Dab> DabPlacer::path(const QPolygonF& points)
{
    QList<Dab> dabs;
    if (points.isEmpty()) return dabs;
    mStrokeLength = 0;
    for (int i = 1; i < points.size(); ++i)
        mStrokeLength += QLineF(points[i - 1], points[i]).length();
    mStrokeEnding = true;
    mBrushDistance = 0;
    mDistanceToNextDab = 0;
    mLastPoint = points.first();
    mLastPressure = 1;
    for (int i = 1; i < points.size(); ++i)
        segment({ points[i], 1, 0, QPointF() }, dabs);
    return dabs;
}
