#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <cmath>
#include <QtMath>
#include <QPoint>

namespace MathUtils
{
    /** Get the angle from the difference vector a->b to the x-axis.
     *
     * \param a Start point of vector
     * \param b End point of vector
     * \return Angle in radians from [-pi,+pi]
     */
    inline qreal getDifferenceAngle(const QPointF a, const QPointF b)
    {
        return qAtan2(b.y() - a.y(), b.x() - a.x());
    }

    /** Map one range onto another
     *  \param x The input value
     *  \param inputMin The input min value
     *  \param inputMax The input max value
     *  \param outputMin The output min value
     *  \param outputMax The output max value
     *  \return The value of x mapped to the corresponding range between outputMin and outputMax
     */
    inline qreal map(qreal x, qreal inputMin, qreal inputMax, qreal outputMin, qreal outputMax)
    {
        qreal slope = (outputMax - outputMin) / (inputMax - inputMin);
        return outputMin + slope * (x - inputMin);
    }

    /** Normalize x to a value between 0 and 1;
    *  \param x The input value
    *  \param min The input min value
    *  \param max The input max value
    *  \return The value of x normalized to a range between 0 and 1
    */
    inline qreal normalize(qreal x, qreal min, qreal max)
    {
        return qAbs((x - max) / (min - max));
    }

    /// The slice under `offset` (px from the ring centre) in a ring of `count` slices,
    /// slice 0 at the top, then clockwise; -1 inside `inner` or outside `outer`.
    inline int radialSliceAt(QPointF offset, int count, qreal inner, qreal outer)
    {
        const qreal r = std::hypot(offset.x(), offset.y());
        if (count <= 0 || r < inner || r > outer) { return -1; }
        // Degrees clockwise from up; screen y points down.
        qreal angle = qRadiansToDegrees(std::atan2(offset.x(), -offset.y()));
        const qreal step = 360.0 / count;
        angle = std::fmod(angle + step / 2 + 360.0, 360.0);
        return qMin(static_cast<int>(angle / step), count - 1);
    }
}

#endif // MATHUTILS_H
