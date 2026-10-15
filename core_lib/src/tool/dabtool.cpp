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

#include <QPainterPath>

#include "editor.h"
#include "layer.h"
#include "layermanager.h"
#include "scribblearea.h"

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
    }

    StrokeTool::pointerMoveEvent(event);
}

void DabTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->inputType() != mCurrentInputType) return;

    qreal distance = QLineF(getCurrentPoint(), mMouseDownPoint).length();
    if (distance < 1)
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

void DabTool::paintAt(QPointF point)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        const StrokeToolProperties& settings = strokeToolProperties();
        qreal pressure = (settings.pressureEnabled()) ? mCurrentPressure : 1.0;
        qreal opacity = (settings.pressureEnabled()) ? (mCurrentPressure * 0.5) : 1.0;
        qreal brushWidth = settings.width() * pressure;
        mCurrentWidth = brushWidth;
        drawDab(point, brushWidth, opacity);
    }
}

void DabTool::drawStroke()
{
    StrokeTool::drawStroke();
    QList<QPointF> p = mInterpolator.interpolateStroke();

    Layer* layer = mEditor->layers()->currentLayer();

    if (layer->type() == Layer::BITMAP)
    {
        const StrokeToolProperties& settings = strokeToolProperties();
        qreal pressure = (settings.pressureEnabled()) ? mCurrentPressure : 1.0;
        qreal opacity = (settings.pressureEnabled()) ? (mCurrentPressure * 0.5) : 1.0;
        qreal brushWidth = settings.width() * pressure;
        mCurrentWidth = brushWidth;

        qreal brushStep = qMax(1.0, (0.5 * brushWidth));

        QPointF a = mLastBrushPoint;
        QPointF b = getCurrentPoint();

        qreal distance = 4 * QLineF(b, a).length();
        int steps = qRound(distance / brushStep);

        for (int i = 0; i < steps; i++)
        {
            QPointF point = mLastBrushPoint + (i + 1) * brushStep * (getCurrentPoint() - mLastBrushPoint) / distance;
            drawDab(point, brushWidth, opacity);

            if (i == (steps - 1))
            {
                mLastBrushPoint = getCurrentPoint();
            }
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
