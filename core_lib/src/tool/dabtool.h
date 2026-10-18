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

#include "dabplacer.h"
#include "pressurecurve.h"
#include "stroketool.h"
#include "tiledbuffer.h"

#include <QPen>
#include <QPolygonF>
#include <QRandomGenerator>

class Layer;

/// The shared press, move and release path of Pencil, Brush and Eraser: it collects pen samples,
/// and paints the dabs DabPlacer places from them.
class DabTool : public StrokeTool
{
    Q_OBJECT

public:
    explicit DabTool(QObject* parent) : StrokeTool(parent) {}

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

    void drawStroke();

    /// Shows the predicted pen path in the canvas preview; it never reaches the layer.
    void setPredictionEnabled(bool enabled) { mPredictionEnabled = enabled; }

    void paint(QPainter& painter, const QRect& blitRect) override;

    /// Restarts the size jitter sequence, so a replay paints the same dabs.
    void seedRandom(quint32 seed) { mRandom.seed(seed); }

protected:
    /// Paints one bitmap dab.
    virtual void drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip) = 0;
    /// The pen of the live vector stroke preview.
    virtual QPen vectorPreviewPen() = 0;
    /// Commits the stroke to a vector layer on release.
    virtual void finishVectorStroke(Layer* layer) = 0;
    virtual void strokeStarted() {}
    virtual void strokeMoved() {}
    /// Whether the canvas preview shows the predicted pen path.
    virtual bool showsPrediction() const { return true; }

    qreal mFeather = 0; // read at each press

    /// Places dabs along these points as one full-pressure stroke, with both tapers.
    /// Call after pointerPressEvent, which reads the stroke settings.
    void dabPath(const QPolygonF& points);

private:
    /// Paints these dabs with the tip read at press.
    void drawDabs(const QList<Dab>& dabs);
    /// Moves the prediction to the current pen state and repaints where it was and is.
    void updatePrediction();
    QRect predictionRect() const;
    // Prints one dab when JUSTANIMATE_PRESSURE_LOG=1.
    void logDab(const Dab& dab);

    DabPlacer mPlacer;
    BrushTip mTip; // read at each press; each dab sets its angle
    PressureCurve mPressureCurve; // read from preferences at each press
    QPointF mMouseDownPoint;
    bool mStrokeEnding = false;
    QRandomGenerator mRandom{ 1 };

    bool mPredictionEnabled = true;
    QLineF mPrediction; // canvas coordinates; null when hidden
    qreal mPredictionWidth = 0;
};
#endif // DABTOOL_H
