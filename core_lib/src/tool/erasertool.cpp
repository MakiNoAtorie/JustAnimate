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
#include "erasertool.h"

#include <cmath>
#include <QPixmap>
#include <QPainter>

#include "editor.h"
#include "blitrect.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "layervector.h"
#include "vectorimage.h"
#include "pointerevent.h"


EraserTool::EraserTool(QObject* parent) : DabTool(parent)
{
}

ToolType EraserTool::type() const
{
    return ERASER;
}

void EraserTool::loadSettings()
{
    StrokeTool::loadSettings();

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);
}

void EraserTool::drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip)
{
    // Without feather the dab is solid and ignores opacity.
    const bool feather = get(ToolProperty::FeatherEnabled);
    const QColor dabColor = Qt::white; // palette-exempt: only the eraser dab's alpha counts
    mBuffer.dab(point, width, feather ? get(ToolProperty::Feather) : 0, dabColor, feather ? opacity : 1.0,
                get(ToolProperty::AntiAliasingEnabled) == ON, tip);
}

QPen EraserTool::vectorPreviewPen()
{
    mCurrentWidth = get(ToolProperty::Width);
    if (get(ToolProperty::PressureEnabled))
    {
        mCurrentWidth = (mCurrentWidth + (mInterpolator.getPressure() * mCurrentWidth)) * 0.5;
    }
    return QPen(Qt::white, mCurrentWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

void EraserTool::finishVectorStroke(Layer*)
{
    removeVectorPaint();
}

void EraserTool::removeVectorPaint()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::VECTOR)
    {
        mBuffer.clear();
        VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame());
        if (vectorImage == nullptr) { return; } // Can happen if the first frame is deleted while drawing
        // Clear the area containing the last point
        //vectorImage->removeArea(lastPoint);
        // Clear the temporary pixel path
        vectorImage->deleteSelectedPoints();

        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    }
}

void EraserTool::strokeMoved()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() == Layer::VECTOR)
    {
        qreal radius = get(ToolProperty::Width) / 2;

        VectorImage* currKey = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mEditor->currentFrame()));
        if (currKey == nullptr) { return; } // Before the layer's first keyframe: nothing to erase
        const QTransform toKey = canvasToKey();
        QList<VertexRef> nearbyVertices = currKey->getVerticesCloseTo(toKey.map(getCurrentPoint()), radius * std::sqrt(std::abs(toKey.determinant())));
        for (auto nearbyVertice : nearbyVertices)
        {
            currKey->setSelected(nearbyVertice, true);
        }
    }
}
