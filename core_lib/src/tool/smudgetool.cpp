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
#include <QSettings>

#include "pointerevent.h"
#include "vectorimage.h"
#include "editor.h"
#include "scribblearea.h"

#include "colormanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"

#include "layervector.h"
#include "blitrect.h"
#include "pencilsettings.h"

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

    for (int property : { StrokeToolProperties::WIDTH_VALUE, StrokeToolProperties::FEATHER_VALUE,
                          StrokeToolProperties::PRESSURE_ENABLED, StrokeToolProperties::SMOOTHING_MODE,
                          StrokeToolProperties::SPACING_VALUE, StrokeToolProperties::FLOW_VALUE,
                          StrokeToolProperties::TIP_SQUARE_ENABLED, StrokeToolProperties::TIP_ASPECT_VALUE,
                          StrokeToolProperties::TIP_ANGLE_VALUE, StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED,
                          StrokeToolProperties::SPEED_TO_SIZE_VALUE, StrokeToolProperties::TILT_TO_SIZE_VALUE,
                          StrokeToolProperties::TILT_TO_ANGLE_VALUE, StrokeToolProperties::SIZE_JITTER_VALUE,
                          StrokeToolProperties::SCATTER_VALUE, StrokeToolProperties::ANGLE_JITTER_VALUE,
                          StrokeToolProperties::DAB_COUNT_VALUE, StrokeToolProperties::TAPER_START_VALUE,
                          StrokeToolProperties::TAPER_END_VALUE, StrokeToolProperties::GRAIN_MODE_VALUE,
                          StrokeToolProperties::GRAIN_SCALE_VALUE, StrokeToolProperties::GRAIN_CONTRAST_VALUE,
                          StrokeToolProperties::GRAIN_BRIGHTNESS_VALUE, StrokeToolProperties::SMUDGE_LENGTH_VALUE,
                          StrokeToolProperties::SMUDGE_RADIUS_VALUE, StrokeToolProperties::SMUDGE_COLOR_RATE_VALUE,
                          StrokeToolProperties::SMUDGE_MIX_VALUE })
        mPropertyUsed[property] = { Layer::BITMAP };

    QHash<int, PropertyInfo> info;
    QSettings& settings = pencilSettings();
    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 24.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 48.0 };
    info[StrokeToolProperties::SMOOTHING_MODE] = smoothingModeInfo(SmoothingMode::OneEuro);
    // About the 2 px step of the smudge loop before v0.5 at the default width.
    info[StrokeToolProperties::SPACING_VALUE] = { SPACING_MIN, SPACING_MAX, 10.0 };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), settings);

    if (toolProperties().requireMigration(settings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, settings.value("smudgeWidth", 24.0).toReal());
        toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, settings.value("smudgeFeather", 48.0).toReal());

        settings.remove("smudgeWidth");
        settings.remove("smudgeFeather");
    }

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
    smudge.length = mSettings.smudgeLength() / 100.0f;
    smudge.radius = mSettings.smudgeRadius() / 100.0f;
    smudge.colorRate = mSettings.smudgeColorRate() / 100.0f;
    smudge.wet = mSettings.smudgeMix() == 1;
    mScribbleArea->smudgeDab(point, width, mSettings.feather(), mEditor->color()->frontColor(), opacity, tip, smudge);
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
            mEditor->undoRedo()->discardState(mUndoSaveStateId);
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
        mEditor->undoRedo()->record(mUndoSaveStateId, typeName());
    }

    StrokeTool::pointerReleaseEvent(event);
}

QPointF SmudgeTool::offsetFromPressPos()
{
    const QTransform toKey = canvasToKey();
    return toKey.map(getCurrentPoint()) - toKey.map(getCurrentPressPoint());
}
