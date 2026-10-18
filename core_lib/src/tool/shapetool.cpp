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
#include "shapetool.h"

#include <cmath>

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "beziercurve.h"
#include "colormanager.h"
#include "editor.h"
#include "layermanager.h"
#include "layervector.h"
#include "pointerevent.h"
#include "scribblearea.h"
#include "vectorimage.h"
#include "viewmanager.h"

ShapeTool::ShapeTool(QObject* parent) : BrushTool(parent)
{
    // A new mode drops the polygon being drawn.
    connect(this, &StrokeTool::propertyChanged, this, [this](int id) {
        if (id == ShapeToolProperties::SHAPEMODE_VALUE) { clearToolData(); }
    });
}

ToolType ShapeTool::type() const
{
    return SHAPE;
}

void ShapeTool::loadSettings()
{
    BrushTool::loadSettings();

    // Shift and Alt shape the outline; quick sizing would take them.
    mQuickSizingProperties.clear();
}

QCursor ShapeTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

ShapeTool::Mode ShapeTool::shapeMode() const
{
    return static_cast<Mode>(get(ToolProperty::ShapeMode));
}

QPolygonF ShapeTool::outline(Mode mode, QPointF from, QPointF to, Qt::KeyboardModifiers modifiers)
{
    QPointF d = to - from;
    if (modifiers & Qt::ShiftModifier)
    {
        if (mode == LINE)
        {
            QLineF line(QPointF(), d);
            line.setAngle(std::round(line.angle() / 45) * 45);
            d = line.p2();
        }
        else
        {
            const qreal side = qMax(qAbs(d.x()), qAbs(d.y()));
            d = QPointF(std::copysign(side, d.x()), std::copysign(side, d.y()));
        }
    }
    const QPointF a = (modifiers & Qt::AltModifier) ? from - d : from;
    const QPointF b = from + d;
    if (mode == LINE)
        return QPolygonF({ a, b });

    const QRectF rect = QRectF(a, b).normalized();
    if (mode == RECTANGLE)
        return QPolygonF(rect);
    QPainterPath path;
    path.addEllipse(rect);
    return path.toSubpathPolygons().value(0);
}

QRect ShapeTool::outlineScreenRect() const
{
    const qreal r = get(ToolProperty::Width) / 2 + 2;
    return mEditor->view()->mapCanvasToScreen(mOutline.boundingRect().adjusted(-r, -r, r, r)).toAlignedRect();
}

void ShapeTool::setOutline(const QPolygonF& outline)
{
    const QRect before = outlineScreenRect();
    mOutline = outline;
    mScribbleArea->update(before | outlineScreenRect());
}

void ShapeTool::pointerPressEvent(PointerEvent* event)
{
    const Layer::LAYER_TYPE layerType = mEditor->layers()->currentLayer()->type();
    if (event->button() != Qt::LeftButton || (layerType != Layer::BITMAP && layerType != Layer::VECTOR))
        return;

    if (shapeMode() == POLYGON)
    {
        // The stroke and its undo state start only on the closing click.
        mInterpolator.pointerPressEvent(event);
        const QPointF p = getCurrentPoint();
        if (!mDrawing)
        {
            mDrawing = true;
            mPolygon = QPolygonF({ p });
            emit isActiveChanged(SHAPE, true);
            setOutline(mPolygon);
            return;
        }
        const QPointF d = mEditor->view()->mapCanvasToScreen(p) - mEditor->view()->mapCanvasToScreen(mPolygon[0]);
        if (mPolygon.size() >= 3 && d.manhattanLength() < 10)
            finishShape(mPolygon, event);
        else
            setOutline(mPolygon << p);
        return;
    }

    // Starts the stroke: empty-frame handling, undo state and brush settings.
    DabTool::pointerPressEvent(event);
    mDrawing = true;
    mFrom = getCurrentPoint();
    setOutline(outline(shapeMode(), mFrom, mFrom, event->modifiers()));
}

void ShapeTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (mDrawing && shapeMode() == POLYGON)
        setOutline(QPolygonF(mPolygon) << getCurrentPoint());
    else if (mDrawing)
        setOutline(outline(shapeMode(), mFrom, getCurrentPoint(), event->modifiers()));
    StrokeTool::pointerMoveEvent(event);
}

void ShapeTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);
    if (!mDrawing) return;
    if (shapeMode() == POLYGON) // the shape ends on its closing click
    {
        StrokeTool::pointerReleaseEvent(event);
        return;
    }
    finishShape(outline(shapeMode(), mFrom, getCurrentPoint(), event->modifiers()), nullptr);
    StrokeTool::pointerReleaseEvent(event);
}

void ShapeTool::pointerDoubleClickEvent(PointerEvent* event)
{
    if (mDrawing && shapeMode() == POLYGON)
        finishShape(mPolygon, event);
    else
        pointerPressEvent(event);
}

void ShapeTool::finishShape(const QPolygonF& points, PointerEvent* event)
{
    QPolygonF shape = points;
    if (shapeMode() == POLYGON)
    {
        shape << shape.first();
        clearToolData();
        DabTool::pointerPressEvent(event);
    }
    mDrawing = false;
    setOutline(QPolygonF());

    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::BITMAP)
    {
        dabPath(shape);
    }
    else if (layer->type() == Layer::VECTOR)
    {
        VectorImage* image = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame());
        if (image != nullptr) // null if the frame was deleted while drawing
        {
            BezierCurve curve(shape, shapeMode() == ELLIPSE);
            curve.setWidth(get(ToolProperty::InvisibilityEnabled) ? 0 : get(ToolProperty::Width));
            curve.setFeather(get(ToolProperty::Feather));
            curve.setFilled(false);
            curve.setInvisibility(get(ToolProperty::InvisibilityEnabled));
            curve.setVariableWidth(false);
            curve.setColorNumber(mEditor->color()->frontColorNumber());
            image->deselectAll();
            mapCurveToKey(curve);
            image->addCurve(curve, mEditor->view()->scaling(), false);
        }
    }
    endStroke();
}

bool ShapeTool::enteringThisTool()
{
    BrushTool::enteringThisTool();
    mActiveConnections.append(connect(mEditor->layers(), &LayerManager::currentLayerChanged, this, &ShapeTool::clearToolData));
    return true;
}

bool ShapeTool::leavingThisTool()
{
    clearToolData();
    return BrushTool::leavingThisTool();
}

void ShapeTool::clearToolData()
{
    if (mPolygon.isEmpty()) return;
    mDrawing = false;
    mPolygon.clear();
    setOutline(QPolygonF());
    emit isActiveChanged(SHAPE, false);
}

bool ShapeTool::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && !mPolygon.isEmpty())
    {
        clearToolData();
        return true;
    }
    // Alt draws from the centre here, so it does not switch to the eyedropper.
    if (event->key() == Qt::Key_Alt)
        return false;
    return BrushTool::keyPressEvent(event);
}

void ShapeTool::paint(QPainter& painter, const QRect& blitRect)
{
    BrushTool::paint(painter, blitRect);
    if (mOutline.isEmpty()) return;

    painter.save();
    painter.setTransform(mEditor->view()->getView());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(mEditor->color()->frontColor(), get(ToolProperty::Width), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(mOutline);
    painter.restore();
}
