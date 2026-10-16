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
#ifndef PRESSURECURVEWIDGET_H
#define PRESSURECURVEWIDGET_H

#include <QWidget>

#include "pressurecurve.h"

/// Control-point editor for the pressure curve: drag a point to move it,
/// double-click to add one, right-click to remove one. End points move only up and down.
class PressureCurveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PressureCurveWidget(QWidget* parent = nullptr);

    void setCurve(const PressureCurve& curve);
    const PressureCurve& curve() const { return mCurve; }

    QSize sizeHint() const override { return QSize(200, 200); }

signals:
    void curveChanged(const PressureCurve& curve);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void changeEvent(QEvent*) override;

private:
    QRectF plotRect() const;
    QPointF toWidget(QPointF curvePoint) const;
    QPointF toCurve(QPointF widgetPoint) const;
    int pointAt(QPointF widgetPoint) const;
    void updateToolTip();
    void setPoints(const QList<QPointF>& points);

    PressureCurve mCurve;
    int mDragged = -1;
};

#endif // PRESSURECURVEWIDGET_H
