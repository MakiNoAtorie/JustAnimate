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
#include "pencilsettings.h"


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

    QSettings& settings = pencilSettings();

    QHash<int, PropertyInfo> info;

    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::FEATHER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FEATHER_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::PRESSURE_ENABLED] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::SMOOTHING_MODE] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::SPACING_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_SQUARE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_ASPECT_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_ANGLE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::SPEED_TO_SIZE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TILT_TO_SIZE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TILT_TO_ANGLE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::SIZE_JITTER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::SCATTER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::ANGLE_JITTER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::DAB_COUNT_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TAPER_START_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TAPER_END_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::GRAIN_MODE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::GRAIN_TEXTURE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::GRAIN_SCALE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::GRAIN_CONTRAST_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::GRAIN_BRIGHTNESS_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::ANTI_ALIASING_ENABLED] = { Layer::BITMAP };

    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 24.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 48.0 };
    info[StrokeToolProperties::FEATHER_ENABLED] = true;
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::SMOOTHING_MODE] = smoothingModeInfo(SmoothingMode::None);
    info[StrokeToolProperties::SPACING_VALUE] = { SPACING_MIN, SPACING_MAX, 12.5 };
    info[StrokeToolProperties::ANTI_ALIASING_ENABLED] = true;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), settings);
    migrateLineStabilization(settings);

    if (toolProperties().requireMigration(settings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, settings.value("eraserWidth", 24.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, settings.value("eraserFeather", 48.0).toReal());
        setLegacySmoothing(settings.value("stabilizerLevel", StabilizationLevel::NONE).toInt());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_ENABLED, settings.value("eraserUseFeather", true).toBool());
        toolProperties().setBaseValue(StrokeToolProperties::PRESSURE_ENABLED, settings.value("eraserPressure", true).toBool());
        toolProperties().setBaseValue(StrokeToolProperties::ANTI_ALIASING_ENABLED, settings.value("eraserAA", true).toBool());

        settings.remove("eraserWidth");
        settings.remove("eraserFeather");
        settings.remove("stabilizerLevel");
        settings.remove("eraserUseFeather");
        settings.remove("eraserPressure");
        settings.remove("eraserAA");
    }

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);
}

void EraserTool::drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip)
{
    mScribbleArea->drawBrush(point,
                             width,
                             mSettings.feather(),
                             Qt::white,
                             opacity,
                             mSettings.featherEnabled(),
                             mSettings.AntiAliasingEnabled() == ON,
                             tip);
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
        const QTransform toKey = canvasToKey();
        QList<VertexRef> nearbyVertices = currKey->getVerticesCloseTo(toKey.map(getCurrentPoint()), radius * std::sqrt(std::abs(toKey.determinant())));
        for (auto nearbyVertice : nearbyVertices)
        {
            currKey->setSelected(nearbyVertice, true);
        }
    }
}
