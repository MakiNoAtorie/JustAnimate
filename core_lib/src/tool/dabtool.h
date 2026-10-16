/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef DABTOOL_H
#define DABTOOL_H

#include "pressurecurve.h"
#include "stroketool.h"
#include "tiledbuffer.h"

#include <QPen>
#include <QPolygonF>
#include <random>

class Layer;

/// The shared press, move, release and dab-placement path of Pencil, Brush and Eraser.
class DabTool : public StrokeTool
{
    Q_OBJECT

public:
    explicit DabTool(QObject* parent) : StrokeTool(parent) {}

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void paintAt(QPointF point);
    void drawStroke();

    /// Shows the predicted pen path in the canvas preview; it never reaches the layer.
    void setPredictionEnabled(bool enabled) { mPredictionEnabled = enabled; }

    void paint(QPainter& painter, const QRect& blitRect) override;

    /// Restarts the size jitter sequence, so a replay paints the same dabs.
    void seedRandom(quint32 seed) { mRandom.seed(seed); }

protected:
    /// Paints one bitmap dab.
    virtual void drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip) = 0;
    /// The pen of the live vector stroke preview.
    virtual QPen vectorPreviewPen() = 0;
    /// Commits the stroke to a vector layer on release.
    virtual void finishVectorStroke(Layer* layer) = 0;
    virtual void strokeStarted() {}
    virtual void strokeMoved() {}
    /// Whether the canvas preview shows the predicted pen path.
    virtual bool showsPrediction() const { return true; }

    /// Distance between dab centres for a dab of this width.
    qreal dabSpacing(qreal brushWidth) const;
    /// Dab width and opacity at this pen pressure, through the pressure curve; both ignore pressure when it is off.
    qreal dabWidth(qreal pressure) const;
    qreal dabOpacity(qreal pressure) const;
    /// Width factor from 0 to 1 at this distance along the stroke, for the start and end taper.
    qreal taperAt(qreal distance) const;
    /// The tip settings; with follow direction on, the angle adds the direction of travel.
    BrushTip dabTip(QPointF direction) const;
    /// Alpha of one dab in a spaced line, so the line reaches dabOpacity at any spacing;
    /// with flow per dab (Classic) it is dabOpacity and closer spacing builds up darker.
    qreal spacedDabOpacity(qreal pressure, qreal brushWidth) const;
    /// Places dabs along these points as one full-pressure stroke, with both tapers.
    /// Call after pointerPressEvent, which reads the stroke settings.
    void dabPath(const QPolygonF& points);

    QPointF mLastBrushPoint{ 0, 0 };
    qreal mLastBrushPressure = 1.0;
    qreal mDistanceToNextDab = 0;
    QPointF mMouseDownPoint;
    PressureCurve mPressureCurve; // read from preferences at each press

private:
    /// A pen point with the pen state its dabs use.
    struct PenPoint
    {
        QPointF pos;
        qreal pressure;
        qreal speed;
        QPointF tilt;
    };
    /// Places dabs from mLastBrushPoint to b.pos, with b's speed and tilt.
    void dabSegment(const PenPoint& b);

    // Taper lengths in canvas px, 0 is off; read from the tool at each press.
    qreal mTaperStart = 0;
    qreal mTaperEnd = 0;
    qreal mStrokeLength = 0; // to the newest pen point
    qreal mBrushDistance = 0; // to mLastBrushPoint
    bool mStrokeEnding = false;
    // Pen points not yet dabbed: the last mTaperEnd px wait until the end of the stroke is known.
    QList<PenPoint> mPendingPoints;
    // Speed and tilt of the dabs being placed: the pen state of their pen point.
    qreal mDabSpeed = 0;
    QPointF mDabTilt;
    /// Moves the prediction to the current pen state and repaints where it was and is.
    void updatePrediction();
    QRect predictionRect() const;
    /// Size factor from pen speed, tilt and random jitter; draws one random number per call.
    qreal sizeDynamics();
    /// A random number in -1..1 from mRandom.
    qreal randomSigned();

    // Preferences in -1..1 (jitter 0..1), read at each press.
    qreal mSpeedToSize = 0;
    qreal mTiltToSize = 0;
    qreal mTiltToAngle = 0;
    qreal mSizeJitter = 0;
    qreal mScatter = 0; // dab offset as a fraction of dab width
    qreal mAngleJitter = 0; // degrees
    int mDabCount = 1; // dabs per spacing step
    std::mt19937 mRandom{ 1 };

    bool mPredictionEnabled = true;
    QLineF mPrediction; // canvas coordinates; null when hidden
    qreal mPredictionWidth = 0;
};

#endif // DABTOOL_H
