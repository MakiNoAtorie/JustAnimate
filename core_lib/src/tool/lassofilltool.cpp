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
#include "lassofilltool.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "beziercurve.h"
#include "bitmapimage.h"
#include "colormanager.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "pointerevent.h"
#include "scribblearea.h"
#include "vectorimage.h"
#include "viewmanager.h"

LassoFillTool::LassoFillTool(QObject* parent) : BaseTool(parent)
{
}

QCursor LassoFillTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void LassoFillTool::updateLoopRect()
{
    mScribbleArea->update(mEditor->view()->mapCanvasToScreen(mLoop.boundingRect()).toAlignedRect().adjusted(-2, -2, 2, 2));
}

void LassoFillTool::pointerPressEvent(PointerEvent* event)
{
    const Layer::LAYER_TYPE layerType = mEditor->layers()->currentLayer()->type();
    if (event->button() != Qt::LeftButton || (layerType != Layer::BITMAP && layerType != Layer::VECTOR))
        return;
    mLoop = QPolygonF({ event->canvasPos() });
}

void LassoFillTool::pointerMoveEvent(PointerEvent* event)
{
    if (mLoop.isEmpty()) return;
    mLoop << event->canvasPos();
    updateLoopRect();
}

void LassoFillTool::pointerReleaseEvent(PointerEvent* event)
{
    if (mLoop.isEmpty()) return;
    mLoop << event->canvasPos();
    updateLoopRect();
    if (mLoop.size() >= 3)
        fill();
    mLoop.clear();
}

void LassoFillTool::fill()
{
    // A Drawing added for this fill undoes with the fill in one step.
    Edit step = mEditor->edit(typeName());
    mScribbleArea->handleDrawingOnEmptyFrame();

    Layer* layer = mEditor->layers()->currentLayer();
    const int frame = mEditor->currentFrame();
    Edit drawing = mEditor->edit(layer, frame, typeName());
    if (layer->type() == Layer::BITMAP)
    {
        BitmapImage* image = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(frame);
        QPainterPath path;
        path.addPolygon(mLoop);
        path.closeSubpath();
        const QPainter::CompositionMode mode = static_cast<LayerBitmap*>(layer)->lockAlpha()
            ? QPainter::CompositionMode_SourceAtop : QPainter::CompositionMode_SourceOver;
        if (image != nullptr)
            image->drawPath(path, Qt::NoPen, mEditor->color()->frontColor(), mode, true);
    }
    else if (layer->type() == Layer::VECTOR)
    {
        VectorImage* image = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(frame);
        if (image != nullptr)
        {
            // An invisible closed curve bounds the new area, as with the pencil's fill contour.
            QList<QPointF> points = mLoop;
            points << points.first();
            // Not smoothed: smoothing a closed loop collapses it, as its ends meet.
            BezierCurve curve(points, false);
            curve.setWidth(0);
            curve.setFeather(0);
            curve.setFilled(false);
            curve.setInvisibility(true);
            curve.setVariableWidth(false);
            curve.setColorNumber(mEditor->color()->frontColorNumber());
            mapCurveToKey(curve);
            image->addCurve(curve, qAbs(mEditor->view()->scaling()), false);
            // Use every vertex: fillContour drops the point nearest the first curve's vertex 0.
            const int c = image->getLastCurveNumber();
            QList<VertexRef> vertices;
            for (int i = -1; i < image->curve(c).getVertexSize(); ++i)
                vertices << VertexRef(c, i);
            image->addArea(BezierArea(vertices, mEditor->color()->frontColorNumber()));
        }
    }

    mEditor->setModified(mEditor->currentLayerIndex(), frame);
    drawing.commit();
    step.commit();
}

void LassoFillTool::paint(QPainter& painter, const QRect& blitRect)
{
    Q_UNUSED(blitRect)
    if (mLoop.isEmpty()) return;

    painter.save();
    painter.setTransform(mEditor->view()->getView());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(mEditor->color()->frontColor(), 0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(mLoop);
    painter.restore();
}
