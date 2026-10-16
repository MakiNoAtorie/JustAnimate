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

#include <algorithm>
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

qreal distance(const QLineF& line, const QPointF& p)
{
    const QPointF off = p - project(line, p);
    return std::sqrt(QPointF::dotProduct(off, off));
}

// The guide in `guides` nearest to `p`.
QLineF nearest(const QList<QLineF>& guides, const QPointF& p)
{
    return *std::min_element(guides.begin(), guides.end(), [&p](const QLineF& a, const QLineF& b) {
        return distance(a, p) < distance(b, p);
    });
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
    mCandidates.clear();
    for (const QLineF& guide : mGuides)
    {
        if (distance(guide, press) <= mTolerance) { mCandidates << guide; }
    }
    if (mCandidates.isEmpty()) { return; }
    mPress = project(nearest(mCandidates, press), press);
    if (mCandidates.size() == 1) { mLocked = mCandidates.first(); }
}

QPointF StrokeSnapper::snap(const QPointF& point)
{
    if (!mLocked && !mCandidates.isEmpty())
    {
        // Several guides near the press: wait until the pen shows a direction.
        if (QLineF(mPress, point).length() < mTolerance) { return mPress; }
        mLocked = nearest(mCandidates, point);
    }
    return mLocked ? project(*mLocked, point) : point;
}