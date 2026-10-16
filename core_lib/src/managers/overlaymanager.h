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

#ifndef OVERLAYMANAGER_H
#define OVERLAYMANAGER_H

#include "pencildef.h"
#include "movemode.h"
#include "basemanager.h"

#include <QLineF>
#include <QPointF>

#include "preferencesdef.h"

class Editor;

class OverlayManager : public BaseManager
{
    Q_OBJECT

public:
    explicit OverlayManager(Editor* editor);
    ~OverlayManager() override;

    bool init() override;
    Status load(Object *o) override;
    Status save(Object *o) override;

    void settingsUpdated(SETTING setting, bool state);

    void workingLayerChanged(Layer *) override;

    MoveMode getMoveModeForPoint(const QPointF& pos, const QTransform& transform);
    double selectionTolerance();

    void updatePerspective(const QPointF& point);
    /// Moves the ruler end that the move mode names to a canvas point.
    void updateRuler(const QPointF& point);

    MoveMode getMoveMode() const { return mMoveMode; }
    void setMoveMode(MoveMode mode) { mMoveMode = mode; }
    QPointF getSinglePerspectivePoint() const { return mSinglePerspectivePoint; }
    QPointF getLeftPerspectivePoint() const { return mLeftPerspectivePoint; }
    QPointF getRightPerspectivePoint() const { return mRightPerspectivePoint; }
    QPointF getMiddlePerspectivePoint() const { return mMiddlePerspectivePoint; }

    /// Canvas line that strokes starting near it snap to, when enabled.
    QLineF ruler() const { return mRuler; }
    void setRuler(const QLineF& line) { mRuler = line; }
    bool isRulerEnabled() const { return mRulerEnabled; }
    void setRulerEnabled(bool enabled) { mRulerEnabled = enabled; }
    /// Strokes snap to lines toward the vanishing points of the visible perspective overlays.
    bool isPerspectiveSnapEnabled() const { return mPerspectiveSnapEnabled; }
    void setPerspectiveSnapEnabled(bool enabled) { mPerspectiveSnapEnabled = enabled; }

    bool anyOverlayEnabled() const { return mRulerEnabled || mSinglePerspectiveEnabled || mTwoPointPerspectiveEnabled || mThreePointPerspectiveEnabled; }

private:
    Editor* mEditor = nullptr;

    QPointF mSinglePerspectivePoint;   // for single point perspective.
    QPointF mLeftPerspectivePoint;
    QPointF mRightPerspectivePoint;    // Left, right and middle are for
    QPointF mMiddlePerspectivePoint;   // two and three point perspective

    MoveMode mMoveMode = MoveMode::NONE;

    QLineF mRuler { -300.0, 0.0, 300.0, 0.0 };
    bool mRulerEnabled = false;
    bool mPerspectiveSnapEnabled = false;

    bool mSinglePerspectiveEnabled = false;
    bool mTwoPointPerspectiveEnabled = false;
    bool mThreePointPerspectiveEnabled = false;

    const qreal mSelectionTolerance = 8.0;
};

#endif // OVERLAYMANAGER_H
