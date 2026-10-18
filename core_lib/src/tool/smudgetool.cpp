/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "smudgetool.h"
#include <QPixmap>

#include "pointerevent.h"
#include "vectorimage.h"
#include "editor.h"
#include "scribblearea.h"

#include "colormanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"

#include "layervector.h"
#include "blitrect.h"

SmudgeTool::SmudgeTool(QObject* parent) : DabTool(parent)
{
}

ToolType SmudgeTool::type() const
{
    return SMUDGE;
}

void SmudgeTool::loadSettings()
{
    StrokeTool::loadSettings();

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);
}

bool SmudgeTool::emptyFrameActionEnabled()
{
    // Disabled till we get it working for vector layers...
    return false;
}

void SmudgeTool::drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip)
{
    SmudgeSettings smudge;
    smudge.length = get(ToolProperty::SmudgeLength) / 100.0f;
    smudge.radius = get(ToolProperty::SmudgeRadius) / 100.0f;
    smudge.colorRate = get(ToolProperty::SmudgeColorRate) / 100.0f;
    smudge.wet = get(ToolProperty::SmudgeMix) == 1;
    mBuffer.dab(point, width, get(ToolProperty::Feather), mEditor->color()->frontColor(), opacity, false, tip, smudge);
}

void SmudgeTool::pointerPressEvent(PointerEvent* event)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }
    if (layer->type() == Layer::BITMAP)
    {
        DabTool::pointerPressEvent(event);
        return;
    }

    mInterpolator.pointerPressEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    auto selectMan = mEditor->select();
    if (event->button() == Qt::LeftButton)
    {
        startStroke(event->inputType());
        if (layer->type() == Layer::VECTOR)
        {
            const int currentFrame = mEditor->currentFrame();
            const float distanceFrom = selectMan->selectionTolerance();
            VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(currentFrame);
            if (vectorImage == nullptr) { return; }
            const QPointF point = canvasToKey().map(getCurrentPoint());
            selectMan->setCurves(vectorImage->getCurvesCloseTo(point, distanceFrom));
            selectMan->setVertices(vectorImage->getVerticesCloseTo(point, distanceFrom));

            if (selectMan->closestCurves().size() > 0 || selectMan->closestCurves().size() > 0)      // the user clicks near a vertex or a curve
            {
                if (event->modifiers() != Qt::ShiftModifier && !vectorImage->isSelected(selectMan->closestVertices()))
                {
                    mEditor->deselectAll();
                }

                vectorImage->setSelected(selectMan->closestVertices(), true);
                selectMan->vectorSelection.add(selectMan->closestCurves());
                selectMan->vectorSelection.add(selectMan->closestVertices());

                emit mEditor->frameModified(mEditor->currentFrame());
            }
            else
            {
                mEditor->deselectAll();
            }
        }
    }

    StrokeTool::pointerPressEvent(event);
}

void SmudgeTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }
    if (layer->type() == Layer::BITMAP)
    {
        DabTool::pointerMoveEvent(event);
        return;
    }

    mInterpolator.pointerMoveEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (layer->type() != Layer::VECTOR)
    {
        return;
    }

    auto selectMan = mEditor->select();
    if (event->inputType() == mCurrentInputType) {
        VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame());
        if (vectorImage == nullptr) { return; }
        if (event->buttons() & Qt::LeftButton)   // the user is also pressing the mouse (dragging)
        {
            if (event->modifiers() != Qt::ShiftModifier)    // (and the user doesn't press shift)
            {
                // transforms the selection
                BlitRect blit;

                // Use the previous dirty bound and extend it with the current dirty bound
                // this ensures that we won't get painting artifacts
                blit.extend(vectorImage->getBoundsOfTransformedCurves().toRect());
                selectMan->setSelectionTransform(QTransform().translate(offsetFromPressPos().x(), offsetFromPressPos().y()));
                vectorImage->setSelectionTransformation(selectMan->selectionTransform());
                blit.extend(vectorImage->getBoundsOfTransformedCurves().toRect());

                // And now tell the widget to update the portion in local coordinates
                mScribbleArea->update(mEditor->view()->mapCanvasToScreen(blit).toRect().adjusted(-1, -1, 1, 1));
            }
        }
        else     // the user is moving the mouse without pressing it
        {
            selectMan->setVertices(vectorImage->getVerticesCloseTo(canvasToKey().map(getCurrentPoint()), selectMan->selectionTolerance()));
            mScribbleArea->update();
        }
    }

    StrokeTool::pointerMoveEvent(event);
}

void SmudgeTool::pointerReleaseEvent(PointerEvent* event)
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return; }
    if (layer->type() == Layer::BITMAP)
    {
        DabTool::pointerReleaseEvent(event);
        return;
    }

    mInterpolator.pointerReleaseEvent(event);
    if (handleQuickSizing(event)) {
        return;
    }

    if (event->inputType() != mCurrentInputType) return;

    if (event->button() == Qt::LeftButton && layer->type() == Layer::VECTOR)
    {
        VectorImage *vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame());
        if (vectorImage == nullptr)
        {
            endStrokeEdit(false);
            return;
        }
        vectorImage->applySelectionTransformation();

        auto selectMan = mEditor->select();
        selectMan->resetSelectionTransform();
        for (int k = 0; k < selectMan->vectorSelection.curve.size(); k++)
        {
            int curveNumber = selectMan->vectorSelection.curve.at(k);
            vectorImage->curve(curveNumber).smoothCurve();
        }
        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
        endStrokeEdit(true);
    }

    StrokeTool::pointerReleaseEvent(event);
}

QPointF SmudgeTool::offsetFromPressPos()
{
    const QTransform toKey = canvasToKey();
    return toKey.map(getCurrentPoint()) - toKey.map(getCurrentPressPoint());
}
