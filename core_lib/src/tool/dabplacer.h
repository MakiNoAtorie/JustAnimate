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
#ifndef DABPLACER_H
#define DABPLACER_H

#include <QList>
#include <QPointF>
#include <QPolygonF>

#include "pressurecurve.h"

class QRandomGenerator;
struct ToolProperties;

/// One pen sample: canvas position, pressure 0..1, speed in viewport px/ms and tilt in degrees.
struct DabSample
{
    QPointF pos;
    qreal pressure = 1;
    qreal speed = 0;
    QPointF tilt;
};

/// One dab: centre, width, alpha and tip angle (degrees), with the factors that made its width.
struct Dab
{
    QPointF pos;
    qreal width = 0;
    qreal opacity = 0;
    qreal angle = 0;
    qreal pressure = 0;
    qreal taper = 1;
    qreal sizeFactor = 1;
};

/// Places the dabs of one stroke from pen samples: spacing, taper, pressure curve, size dynamics
/// and scatter. No widgets, no pixels.
class DabPlacer
{
public:
    /// The dabs of a whole stroke, from its press sample to its release sample.
    static QList<Dab> place(const QList<DabSample>& samples, const ToolProperties& properties,
                            const PressureCurve& curve, QRandomGenerator& random);

    /// Reads the stroke settings and starts a stroke at this sample. Call once per press.
    void begin(const ToolProperties& properties, const PressureCurve& curve, const DabSample& first,
               QRandomGenerator& random);
    /// Dabs up to this sample. The last TaperEnd px wait until a sample that ends the stroke.
    QList<Dab> add(const DabSample& sample, bool ending = false);
    /// One dab at this sample, for a stroke that did not move.
    Dab tap(const DabSample& sample);
    /// Dabs along these points as one full-pressure stroke with both tapers. Call after begin.
    QList<Dab> path(const QPolygonF& points);
    /// Whether samples wait for the end taper.
    bool hasPending() const { return !mPending.isEmpty(); }

    /// Dab width and opacity at this pen pressure, through the pressure curve; both ignore pressure when it is off.
    qreal width(qreal pressure) const;
    qreal opacity(qreal pressure) const;

private:
    qreal spacing(qreal brushWidth) const;
    /// Alpha of one dab in a spaced line, so the line reaches opacity() at any spacing;
    /// with flow per dab (Classic) it is opacity() and closer spacing builds up darker.
    qreal spacedOpacity(qreal pressure, qreal brushWidth) const;
    /// Width factor from 0 to 1 at this distance along the stroke, for the start and end taper.
    qreal taperAt(qreal distance) const;
    /// The tip angle; with follow direction on it adds the direction of travel.
    qreal angleAt(QPointF direction, QPointF tilt) const;
    /// Size factor from pen speed, tilt and random jitter; draws one random number per call.
    qreal sizeDynamics(qreal speed, QPointF tilt);
    /// A random number in -1..1.
    qreal randomSigned();
    /// Places dabs from the last placed point to b.
    void segment(const DabSample& b, QList<Dab>& dabs);

    // Settings, read at begin().
    PressureCurve mCurve;
    qreal mWidth = 0;
    bool mPressureEnabled = false;
    qreal mFlow = 100;
    bool mFlowPerDab = false;
    bool mWashEnabled = false;
    qreal mOpacity = 100;
    qreal mSpacing = 0;
    qreal mTipAngle = 0;
    bool mFollowDirection = false;
    qreal mTaperStart = 0; // canvas px, 0 is off
    qreal mTaperEnd = 0;
    // In -1..1 (jitter 0..1).
    qreal mSpeedToSize = 0;
    qreal mTiltToSize = 0;
    qreal mTiltToAngle = 0;
    qreal mSizeJitter = 0;
    qreal mScatter = 0; // dab offset as a fraction of dab width
    qreal mAngleJitter = 0; // degrees
    int mDabCount = 1; // dabs per spacing step
    QRandomGenerator* mRandom = nullptr;

    // Stroke state.
    QPointF mLastPoint;
    qreal mLastPressure = 1;
    qreal mDistanceToNextDab = 0;
    qreal mStrokeLength = 0; // to the newest sample
    qreal mBrushDistance = 0; // to mLastPoint
    bool mStrokeEnding = false;
    QList<DabSample> mPending;
};

#endif // DABPLACER_H
