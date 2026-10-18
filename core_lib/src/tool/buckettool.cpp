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
#include "buckettool.h"

#include <QPixmap>
#include <QPainter>
#include <QSettings>
#include "pointerevent.h"

#include "layer.h"
#include "layervector.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "colormanager.h"
#include "toolmanager.h"
#include "viewmanager.h"
#include "vectorimage.h"
#include "editor.h"
#include "scribblearea.h"
#include "pencilsettings.h"


BucketTool::BucketTool(QObject* parent) : BaseTool(parent)
{
}

void BucketTool::loadSettings()
{
    mPropertyUsed[BucketToolProperties::FILLTHICKNESS_VALUE] = { Layer::VECTOR };
    mPropertyUsed[BucketToolProperties::COLORTOLERANCE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::COLORTOLERANCE_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLEXPAND_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLEXPAND_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLMODE_VALUE] = { Layer::BITMAP };
    mPropertyUsed[BucketToolProperties::FILLCLOSEGAPS_VALUE] = { Layer::BITMAP };

    QSettings& settings = pencilSettings();

    QHash<int, PropertyInfo> info;

    info[BucketToolProperties::FILLTHICKNESS_VALUE] = { 1.0, 100.0, 4.0 };
    info[BucketToolProperties::COLORTOLERANCE_VALUE] = { 1, 100, 32 };
    info[BucketToolProperties::COLORTOLERANCE_ENABLED] = false;
    info[BucketToolProperties::FILLEXPAND_VALUE] = { 1, 25, 2 };
    info[BucketToolProperties::FILLEXPAND_ENABLED] = true;
    info[BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE] = { 0, 1, 0 };
    info[BucketToolProperties::FILLMODE_VALUE] = { 0, 2, 0 };
    info[BucketToolProperties::FILLCLOSEGAPS_VALUE] = { 0, 20, 0 };

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), settings);

    if (toolProperties().requireMigration(settings, ToolProperties::VERSION_1)) {
        toolProperties().setValue(BucketToolProperties::FILLTHICKNESS_VALUE, settings.value("fillThickness", 4.0).toReal());
        toolProperties().setValue(BucketToolProperties::COLORTOLERANCE_VALUE, settings.value("Tolerance", 32).toInt());
        toolProperties().setValue(BucketToolProperties::COLORTOLERANCE_ENABLED, settings.value("BucketToleranceEnabled", false).toBool());
        toolProperties().setValue(BucketToolProperties::FILLEXPAND_VALUE, settings.value("BucketFillExpand", 2).toInt());
        toolProperties().setValue(BucketToolProperties::FILLEXPAND_ENABLED, settings.value("BucketFillExpandEnabled", true).toBool());
        toolProperties().setValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, settings.value("BucketFillReferenceMode", 0).toInt());
        toolProperties().setValue(BucketToolProperties::FILLMODE_VALUE, settings.value("FillMode", 0).toInt());

        settings.remove("fillThickness");
        settings.remove("Tolerance");
        settings.remove("BucketToleranceEnabled");
        settings.remove("BucketFillExpand");
        settings.remove("BucketFillExpandEnabled");
        settings.remove("BucketFillReferenceMode");
        settings.remove("FillMode");
    }
}

QCursor BucketTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cursor-bucket.svg"), 4, 16);
}

void BucketTool::pointerPressEvent(PointerEvent* event)
{
    mInterpolator.pointerPressEvent(event);

    Layer* targetLayer = mEditor->layers()->currentLayer();

    if (targetLayer->type() != Layer::BITMAP) { return; }

    LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());

    mFillEdit.emplace(mEditor->edit(targetLayer, mEditor->currentFrame(), typeName()));
    mBitmapBucket = BitmapBucket(mEditor,
                                 mEditor->color()->frontColor(),
                                 layerCam ? canvasToKey().mapRect(layerCam->getViewAtFrame(mEditor->currentFrame()).inverted().mapRect(layerCam->getViewRect())) : QRect(),
                                 getCurrentPoint(),
                                 mSettings);

    // Because we can change layer to on the fly, but we do not act reactively
    // on it, it's necessary to invalidate layer cache on press event.
    // Otherwise, the cache will be drawn until a move event has been initiated.
    mScribbleArea->invalidatePainterCaches();
}

void BucketTool::pointerMoveEvent(PointerEvent* event)
{
    mInterpolator.pointerMoveEvent(event);
    if (event->buttons() & Qt::LeftButton)
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer->type() == Layer::BITMAP)
        {
            paintBitmap();
            mFilledOnMove = true;
        }
    }
}

void BucketTool::pointerReleaseEvent(PointerEvent* event)
{
    mInterpolator.pointerReleaseEvent(event);

    Layer* layer = editor()->layers()->currentLayer();
    if (layer == nullptr) { return; }

    if (event->button() == Qt::LeftButton)
    {
        if (layer->type() == Layer::VECTOR) {
            Edit edit = mEditor->edit(layer, mEditor->currentFrame(), typeName());
            paintVector(layer);
            edit.commit();
        }
        else if (layer->type() == Layer::BITMAP && !mFilledOnMove)
        {
            paintBitmap();
        }
    }
    // A drag can fill many times: record once, after the last fill.
    if (mDidFill && mFillEdit) { mFillEdit->commit(); }
    mFillEdit.reset();
    mFilledOnMove = false;
    mDidFill = false;
}

void BucketTool::paintBitmap()
{
    mBitmapBucket.paint(getCurrentPoint(), [this](BucketState progress, int layerIndex, int frameIndex)
    {
        if (progress == BucketState::DidFillTarget)
        {
            mDidFill = true;
            mEditor->setModified(layerIndex, frameIndex);
        }
    });
}

void BucketTool::paintVector(Layer* layer)
{
    VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(mEditor->currentFrame());
    if (vectorImage == nullptr) { return; } // Can happen if the first frame is deleted while drawing

    if (!vectorImage->isPathFilled())
    {
        vectorImage->fillSelectedPath(mEditor->color()->frontColorNumber());
    }

    vectorImage->applyWidthToSelection(mSettings.fillThickness());
    vectorImage->applyColorToSelectedCurve(mEditor->color()->frontColorNumber());
    vectorImage->applyColorToSelectedArea(mEditor->color()->frontColorNumber());

    applyChanges();

    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
}

void BucketTool::applyChanges()
{
    mScribbleArea->applyTransformedSelection();
}

void BucketTool::setStrokeThickness(qreal width)
{
    toolProperties().setValue(BucketToolProperties::FILLTHICKNESS_VALUE, width);
    emit strokeThicknessChanged(width);
}

void BucketTool::setColorTolerance(int tolerance)
{
    toolProperties().setValue(BucketToolProperties::COLORTOLERANCE_VALUE, tolerance);
    emit toleranceChanged(tolerance);
}

void BucketTool::setColorToleranceEnabled(bool enabled)
{
    toolProperties().setValue(BucketToolProperties::COLORTOLERANCE_ENABLED, enabled);
    emit toleranceEnabledChanged(enabled);
}

void BucketTool::setFillExpand(int fillExpandValue)
{
    toolProperties().setValue(BucketToolProperties::FILLEXPAND_VALUE, fillExpandValue);
    emit fillExpandChanged(fillExpandValue);
}

void BucketTool::setFillExpandEnabled(bool enabled)
{
    toolProperties().setValue(BucketToolProperties::FILLEXPAND_ENABLED, enabled);
    emit fillExpandEnabledChanged(enabled);
}

void BucketTool::setFillReferenceMode(int referenceMode)
{
    toolProperties().setValue(BucketToolProperties::FILLLAYERREFERENCEMODE_VALUE, referenceMode);
    emit fillReferenceModeChanged(referenceMode);
}

void BucketTool::setFillCloseGaps(int pixels)
{
    toolProperties().setValue(BucketToolProperties::FILLCLOSEGAPS_VALUE, pixels);
    emit fillCloseGapsChanged(pixels);
}

void BucketTool::setFillMode(int mode)
{
    toolProperties().setValue(BucketToolProperties::FILLMODE_VALUE, mode);
    emit fillModeChanged(mode);
}

QPointF BucketTool::getCurrentPoint() const
{
    return canvasToKey().map(mEditor->view()->mapScreenToCanvas(getCurrentPixel()));
}

QPointF BucketTool::getCurrentPixel() const
{
    return mInterpolator.getCurrentPixel();
}

