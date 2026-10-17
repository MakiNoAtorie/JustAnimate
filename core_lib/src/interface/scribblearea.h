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


#ifndef SCRIBBLEAREA_H
#define SCRIBBLEAREA_H

#include <cstdint>
#include <ctime>
#include <deque>
#include <memory>

#include <QColor>
#include <QMap>
#include <QPoint>
#include <QStringList>
#include <QWidget>
#include <QPixmapCache>

#include "movemode.h"
#include "pencildef.h"
#include "bitmapimage.h"
#include "canvaspainter.h"
#include "overlaypainter.h"
#include "preferencemanager.h"
#include "selectionpainter.h"
#include "camerapainter.h"
#include "tiledbuffer.h"

class Layer;
class Editor;
class BaseTool;
class PointerEvent;
class BitmapImage;
class VectorImage;
struct OnionSkinPainterOptions;

/// What the canvas HUD shows, in screen units.
struct CanvasHud
{
    qreal ringRadius = 0; ///< brush radius on screen in px; 0 when the tool has no brush size
    int zoomPercent = 100;
    qreal rotation = 0; ///< view rotation in degrees
};

/// A ring of slices on the canvas: the radial tool menu or the quick preset picker.
struct RadialMenu
{
    enum Kind { Closed, Tools, Presets };
    static constexpr qreal kInner = 30; ///< screen px; a release inside closes the menu
    static constexpr qreal kOuter = 130;
    Kind kind = Closed;
    QPointF center; ///< widget px
    QStringList labels; ///< slice 0 at the top, then clockwise
    int hovered = -1;
};


class ScribbleArea : public QWidget
{
    Q_OBJECT

    friend class MoveTool;
    friend class BucketTool;

public:
    ScribbleArea(QWidget* parent);
    ~ScribbleArea() override;

    bool init();
    void setEditor(Editor* e) { mEditor = e; }
    Editor* editor() const { return mEditor; }

    void deleteSelection();

    void applyTransformedSelection();
    void cancelTransformedSelection();

    void setEffect(SETTING e, bool isOn);

    LayerVisibility getLayerVisibility() const { return mLayerVisibility; }
    qreal getCurveSmoothing() const { return mCurveSmoothingLevel; }
    bool makeInvisible() const { return mMakeInvisible; }

    QPointF getCentralPoint();

    /** Update frame.
     * calls update() behind the scene and update cache if necessary */
    void updateFrame();

    /** Frame scrubbed, invalidate relevant cache */
    void onScrubbed(int frameNumber);

    /** Multiple frames modified, invalidate cache for affected frames */
    void onFramesModified();

    /** Playstate changed, invalidate relevant cache */
    void onPlayStateChanged();

    /** View updated, invalidate relevant cache */
    void onViewChanged();

    /** Frame modified, invalidate cache for frame if any */
    void onFrameModified(int frameNumber);

    /** Layer changed, invalidate relevant cache */
    void onLayerChanged();

    /** Selection was changed, keep cache */
    void onSelectionChanged();

    /** Onion skin type changed, all frames will be affected.
     * All cache will be invalidated */
    void onOnionSkinTypeChanged();

    /** Object updated, invalidate all cache */
    void onObjectLoaded();

    /** Tool changed, invalidate cache and frame if needed */
    void onToolChanged(ToolType);

    void endStroke();

    void flipSelection(bool flipVertical);

    BaseTool* currentTool() const;

    CanvasHud canvasHud() const;

    /// Opens the radial tool menu around `center` (widget px). The next pen or mouse
    /// release picks the slice under it, or closes the menu off the ring.
    void openToolMenu(QPointF center);
    /// Like openToolMenu, with the brush presets in `presetFolder` (empty = default folder).
    void openPresetPicker(QPointF center, const QString& presetFolder = QString());
    const RadialMenu& radialMenu() const { return mRadialMenu; }

    bool isMouseInUse() const { return mMouseInUse; }
    bool isTabletInUse() const { return mTabletInUse; }
    bool isPointerInUse() const { return mMouseInUse || mTabletInUse; }

    /// Shift and Trace: the onion neighbours of the current Drawing move and rotate in the View only.
    void setShiftAndTrace(bool on);
    bool isShiftAndTrace() const { return mShiftAndTrace; }
    /// The onion frames of the current layer that show handles.
    QVector<int> shiftAndTraceFrames() const;
    /// Canvas-space offset of an onion frame, applied after its tween.
    QTransform shiftAndTraceOffset(int frame) const { return mShiftAndTraceOffsets.value(frame); }
    /// Widget positions of a neighbour's move and rotate handles.
    QPointF shiftAndTraceHandle(int frame) const;
    QPointF shiftAndTraceRotateHandle(int frame) const;

    void keyEvent(QKeyEvent* event);
    void keyEventForSelection(QKeyEvent* event);

    /// The notice the canvas shows now, or empty.
    QString note() const { return mNote; }

signals:
    void multiLayerOnionSkinChanged(bool);
    void selectionUpdated();

    void requestFocus(QWidget* widget);

public slots:
    void clearImage();
    void setCurveSmoothing(int);
    void toggleThinLines();
    void increaseLayerVisibilityIndex();
    void decreaseLayerVisibilityIndex();
    void setLayerVisibility(LayerVisibility visibility);

    void updateToolCursor();
    void paletteColorChanged(QColor);

    /// Shows a short notice on the canvas for a few seconds.
    void showNote(const QString& text);
    void onTileUpdated(TiledBuffer* tiledBuffer, Tile* tile);
    void onTileCreated(TiledBuffer* tiledBuffer, Tile* tile);

protected:
    bool event(QEvent *event) override;
    void tabletEvent(QTabletEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

public:
    void drawPolyline(QPainterPath path, QPen pen, bool useAA);
    void drawPath(QPainterPath path, QPen pen, QBrush brush, QPainter::CompositionMode cm);
    void drawPen(QPointF thePoint, qreal brushWidth, QColor fillColor, bool useAA = true);
    void drawPencil(QPointF thePoint, qreal brushWidth, qreal fixedBrushFeather, QColor fillColor, qreal opacity, const BrushTip& tip = {});
    void drawBrush(QPointF thePoint, qreal brushWidth, qreal offset, QColor fillColor, qreal opacity, bool usingFeather = true, bool useAA = false, const BrushTip& tip = {});
    void setWashOpacity(qreal opacity) { mTiledBuffer.setWashOpacity(opacity); }
    /** One smudge dab on the current bitmap frame; the first dab of a stroke starts the smudge buffer. */
    void smudgeDab(QPointF point, qreal width, qreal feather, QColor color, qreal opacity, const BrushTip& tip,
                   const SmudgeSettings& smudge);

    void paintBitmapBuffer();
    QPainter::CompositionMode bufferCompositionMode() const;
    void clearDrawingBuffer();

    void pointerPressEvent(PointerEvent*);
    void pointerMoveEvent(PointerEvent*);
    void pointerReleaseEvent(PointerEvent*);

    /// Call this when starting to use a paint tool. Checks whether we are drawing
    /// on an empty frame, and if so, takes action according to use preference.
    void handleDrawingOnEmptyFrame();

    TiledBuffer mTiledBuffer;
private:

    /** Invalidate the layer pixmap and camera painter caches.
     * Call this in most situations where the layer rendering order is affected.
     * Peviously known as setAllDirty.
    */
    void invalidatePainterCaches();

    /** Invalidate cache for the given frame */
    void invalidateCacheForFrame(int frameNumber);

    /** Invalidate all cache.
     * call this if you're certain that the change you've made affects all frames */
    void invalidateAllCache();

    /** invalidate cache for dirty keyframes. */
    void invalidateCacheForDirtyFrames();

    /** invalidate onion skin cache around frame */
    void invalidateOnionSkinsCacheAround(int frame);

    void prepOverlays(int frame);
    OnionSkinPainterOptions onionSkinOptions() const;
    void prepCameraPainter(int frame);
    void prepCanvas(int frame);
    void drawCanvas(int frame, QRect rect);
    void settingUpdated(SETTING setting);
    void paintSelectionVisuals(QPainter &painter);
    void paintCanvasHud(QPainter& painter) const;
    void paintNote(QPainter& painter) const;
    void paintRadialMenu(QPainter& painter) const;
    /// Feeds a pointer event at `pos` (widget px) to the open radial menu.
    void radialMenuEvent(QEvent::Type type, QPointF pos);
    /// Whether this key event is the Flip key (`CMD_FLIP_HOLD`).
    bool isPenFlipKey(const QKeyEvent* event) const;

    bool shiftAndTracePress(const PointerEvent* event);
    void shiftAndTraceDrag(const PointerEvent* event);
    void clearShiftAndTraceOffsets();
    void paintShiftAndTraceHandles(QPainter& painter) const;

    BitmapImage* currentBitmapImage(Layer* layer) const;
    VectorImage* currentVectorImage(Layer* layer) const;

    Editor* mEditor = nullptr;

    LayerVisibility mLayerVisibility = LayerVisibility::ALL;
    bool mMakeInvisible = false;
    qreal mCurveSmoothingLevel = 0.0;
    int mDeltaFactor = 1;

    /* Under certain circumstances a mouse press event will fire after a tablet release event.
       This causes unexpected behaviours for some of the tools, eg. the bucket.
       The problem only seems to occur on windows and only when tapping.
       prior to this fix the event queue would look like this:
       eg: TabletPress -> TabletRelease -> MousePress
       The following will filter mouse events created after a tablet release event.
    */
    void tabletReleaseEventFired();
    /// True while mouse events come from the pen: during a tablet stroke and just after its release.
    bool isMouseFromTablet() const;
    bool mMouseInUse = false;
    bool mTabletInUse = false;
    bool mPenFlipDrag = false; // this pen stroke drives a Pen-drag Flip, not the tool

    bool mShiftAndTrace = false;
    QMap<int, QTransform> mShiftAndTraceOffsets; // View state only, never in Object
    struct ShiftAndTraceDrag { int frame = -1; bool rotate = false; QPointF press; QPointF pivot; QTransform start; };
    ShiftAndTraceDrag mShiftAndTraceDrag; // canvas space; frame -1 when no handle is held
    qreal mDevicePixelRatio = 1.;

    // Double click handling for tablet input
    bool mHasTabletPress = false;
    ulong mTabletPressTime = 0;
    // Microsoft suggests that a double click action should be no more than 500 ms
    const ulong DOUBLE_CLICK_THRESHOLD = 500;
    QPointF mTabletPressPos;
    RadialMenu mRadialMenu;
    QString mPresetFolder;
    int mTabletReleaseMillisAgo;
    const int MOUSE_FILTER_THRESHOLD = 200;
    bool mTabletHasEntered = false;

    QTimer* mMouseFilterTimer = nullptr;
    QTimer* mNoteTimer = nullptr;
    QString mNote;

    PreferenceManager* mPrefs = nullptr;

    QPixmap mCanvas;
    CanvasPainter mCanvasPainter;
    OverlayPainter mOverlayPainter;
    SelectionPainter mSelectionPainter;
    CameraPainter mCameraPainter;

    QPolygonF mOriginalPolygonF = QPolygonF();

    // Pixmap Cache keys
    QMap<unsigned int, QPixmapCache::Key> mPixmapCacheKeys;

    // debug
    QLoggingCategory mLog{ "ScribbleArea" };
};

#endif
