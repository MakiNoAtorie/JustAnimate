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
#ifndef PRESSURECURVE_H
#define PRESSURECURVE_H

#include <QList>
#include <QPointF>
#include <QString>

/// Maps pen pressure 0..1 to 0..1 with a monotone cubic through control points.
class PressureCurve
{
public:
    /// The identity curve.
    PressureCurve() : PressureCurve({ { 0, 0 }, { 1, 1 } }) {}
    /// Invalid points (see isValid) give the identity curve.
    explicit PressureCurve(const QList<QPointF>& points);

    /// Parses "x,y x,y ..."; an invalid string gives the identity curve.
    static PressureCurve fromString(const QString& text);
    QString toString() const;

    /// At least two points, all in [0, 1], x strictly increasing.
    static bool isValid(const QList<QPointF>& points);

    const QList<QPointF>& points() const { return mPoints; }
    qreal value(qreal pressure) const;

private:
    QList<QPointF> mPoints;
    QList<qreal> mTangents;
};

#endif // PRESSURECURVE_H
