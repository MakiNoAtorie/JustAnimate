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
#ifndef STROKESNAPPER_H
#define STROKESNAPPER_H

#include <optional>

#include <QLineF>
#include <QList>

/// Locks a stroke to the nearest guide line at press. Guides are infinite lines in the input's coordinates.
class StrokeSnapper
{
public:
    /// Applies from the next begin().
    void setGuides(const QList<QLineF>& guides, qreal tolerance);
    /// Picks the guides within tolerance of the press point. One guide locks now;
    /// several lock on the first point a tolerance away, to the guide nearest that point.
    void begin(const QPointF& press);
    /// The point projected onto the locked guide; unchanged when no guide is near the press.
    QPointF snap(const QPointF& point);

private:
    QList<QLineF> mGuides;
    qreal mTolerance = 0;
    QList<QLineF> mCandidates;
    QPointF mPress;
    std::optional<QLineF> mLocked;
};

#endif // STROKESNAPPER_H