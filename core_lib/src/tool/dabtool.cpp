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
    prepareInput(event->viewportPos());
    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    mMouseDownPoint = getCurrentPoint();

    startStroke(event->inputType());
    mPressureCurve = PressureCurve::fromString(pressureCurve().isEmpty() ? mEditor->preference()->get<SETTING::PRESSURE_CURVE>()
                                                                         : pressureCurve());
    mBuffer.tiles().setWashOpacity(get(ToolProperty::WashEnabled) ? get(ToolProperty::Opacity) / 100 : 1.0);
    mFeather = get(ToolProperty::Feather);
    mTip = BrushTip();
    mTip.square = get(ToolProperty::TipSquare);
    mTip.aspect = get(ToolProperty::TipAspect) / 100.0;
    mTip.image = mTipImage;
    mTip.grainTexture = get(ToolProperty::GrainTexture);
    mTip.grainMode = get(ToolProperty::GrainMode);
    mTip.grainScale = get(ToolProperty::GrainScale) / 100.0f;
    mTip.grainContrast = get(ToolProperty::GrainContrast) / 100.0f;
    mTip.grainBrightness = get(ToolProperty::GrainBrightness) / 100.0f;
    mPlacer.begin(properties(), mPressureCurve, { mMouseDownPoint, mCurrentPressure }, mRandom);
    mStrokeEnding = false;
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
    Layer* layer = mEditor->layers()->currentLayer();
    if (distance < 1 && !mPlacer.hasPending())
    {
        if (layer->type() == Layer::BITMAP)
            drawDabs({ mPlacer.tap({ mMouseDownPoint, mCurrentPressure, mInterpolator.speed(), mInterpolator.tilt() }) });
    }
    else
    {
        drawStroke();
    }

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
        mPredictionWidth = mPlacer.width(mCurrentPressure);
    }
    mScribbleArea->update(before | predictionRect());
}

void DabTool::paint(QPainter& painter, const QRect& blitRect)
{
    StrokeTool::paint(painter, blitRect);
    if (mPrediction.isNull()) return;

    QColor color = mEditor->color()->frontColor();
    color.setAlphaF(color.alphaF() * mPlacer.opacity(mCurrentPressure));
    painter.save();
    painter.setTransform(mEditor->view()->getView());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, mPredictionWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(mPrediction);
    painter.restore();
}

void DabTool::logDab(const Dab& dab)
{
    static bool on = qEnvironmentVariableIntValue("JUSTANIMATE_PRESSURE_LOG") == 1;
    if (!on) return;
    const char* input = mCurrentInputType == PointerEvent::Tablet ? "tablet" : mCurrentInputType == PointerEvent::Mouse ? "mouse" : "unknown";
    qDebug("dab input=%s pressure=%.3f curve=%.3f taper=%.3f size=%.3f width=%.2f", input, dab.pressure,
           mPressureCurve.value(dab.pressure), dab.taper, dab.sizeFactor, dab.width);
}

void DabTool::drawDabs(const QList<Dab>& dabs)
{
    for (const Dab& dab : dabs)
    {
        mCurrentWidth = dab.width;
        logDab(dab);
        BrushTip tip = mTip;
        tip.angle = dab.angle;
        drawDab(dab.pos, dab.width, dab.opacity, tip);
    }
    mBuffer.tiles().emitUpdated();
}

void DabTool::dabPath(const QPolygonF& points)
{
    drawDabs(mPlacer.path(points));
}

void DabTool::drawStroke()
{
    StrokeTool::drawStroke();
    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::BITMAP)
    {
        drawDabs(mPlacer.add({ getCurrentPoint(), mCurrentPressure, mInterpolator.speed(), mInterpolator.tilt() }, mStrokeEnding));
    }
    else if (layer->type() == Layer::VECTOR)
    {
        QPen pen = vectorPreviewPen();
        if (p.size() == 4)
        {
            QPainterPath path(p[0]);
            path.cubicTo(p[1], p[2], p[3]);
            drawScreenPath(path, pen);
        }
    }
}
