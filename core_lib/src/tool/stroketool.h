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

#ifndef STROKETOOL_H
#define STROKETOOL_H

#include "basetool.h"
#include "pencilerror.h"
#include "pointerevent.h"
#include "preferencesdef.h"
#include "strokeinterpolator.h"
#include "undoredomanager.h"

#include "canvascursorpainter.h"
#include "radialoffsettool.h"

#include <QImage>
#include <QList>
#include <QPointF>


class StrokeTool : public BaseTool
{
    Q_OBJECT

public:
    explicit StrokeTool(QObject* parent);
    ~StrokeTool();

    virtual const StrokeToolProperties& strokeToolProperties() const = 0;

    void startStroke(PointerEvent::InputType inputType);
    void drawStroke();
    void endStroke();

    bool leavingThisTool() override;
    bool enteringThisTool() override;

    void updateCanvasCursor();

    void loadSettings() override;
    bool isActive() const override { return mInterpolator.isActive(); }

    bool keyPressEvent(QKeyEvent* event) override;
    void pointerPressEvent(PointerEvent* event) override;
    void pointerMoveEvent(PointerEvent* event) override;
    void pointerReleaseEvent(PointerEvent* event) override;
    bool enterEvent(QEnterEvent*) override;
    bool leaveEvent(QEvent*) override;

    bool handleQuickSizing(PointerEvent* event);

    void paint(QPainter& painter, const QRect& blitRect) override;

    virtual void setSmoothingMode(int mode);
    virtual void setSmoothingStrength(int strength);
    virtual void setWidth(qreal width);
    virtual void setFeather(qreal feather);
    virtual void setSpacing(qreal percent);
    /// Sets a dynamics or scatter property, such as StrokeToolProperties::SCATTER_VALUE.
    void setDynamic(int type, int value);
    void setTipSquare(bool square);
    void setTipAspect(qreal percent);
    void setTipAngle(qreal degrees);
    void setTipFollowsDirection(bool follow);
    /// Per-dab alpha in percent; overlapping dabs build up.
    void setFlow(qreal percent);
    /// Stroke alpha in percent; in wash mode no overlap goes past it.
    void setStrokeOpacity(qreal percent);
    void setWashEnabled(bool enabled);
    /// Loads an image tip from a PNG or .gbr file; an empty name restores the generated tip.
    /// On failure the tip does not change.
    Status setTipImage(const QString& fileName);
    /// The file of the current image tip; empty for the generated tip.
    QString tipImagePath() const { return mTipImagePath; }
    /// A pressure curve string (`PressureCurve::fromString`) that overrides the global
    /// preference, set by a preset; empty uses the global curve.
    void setPressureCurve(const QString& curve) { mPressureCurveOverride = curve; }
    QString pressureCurve() const { return mPressureCurveOverride; }
    virtual void setPressureEnabled(bool enabled);
    virtual void setFeatherEnabled(bool enabled);
    virtual void setAntiAliasingEnabled(bool enabled);
    virtual void setFillContourEnabled(bool enabled);
    virtual void setStrokeInvisibleEnabled(bool enabled);

signals:
    void widthChanged(qreal value);
    void featherChanged(qreal value);
    void spacingChanged(qreal value);
    void tipChanged();
    void dynamicsChanged();
    void flowChanged(qreal value);
    void strokeOpacityChanged(qreal value);
    void washEnabledChanged(bool enabled);
    void pressureEnabledChanged(bool enabled);
    void featherEnabledChanged(bool enabled);
    void antiAliasingEnabledChanged(bool enabled);
    void fillContourEnabledChanged(bool enabled);
    void invisibleStrokeEnabledChanged(bool enabled);
    void smoothingModeChanged(int mode);
    void smoothingStrengthChanged(int strength);

public slots:
    void onPreferenceChanged(SETTING setting);
    void onViewUpdated();

protected:
    QPointF getCurrentPressPixel() const;
    QPointF getCurrentPressPoint() const;
    QPointF getCurrentPixel() const;
    QPointF getCurrentPoint() const;
    QPointF getLastPixel() const;
    QPointF getLastPoint() const;

    QRectF cursorRect(StrokeToolProperties::Type settingType, const QPointF& point);

    static PropertyInfo smoothingModeInfo(SmoothingMode defaultMode);
    /// Sets smoothing from an old None/Simple/Strong stabilizer level.
    void setLegacySmoothing(int level);
    /// Maps the old LineStabilization setting once, while the tool has no SmoothingMode yet.
    void migrateLineStabilization(const QSettings& settings);
    /// Gives the interpolator this tool's smoothing and snap guides; call before its press event.
    void prepareInput(const QPointF& pressViewportPos);

    static bool mQuickSizingEnabled;

    QHash<Qt::KeyboardModifiers, int> mQuickSizingProperties;
    bool mFirstDraw = false;

    QList<QPointF> mStrokePoints;
    QList<qreal> mStrokePressures;

    qreal mCurrentWidth    = 0.0;
    qreal mCurrentPressure = 0.5;

    PointerEvent::InputType mCurrentInputType = PointerEvent::Unknown;

    /// Whether to enable the "drawing on empty frame" preference.
    /// If true, then the user preference is honored.
    /// If false, then the stroke is drawn on the previous key-frame (i.e. the
    /// "old" Pencil behaviour).
    /// Returns true by default.
    virtual bool emptyFrameActionEnabled();

    bool mCanvasCursorEnabled = false;
    QPointF mLastPixel { 0, 0 };

    StrokeInterpolator mInterpolator;
    QImage mTipImage; // null for the generated tip
    QString mTipImagePath;
    QString mPressureCurveOverride;

    static const qreal FEATHER_MIN;
    static const qreal FEATHER_MAX;
    static constexpr qreal SPACING_MIN = 1.0;
    static constexpr qreal SPACING_MAX = 400.0;
    static constexpr qreal TIP_ASPECT_MIN = 5.0;
    static const qreal WIDTH_MIN;
    static const qreal WIDTH_MAX;

    SAVESTATE_ID mUndoSaveStateId = 0;

private:
    CanvasCursorPainter mWidthCursorPainter;
    CanvasCursorPainter mFeatherCursorPainter;

    RadialOffsetTool mWidthSizingTool;
    RadialOffsetTool mFeatherSizingTool;
};

#endif // STROKETOOL_H
