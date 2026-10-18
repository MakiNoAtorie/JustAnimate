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
#include "dabtool.h"

#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include "colormanager.h"
#include "editor.h"
#include "layer.h"
#include "layermanager.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "viewmanager.h"

namespace
{
// One 60 Hz frame ahead, and never further than this many viewport px.
constexpr qreal kPredictionHorizonMs = 16;
constexpr qreal kPredictionMaxPx = 64;
// Pen speed (viewport px/ms) at which speed to size has full effect.
constexpr qreal kFullSpeedPxPerMs = 4;
// Dynamics never shrink a dab below this part of its width; spacing stays sane.
constexpr qreal kMinSizeFactor = 0.1;
} // namespace

void DabTool::pointerPressEvent(PointerEvent* event)
{
    prepareInput(event->viewportPos());
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();
    mLastBrushPoint = getCurrentPoint();

    startStroke(event->inputType());
    mPressureCurve = PressureCurve::fromString(pressureCurve().isEmpty() ? mEditor->preference()->getString(SETTING::PRESSURE_CURVE)
                                                                         : pressureCurve());
    mScribbleArea->setWashOpacity(get(ToolProperty::WashEnabled) ? get(ToolProperty::Opacity) / 100 : 1.0);
    mSpeedToSize = get(ToolProperty::SpeedToSize) / 100.0;
    mTiltToSize = get(ToolProperty::TiltToSize) / 100.0;
    mTiltToAngle = get(ToolProperty::TiltToAngle) / 100.0;
    mSizeJitter = get(ToolProperty::SizeJitter) / 100.0;
    mScatter = get(ToolProperty::Scatter) / 100.0;
    mAngleJitter = get(ToolProperty::AngleJitter);
    mDabCount = qMax(1, get(ToolProperty::DabCount));
    mLastBrushPressure = mCurrentPressure;
    mTaperStart = get(ToolProperty::TaperStart);
    mTaperEnd = get(ToolProperty::TaperEnd);
    mStrokeLength = 0;
    mBrushDistance = 0;
    mStrokeEnding = false;
    mPendingPoints.clear();
    mDistanceToNextDab = dabSpacing(dabWidth(mCurrentPressure) * taperAt(0));
    strokeStarted();

    StrokeTool::pointerPressEvent(event);
}

void DabTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->buttons() & Qt::LeftButton && event->inputType() == mCurrentInputType)
    {
        mCurrentPressure = mInterpolator.getPressure();
        drawStroke();
        strokeMoved();
        updatePrediction();
    }

    StrokeTool::pointerMoveEvent(event);
}

void DabTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);
    updatePrediction();
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->inputType() != mCurrentInputType) return;

    qreal distance = QLineF(getCurrentPoint(), mMouseDownPoint).length();
    mStrokeEnding = true;
    if (distance < 1 && mPendingPoints.isEmpty())
    {
        paintAt(mMouseDownPoint);
    }
    else
    {
        drawStroke();
    }

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::VECTOR) {
        finishVectorStroke(layer);
    }
    endStroke();

    StrokeTool::pointerReleaseEvent(event);
}

QRect DabTool::predictionRect() const
{
    if (mPrediction.isNull()) return QRect();
    const qreal r = mPredictionWidth / 2 + 2;
    const QRectF canvasRect = QRectF(mPrediction.p1(), mPrediction.p2()).normalized().adjusted(-r, -r, r, r);
    return mEditor->view()->mapCanvasToScreen(canvasRect).toAlignedRect();
}

void DabTool::updatePrediction()
{
    const QRect before = predictionRect();
    mPrediction = QLineF();
    if (mPredictionEnabled && mInterpolator.isActive() && showsPrediction() &&
        mEditor->layers()->currentLayer()->type() == Layer::BITMAP)
    {
        const QPointF ahead = mEditor->view()->mapScreenToCanvas(
            mInterpolator.predictedPixel(kPredictionHorizonMs, kPredictionMaxPx));
        mPrediction = QLineF(getCurrentPoint(), ahead);
        mPredictionWidth = dabWidth(mCurrentPressure);
    }
    mScribbleArea->update(before | predictionRect());
}

void DabTool::paint(QPainter& painter, const QRect& blitRect)
{
    StrokeTool::paint(painter, blitRect);
    if (mPrediction.isNull()) return;

    QColor color = mEditor->color()->frontColor();
    color.setAlphaF(color.alphaF() * dabOpacity(mCurrentPressure));
    painter.save();
    painter.setTransform(mEditor->view()->getView());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, mPredictionWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(mPrediction);
    painter.restore();
}

qreal DabTool::dabSpacing(qreal brushWidth) const
{
    return qMax(0.25, brushWidth * (get(ToolProperty::Spacing) / 100.0));
}

qreal DabTool::dabWidth(qreal pressure) const
{
    return get(ToolProperty::Width) * (get(ToolProperty::PressureEnabled) ? mPressureCurve.value(pressure) : 1.0);
}

qreal DabTool::dabOpacity(qreal pressure) const
{
    // Flow per dab with pressure off gives full flow, like the tools before v0.3.
    const qreal flow = get(ToolProperty::PressureEnabled) ? get(ToolProperty::Flow) / 100 * mPressureCurve.value(pressure)
                                           : (get(ToolProperty::FlowPerDab) ? 1.0 : get(ToolProperty::Flow) / 100);
    return get(ToolProperty::WashEnabled) ? flow : flow * get(ToolProperty::Opacity) / 100;
}

qreal DabTool::spacedDabOpacity(qreal pressure, qreal brushWidth) const
{
    if (get(ToolProperty::FlowPerDab))
        return dabOpacity(pressure);
    // About width / spacing dabs cover each pixel; this alpha makes them add up to dabOpacity.
    const qreal overlap = brushWidth > 0 ? qMin(1.0, dabSpacing(brushWidth) / brushWidth) : 1.0;
    return 1 - std::pow(1 - dabOpacity(pressure), overlap);
}

qreal DabTool::taperAt(qreal distance) const
{
    qreal f = 1;
    if (mTaperStart > 0) f = qMin(f, distance / mTaperStart);
    if (mTaperEnd > 0 && mStrokeEnding) f = qMin(f, (mStrokeLength - distance) / mTaperEnd);
    return qBound(0.0, f, 1.0);
}

BrushTip DabTool::dabTip(QPointF direction) const
{
    BrushTip tip;
    tip.square = get(ToolProperty::TipSquare);
    tip.aspect = get(ToolProperty::TipAspect) / 100.0;
    tip.angle = get(ToolProperty::TipAngle);
    tip.image = mTipImage;
    tip.grainTexture = get(ToolProperty::GrainTexture);
    tip.grainMode = get(ToolProperty::GrainMode);
    tip.grainScale = get(ToolProperty::GrainScale) / 100.0f;
    tip.grainContrast = get(ToolProperty::GrainContrast) / 100.0f;
    tip.grainBrightness = get(ToolProperty::GrainBrightness) / 100.0f;
    // Canvas y points down, so the angle counts counter-clockwise on screen.
    if (get(ToolProperty::TipFollowDirection) && !direction.isNull())
        tip.angle += qRadiansToDegrees(std::atan2(-direction.y(), direction.x()));
    // Tablet y tilt points toward the user (down), like canvas y.
    if (mTiltToAngle != 0 && !mDabTilt.isNull())
        tip.angle += mTiltToAngle * qRadiansToDegrees(std::atan2(-mDabTilt.y(), mDabTilt.x()));
    return tip;
}

qreal DabTool::sizeDynamics()
{
    // Speed reaches full effect at kFullSpeedPxPerMs; tilt at 60 degrees.
    const qreal speed = qMin(mDabSpeed / kFullSpeedPxPerMs, 1.0);
    const QPointF tiltDeg = mDabTilt;
    const qreal tilt = qMin(std::sqrt(tiltDeg.x() * tiltDeg.x() + tiltDeg.y() * tiltDeg.y()) / 60.0, 1.0);
    // mt19937 raw output only: std distributions differ between platforms.
    const qreal random = (mRandom() % 10001) / 10000.0;
    const qreal factor = (1 + mSpeedToSize * speed) * (1 + mTiltToSize * tilt) * (1 - mSizeJitter * random);
    return qMax(factor, kMinSizeFactor);
}

void DabTool::logDab(qreal pressure, qreal taper, qreal sizeFactor, qreal width)
{
    static bool on = qEnvironmentVariableIntValue("JUSTANIMATE_PRESSURE_LOG") == 1;
    if (!on) return;
    const char* input = mCurrentInputType == PointerEvent::Tablet ? "tablet" : mCurrentInputType == PointerEvent::Mouse ? "mouse" : "unknown";
    qDebug("dab input=%s pressure=%.3f curve=%.3f taper=%.3f size=%.3f width=%.2f", input, pressure, mPressureCurve.value(pressure), taper, sizeFactor, width);
}

qreal DabTool::randomSigned()
{
    // mt19937 raw output only: std distributions differ between platforms.
    return (mRandom() % 20001) / 10000.0 - 1.0;
}

void DabTool::paintAt(QPointF point)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        mDabSpeed = mInterpolator.speed();
        mDabTilt = mInterpolator.tilt();
        const qreal factor = sizeDynamics();
        mCurrentWidth = dabWidth(mCurrentPressure) * factor;
        logDab(mCurrentPressure, 1, factor, mCurrentWidth);
        drawDab(point, mCurrentWidth, dabOpacity(mCurrentPressure), dabTip(QPointF()));
    }
}

void DabTool::dabSegment(const PenPoint& b)
{
    const qreal startPressure = mLastBrushPressure;
    const QPointF a = mLastBrushPoint;
    const qreal length = QLineF(a, b.pos).length();
    mDabSpeed = b.speed;
    mDabTilt = b.tilt;

    // Carry the leftover distance to the next event, so spacing does not depend on the event rate.
    qreal t = mDistanceToNextDab;
    const BrushTip tip = dabTip(b.pos - a);
    while (t <= length)
    {
        const qreal f = (length > 0) ? t / length : 1.0;
        const qreal pressure = startPressure + (b.pressure - startPressure) * f;
        const qreal taper = taperAt(mBrushDistance + t);
        const qreal baseWidth = dabWidth(pressure) * taper;
        qreal brushWidth = baseWidth;
        for (int i = 0; i < mDabCount; ++i)
        {
            const qreal factor = sizeDynamics();
            brushWidth = baseWidth * factor;
            mCurrentWidth = brushWidth;
            logDab(pressure, taper, factor, brushWidth);
            BrushTip scattered = tip;
            QPointF offset;
            // Draw random numbers only for scatter that is on, so 0 scatter keeps the jitter sequence.
            if (mScatter > 0)
                offset = QPointF(randomSigned(), randomSigned()) * mScatter * baseWidth;
            if (mAngleJitter > 0)
                scattered.angle += randomSigned() * mAngleJitter;
            drawDab(a + (b.pos - a) * f + offset, brushWidth, spacedDabOpacity(pressure, brushWidth), scattered);
        }
        t += dabSpacing(brushWidth);
    }
    mDistanceToNextDab = t - length;
    mBrushDistance += length;
    mLastBrushPoint = b.pos;
    mLastBrushPressure = b.pressure;
}

void DabTool::dabPath(const QPolygonF& points)
{
    if (points.isEmpty()) return;
    mStrokeLength = 0;
    for (int i = 1; i < points.size(); ++i)
        mStrokeLength += QLineF(points[i - 1], points[i]).length();
    mStrokeEnding = true;
    mBrushDistance = 0;
    mDistanceToNextDab = 0;
    mLastBrushPoint = points.first();
    mLastBrushPressure = 1;
    for (int i = 1; i < points.size(); ++i)
        dabSegment({ points[i], 1, 0, QPointF() });
}

void DabTool::drawStroke()
{
    StrokeTool::drawStroke();
    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::BITMAP)
    {
        const QPointF b = getCurrentPoint();
        mStrokeLength += QLineF(mPendingPoints.isEmpty() ? mLastBrushPoint : mPendingPoints.last().pos, b).length();
        mPendingPoints.append({ b, mCurrentPressure, mInterpolator.speed(), mInterpolator.tilt() });

        // Dab up to the point where the end taper can start; all of it once the stroke ends.
        const qreal limit = mStrokeEnding ? mStrokeLength : mStrokeLength - mTaperEnd;
        while (!mPendingPoints.isEmpty())
        {
            const PenPoint point = mPendingPoints.first();
            const qreal length = QLineF(mLastBrushPoint, point.pos).length();
            if (mBrushDistance + length <= limit)
            {
                dabSegment(point);
                mPendingPoints.removeFirst();
                continue;
            }
            if (limit > mBrushDistance)
            {
                const qreal f = (limit - mBrushDistance) / length;
                dabSegment({ mLastBrushPoint + (point.pos - mLastBrushPoint) * f,
                             mLastBrushPressure + (point.pressure - mLastBrushPressure) * f, point.speed, point.tilt });
            }
            break;
        }
    }
    else if (layer->type() == Layer::VECTOR)
    {
        QPen pen = vectorPreviewPen();
        if (p.size() == 4)
        {
            QPainterPath path(p[0]);
            path.cubicTo(p[1], p[2], p[3]);
            mScribbleArea->drawPath(path, pen, Qt::NoBrush, QPainter::CompositionMode_Source);
        }
    }
}
