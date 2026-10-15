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

#include "brushtool.h"

#include <cmath>
#include <QSettings>
#include <QPixmap>
#include <QPainter>
#include <QColor>

#include "beziercurve.h"
#include "vectorimage.h"
#include "editor.h"
#include "colormanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "scribblearea.h"
#include "pointerevent.h"


BrushTool::BrushTool(QObject* parent) : DabTool(parent)
{
}

ToolType BrushTool::type() const
{
    return BRUSH;
}

void BrushTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::FEATHER_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::PRESSURE_ENABLED] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::INVISIBILITY_ENABLED] = { Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::SMOOTHING_MODE] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::SPACING_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FLOW_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::OPACITY_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::WASH_ENABLED] = { Layer::BITMAP };

    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    QHash<int, PropertyInfo> info;
    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 24.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 48.0 };
    info[StrokeToolProperties::FEATHER_ENABLED] = true;
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::INVISIBILITY_ENABLED] = false;
    info[StrokeToolProperties::SMOOTHING_MODE] = smoothingModeInfo(SmoothingMode::OneEuro);
    info[StrokeToolProperties::SPACING_VALUE] = { SPACING_MIN, SPACING_MAX, 12.5 };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);
    migrateLineStabilization(pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, pencilSettings.value("brushWidth", 24.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, pencilSettings.value("brushFeather", 48.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::PRESSURE_ENABLED, pencilSettings.value("brushPressure", true).toBool());
        toolProperties().setBaseValue(StrokeToolProperties::INVISIBILITY_ENABLED, pencilSettings.value("brushInvisibility", false).toBool());
        setLegacySmoothing(pencilSettings.value("brushLineStabilization", StabilizationLevel::STRONG).toInt());

        pencilSettings.remove("brushWidth");
        pencilSettings.remove("brushFeather");
        pencilSettings.remove("brushPressure");
        pencilSettings.remove("brushInvisibility");
        pencilSettings.remove("brushLineStabilization");
    }

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);
}

QCursor BrushTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":icons/general/cursor-brush.svg"), 4, 14);
    }
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void BrushTool::drawDab(QPointF point, qreal width, qreal opacity)
{
    mScribbleArea->drawBrush(point,
                             width,
                             mSettings.feather(),
                             mEditor->color()->frontColor(),
                             opacity,
                             true);
}

QPen BrushTool::vectorPreviewPen()
{
    qreal pressure = (mSettings.pressureEnabled()) ? mCurrentPressure : 1;
    return QPen(mEditor->color()->frontColor(), mSettings.width() * pressure, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

void BrushTool::finishVectorStroke(Layer* layer)
{
    paintVectorStroke(layer);
}

// This function uses the points from DrawStroke
// and turns them into vector lines.
void BrushTool::paintVectorStroke(Layer* layer)
{
    if (mStrokePoints.empty())
        return;

    if (layer->type() == Layer::VECTOR && mStrokePoints.size() > -1)
    {
        // Clear the temporary pixel path
        mScribbleArea->clearDrawingBuffer();
        qreal tol = mScribbleArea->getCurveSmoothing() / mEditor->view()->scaling();

        BezierCurve curve(mStrokePoints, mStrokePressures, tol);
        curve.setWidth(mSettings.width());
        curve.setFeather(mSettings.feather());
        curve.setFilled(false);
        curve.setInvisibility(mSettings.invisibilityEnabled());
        curve.setVariableWidth(mSettings.pressureEnabled());
        curve.setColorNumber(mEditor->color()->frontColorNumber());

        VectorImage* vectorImage = static_cast<VectorImage*>(layer->getLastKeyFrameAtPosition(mEditor->currentFrame()));
        vectorImage->addCurve(curve, mEditor->view()->scaling(), false);

        if (vectorImage->isAnyCurveSelected() || mEditor->select()->somethingSelected())
        {
            mEditor->deselectAll();
        }

        vectorImage->setSelected(vectorImage->getLastCurveNumber(), true);

        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    }
}
