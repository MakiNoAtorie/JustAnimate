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

#include <QPainter>
#include <QPainterPath>

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
} // namespace

void DabTool::pointerPressEvent(PointerEvent* event)
{
    applySmoothing();
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();
    mLastBrushPoint = getCurrentPoint();

    startStroke(event->inputType());
    mPressureCurve = PressureCurve::fromString(mEditor->preference()->getString(SETTING::PRESSURE_CURVE));
    mLastBrushPressure = mCurrentPressure;
    mTaperStart = mEditor->preference()->getInt(SETTING::TAPER_START);
    mTaperEnd = mEditor->preference()->getInt(SETTING::TAPER_END);
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
    return qMax(0.25, brushWidth * (strokeToolProperties().spacing() / 100.0));
}

qreal DabTool::dabWidth(qreal pressure) const
{
    return strokeToolProperties().width() * (strokeToolProperties().pressureEnabled() ? mPressureCurve.value(pressure) : 1.0);
}

qreal DabTool::dabOpacity(qreal pressure) const
{
    return strokeToolProperties().pressureEnabled() ? mPressureCurve.value(pressure) * 0.5 : 1.0;
}

qreal DabTool::taperAt(qreal distance) const
{
    qreal f = 1;
    if (mTaperStart > 0) f = qMin(f, distance / mTaperStart);
    if (mTaperEnd > 0 && mStrokeEnding) f = qMin(f, (mStrokeLength - distance) / mTaperEnd);
    return qBound(0.0, f, 1.0);
}

void DabTool::paintAt(QPointF point)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        mCurrentWidth = dabWidth(mCurrentPressure);
        drawDab(point, mCurrentWidth, dabOpacity(mCurrentPressure));
    }
}

void DabTool::dabSegment(QPointF b, qreal endPressure)
{
    const qreal startPressure = mLastBrushPressure;
    const QPointF a = mLastBrushPoint;
    const qreal length = QLineF(a, b).length();

    // Carry the leftover distance to the next event, so spacing does not depend on the event rate.
    qreal t = mDistanceToNextDab;
    while (t <= length)
    {
        const qreal f = (length > 0) ? t / length : 1.0;
        const qreal pressure = startPressure + (endPressure - startPressure) * f;
        const qreal brushWidth = dabWidth(pressure) * taperAt(mBrushDistance + t);
        mCurrentWidth = brushWidth;
        drawDab(a + (b - a) * f, brushWidth, dabOpacity(pressure));
        t += dabSpacing(brushWidth);
    }
    mDistanceToNextDab = t - length;
    mBrushDistance += length;
    mLastBrushPoint = b;
    mLastBrushPressure = endPressure;
}

void DabTool::drawStroke()
{
    StrokeTool::drawStroke();
    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::BITMAP)
    {
        const QPointF b = getCurrentPoint();
        mStrokeLength += QLineF(mPendingPoints.isEmpty() ? mLastBrushPoint : mPendingPoints.last().first, b).length();
        mPendingPoints.append({ b, mCurrentPressure });

        // Dab up to the point where the end taper can start; all of it once the stroke ends.
        const qreal limit = mStrokeEnding ? mStrokeLength : mStrokeLength - mTaperEnd;
        while (!mPendingPoints.isEmpty())
        {
            const auto [point, pressure] = mPendingPoints.first();
            const qreal length = QLineF(mLastBrushPoint, point).length();
            if (mBrushDistance + length <= limit)
            {
                dabSegment(point, pressure);
                mPendingPoints.removeFirst();
                continue;
            }
            if (limit > mBrushDistance)
            {
                const qreal f = (limit - mBrushDistance) / length;
                dabSegment(mLastBrushPoint + (point - mLastBrushPoint) * f,
                           mLastBrushPressure + (pressure - mLastBrushPressure) * f);
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
