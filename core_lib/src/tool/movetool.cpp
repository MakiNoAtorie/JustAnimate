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

#include "movetool.h"

#include <cassert>
#include <cmath>
#include <QtMath>
#include <QLineF>
#include <QMessageBox>
#include <QSettings>

#include "pointerevent.h"
#include "editor.h"
#include "toolmanager.h"
#include "strokeinterpolator.h"
#include "selectionmanager.h"
#include "overlaymanager.h"
#include "scribblearea.h"
#include "layervector.h"
#include "layermanager.h"
#include "layercamera.h"
#include "mathutils.h"
#include "vectorimage.h"
#include "pencilsettings.h"

MoveTool::MoveTool(QObject* parent) : TransformTool(parent)
{
}

ToolType MoveTool::type() const
{
    return MOVE;
}

void MoveTool::loadSettings()
{
    mRotationIncrement = mEditor->preference()->getInt(SETTING::ROTATION_INCREMENT);
    QSettings& settings = pencilSettings();

    mPropertyUsed[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = { Layer::BITMAP, Layer::VECTOR };
    mPropertyUsed[TransformToolProperties::ANTI_ALIASING_ENABLED] = { Layer::BITMAP };
    mPropertyUsed[TransformToolProperties::KEYTRANSFORM_ENABLED] = { Layer::BITMAP, Layer::VECTOR };
    QHash<int, PropertyInfo> info;

    info[TransformToolProperties::SHOWSELECTIONINFO_ENABLED] = false;
    info[TransformToolProperties::ANTI_ALIASING_ENABLED] = true;
    info[TransformToolProperties::KEYTRANSFORM_ENABLED] = false;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), settings);

    if (toolProperties().requireMigration(settings, ToolProperties::VERSION_1)) {
        toolProperties().setBaseValue(TransformToolProperties::SHOWSELECTIONINFO_ENABLED, settings.value("ShowSelectionInfo", false).toBool());
        toolProperties().setBaseValue(TransformToolProperties::ANTI_ALIASING_ENABLED, settings.value("moveAA", true).toBool());
    }

    connect(mEditor->preference(), &PreferenceManager::optionChanged, this, &MoveTool::updateSettings);
}

QCursor MoveTool::cursor()
{
    MoveMode mode = MoveMode::NONE;
    SelectionManager* selectMan = mEditor->select();
    if (mSettings.keyTransformEnabled())
    {
        mode = MoveMode::MIDDLE;
    }
    else if (selectMan->somethingSelected())
    {
        mode = mEditor->select()->getMoveMode();
    }
    else if (mEditor->overlays()->anyOverlayEnabled())
    {
        mode = mPerspMode;
    }

    return cursor(mode);
}

void MoveTool::updateSettings(const SETTING setting)
{
    switch (setting)
    {
    case SETTING::ROTATION_INCREMENT:
        mRotationIncrement = mEditor->preference()->getInt(SETTING::ROTATION_INCREMENT);
        break;
    case SETTING::OVERLAY_PERSPECTIVE1:
        mEditor->overlays()->settingsUpdated(setting, mEditor->preference()->isOn(setting));
        break;
    case SETTING::OVERLAY_PERSPECTIVE2:
        mEditor->overlays()->settingsUpdated(setting, mEditor->preference()->isOn(setting));
        break;
    case SETTING::OVERLAY_PERSPECTIVE3:
        mEditor->overlays()->settingsUpdated(setting, mEditor->preference()->isOn(setting));
        break;
    default:
        break;
    }
}

void MoveTool::pointerPressEvent(PointerEvent* event)
{
    Layer* currentLayer = currentPaintableLayer();
    if (currentLayer == nullptr) return;

    if (mSettings.keyTransformEnabled())
    {
        const KeyFrame* key = currentLayer->getLastKeyFrameAtPosition(mEditor->currentFrame());
        if (key == nullptr) return;
        mKeyFrame = key->pos();
        mKeyStart = key->tweenState();
        mKeyPress = event->canvasPos();
        mKeyMods = event->modifiers();
        return;
    }

    if (mEditor->select()->somethingSelected())
    {
        beginInteraction(canvasToKey().map(event->canvasPos()), event->modifiers(), currentLayer);
    }
    else if (mEditor->overlays()->anyOverlayEnabled())
    {
        LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
        Q_ASSERT(layerCam);

        mPerspMode = mEditor->overlays()->getMoveModeForPoint(event->canvasPos(), layerCam->getViewAtFrame(mEditor->currentFrame()));
        mEditor->overlays()->setMoveMode(mPerspMode);
        QPoint mapped = layerCam->getViewAtFrame(mEditor->currentFrame()).map(event->canvasPos()).toPoint();
        mEditor->overlays()->updatePerspective(mapped);
        mEditor->overlays()->updateRuler(event->canvasPos());
    }

    mEditor->updateFrame();
}

void MoveTool::pointerMoveEvent(PointerEvent* event)
{
    Layer* currentLayer = currentPaintableLayer();
    if (currentLayer == nullptr) return;

    if (mSettings.keyTransformEnabled())
    {
        KeyFrame* key = mKeyFrame > 0 ? currentLayer->getKeyFrameAt(mKeyFrame) : nullptr;
        if (key && mScribbleArea->isPointerInUse())
        {
            // Live preview; the release records the gesture through LayerManager::setKeyTween.
            key->setTweenState(keyTransformAt(event->canvasPos()));
            emit mEditor->frameModified(mEditor->currentFrame());
        }
        return;
    }

    // The selection lives in the space of the current key.
    const QPointF keyPos = canvasToKey().map(event->canvasPos());
    if (mScribbleArea->isPointerInUse())   // the user is also pressing the mouse (dragging)
    {
        transformSelection(keyPos, event->modifiers());

        if (mEditor->overlays()->anyOverlayEnabled())
        {
            LayerCamera* layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
            Q_ASSERT(layerCam);
            mEditor->overlays()->updatePerspective(layerCam->getViewAtFrame(mEditor->currentFrame()).map(event->canvasPos()));
            mEditor->overlays()->updateRuler(event->canvasPos());
        }
        if (mEditor->select()->somethingSelected())
        {
            transformSelection(keyPos, event->modifiers());
        }
    }
    else
    {
        // the user is moving the mouse without pressing it
        // update cursor to reflect selection corner interaction
        mEditor->select()->setMoveModeForAnchorInRange(keyPos, true);
        if (mEditor->overlays()->anyOverlayEnabled())
        {
            LayerCamera *layerCam = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
            Q_ASSERT(layerCam);
            mPerspMode = mEditor->overlays()->getMoveModeForPoint(event->canvasPos(), layerCam->getViewAtFrame(mEditor->currentFrame()));
        }
        mScribbleArea->updateToolCursor();

        if (currentLayer->type() == Layer::VECTOR)
        {
            storeClosestVectorCurve(keyPos, currentLayer);
        }
    }
    mEditor->updateFrame();
}

void MoveTool::pointerReleaseEvent(PointerEvent* event)
{
    if (mKeyFrame > 0)
    {
        Layer* layer = currentPaintableLayer();
        KeyFrame* key = layer ? layer->getKeyFrameAt(mKeyFrame) : nullptr;
        const int frame = mKeyFrame;
        mKeyFrame = 0;
        if (key == nullptr) return;
        // Put the start state back so the undo state captures it, then record the gesture once.
        key->setTweenState(mKeyStart);
        if (event->canvasPos() != mKeyPress)
        {
            mEditor->layers()->setKeyTween(frame, key->tween(), key->tweenEasing(), keyTransformAt(event->canvasPos()));
        }
        emit mEditor->frameModified(mEditor->currentFrame());
        return;
    }

    if (mEdit) { mEdit->commit(); mEdit.reset(); }

    if (mEditor->overlays()->anyOverlayEnabled())
    {
        mEditor->overlays()->setMoveMode(MoveMode::NONE);
        mPerspMode = MoveMode::NONE;
    }

    auto selectMan = mEditor->select();
    if (!selectMan->somethingSelected())
        return;

    mScribbleArea->updateToolCursor();
    emit mEditor->frameModified(mEditor->currentFrame());
}

void MoveTool::transformSelection(const QPointF& pos, Qt::KeyboardModifiers keyMod)
{
    auto selectMan = mEditor->select();
    if (selectMan->somethingSelected())
    {
        int rotationIncrement = 0;
        if (selectMan->getMoveMode() == MoveMode::ROTATION && keyMod & Qt::ShiftModifier)
        {
            rotationIncrement = mRotationIncrement;
        }

        selectMan->maintainAspectRatio(keyMod != Qt::ShiftModifier);
        selectMan->alignPositionToAxis(keyMod == Qt::ShiftModifier);

        qreal newAngle = 0;
        if (selectMan->getMoveMode() == MoveMode::ROTATION) {
            QPointF anchorPoint = selectMan->currentTransformAnchor();
            newAngle = selectMan->angleFromPoint(pos, anchorPoint) - mRotatedAngle;
        }

        selectMan->adjustSelection(pos, mOffset, newAngle, rotationIncrement);
    }
    else // there is nothing selected
    {
        selectMan->setMoveMode(MoveMode::NONE);
    }
}

void MoveTool::beginInteraction(const QPointF& pos, Qt::KeyboardModifiers keyMod, Layer* layer)
{
    auto selectMan = mEditor->select();
    QRectF selectionRect = selectMan->mySelectionRect();
    if (!selectionRect.isNull())
    {
        mEdit.emplace(mEditor->edit(layer, mEditor->currentFrame(), typeName()));
    }

    if (keyMod != Qt::ShiftModifier)
    {
        if (selectMan->isOutsideSelectionArea(pos))
        {
            applyTransformation();
            mEditor->deselectAll();
        }
    }

    if (selectMan->getMoveMode() == MoveMode::MIDDLE)
    {
        if (keyMod == Qt::ControlModifier) // --- rotation
        {
            selectMan->setMoveMode(MoveMode::ROTATION);
        }
    }

    if (layer->type() == Layer::VECTOR)
    {
        createVectorSelection(pos, keyMod, layer);
    }

    selectMan->setTransformAnchor(selectMan->getSelectionAnchorPoint());
    selectMan->setDragOrigin(pos);
    mOffset = selectMan->myTranslation();

    if(selectMan->getMoveMode() == MoveMode::ROTATION) {
        mRotatedAngle = selectMan->angleFromPoint(pos, selectMan->currentTransformAnchor()) - selectMan->myRotation();
    }
}

/**
 * @brief MoveTool::createVectorSelection
 * In vector the selection rectangle is based on the bounding box of the curves
 * We can therefore create a selection just by clicking near/on a curve
 */
void MoveTool::createVectorSelection(const QPointF& pos, Qt::KeyboardModifiers keyMod, Layer* layer)
{
    assert(layer->type() == Layer::VECTOR);
    LayerVector* vecLayer = static_cast<LayerVector*>(layer);
    VectorImage* vectorImage = vecLayer->getLastVectorImageAtFrame(mEditor->currentFrame());
    if (vectorImage == nullptr) { return; }

    if (!mEditor->select()->closestCurves().empty()) // the user clicks near a curve
    {
        setCurveSelected(vectorImage, keyMod);
    }
    else if (vectorImage->getLastAreaNumber(pos) > -1)
    {
        setAreaSelected(pos, vectorImage, keyMod);
    }
}

void MoveTool::setCurveSelected(VectorImage* vectorImage, Qt::KeyboardModifiers keyMod)
{
    auto selectMan = mEditor->select();
    if (!vectorImage->isSelected(selectMan->closestCurves()))
    {
        if (keyMod != Qt::ShiftModifier)
        {
            applyTransformation();
        }
        vectorImage->setSelected(selectMan->closestCurves(), true);
        selectMan->setSelection(vectorImage->getSelectionRect(), false);
    }
}

void MoveTool::setAreaSelected(const QPointF& pos, VectorImage* vectorImage, Qt::KeyboardModifiers keyMod)
{
    int areaNumber = vectorImage->getLastAreaNumber(pos);
    if (!vectorImage->isAreaSelected(areaNumber))
    {
        if (keyMod != Qt::ShiftModifier)
        {
            applyTransformation();
        }
        vectorImage->setAreaSelected(areaNumber, true);
        mEditor->select()->setSelection(vectorImage->getSelectionRect(), false);
    }
}

/**
 * @brief MoveTool::storeClosestVectorCurve
 * stores the curves closest to the mouse position in mClosestCurves
 */
void MoveTool::storeClosestVectorCurve(const QPointF& pos, Layer* layer)
{
    auto selectMan = mEditor->select();
    auto layerVector = static_cast<LayerVector*>(layer);
    VectorImage* pVecImg = layerVector->getLastVectorImageAtFrame(mEditor->currentFrame());
    if (pVecImg == nullptr) { return; }
    selectMan->setCurves(pVecImg->getCurvesCloseTo(pos, selectMan->selectionTolerance()));
}

void MoveTool::applyTransformation()
{
    mScribbleArea->applyTransformedSelection();
    mRotatedAngle = 0;
}

void MoveTool::applyTransformationAndDeselect()
{
    // We apply transform changes upon leaving a layer and deselect all
    mScribbleArea->applyTransformedSelection();

    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (currentLayer->type() == Layer::VECTOR) {
        auto keyFrame = static_cast<VectorImage*>(currentLayer->getLastKeyFrameAtPosition(mEditor->currentFrame()));
        if (keyFrame)
        {
            keyFrame->deselectAll();
        }
    }

    mEditor->select()->resetSelectionProperties();
    mRotatedAngle = 0;
}

bool MoveTool::leavingThisTool()
{
    TransformTool::leavingThisTool();

    if (currentPaintableLayer())
    {
        applyTransformation();
    }

    return true;
}

bool MoveTool::isActive() const {
    return mScribbleArea->isPointerInUse() &&
           (mKeyFrame > 0 || mEditor->select()->somethingSelected() || mEditor->overlays()->getMoveMode() != MoveMode::NONE);
}

TweenState MoveTool::keyTransformAt(const QPointF& canvasPos) const
{
    TweenState s = mKeyStart;
    // Rotation and scale pivot on the Drawing's origin, which the key moves to (tx, ty).
    const QPointF origin(s.tx, s.ty);
    if (mKeyMods & Qt::ControlModifier)
    {
        auto angle = [&origin](const QPointF& p) { return qRadiansToDegrees(std::atan2(p.y() - origin.y(), p.x() - origin.x())); };
        s.rot += angle(canvasPos) - angle(mKeyPress);
    }
    else if (mKeyMods & Qt::ShiftModifier)
    {
        const qreal from = QLineF(origin, mKeyPress).length();
        const qreal to = QLineF(origin, canvasPos).length();
        // A press on the origin gives no ratio; 1% matches the Scale field's minimum.
        if (from > 0) { s.scale = qMax(0.01, s.scale * to / from); }
    }
    else
    {
        s.tx += canvasPos.x() - mKeyPress.x();
        s.ty += canvasPos.y() - mKeyPress.y();
    }
    return s;
}

Layer* MoveTool::currentPaintableLayer()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
        return nullptr;
    if (!layer->isPaintable())
        return nullptr;
    return layer;
}

QCursor MoveTool::cursor(MoveMode mode) const
{
    QPixmap cursorPixmap = QPixmap(24, 24);

    cursorPixmap.fill(QColor(255, 255, 255, 0));
    QPainter cursorPainter(&cursorPixmap);
    cursorPainter.setRenderHint(QPainter::Antialiasing);

    switch(mode)
    {
    case MoveMode::PERSP_LEFT:
    case MoveMode::PERSP_RIGHT:
    case MoveMode::PERSP_MIDDLE:
    case MoveMode::PERSP_SINGLE:
    case MoveMode::RULER_START:
    case MoveMode::RULER_END:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-move.svg"));
        break;
    }
    case MoveMode::TOPLEFT:
    case MoveMode::BOTTOMRIGHT:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-diagonal-left.svg"));
        break;
    }
    case MoveMode::TOPRIGHT:
    case MoveMode::BOTTOMLEFT:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-diagonal-right.svg"));
        break;
    }
    case MoveMode::SKEW_HORIZONTAL:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-horizontal.svg"));
        break;
    }
    case MoveMode::SKEW_VERTICAL:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-vertical.svg"));
        break;
    }
    case MoveMode::ROTATIONLEFT:
    case MoveMode::ROTATIONRIGHT:
    case MoveMode::ROTATION:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-rotate.svg"));
        break;
    }
    case MoveMode::MIDDLE:
    case MoveMode::CENTER:
    {
        cursorPainter.drawImage(QPoint(6,6),QImage("://icons/general/cursor-move.svg"));
        break;
    }
    default:
        return Qt::ArrowCursor;
    }
    cursorPainter.end();

    return QCursor(cursorPixmap);
}
