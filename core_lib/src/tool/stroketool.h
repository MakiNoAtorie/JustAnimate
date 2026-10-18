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
#include "edit.h"
#include "strokebuffer.h"

#include "canvascursorpainter.h"
#include "radialoffsettool.h"

#include <QImage>
#include <QList>
#include <QPointF>

#include <optional>


class StrokeTool : public BaseTool
{
    Q_OBJECT

public:
    explicit StrokeTool(QObject* parent);
    ~StrokeTool();

    ToolProperties& toolProperties() override { return mProperties; }
    const ToolProperties& properties() const { return mProperties; }
    template <typename T>
    T get(ToolPropertyId<T> id) const { return mProperties.get(id); }
    template <typename T, typename V, typename = std::enable_if_t<kToolPropertyAccepts<T, V>>>
    void set(ToolPropertyId<T> id, V value) { setValue(id.raw, QVariant(static_cast<T>(value))); }
    /// Sets a registered property, converted to its type and clamped to its range, and emits propertyChanged.
    void setValue(int id, const QVariant& value);

    void startStroke(PointerEvent::InputType inputType);
    void drawStroke();
    void endStroke();
    /** Ends the stroke's undo step; without commit it records only the Drawing added for the stroke. */
    void endStrokeEdit(bool commit);

    /// The stroke the canvas shows over the current layer.
    StrokeBuffer& buffer() { return mBuffer; }

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
    /// The cross; the Brush outline around it is painted by paint().
    QCursor cursor() override;
    /// Below this size on screen the Brush outline hides and only the cross shows.
    static constexpr qreal OUTLINE_MIN_SCREEN_PX = 6.0;

    /// Loads an image tip from a PNG or .gbr file; an empty name restores the generated tip.
    /// On failure the tip does not change.
    Status setTipImage(const QString& fileName);
    /// The file of the current image tip; empty for the generated tip.
    QString tipImagePath() const { return mTipImagePath; }
    /// A pressure curve string (`PressureCurve::fromString`) that overrides the global
    /// preference, set by a preset; empty uses the global curve.
    void setPressureCurve(const QString& curve) { mPressureCurveOverride = curve; }
    QString pressureCurve() const { return mPressureCurveOverride; }

signals:
    void propertyChanged(int id);

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
    /// The outline of one dab of the base width and tip at a canvas point.
    QPainterPath brushOutline(const QPointF& point) const;

    /// Writes the buffer into the current bitmap Drawing and repaints it.
    void commitBuffer();
    /// Adds the stroke points as a curve to the current vector Drawing and selects only that curve.
    /// Returns the Drawing; nullptr without stroke points or a Drawing.
    VectorImage* commitVectorStroke(qreal width, qreal feather, bool invisible, bool variableWidth);
    /// Draws a vector stroke preview, given in screen space, into the buffer.
    void drawScreenPath(const QPainterPath& path, const QPen& pen);

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
    /// How this tool's strokes land on a bitmap layer.
    virtual StrokeBuffer::Mode bufferMode() const { return StrokeBuffer::Mode::Paint; }

    bool mCanvasCursorEnabled = true; // false while the pointer is off the canvas
    bool mOutlineStyle = true; // SETTING::CURSOR_STYLE is CURSOR_BRUSH_OUTLINE
    QPointF mLastPixel { 0, 0 };

    StrokeInterpolator mInterpolator;
    QImage mTipImage; // null for the generated tip
    QString mTipImagePath;
    QString mPressureCurveOverride;

    static const qreal FEATHER_MIN;
    static const qreal FEATHER_MAX;

    // The step joins a Drawing added for the stroke; the inner Edit records the stroke itself.
    std::optional<Edit> mStrokeStep;
    std::optional<Edit> mStrokeEdit;
    StrokeBuffer mBuffer;
    ToolProperties mProperties;

private:
    CanvasCursorPainter mWidthCursorPainter;
    CanvasCursorPainter mFeatherCursorPainter;

    RadialOffsetTool mWidthSizingTool;
    RadialOffsetTool mFeatherSizingTool;
};

#endif // STROKETOOL_H
