/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef DABTOOL_H
#define DABTOOL_H

#include "stroketool.h"

#include <QPen>

class Layer;

/// The shared press, move, release and dab-placement path of Pencil, Brush and Eraser.
class DabTool : public StrokeTool
{
    Q_OBJECT

public:
    explicit DabTool(QObject* parent) : StrokeTool(parent) {}

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void paintAt(QPointF point);
    void drawStroke();

protected:
    /// Paints one bitmap dab.
    virtual void drawDab(QPointF point, qreal width, qreal opacity) = 0;
    /// The pen of the live vector stroke preview.
    virtual QPen vectorPreviewPen() = 0;
    /// Commits the stroke to a vector layer on release.
    virtual void finishVectorStroke(Layer* layer) = 0;
    virtual void strokeStarted() {}
    virtual void strokeMoved() {}

    /// Distance between dab centres for a dab of this width.
    static qreal dabSpacing(qreal brushWidth);

    QPointF mLastBrushPoint{ 0, 0 };
    qreal mLastBrushPressure = 1.0;
    qreal mDistanceToNextDab = 0;
    QPointF mMouseDownPoint;
};

#endif // DABTOOL_H
