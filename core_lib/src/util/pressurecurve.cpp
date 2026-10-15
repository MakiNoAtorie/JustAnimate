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
#include "pressurecurve.h"

#include <QStringList>
#include <QtMath>

PressureCurve::PressureCurve(const QList<QPointF>& points)
{
    mPoints = isValid(points) ? points : QList<QPointF>{ { 0, 0 }, { 1, 1 } };

    // Fritsch-Carlson tangents keep each segment monotone.
    const int n = mPoints.size();
    QList<qreal> slopes;
    for (int i = 0; i + 1 < n; ++i)
        slopes << (mPoints[i + 1].y() - mPoints[i].y()) / (mPoints[i + 1].x() - mPoints[i].x());

    mTangents.resize(n);
    mTangents[0] = slopes.first();
    mTangents[n - 1] = slopes.last();
    for (int i = 1; i + 1 < n; ++i)
        mTangents[i] = (slopes[i - 1] * slopes[i] <= 0) ? 0 : (slopes[i - 1] + slopes[i]) / 2;

    for (int i = 0; i + 1 < n; ++i)
    {
        if (slopes[i] == 0)
        {
            mTangents[i] = mTangents[i + 1] = 0;
            continue;
        }
        const qreal a = mTangents[i] / slopes[i];
        const qreal b = mTangents[i + 1] / slopes[i];
        const qreal s = a * a + b * b;
        if (s > 9)
        {
            const qreal t = 3 / qSqrt(s);
            mTangents[i] = t * a * slopes[i];
            mTangents[i + 1] = t * b * slopes[i];
        }
    }
}

bool PressureCurve::isValid(const QList<QPointF>& points)
{
    if (points.size() < 2)
        return false;
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF& p = points[i];
        if (!(p.x() >= 0 && p.x() <= 1 && p.y() >= 0 && p.y() <= 1))
            return false;
        if (i > 0 && !(p.x() > points[i - 1].x()))
            return false;
    }
    return true;
}

PressureCurve PressureCurve::fromString(const QString& text)
{
    QList<QPointF> points;
    for (const QString& pair : text.split(' ', Qt::SkipEmptyParts))
    {
        const QStringList xy = pair.split(',');
        bool okX = false, okY = false;
        if (xy.size() == 2)
            points << QPointF(xy[0].toDouble(&okX), xy[1].toDouble(&okY));
        if (!okX || !okY)
            return PressureCurve();
    }
    return PressureCurve(points);
}

QString PressureCurve::toString() const
{
    QStringList pairs;
    for (const QPointF& p : mPoints)
        pairs << QString::number(p.x()) + ',' + QString::number(p.y());
    return pairs.join(' ');
}

qreal PressureCurve::value(qreal pressure) const
{
    const qreal x = qBound(mPoints.first().x(), pressure, mPoints.last().x());
    int i = 0;
    while (i + 2 < mPoints.size() && x > mPoints[i + 1].x())
        ++i;

    const QPointF& p0 = mPoints[i];
    const QPointF& p1 = mPoints[i + 1];
    const qreal h = p1.x() - p0.x();
    const qreal t = (x - p0.x()) / h;
    const qreal t2 = t * t, t3 = t2 * t;
    const qreal y = (2 * t3 - 3 * t2 + 1) * p0.y() + (t3 - 2 * t2 + t) * h * mTangents[i]
                  + (-2 * t3 + 3 * t2) * p1.y() + (t3 - t2) * h * mTangents[i + 1];
    return qBound<qreal>(0, y, 1);
}
