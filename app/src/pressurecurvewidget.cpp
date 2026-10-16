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
#include "pressurecurvewidget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace
{
constexpr qreal kMargin = 6;
constexpr qreal kHandleRadius = 4;
// Neighbouring points keep at least this x gap.
constexpr qreal kMinGap = 0.02;
} // namespace

PressureCurveWidget::PressureCurveWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(120, 120);
    updateToolTip();
}

void PressureCurveWidget::updateToolTip()
{
    setToolTip(isEnabled()
                   ? tr("Drag a point to move it, double-click to add a point, right-click to remove one.")
                   : tr("Uncheck 'Use the global pressure curve' to edit"));
}

void PressureCurveWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange)
        updateToolTip();
}

void PressureCurveWidget::setCurve(const PressureCurve& curve)
{
    mCurve = curve;
    update();
}

QRectF PressureCurveWidget::plotRect() const
{
    return QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
}

QPointF PressureCurveWidget::toWidget(QPointF p) const
{
    const QRectF r = plotRect();
    return QPointF(r.left() + p.x() * r.width(), r.bottom() - p.y() * r.height());
}

QPointF PressureCurveWidget::toCurve(QPointF p) const
{
    const QRectF r = plotRect();
    return QPointF(qBound<qreal>(0, (p.x() - r.left()) / r.width(), 1),
                   qBound<qreal>(0, (r.bottom() - p.y()) / r.height(), 1));
}

int PressureCurveWidget::pointAt(QPointF widgetPoint) const
{
    const QList<QPointF>& points = mCurve.points();
    for (int i = 0; i < points.size(); ++i)
    {
        if (QLineF(toWidget(points[i]), widgetPoint).length() <= kHandleRadius * 2)
            return i;
    }
    return -1;
}

void PressureCurveWidget::setPoints(const QList<QPointF>& points)
{
    if (!PressureCurve::isValid(points))
        return;
    mCurve = PressureCurve(points);
    update();
    emit curveChanged(mCurve);
}

void PressureCurveWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = plotRect();
    const QPalette& pal = palette();

    painter.fillRect(r, pal.base());
    painter.setPen(QPen(pal.mid().color(), 1, Qt::DotLine));
    for (int i = 1; i < 4; ++i)
    {
        const qreal f = i / 4.0;
        painter.drawLine(toWidget(QPointF(f, 0)), toWidget(QPointF(f, 1)));
        painter.drawLine(toWidget(QPointF(0, f)), toWidget(QPointF(1, f)));
    }
    painter.setPen(pal.mid().color());
    painter.drawRect(r);

    QPainterPath path(toWidget(QPointF(0, mCurve.value(0))));
    const int steps = qMax(2, static_cast<int>(r.width()));
    for (int i = 1; i <= steps; ++i)
    {
        const qreal x = i / static_cast<qreal>(steps);
        path.lineTo(toWidget(QPointF(x, mCurve.value(x))));
    }
    const QColor mark = isEnabled() ? pal.highlight().color() : pal.color(QPalette::Disabled, QPalette::Text);
    painter.setPen(QPen(mark, 2));
    painter.drawPath(path);

    painter.setPen(pal.text().color());
    painter.setBrush(mark);
    for (const QPointF& p : mCurve.points())
        painter.drawEllipse(toWidget(p), kHandleRadius, kHandleRadius);
}

void PressureCurveWidget::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled())
        return;
    const int i = pointAt(event->position());
    const QList<QPointF>& points = mCurve.points();
    if (event->button() == Qt::RightButton)
    {
        if (i > 0 && i < points.size() - 1)
        {
            QList<QPointF> next = points;
            next.removeAt(i);
            setPoints(next);
        }
        return;
    }
    if (event->button() == Qt::LeftButton)
        mDragged = i;
}

void PressureCurveWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (mDragged < 0)
        return;
    QList<QPointF> points = mCurve.points();
    QPointF p = toCurve(event->position());
    const int last = points.size() - 1;
    if (mDragged == 0 || mDragged == last)
        p.setX(points[mDragged].x());
    else
        p.setX(qBound(points[mDragged - 1].x() + kMinGap, p.x(), points[mDragged + 1].x() - kMinGap));
    points[mDragged] = p;
    setPoints(points);
}

void PressureCurveWidget::mouseReleaseEvent(QMouseEvent*)
{
    mDragged = -1;
}

void PressureCurveWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!isEnabled())
        return;
    if (pointAt(event->position()) >= 0)
        return;
    const qreal x = toCurve(event->position()).x();
    QList<QPointF> points = mCurve.points();
    for (int i = 1; i < points.size(); ++i)
    {
        if (x > points[i - 1].x() + kMinGap && x < points[i].x() - kMinGap)
        {
            points.insert(i, QPointF(x, mCurve.value(x)));
            setPoints(points);
            return;
        }
    }
}
