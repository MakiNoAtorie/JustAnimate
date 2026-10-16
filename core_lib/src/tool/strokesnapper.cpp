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
#include "strokesnapper.h"

#include <cmath>

namespace
{

QPointF project(const QLineF& line, const QPointF& p)
{
    const QPointF d = line.p2() - line.p1();
    const qreal len2 = QPointF::dotProduct(d, d);
    if (len2 == 0) { return line.p1(); }
    return line.p1() + d * (QPointF::dotProduct(p - line.p1(), d) / len2);
}

} // namespace

void StrokeSnapper::setGuides(const QList<QLineF>& guides, qreal tolerance)
{
    mGuides = guides;
    mTolerance = tolerance;
}

void StrokeSnapper::begin(const QPointF& press)
{
    mLocked.reset();
    qreal best = mTolerance;
    for (const QLineF& guide : mGuides)
    {
        const QPointF off = press - project(guide, press);
        const qreal distance = std::sqrt(QPointF::dotProduct(off, off));
        if (distance <= best)
        {
            best = distance;
            mLocked = guide;
        }
    }
}

QPointF StrokeSnapper::snap(const QPointF& point) const
{
    return mLocked ? project(*mLocked, point) : point;
}