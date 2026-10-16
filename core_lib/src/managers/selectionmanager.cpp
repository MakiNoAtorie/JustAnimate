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
#include "selectionmanager.h"
#include "editor.h"

#include "vectorimage.h"

#include "mathutils.h"

#include <QVector2D>


SelectionManager::SelectionManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

SelectionManager::~SelectionManager()
{
}

bool SelectionManager::init()
{
    return true;
}

Status SelectionManager::load(Object*)
{
    resetSelectionProperties();
    return Status::OK;
}

Status SelectionManager::save(Object*)
{
    return Status::OK;
}

void SelectionManager::workingLayerChanged(Layer *)
{
}

void SelectionManager::resetSelectionTransformProperties()
{
    mRotatedAngle = 0;
    mTranslation = QPointF(0, 0);
    mScaleX = 1;
    mScaleY = 1;
    mSkewX = 0;
    mSkewY = 0;
    mAnchorPoint = QPoint();
    mSelectionTransform.reset();
}

void SelectionManager::resetSelectionTransform()
{
    mSelectionTransform.reset();
}

bool SelectionManager::isOutsideSelectionArea(const QPointF& point) const
{
    return (!mSelectionTransform.map(mSelectionPolygon).containsPoint(point, Qt::WindingFill)) && mMoveMode == MoveMode::NONE;
}

void SelectionManager::deleteSelection()
{
    emit needDeleteSelection();
}

qreal SelectionManager::selectionTolerance() const
{
    return qAbs(mSelectionTolerance * editor()->viewScaleInversed());
}

QPointF SelectionManager::getSelectionAnchorPoint() const
{
    QPointF anchorPoint;
    if (mSelectionPolygon.count() < 3) { return anchorPoint; }

    if (mMoveMode == MoveMode::BOTTOMRIGHT)
    {
        anchorPoint = mSelectionPolygon[0];
    }
    else if (mMoveMode == MoveMode::BOTTOMLEFT)
    {
        anchorPoint = mSelectionPolygon[1];
    }
    else if (mMoveMode == MoveMode::TOPLEFT)
    {
        anchorPoint = mSelectionPolygon[2];
    }
    else if (mMoveMode == MoveMode::TOPRIGHT)
    {
        anchorPoint = mSelectionPolygon[3];
    } else {
        anchorPoint = QLineF(mSelectionPolygon[0], mSelectionPolygon[2]).pointAt(.5);
    }
    return anchorPoint;
}


void SelectionManager::setMoveModeForAnchorInRange(const QPointF& point, bool skewHandles)
{
    if (mSelectionPolygon.count() < 4)
    {
        mMoveMode = MoveMode::NONE;
        return;
    }

    QPolygonF projectedPolygon = mapToSelection(mSelectionPolygon);

    const double calculatedSelectionTol = selectionTolerance();
    auto nearEdge = [&](int a, int b)
    {
        return QLineF(point, (projectedPolygon[a] + projectedPolygon[b]) / 2).length() < calculatedSelectionTol;
    };

    if (QLineF(point, projectedPolygon[0]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::TOPLEFT;
    }
    else if (QLineF(point, projectedPolygon[1]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::TOPRIGHT;
    }
    else if (QLineF(point, projectedPolygon[2]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::BOTTOMRIGHT;
    }
    else if (QLineF(point, projectedPolygon[3]).length() < calculatedSelectionTol)
    {
        mMoveMode = MoveMode::BOTTOMLEFT;
    }
    else if (skewHandles && (nearEdge(0, 1) || nearEdge(2, 3)))
    {
        mMoveMode = MoveMode::SKEW_HORIZONTAL;
    }
    else if (skewHandles && (nearEdge(1, 2) || nearEdge(3, 0)))
    {
        mMoveMode = MoveMode::SKEW_VERTICAL;
    }
    else if (projectedPolygon.containsPoint(point, Qt::WindingFill))
    {
        mMoveMode = MoveMode::MIDDLE;
    }
    else
    {
        mMoveMode = MoveMode::NONE;
    }
}

void SelectionManager::adjustSelection(const QPointF& currentPoint, const QPointF& offset, qreal rotationOffset, int rotationIncrement)
{
    switch (mMoveMode)
    {
    case MoveMode::MIDDLE: {
        QPointF newOffset = currentPoint - mDragOrigin;

        if (mLockAxis) {
            mTranslation = offset + alignPositionToAxis(newOffset);
        } else {
            mTranslation = offset + newOffset;
        }
        break;
    }
    case MoveMode::TOPLEFT:
    case MoveMode::TOPRIGHT:
    case MoveMode::BOTTOMRIGHT:
    case MoveMode::BOTTOMLEFT: {
        // The anchor is the opposite corner. Without translation, rotation and skew,
        // the cursor is the scaled offset of the dragged corner from the anchor.
        const QPointF local = rotationAndSkew().inverted().map(currentPoint);
        int corner = 3;
        if (mMoveMode == MoveMode::TOPLEFT) { corner = 0; }
        else if (mMoveMode == MoveMode::TOPRIGHT) { corner = 1; }
        else if (mMoveMode == MoveMode::BOTTOMRIGHT) { corner = 2; }
        const QPointF offset = mSelectionPolygon[corner] - mAnchorPoint;

        qreal scaleX = local.x() / offset.x();
        qreal scaleY = local.y() / offset.y();
        if (mAspectRatioFixed) {
            scaleY = (mScaleX < 0) == (mScaleY < 0) ? scaleX : -scaleX;
        }

        scale(scaleX, scaleY);

        break;
    }
    case MoveMode::SKEW_HORIZONTAL:
    case MoveMode::SKEW_VERTICAL: {
        // The anchor is the centre. Without translation and rotation, the dragged
        // edge midpoint sits at (skewX * y, y) or (x, skewY * x).
        const QTransform toLocal = rotationAndSkew(false).inverted();
        const QPointF local = toLocal.map(currentPoint);
        const QPointF origin = toLocal.map(mDragOrigin);
        if (mMoveMode == MoveMode::SKEW_HORIZONTAL && !qFuzzyIsNull(origin.y())) {
            mSkewX = local.x() / origin.y();
        } else if (mMoveMode == MoveMode::SKEW_VERTICAL && !qFuzzyIsNull(origin.x())) {
            mSkewY = local.y() / origin.x();
        }
        break;
    }
    case MoveMode::ROTATION: {
        rotate(rotationOffset, rotationIncrement);
        break;
    }
    default:
        break;
    }
    calculateSelectionTransformation();
}

void SelectionManager::translate(QPointF newPos)
{
    mTranslation += newPos;
}

void SelectionManager::rotate(qreal angle, qreal lockedAngle)
{
    if (lockedAngle > 0) {
        mRotatedAngle = constrainRotationToAngle(angle, lockedAngle);
    } else {
        mRotatedAngle = angle;
    }
}

void SelectionManager::scale(qreal sX, qreal sY)
{
    if (qFuzzyIsNull(sX)) {
        // Scale must not become 0
        sX = 0.0001;
    }
    if (qFuzzyIsNull(sY)) {
        // Scale must not become 0
        sY = 0.0001;
    }

    mScaleX = sX;
    mScaleY = sY;
}

int SelectionManager::constrainRotationToAngle(const qreal rotatedAngle, const int rotationIncrement) const
{
    return qRound(rotatedAngle / rotationIncrement) * rotationIncrement;
}

qreal SelectionManager::angleFromPoint(const QPointF& point, const QPointF& anchorPoint) const
{
    return qRadiansToDegrees(MathUtils::getDifferenceAngle(mSelectionTransform.map(anchorPoint), point));
}

void SelectionManager::setSelection(QRectF rect, bool roundPixels)
{
    resetSelectionTransformProperties();
    if (roundPixels)
    {
        rect = rect.toAlignedRect();
    }
    mSelectionPolygon = rect;
    mOriginalRect = rect;
    mScaleX = 1;
    mScaleY = 1;
    mRotatedAngle = 0;

    emit selectionChanged();
}

void SelectionManager::setTransformAnchor(const QPointF& point)
{
    QPointF newPos = mapToSelection(point);
    QPointF oldPos = mapToSelection(mAnchorPoint);

    // Adjust translation based on anchor point to avoid moving the selection
    mTranslation = mTranslation - oldPos + newPos;
    mAnchorPoint = point;
}

QTransform SelectionManager::rotationAndSkew(bool withSkew) const
{
    QTransform k;
    if (withSkew) { k.shear(mSkewX, mSkewY); }
    QTransform r;
    r.rotate(mRotatedAngle);
    QTransform t2;
    t2.translate(mTranslation.x(), mTranslation.y());
    return k * r * t2;
}

void SelectionManager::calculateSelectionTransformation()
{
    QTransform t;
    t.translate(-mAnchorPoint.x(), -mAnchorPoint.y());
    QTransform s;
    s.scale(mScaleX, mScaleY);
    mSelectionTransform = t * s * rotationAndSkew();
}

QPointF SelectionManager::alignPositionToAxis(QPointF currentPoint) const
{
    if (qAbs(currentPoint.y()) > qAbs(currentPoint.x())) {
        // Align to y axis
        return QPointF(0, currentPoint.y());
    }

    // Align to x axis
    return QPointF(currentPoint.x(), 0);
}

/**
 * @brief ScribbleArea::flipSelection
 * flip selection along the X or Y axis
*/
void SelectionManager::flipSelection(bool flipVertical)
{
    if (flipVertical)
    {
        mScaleY = -mScaleY;
    }
    else
    {
        mScaleX = -mScaleX;
    }
    setTransformAnchor(mOriginalRect.center());
    calculateSelectionTransformation();
    emit selectionChanged();
}

void SelectionManager::resetSelectionProperties()
{
    resetSelectionTransformProperties();
    mSelectionPolygon = QPolygonF();
    mOriginalRect = QRectF();
    mMoveMode = MoveMode::NONE;
    emit selectionChanged();
}

