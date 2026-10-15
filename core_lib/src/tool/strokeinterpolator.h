/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef STROKEINTERPOLATOR_H
#define STROKEINTERPOLATOR_H

#include <QPointF>
#include <QList>

#include "inputsmoother.h"

class PointerEvent;

class StrokeInterpolator
{
public:
    void pointerPressEvent(PointerEvent* event);
    void pointerMoveEvent(PointerEvent* event);
    void pointerReleaseEvent(PointerEvent* event);
    void setPressure(float pressure);
    /// Applies from the next press.
    void setSmoothing(SmoothingMode mode, int strength) { mSmoother.setMode(mode, strength); }

    float getPressure() { return mTabletPressure; }
    bool isActive() const { return mStrokeStarted; }

    /// The segment from the last to the current pixel, as cubic Bezier points; empty if it has no length.
    QList<QPointF> interpolateStroke();

    QPointF getCurrentPixel() const { return mCurrentPixel; }
    QPointF getLastPixel() const { return mLastPixel; }
    QPointF getCurrentPressPixel() const { return mCurrentPressPixel; }

private:
    InputSmoother mSmoother;

    QPointF mCurrentPressPixel = { 0, 0 };
    QPointF mCurrentPixel = { 0, 0 };
    QPointF mLastPixel = { 0, 0 };

    bool    mStrokeStarted = false;
    float   mTabletPressure = 1.f;
};

#endif // STROKEINTERPOLATOR_H
