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
#include "penciltool.h"

#include <QSettings>
#include <QPixmap>
#include "pointerevent.h"

#include "layermanager.h"
#include "colormanager.h"
#include "viewmanager.h"
#include "preferencemanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"

#include "editor.h"
#include "scribblearea.h"
#include "layervector.h"
#include "vectorimage.h"


PencilTool::PencilTool(QObject* parent) : DabTool(parent)
{
}

void PencilTool::loadSettings()
{
    StrokeTool::loadSettings();

    mPropertyUsed[StrokeToolProperties::WIDTH_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::PRESSURE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::FILLCONTOUR_ENABLED] = { Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::SMOOTHING_MODE] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[StrokeToolProperties::SPACING_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_SQUARE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_ASPECT_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_ANGLE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED] = { Layer::BITMAP };

    QSettings pencilSettings(PENCIL2D, PENCIL2D);

    QHash<int, PropertyInfo> info;

    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 4.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 50.0 };
    info[StrokeToolProperties::PRESSURE_ENABLED] = true;
    info[StrokeToolProperties::FEATHER_ENABLED] = false;
    info[StrokeToolProperties::SMOOTHING_MODE] = smoothingModeInfo(SmoothingMode::OneEuro);
    info[StrokeToolProperties::SPACING_VALUE] = { SPACING_MIN, SPACING_MAX, 12.5 };
    info[StrokeToolProperties::FILLCONTOUR_ENABLED] = false;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);
    migrateLineStabilization(pencilSettings);

    if (toolProperties().requireMigration(pencilSettings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, pencilSettings.value("pencilWidth", 4.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::PRESSURE_ENABLED, pencilSettings.value("pencilPressure", true).toBool());
        setLegacySmoothing(pencilSettings.value("pencilLineStabilization", StabilizationLevel::STRONG).toInt());
        toolProperties().setBaseValue(StrokeToolProperties::FILLCONTOUR_ENABLED, pencilSettings.value("FillContour", false).toBool());

        pencilSettings.remove("pencilWidth");
        pencilSettings.remove("pencilPressure");
        pencilSettings.remove("pencilLineStabilization");
        pencilSettings.remove("FillContour");
    }

    toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, info[StrokeToolProperties::FEATHER_VALUE].defaultReal());

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
}

QCursor PencilTool::cursor()
{
    if (mEditor->preference()->isOn(SETTING::TOOL_CURSOR))
    {
        return QCursor(QPixmap(":icons/general/cursor-pencil.svg"), 4, 14);
    }
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

void PencilTool::strokeStarted()
{
    // note: why are we doing this on device press event?
    if (mEditor->layers()->currentLayer()->type() == Layer::VECTOR && !mEditor->preference()->isOn(SETTING::INVISIBLE_LINES))
    {
        mScribbleArea->toggleThinLines();
    }
}

void PencilTool::drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip)
{
    mScribbleArea->drawPencil(point, width, mSettings.feather(), mEditor->color()->frontColor(), opacity, tip);
}

QPen PencilTool::vectorPreviewPen()
{
    return QPen(mEditor->color()->frontColor(), 1, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
}

void PencilTool::finishVectorStroke(Layer* layer)
{
    paintVectorStroke(layer);
}

void PencilTool::paintVectorStroke(Layer* layer)
{
    if (mStrokePoints.empty())
        return;

    // Clear the temporary pixel path
    mScribbleArea->clearDrawingBuffer();
    qreal tol = mScribbleArea->getCurveSmoothing() / mEditor->view()->scaling();

    BezierCurve curve(mStrokePoints, mStrokePressures, tol);
    curve.setWidth(0);
    curve.setFeather(0);
    curve.setFilled(false);
    curve.setInvisibility(true);
    curve.setVariableWidth(false);
    curve.setColorNumber(mEditor->color()->frontColorNumber());
    VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame());
    if (vectorImage == nullptr) { return; } // Can happen if the first frame is deleted while drawing
    vectorImage->addCurve(curve, qAbs(mEditor->view()->scaling()), false);

    if (mSettings.fillContourEnabled())
    {
        vectorImage->fillContour(mStrokePoints,
                                 mEditor->color()->frontColorNumber());
    }

    if (vectorImage->isAnyCurveSelected() || mEditor->select()->somethingSelected())
    {
        mEditor->deselectAll();
    }

    // select last/newest curve
    vectorImage->setSelected(vectorImage->getLastCurveNumber(), true);

    // TODO: selection doesn't apply on enter

    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
}
