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

#include <QSettings>
#include <QPixmap>
#include <QPainter>

#include "editor.h"
#include "blitrect.h"
#include "scribblearea.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "undoredomanager.h"
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

    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    QHash<int, PropertyInfo> info;

    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::FEATHER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FEATHER_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::PRESSURE_ENABLED] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::STABILIZATION_VALUE] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::ANTI_ALIASING_ENABLED] = { Layer::BITMAP };

    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 24.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 48.0 };
    info[StrokeToolProperties::FEATHER_ENABLED] = true;
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::STABILIZATION_VALUE] = { StabilizationLevel::NONE, StabilizationLevel::STRONG, StabilizationLevel::NONE };
    info[StrokeToolProperties::ANTI_ALIASING_ENABLED] = true;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, pencilSettings.value("eraserWidth", 24.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, pencilSettings.value("eraserFeather", 48.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::STABILIZATION_VALUE, pencilSettings.value("stabilizerLevel", StabilizationLevel::NONE).toInt());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_ENABLED, pencilSettings.value("eraserUseFeather", true).toBool());
        toolProperties().setBaseValue(StrokeToolProperties::PRESSURE_ENABLED, pencilSettings.value("eraserPressure", true).toBool());
        toolProperties().setBaseValue(StrokeToolProperties::ANTI_ALIASING_ENABLED, pencilSettings.value("eraserAA", true).toBool());

        pencilSettings.remove("eraserWidth");
        pencilSettings.remove("eraserFeather");
        pencilSettings.remove("stabilizerLevel");
        pencilSettings.remove("eraserUseFeather");
        pencilSettings.remove("eraserPressure");
        pencilSettings.remove("eraserAA");
    }

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);
}

QCursor EraserTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void EraserTool::drawDab(QPointF point, qreal width, qreal opacity)
{
    mScribbleArea->drawBrush(point,
                             width,
                             mSettings.feather(),
                             Qt::white,
                             opacity,
                             mSettings.featherEnabled(),
                             mSettings.AntiAliasingEnabled() == ON);
}

QPen EraserTool::vectorPreviewPen()
{
    mCurrentWidth = mSettings.width();
    if (mSettings.pressureEnabled())
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
        mScribbleArea->clearDrawingBuffer();
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
        qreal radius = mSettings.width() / 2;

        VectorImage* currKey = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mEditor->currentFrame()));
        if (currKey == nullptr) { return; } // Before the layer's first keyframe: nothing to erase
        QList<VertexRef> nearbyVertices = currKey->getVerticesCloseTo(getCurrentPoint(), radius);
        for (auto nearbyVertice : nearbyVertices)
        {
            currKey->setSelected(nearbyVertice, true);
        }
    }
}
