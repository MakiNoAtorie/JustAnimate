/***************************************************************************
 * This code is heavily influenced by the instrument proxy from QAquarelle *
 * QAquarelle -   Copyright (C) 2009 by Anton R. <commanderkyle@gmail.com> *
 *                                                                         *
 *   QAquarelle is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "strokeinterpolator.h"

#include <cmath>

#include "pointerevent.h"

void StrokeInterpolator::setPressure(float pressure)
{
    mTabletPressure = pressure;
}

void StrokeInterpolator::pointerPressEvent(PointerEvent* event)
{
    if (!(event->button() == Qt::NoButton)) // if the user is pressing the left/right button
    {
        mCurrentPressPixel = event->viewportPos();
    }

    mLastPixel = mCurrentPixel = event->viewportPos();
    mSmoother.reset(mCurrentPixel, event->timestamp());
    mVelocity = QPointF();
    mTime = event->timestamp();

    mStrokeStarted = true;
    setPressure(event->pressure());
}

void StrokeInterpolator::pointerMoveEvent(PointerEvent* event)
{
    mLastPixel = mCurrentPixel;
    // Hover is not smoothed, so the cursor stays on the pen.
    mCurrentPixel = mStrokeStarted ? mSmoother.filter(event->viewportPos(), event->timestamp())
                                   : event->viewportPos();
    if (mStrokeStarted)
    {
        // Same rule as the smoother: samples that share a timestamp count 1 ms apart.
        const quint64 t = event->timestamp();
        mVelocity = (mCurrentPixel - mLastPixel) / qreal(t > mTime ? t - mTime : 1);
        mTime = qMax(mTime, t);
    }
    if (event->isTabletEvent())
    {
        setPressure(event->pressure());
    }
}

void StrokeInterpolator::pointerReleaseEvent(PointerEvent* event)
{
    // flush out stroke
    if (mStrokeStarted)
    {
        mLastPixel = mCurrentPixel;
        mCurrentPixel = mSmoother.release(event->viewportPos(), event->timestamp());
        if (event->isTabletEvent())
        {
            setPressure(event->pressure());
        }
    }

    mStrokeStarted = false;
}

QPointF StrokeInterpolator::predictedPixel(qreal horizonMs, qreal maxDistance) const
{
    // ponytail: constant-velocity extrapolation; fit acceleration over more samples if it overshoots on curves.
    QPointF ahead = mVelocity * horizonMs;
    const qreal length = std::hypot(ahead.x(), ahead.y());
    if (length > maxDistance)
        ahead *= maxDistance / length;
    return mCurrentPixel + ahead;
}

QList<QPointF> StrokeInterpolator::interpolateStroke()
{
    if (mLastPixel == mCurrentPixel) { return {}; }
    QList<QPointF> points{ mLastPixel, mLastPixel, mCurrentPixel, mCurrentPixel };
    mLastPixel = mCurrentPixel;
    return points;
}
