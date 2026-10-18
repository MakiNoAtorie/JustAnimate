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

#include "scribblearea.h"

#include "mathutils.h"

#include <algorithm>
#include <cmath>
#include <QGuiApplication>
#include <QStyleHints>
#include <QKeySequence>
#include <QPainterPath>
#include <QtMath>
#include <QPixmapCache>
#include <QTimer>

#include "basetool.h"
#include "brushpresets.h"
#include "stroketool.h"
#include "transformtool.h"
#include "pointerevent.h"
#include "stageprofiler.h"
#include "beziercurve.h"
#include "object.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layercamera.h"
#include "bitmapimage.h"
#include "vectorimage.h"
#include "blitrect.h"
#include "tile.h"

#include "onionskinpainteroptions.h"

#include "colormanager.h"
#include "toolmanager.h"
#include "layermanager.h"
#include "pencilsettings.h"
#include "playbackmanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "overlaymanager.h"

ScribbleArea::ScribbleArea(QWidget* parent) : QWidget(parent),
    mCanvasPainter(mCanvas),
    mCameraPainter(mCanvas)
{
    setObjectName("ScribbleArea");

    // Qt::WA_StaticContents ensure that the widget contents are rooted to the top-left corner
    // and don't change when the widget is resized.
    setAttribute(Qt::WA_StaticContents);
}

ScribbleArea::~ScribbleArea()
{
}

bool ScribbleArea::init()
{
    mPrefs = mEditor->preference();
    mMouseFilterTimer = new QTimer(this);

    connect(mPrefs, &PreferenceManager::optionChanged, this, &ScribbleArea::settingUpdated);
    connect(mEditor->tools(), &ToolManager::toolChanged, this, &ScribbleArea::onToolChanged);
    for (int t = 0; t < TOOL_TYPE_COUNT; ++t)
    {
        // The HUD ring follows the brush size.
        if (auto* tool = dynamic_cast<StrokeTool*>(mEditor->tools()->getTool(static_cast<ToolType>(t))))
        {
            connect(tool, &StrokeTool::propertyChanged, this, [this](int id) {
                if (id == StrokeToolProperties::WIDTH_VALUE) { update(); }
            });
            connect(&tool->buffer().tiles(), &TiledBuffer::tileUpdated, this, &ScribbleArea::onTileUpdated);
            connect(&tool->buffer().tiles(), &TiledBuffer::tileCreated, this, &ScribbleArea::onTileCreated);
        }
    }

    connect(mMouseFilterTimer, &QTimer::timeout, this, &ScribbleArea::tabletReleaseEventFired);

    // The canvas shows every notice, so it reaches the user with the status bar hidden.
    mNoteTimer = new QTimer(this);
    mNoteTimer->setSingleShot(true);
    mNoteTimer->setInterval(3000);
    connect(mNoteTimer, &QTimer::timeout, this, [this] { mNote.clear(); update(); });
    connect(mEditor, &Editor::statusMessage, this, &ScribbleArea::showNote);

    connect(mEditor->select(), &SelectionManager::selectionChanged, this, &ScribbleArea::onSelectionChanged);
    connect(mEditor->select(), &SelectionManager::needDeleteSelection, this, &ScribbleArea::deleteSelection);

    mMouseFilterTimer->setInterval(50);

    const int curveSmoothingLevel = mPrefs->getInt(SETTING::CURVE_SMOOTHING);
    mCurveSmoothingLevel = curveSmoothingLevel / 20.0; // default value is 1.0

    mMakeInvisible = false;

    mLayerVisibility = static_cast<LayerVisibility>(mPrefs->getInt(SETTING::LAYER_VISIBILITY));

    mDeltaFactor = mEditor->preference()->isOn(SETTING::INVERT_SCROLL_ZOOM_DIRECTION) ? -1 : 1;

    setMouseTracking(true); // reacts to mouse move events, even if the button is not pressed
    setTabletTracking(true); // tablet tracking first added in 5.9

    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));

    QPixmapCache::setCacheLimit(100 * 1024); // unit is kb, so it's 100MB cache
    mPixmapCacheKeys.clear();

    return true;
}

void ScribbleArea::settingUpdated(SETTING setting)
{
    switch (setting)
    {
    case SETTING::CURVE_SMOOTHING:
        setCurveSmoothing(mPrefs->getInt(SETTING::CURVE_SMOOTHING));
        break;
    case SETTING::ONION_PREV_FRAMES_NUM:
    case SETTING::ONION_NEXT_FRAMES_NUM:
    case SETTING::ONION_MIN_OPACITY:
    case SETTING::ONION_MAX_OPACITY:
        invalidateAllCache();
        break;
    case SETTING::ANTIALIAS:
    case SETTING::FAST_RENDER_LAYERS_ABOVE:
    case SETTING::GRID:
    case SETTING::GRID_SIZE_W:
    case SETTING::GRID_SIZE_H:
    case SETTING::OVERLAY_CENTER:
    case SETTING::OVERLAY_THIRDS:
    case SETTING::OVERLAY_GOLDEN:
    case SETTING::OVERLAY_SAFE:
    case SETTING::OVERLAY_PERSPECTIVE1:
    case SETTING::OVERLAY_PERSPECTIVE2:
    case SETTING::OVERLAY_PERSPECTIVE3:
    case SETTING::ACTION_SAFE_ON:
    case SETTING::ACTION_SAFE:
    case SETTING::TITLE_SAFE_ON:
    case SETTING::TITLE_SAFE:
    case SETTING::OVERLAY_SAFE_HELPER_TEXT_ON:
    case SETTING::PREV_ONION:
    case SETTING::NEXT_ONION:
    case SETTING::ONION_BLUE:
    case SETTING::ONION_RED:
    case SETTING::INVISIBLE_LINES:
    case SETTING::OUTLINES:
    case SETTING::ONION_TYPE:
    case SETTING::ONION_FALLOFF:
    case SETTING::ONION_PREV_COLOR:
    case SETTING::ONION_NEXT_COLOR:
    case SETTING::ONION_WHILE_PLAYBACK:
        invalidateAllCache();
        break;
    case SETTING::ONION_MUTLIPLE_LAYERS:
        invalidateAllCache();
        break;
    case SETTING::LAYER_VISIBILITY_THRESHOLD:
    case SETTING::LAYER_VISIBILITY:
        setLayerVisibility(static_cast<LayerVisibility>(mPrefs->getInt(SETTING::LAYER_VISIBILITY)));
        break;
    case SETTING::INVERT_SCROLL_ZOOM_DIRECTION:
        mDeltaFactor = mEditor->preference()->isOn(SETTING::INVERT_SCROLL_ZOOM_DIRECTION) ? -1 : 1;
        break;
    default:
        break;
    }

}

void ScribbleArea::updateToolCursor()
{
    setCursor(currentTool()->cursor());
}

void ScribbleArea::setCurveSmoothing(int newSmoothingLevel)
{
    mCurveSmoothingLevel = newSmoothingLevel / 20.0;
    invalidatePainterCaches();
}

void ScribbleArea::setEffect(SETTING e, bool isOn)
{
    mPrefs->set(e, isOn);
    invalidatePainterCaches();
}

/************************************************************************************/
// update methods

void ScribbleArea::onTileUpdated(TiledBuffer* tiledBuffer, Tile* tile)
{
    Q_UNUSED(tiledBuffer);
    const QRectF& mappedRect = mEditor->view()->getView().mapRect(QRectF(tile->bounds()));
    update(mappedRect.toAlignedRect());
}

void ScribbleArea::onTileCreated(TiledBuffer* tiledBuffer, Tile* tile)
{
    Q_UNUSED(tiledBuffer)
    const QRectF& mappedRect = mEditor->view()->getView().mapRect(QRectF(tile->bounds()));
    update(mappedRect.toAlignedRect());
}

void ScribbleArea::updateFrame()
{
    if (currentTool()->isActive() && currentTool()->isDrawingTool()) {
        return;
    }

    update();
}

void ScribbleArea::invalidateCacheForDirtyFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    for (int pos : currentLayer->dirtyFrames()) {

        invalidateCacheForFrame(pos);
        invalidateOnionSkinsCacheAround(pos);
    }
    currentLayer->clearDirtyFrames();
}

void ScribbleArea::invalidateOnionSkinsCacheAround(int frameNumber)
{
    if (frameNumber < 0) { return; }

    const bool isOnionAbsolute = mPrefs->isOnionAbsolute();
    const MarkFilter filter = mPrefs->markFilter();
    Layer *layer = mEditor->layers()->currentLayer(0);

    // The current layer can be null if updateFrame is triggered when creating a new project
    if (!layer) return;

    if (mPrefs->isOn(SETTING::PREV_ONION))
    {
        int onionFrameNumber = frameNumber;
        if (isOnionAbsolute)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber + 1, true);
        }

        for(int i = 1; i <= mPrefs->getInt(SETTING::ONION_PREV_FRAMES_NUM); i++)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isOnionAbsolute, filter);
            if (onionFrameNumber < 0) break;

            invalidateCacheForFrame(onionFrameNumber);
        }
    }

    if (mPrefs->isOn(SETTING::NEXT_ONION))
    {
        int onionFrameNumber = frameNumber;

        for(int i = 1; i <= mPrefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM); i++)
        {
            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, isOnionAbsolute, filter);
            if (onionFrameNumber < 0) break;

            invalidateCacheForFrame(onionFrameNumber);
        }
    }
}

void ScribbleArea::invalidateAllCache()
{
    if (currentTool()->isDrawingTool() && currentTool()->isActive()) { return; }

    QPixmapCache::clear();
    mPixmapCacheKeys.clear();
    invalidatePainterCaches();
    mEditor->layers()->currentLayer()->clearDirtyFrames();

    updateFrame();
}

void ScribbleArea::invalidateCacheForFrame(int frameNumber)
{
    if (frameNumber < 0) { return; }

    // The frame cache is keyed by frame and the edit does not name its layer:
    // drop every cached frame that any layer's key at frameNumber holds.
    std::vector<std::pair<const Layer*, const KeyFrame*>> holds;
    int start = frameNumber;
    const Object* object = mEditor->object();
    for (int i = 0; object && i < object->getLayerCount(); ++i)
    {
        const Layer* layer = object->getLayer(i);
        if (const KeyFrame* key = layer->getLastKeyFrameAtPosition(frameNumber))
        {
            holds.emplace_back(layer, key);
            start = std::min(start, key->pos());
        }
    }
    auto it = mPixmapCacheKeys.lowerBound(static_cast<unsigned int>(start));
    while (it != mPixmapCacheKeys.end())
    {
        const int frame = static_cast<int>(it.key());
        const bool held = frame == frameNumber || std::any_of(holds.begin(), holds.end(), [frame](const auto& h) {
            return h.first->getLastKeyFrameAtPosition(frame) == h.second;
        });
        if (held)
        {
            QPixmapCache::remove(it.value());
            it = mPixmapCacheKeys.erase(it);
        }
        else if (frame > frameNumber)
        {
            break; // holds are contiguous: none reaches past this frame
        }
        else
        {
            ++it;
        }
    }
}

void ScribbleArea::invalidatePainterCaches()
{
    mCameraPainter.resetCache();
    mCanvasPainter.resetLayerCache();
    updateFrame();
}

void ScribbleArea::onToolChanged(ToolType)
{
    int frame = mEditor->currentFrame();
    prepOverlays(frame);
    prepCameraPainter(frame);
    invalidateCacheForFrame(frame);
    updateFrame();
}


void ScribbleArea::onPlayStateChanged()
{
    int currentFrame = mEditor->currentFrame();
    if (mPrefs->isOn(SETTING::PREV_ONION) ||
        mPrefs->isOn(SETTING::NEXT_ONION)) {
        invalidatePainterCaches();
    }

    prepOverlays(currentFrame);
    prepCameraPainter(currentFrame);
    invalidateCacheForFrame(currentFrame);
    updateFrame();
}

void ScribbleArea::onScrubbed(int frameNumber)
{
    Q_UNUSED(frameNumber)
    clearShiftAndTraceOffsets();
    invalidatePainterCaches();
    updateFrame();
}

void ScribbleArea::onFramesModified()
{
    invalidateCacheForDirtyFrames();
    if (mPrefs->isOn(SETTING::PREV_ONION) || mPrefs->isOn(SETTING::NEXT_ONION)) {
        invalidatePainterCaches();
    }
    updateFrame();
}

void ScribbleArea::onFrameModified(int frameNumber)
{
    if (mPrefs->isOn(SETTING::PREV_ONION) || mPrefs->isOn(SETTING::NEXT_ONION)) {
        invalidateOnionSkinsCacheAround(frameNumber);
        invalidatePainterCaches();
    }
    invalidateCacheForFrame(frameNumber);
    updateFrame();
}

void ScribbleArea::onViewChanged()
{
    invalidateAllCache();
}

void ScribbleArea::onLayerChanged()
{
    clearShiftAndTraceOffsets();
    invalidateAllCache();
}

void ScribbleArea::onSelectionChanged()
{
    int currentFrame = mEditor->currentFrame();
    invalidateCacheForFrame(currentFrame);
    updateFrame();
}

void ScribbleArea::onOnionSkinTypeChanged()
{
    invalidateAllCache();
}

void ScribbleArea::onObjectLoaded()
{
    clearShiftAndTraceOffsets();
    invalidateAllCache();
}

bool ScribbleArea::event(QEvent *event)
{
    bool processed = false;
    if (event->type() == QEvent::WindowDeactivate)
    {
        editor()->tools()->clearTemporaryTool();
        processed = true;
    } else if (event->type() == QEvent::Enter)
    {
        emit requestFocus(this);

        processed = currentTool()->enterEvent(static_cast<QEnterEvent*>(event)) || processed;
    } else if (event->type() == QEvent::Leave)
    {
        processed = currentTool()->leaveEvent(event) || processed;
    }

    return QWidget::event(event) || processed;
}

/************************************************************************/
/* key event handlers                                                   */
/************************************************************************/

void ScribbleArea::keyPressEvent(QKeyEvent *event)
{
    // Don't handle this event on auto repeat
    if (event->isAutoRepeat()) { return; }

    if (isPointerInUse()) { return; } // prevents shortcuts calls while drawing

    if (mRadialMenu.kind != RadialMenu::Closed && event->key() == Qt::Key_Escape)
    {
        mRadialMenu = RadialMenu();
        update();
        return;
    }

    if (isPenFlipKey(event))
    {
        mEditor->playback()->beginPenFlip();
        return;
    }

    if (currentTool()->keyPressEvent(event))
    {
        return; // has been handled by tool
    }

    // --- fixed control key shortcuts ---
    if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
        editor()->tools()->setTemporaryTool(ERASER, {}, event->modifiers()))
    {
        return;
    }

    // ---- fixed normal keys ----

    auto selectMan = mEditor->select();
    bool isSomethingSelected = selectMan->somethingSelected();
    if (isSomethingSelected)
    {
        keyEventForSelection(event);
    }
    else
    {
        keyEvent(event);
    }
}

void ScribbleArea::keyEventForSelection(QKeyEvent* event)
{
    auto selectMan = mEditor->select();
    auto nudge = [&](QPointF offset)
    {
        if (mEditor->refusesEdit(mEditor->layers()->currentLayer())) { return; }
        Edit edit = mEditor->edit(mEditor->layers()->currentLayer(), mEditor->currentFrame(), tr("Nudge selection", "Undo step: arrow key moves the selection"));
        selectMan->translate(offset);
        selectMan->calculateSelectionTransformation();
        edit.commit();
        emit mEditor->frameModified(mEditor->currentFrame());
    };
    switch (event->key())
    {
    case Qt::Key_Right:
        nudge(QPointF(1, 0));
        return;
    case Qt::Key_Left:
        nudge(QPointF(-1, 0));
        return;
    case Qt::Key_Up:
        nudge(QPointF(0, -1));
        return;
    case Qt::Key_Down:
        nudge(QPointF(0, 1));
        return;
    case Qt::Key_Return:
        applyTransformedSelection();
        mEditor->deselectAll();
        return;
    case Qt::Key_Escape:
        cancelTransformedSelection();
        mEditor->deselectAll();
        return;
    case Qt::Key_Backspace:
        deleteSelection();
        mEditor->deselectAll();
        return;
    case Qt::Key_Space:
        if (editor()->tools()->setTemporaryTool(HAND, Qt::Key_Space, Qt::NoModifier))
        {
            return;
        }
        break;
    default:
        break;
    }
    event->ignore();
}

void ScribbleArea::keyEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Right:
        mEditor->scrubForward();
        break;
    case Qt::Key_Left:
        mEditor->scrubBackward();
        break;
    case Qt::Key_Up:
        mEditor->layers()->gotoNextLayer();
        break;
    case Qt::Key_Down:
        mEditor->layers()->gotoPreviouslayer();
        break;
    case Qt::Key_Space:
        if(editor()->tools()->setTemporaryTool(HAND, Qt::Key_Space, Qt::NoModifier))
        {
            return;
        }
        break;
    default:
        break;
    }
    event->ignore();
}

void ScribbleArea::keyReleaseEvent(QKeyEvent *event)
{
    // Don't handle this event on auto repeat
    //
    if (event->isAutoRepeat()) {
        return;
    }

    if (event->key() == 0)
    {
        editor()->tools()->tryClearTemporaryTool(Qt::Key_unknown);
    }
    else
    {
        editor()->tools()->tryClearTemporaryTool(static_cast<Qt::Key>(event->key()));
    }

    if (isPenFlipKey(event))
    {
        mEditor->playback()->endPenFlip();
        return;
    }

    if (isPointerInUse()) { return; }

    if (currentTool()->keyReleaseEvent(event))
    {
        // has been handled by tool
        return;
    }
}

bool ScribbleArea::isPenFlipKey(const QKeyEvent* event) const
{
    const QKeySequence flipKey(pencilSettings().value("shortcuts/" CMD_FLIP_HOLD).toString());
    if (flipKey.isEmpty()) { return false; }
    // A release matches the key alone: the modifiers may have changed while it was held.
    return (event->type() == QEvent::KeyRelease) ? flipKey[0].key() == event->key()
                                                 : flipKey[0] == event->keyCombination();
}

void ScribbleArea::focusOutEvent(QFocusEvent* event)
{
    // A key released in another window never reaches this widget.
    mEditor->playback()->endPenFlip();
    QWidget::focusOutEvent(event);
}

/************************************************************************************/
// mouse and tablet event handlers
void ScribbleArea::wheelEvent(QWheelEvent* event)
{
    // Don't change view if the tool is in use
    if (isPointerInUse()) return;

    static const bool isX11 = QGuiApplication::platformName() == "xcb";
    const QPoint pixels = event->pixelDelta();
    const QPoint angle = event->angleDelta();
    ViewManager* view = mEditor->view();
    const Qt::KeyboardModifiers mods = event->modifiers();

    // From the pixelDelta documentation: On X11 this value is driver-specific and unreliable, use angleDelta() instead
    // A wheel notch (angle 120) counts as 40 px.
    const bool usePixels = !isX11 && !pixels.isNull();
    const QPointF d = usePixels ? QPointF(pixels) : QPointF(angle) / 3.0;
    // Some platforms turn Shift+wheel into a horizontal delta already.
    const qreal main = d.y() != 0 ? d.y() : d.x();

    if (mods & Qt::ControlModifier)
    {
        const qreal zoomDelta = usePixels ? pixels.y() : angle.y();
        if (zoomDelta != 0)
        {
            const QPointF offset = view->mapScreenToCanvas(event->position());
            const qreal newScale = view->scaling() * std::pow(100, (zoomDelta * mDeltaFactor) / (12.0 * 120));
            view->scaleAtOffset(newScale, offset);
        }
    }
    else if (mods & Qt::AltModifier)
    {
        // One notch turns the View 15 degrees.
        if (main != 0) view->rotateAtOffset(static_cast<float>(main * 15.0 / 40.0), event->position());
    }
    else if (mods & Qt::ShiftModifier)
    {
        if (main != 0) view->panByScreen(QPointF(main, 0));
    }
    else if (!d.isNull())
    {
        view->panByScreen(d);
    }
    event->accept();
}

void ScribbleArea::tabletEvent(QTabletEvent *e)
{
    if (mRadialMenu.kind != RadialMenu::Closed)
    {
        if (e->type() == QEvent::TabletRelease)
        {
            // Filter the mouse events Windows makes from this release.
            mTabletReleaseMillisAgo = 0;
            mMouseFilterTimer->start();
        }
        radialMenuEvent(e->type(), e->position());
        e->accept();
        return;
    }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->position()));

    if (event.pointerType() == QPointingDevice::PointerType::Eraser)
    {
        editor()->tools()->tabletSwitchToEraser();
    }
    else
    {
        editor()->tools()->tabletRestorePrevTool();
    }

    if (event.eventType() == PointerEvent::Press)
    {
        event.accept();

        if (!hasFocus()) {
            setFocus();
        }

        // Event timestamps give the elapsed time; a double click consumes both presses.
        const QStyleHints* hints = QGuiApplication::styleHints();
        const bool isDoubleClick = mHasTabletPress
            && e->timestamp() - mTabletPressTime <= static_cast<ulong>(hints->mouseDoubleClickInterval())
            && QLineF(e->position(), mTabletPressPos).length() <= hints->mouseDoubleClickDistance();
        mHasTabletPress = !isDoubleClick;
        mTabletPressPos = e->position();
        mTabletPressTime = e->timestamp();

        if (isDoubleClick)
        {
            currentTool()->pointerDoubleClickEvent(&event);
        }
        else
        {
            pointerPressEvent(&event);
        }
        mTabletInUse = event.isAccepted();
    }
    else if (event.eventType() == PointerEvent::Move)
    {
        if (!mTabletHasEntered && !hasFocus()) {
            emit requestFocus(this);
            mTabletHasEntered = true;
        }
        if (!(event.buttons() & (Qt::LeftButton | Qt::RightButton)) || mTabletInUse)
        {
            pointerMoveEvent(&event);
        }
    }
    else if (event.eventType() == PointerEvent::Release)
    {
        mTabletReleaseMillisAgo = 0;
        mMouseFilterTimer->start();
        if (mTabletInUse)
        {
            pointerReleaseEvent(&event);
            mTabletInUse = false;
        }
        mTabletHasEntered = false;
    }

#if defined Q_OS_LINUX
    // Generate mouse events on linux to work around bug where the
    // widget will not receive mouseEnter/mouseLeave
    // events and the cursor will not update correctly.
    // See https://codereview.qt-project.org/c/qt/qtbase/+/255384
    // Scribblearea should not do anything with the mouse event when mTabletInUse is true.
    event.ignore();
#else
    // Always accept so that mouse events are not generated (theoretically)
    // Unfortunately Windows sometimes generates the events anyway
    // As long as mTabletInUse is true, mouse events *should* be ignored even when
    // the are generated
    event.accept();
#endif
}

void ScribbleArea::pointerPressEvent(PointerEvent* event)
{
    StageScope stage(StageProfiler::InputEvent);
    if (mEditor->playback()->isPenFlipping() && event->button() == Qt::LeftButton)
    {
        mPenFlipDrag = true;
        mEditor->playback()->penFlipPress(event->viewportPos().x());
        return;
    }
    if (mShiftAndTrace && event->button() == Qt::LeftButton && shiftAndTracePress(event)) { return; }
    bool isCameraLayer = mEditor->layers()->currentLayer()->type() == Layer::CAMERA;
    if ((!currentTool()->isViewTool() || isCameraLayer) && (event->button() != Qt::RightButton) && (event->button() != Qt::MiddleButton || isCameraLayer))
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (mEditor->refuses(layer))
        {
            event->ignore();
            return;
        }
    }

    if (event->buttons() & (Qt::MiddleButton | Qt::RightButton) &&
        editor()->tools()->setTemporaryTool(HAND, event->buttons()))
    {
        currentTool()->pointerPressEvent(event);
    } else if (event->button() == Qt::LeftButton)
    {
        // A drawing tool cannot paint over a pending selection transform.
        if (currentTool()->isDrawingTool() && !mEditor->select()->selectionTransform().isIdentity())
        {
            applyTransformedSelection();
        }
        currentTool()->pointerPressEvent(event);
    }
}

void ScribbleArea::pointerMoveEvent(PointerEvent* event)
{
    StageScope stage(StageProfiler::InputEvent);
    if (mPenFlipDrag)
    {
        mEditor->playback()->penFlipMove(event->viewportPos().x());
        return;
    }
    if (mShiftAndTraceDrag.frame >= 0)
    {
        shiftAndTraceDrag(event);
        return;
    }
    currentTool()->pointerMoveEvent(event);
}

void ScribbleArea::pointerReleaseEvent(PointerEvent* event)
{
    StageScope stage(StageProfiler::InputEvent);
    if (mPenFlipDrag)
    {
        mPenFlipDrag = false;
        return;
    }
    if (mShiftAndTraceDrag.frame >= 0)
    {
        shiftAndTraceDrag(event);
        mShiftAndTraceDrag.frame = -1;
        return;
    }
    currentTool()->pointerReleaseEvent(event);

    editor()->tools()->tryClearTemporaryTool(event->button());
}

void ScribbleArea::tabletReleaseEventFired()
{
    // Under certain circumstances, a mouse press event will fire after a tablet release event.
    // This causes unexpected behaviors for some tools, e.g., the bucket tool.
    // The problem only seems to occur on Windows and only when tapping.
    // Prior to this fix, the event queue would look like this:
    // e.g.: TabletPress -> TabletRelease -> MousePress
    // The following will filter mouse events created after a tablet release event.
    mTabletReleaseMillisAgo += 50;

    if (mTabletReleaseMillisAgo >= MOUSE_FILTER_THRESHOLD) {
        mTabletReleaseMillisAgo = 0;
        mMouseFilterTimer->stop();
    }
}

bool ScribbleArea::isMouseFromTablet() const
{
    return mTabletInUse || (mMouseFilterTimer->isActive() && mTabletReleaseMillisAgo < MOUSE_FILTER_THRESHOLD);
}

void ScribbleArea::mousePressEvent(QMouseEvent* e)
{
    if (isMouseFromTablet()) { e->ignore(); return; }

    if (mRadialMenu.kind != RadialMenu::Closed) { radialMenuEvent(e->type(), e->position()); return; }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->position()));
    pointerPressEvent(&event);
    mMouseInUse = event.isAccepted();
}

void ScribbleArea::mouseMoveEvent(QMouseEvent* e)
{
    if (isMouseFromTablet()) { e->ignore(); return; }

    if (mRadialMenu.kind != RadialMenu::Closed) { radialMenuEvent(e->type(), e->position()); return; }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->position()));
    pointerMoveEvent(&event);
}

void ScribbleArea::mouseReleaseEvent(QMouseEvent* e)
{
    if (isMouseFromTablet()) { e->ignore(); return; }

    if (mRadialMenu.kind != RadialMenu::Closed) { radialMenuEvent(e->type(), e->position()); return; }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->position()));
    pointerReleaseEvent(&event);
    mMouseInUse = (e->buttons() & Qt::RightButton) || (e->buttons() & Qt::LeftButton);
}

void ScribbleArea::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (isMouseFromTablet()) { e->ignore(); return; }

    PointerEvent event(e, mEditor->view()->mapScreenToCanvas(e->position()));
    currentTool()->pointerDoubleClickEvent(&event);
}

void ScribbleArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    mDevicePixelRatio = devicePixelRatioF();
    mCanvas = QPixmap(QSizeF(size() * mDevicePixelRatio).toSize());
    mCanvas.setDevicePixelRatio(mDevicePixelRatio);
    mEditor->view()->setCanvasSize(size());

    // Every cached frame has the old size.
    QPixmapCache::clear();
    mPixmapCacheKeys.clear();
    invalidatePainterCaches();
    mCanvasPainter.reset();
    mCameraPainter.reset();
}

void ScribbleArea::showNote(const QString& text)
{
    mNote = text;
    mNoteTimer->start();
    update();
}

void ScribbleArea::handleDrawingOnEmptyFrame()
{
    auto layer = mEditor->layers()->currentLayer();

    if (!layer || !layer->isPaintable())
    {
        return;
    }

    if (currentTool()->type() == ERASER) {
        return;
    }

    int frameNumber = mEditor->currentFrame();
    if (layer->getKeyFrameAt(frameNumber)) { return; }

    // Drawing on an empty frame; take action based on preference.
    int action = mPrefs->getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION);
    auto previousKeyFrame = layer->getLastKeyFrameAtPosition(frameNumber);
    switch (action)
    {
    case KEEP_DRAWING_ON_PREVIOUS_KEY:
    {
        if (previousKeyFrame == nullptr) {
            mEditor->addNewKey();
        } else {
            onFrameModified(previousKeyFrame->pos());
        }
        break;
    }
    case DUPLICATE_PREVIOUS_KEY:
    {
        if (previousKeyFrame != nullptr) {
            KeyFrame* dupKey = previousKeyFrame->clone();
            dupKey->setLinkId(0); // a duplicate is independent; Alt-drag links
            layer->addKeyFrame(frameNumber, dupKey);
            mEditor->scrubTo(frameNumber);
            break;
        }
    }
    // if the previous keyframe doesn't exist,
    // an empty keyframe needs to be created, so
    // fallthrough
    case CREATE_NEW_KEY:
        mEditor->addNewKey();

        // Refresh canvas
        drawCanvas(frameNumber, mCanvas.rect());
        update();
        break;
    default:
        break;
    }
}

void ScribbleArea::paintEvent(QPaintEvent* event)
{
    StageScope stage(StageProfiler::PaintEvent);
    int currentFrame = mEditor->currentFrame();
    if (!currentTool()->isActive())
    {
        // --- we retrieve the canvas from the cache; we create it if it doesn't exist
        const int frameNumber = mEditor->layers()->lastFrameAtFrame(currentFrame);

        // The frame cache is keyed by frame: onion skins and the camera differ inside a hold. A transformed or in-between frame skips it (CanvasPainter caches by content).
        bool transformed = !mShiftAndTraceOffsets.isEmpty();
        for (int i = 0; i < mEditor->object()->getLayerCount() && !transformed; ++i)
        {
            const Layer* layer = mEditor->object()->getLayer(i);
            const KeyFrame* key = layer->getLastKeyFrameAtPosition(currentFrame);
            transformed = !layer->tweenStateAt(currentFrame).isIdentity() || (key && key->tween() && key->pos() != currentFrame);
        }

        if (frameNumber < 0 || transformed)
        {
            drawCanvas(currentFrame, event->rect());
        }
        else
        {
            auto cacheKeyIter = mPixmapCacheKeys.find(static_cast<unsigned>(currentFrame));

            if (cacheKeyIter == mPixmapCacheKeys.end() || !QPixmapCache::find(cacheKeyIter.value(), &mCanvas))
            {
                drawCanvas(currentFrame, event->rect());
                mPixmapCacheKeys[static_cast<unsigned>(currentFrame)] = QPixmapCache::insert(mCanvas);
                //qDebug() << "Repaint canvas!";
            }
            else
            {
                // Simply use the cached canvas from PixmapCache
            }
        }
    }
    else
    {
        prepCanvas(currentFrame);
        prepCameraPainter(currentFrame);
        prepOverlays(currentFrame);

        mCanvasPainter.paintCached(event->rect());
        mCameraPainter.paintCached(event->rect());
    }

    if (currentTool()->type() == MOVE)
    {
        Layer* layer = mEditor->layers()->currentLayer();
        Q_CHECK_PTR(layer);
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage != nullptr)
            {
                vectorImage->setModified(true);
            }
        }
    }

    QPainter painter(this);

    // paints the canvas
    painter.setWorldMatrixEnabled(false);

    // In other places we use the blitRect to paint the buffer pixmap, however
    // the main pixmap which needs to be scaled accordingly to DPI, which is not accounted for when using the event rect
    // instead we can set a clipRect to avoid the area being updated needlessly
    painter.setClipRect(event->rect());
    painter.drawPixmap(QPointF(), mCanvas);

    currentTool()->paint(painter, event->rect());
    if (mShiftAndTrace) { paintShiftAndTraceHandles(painter); }

    if (!editor()->playback()->isPlaying())    // we don't need to display the following when the animation is playing
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage != nullptr)
            {
                switch (currentTool()->type())
                {
                case SMUDGE:
                case HAND:
                {
                    auto selectMan = mEditor->select();
                    painter.save();
                    painter.setWorldMatrixEnabled(false);
                    painter.setRenderHint(QPainter::Antialiasing, false);
                    // ----- paints the edited elements
                    QPen pen2(Qt::black, 0.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                    painter.setPen(pen2);
                    QColor color;
                    // ------------ vertices of the edited curves
                    color = QColor(200, 200, 200);
                    painter.setBrush(color);
                    VectorSelection vectorSelection = selectMan->vectorSelection;
                    for (int k = 0; k < vectorSelection.curve.size(); k++)
                    {
                        int curveNumber = vectorSelection.curve.at(k);

                        for (int vertexNumber = -1; vertexNumber < vectorImage->getCurveSize(curveNumber); vertexNumber++)
                        {
                            QPointF vertexPoint = vectorImage->getVertex(curveNumber, vertexNumber);
                            QRectF rectangle(mEditor->view()->mapCanvasToScreen(vertexPoint) - QPointF(3.0, 3.0), QSizeF(7, 7));
                            if (rect().contains(mEditor->view()->mapCanvasToScreen(vertexPoint).toPoint()))
                            {
                                painter.drawRect(rectangle);
                            }
                        }
                    }
                    // ------------ selected vertices of the edited curves
                    color = QColor(100, 100, 255);
                    painter.setBrush(color);
                    for (int k = 0; k < vectorSelection.vertex.size(); k++)
                    {
                        VertexRef vertexRef = vectorSelection.vertex.at(k);
                        QPointF vertexPoint = vectorImage->getVertex(vertexRef);
                        QRectF rectangle0 = QRectF(mEditor->view()->mapCanvasToScreen(vertexPoint) - QPointF(3.0, 3.0), QSizeF(7, 7));
                        painter.drawRect(rectangle0);
                    }
                    // ----- paints the closest vertices
                    color = QColor(255, 0, 0);
                    painter.setBrush(color);
                    QList<VertexRef> closestVertices = selectMan->closestVertices();
                    if (vectorSelection.curve.size() > 0)
                    {
                        for (int k = 0; k < closestVertices.size(); k++)
                        {
                            VertexRef vertexRef = closestVertices.at(k);
                            QPointF vertexPoint = vectorImage->getVertex(vertexRef);

                            QRectF rectangle = QRectF(mEditor->view()->mapCanvasToScreen(vertexPoint) - QPointF(3.0, 3.0), QSizeF(7, 7));
                            painter.drawRect(rectangle);
                        }
                    }
                    painter.restore();
                    break;
                }
                default:
                {
                    break;
                }
                } // end switch
            }
        }

        mOverlayPainter.paint(painter, rect());
        paintCanvasHud(painter);
        paintNote(painter);
        paintRadialMenu(painter);

        // paints the selection outline
        if (mEditor->select()->somethingSelected())
        {
            paintSelectionVisuals(painter);
        }
    }

    // outlines the frame of the viewport
#ifdef _DEBUG
    painter.setWorldMatrixEnabled(false);
    painter.setPen(QPen(Qt::gray, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(0, 0, width(), height()));
#endif

    event->accept();
}

void ScribbleArea::paintSelectionVisuals(QPainter &painter)
{
    Object* object = mEditor->object();

    auto selectMan = mEditor->select();

    QRectF currentSelectionRect = selectMan->mySelectionRect();

    // The selection lives in the space of the current key.
    const QTransform keyView = mEditor->layers()->currentLayer()->tweenStateAt(mEditor->currentFrame()).transform() * editor()->view()->getView();
    TransformParameters params = { currentSelectionRect, keyView, selectMan->selectionTransform(),
                                   currentTool()->type() == MOVE };

    mSelectionPainter.paint(painter,
                            object,
                            mEditor->currentLayerIndex(),
                            static_cast<const TransformTool*>(editor()->tools()->getTool(SELECT))->transformSettings(),
                            params);
    emit selectionUpdated();
}

BitmapImage* ScribbleArea::currentBitmapImage(Layer* layer) const
{
    Q_ASSERT(layer->type() == Layer::BITMAP);
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    return bitmapLayer->getLastBitmapImageAtFrame(mEditor->currentFrame());
}

VectorImage* ScribbleArea::currentVectorImage(Layer* layer) const
{
    Q_ASSERT(layer->type() == Layer::VECTOR);
    auto vectorLayer = static_cast<LayerVector*>(layer);
    return vectorLayer->getLastVectorImageAtFrame(mEditor->currentFrame());
}

OnionSkinPainterOptions ScribbleArea::onionSkinOptions() const
{
    OnionSkinPainterOptions o;
    o.enabledWhilePlaying = mPrefs->getInt(SETTING::ONION_WHILE_PLAYBACK);
    o.isPlaying = mEditor->playback()->isPlaying();
    o.isAbsolute = mPrefs->isOnionAbsolute();
    o.markFilter = mPrefs->markFilter();
    o.skinPrevFrames = mPrefs->isOn(SETTING::PREV_ONION);
    o.skinNextFrames = mPrefs->isOn(SETTING::NEXT_ONION);
    o.colorizePrevFrames = mPrefs->isOn(SETTING::ONION_RED);
    o.colorizeNextFrames = mPrefs->isOn(SETTING::ONION_BLUE);
    o.framesToSkinPrev = mPrefs->getInt(SETTING::ONION_PREV_FRAMES_NUM);
    o.framesToSkinNext = mPrefs->getInt(SETTING::ONION_NEXT_FRAMES_NUM);
    o.maxOpacity = mPrefs->getInt(SETTING::ONION_MAX_OPACITY);
    o.minOpacity = mPrefs->getInt(SETTING::ONION_MIN_OPACITY);
    o.falloff = static_cast<OnionSkinFalloff>(mPrefs->getInt(SETTING::ONION_FALLOFF));
    o.prevColor = QColor(mPrefs->getString(SETTING::ONION_PREV_COLOR));
    o.nextColor = QColor(mPrefs->getString(SETTING::ONION_NEXT_COLOR));
    return o;
}

void ScribbleArea::prepCameraPainter(int frame)
{
    Object* object = mEditor->object();

    mCameraPainter.preparePainter(object,
                                  mEditor->currentLayerIndex(),
                                  frame,
                                  mEditor->view()->getView(),
                                  mEditor->playback()->isPlaying(),
                                  mLayerVisibility,
                                  mPrefs->getFloat(SETTING::LAYER_VISIBILITY_THRESHOLD),
                                  mEditor->view()->getScaleInversed());

    mCameraPainter.setOnionSkinPainterOptions(onionSkinOptions());
}

void ScribbleArea::prepCanvas(int frame)
{
    Object* object = mEditor->object();

    CanvasPainterOptions o;
    o.bOnionSkinMultiLayer = mPrefs->isOn(SETTING::ONION_MUTLIPLE_LAYERS);
    o.bAntiAlias = mPrefs->isOn(SETTING::ANTIALIAS);
    o.bThinLines = mPrefs->isOn(SETTING::INVISIBLE_LINES);
    o.bFastLayersAbove = mPrefs->isOn(SETTING::FAST_RENDER_LAYERS_ABOVE);
    o.bOutlines = mPrefs->isOn(SETTING::OUTLINES);
    o.eLayerVisibility = mLayerVisibility;
    o.fLayerVisibilityThreshold = mPrefs->getFloat(SETTING::LAYER_VISIBILITY_THRESHOLD);
    o.scaling = mEditor->view()->scaling();
    // A Stroke tool's buffer shows over the current layer as it will commit.
    auto* strokeTool = dynamic_cast<StrokeTool*>(currentTool());
    StrokeBuffer* buffer = strokeTool ? &strokeTool->buffer() : nullptr;
    if (buffer) { o.cmBufferBlendMode = buffer->compositionMode(); }

    mCanvasPainter.setOnionSkinOptions(onionSkinOptions());
    mCanvasPainter.setShiftAndTraceOffsets(mShiftAndTraceOffsets);
    mCanvasPainter.setOptions(o);

    ViewManager* vm = mEditor->view();
    SelectionManager* sm = mEditor->select();
    mCanvasPainter.setViewTransform(vm->getView(), vm->getViewInverse());
    mCanvasPainter.setTransformedSelection(sm->mySelectionRect().toRect(), sm->selectionTransform());

    mCanvasPainter.setPaintSettings(object, mEditor->layers()->currentLayerIndex(), frame, buffer ? &buffer->tiles() : nullptr);
}

void ScribbleArea::drawCanvas(int frame, QRect rect)
{
    prepCanvas(frame);
    prepCameraPainter(frame);
    prepOverlays(frame);
    StageScope stage(StageProfiler::Composite);
    mCanvasPainter.paint(rect);
    mCameraPainter.paint(rect);
}

void ScribbleArea::flipSelection(bool flipVertical)
{
    mEditor->select()->flipSelection(flipVertical);
}

void ScribbleArea::prepOverlays(int frame)
{
    OverlayPainterOptions o;

    o.bGrid = mPrefs->isOn(SETTING::GRID);
    o.nGridSizeW = mPrefs->getInt(SETTING::GRID_SIZE_W);
    o.nGridSizeH = mPrefs->getInt(SETTING::GRID_SIZE_H);
    o.bCenter = mPrefs->isOn(SETTING::OVERLAY_CENTER);
    o.bThirds = mPrefs->isOn(SETTING::OVERLAY_THIRDS);
    o.bGoldenRatio = mPrefs->isOn(SETTING::OVERLAY_GOLDEN);
    o.bSafeArea = mPrefs->isOn(SETTING::OVERLAY_SAFE);
    o.bPerspective1 = mPrefs->isOn(SETTING::OVERLAY_PERSPECTIVE1);
    o.bPerspective2 = mPrefs->isOn(SETTING::OVERLAY_PERSPECTIVE2);
    o.bPerspective3 = mPrefs->isOn(SETTING::OVERLAY_PERSPECTIVE3);
    o.nOverlayAngle = mPrefs->getInt(SETTING::OVERLAY_ANGLE);
    o.bActionSafe = mPrefs->isOn(SETTING::ACTION_SAFE_ON);
    o.nActionSafe = mPrefs->getInt(SETTING::ACTION_SAFE);
    o.bShowSafeAreaHelperText = mPrefs->isOn(SETTING::OVERLAY_SAFE_HELPER_TEXT_ON);
    o.bTitleSafe = mPrefs->isOn(SETTING::TITLE_SAFE_ON);
    o.nTitleSafe = mPrefs->getInt(SETTING::TITLE_SAFE);
    o.nOverlayAngle = mPrefs->getInt(SETTING::OVERLAY_ANGLE);
    o.bShowHandle = mEditor->tools()->currentTool()->type() == MOVE && mEditor->layers()->currentLayer()->type() != Layer::CAMERA;

    o.mSinglePerspPoint = mEditor->overlays()->getSinglePerspectivePoint();
    o.mLeftPerspPoint = mEditor->overlays()->getLeftPerspectivePoint();
    o.mRightPerspPoint = mEditor->overlays()->getRightPerspectivePoint();
    o.mMiddlePerspPoint = mEditor->overlays()->getMiddlePerspectivePoint();
    o.bRuler = mEditor->overlays()->isRulerEnabled();
    o.mRuler = mEditor->overlays()->ruler();

    o.nFrameIndex = frame;

    mOverlayPainter.setOptions(o);
    mOverlayPainter.preparePainter(mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex()), palette());

    ViewManager* vm = mEditor->view();
    mOverlayPainter.setViewTransform(vm->getView());
}

/************************************************************************************/
// view handling

QPointF ScribbleArea::getCentralPoint()
{
    return mEditor->view()->mapScreenToCanvas(QPointF(width() / 2, height() / 2));
}

void ScribbleArea::applyTransformedSelection()
{
    mCanvasPainter.ignoreTransformedSelection();

    Layer* layer = mEditor->layers()->currentLayer();

    bool useAA = mEditor->tools()->getTool(ToolType::MOVE)->toolProperties().getInfo(TransformToolProperties::ANTI_ALIASING_ENABLED).boolValue();

    if (layer == nullptr) { return; }

    auto selectMan = mEditor->select();
    if (selectMan->somethingSelected())
    {
        if (selectMan->mySelectionRect().isEmpty() || selectMan->selectionTransform().isIdentity()) { return; }

        if (layer->type() == Layer::BITMAP)
        {
            handleDrawingOnEmptyFrame();
            BitmapImage* bitmapImage = currentBitmapImage(layer);
            if (bitmapImage == nullptr) { return; }
            BitmapImage transformedImage = bitmapImage->transformed(selectMan->mySelectionRect().toRect(), selectMan->selectionTransform(), useAA);


            bitmapImage->clear(selectMan->mySelectionRect());
            bitmapImage->paste(&transformedImage, QPainter::CompositionMode_SourceOver);
        }
        else if (layer->type() == Layer::VECTOR)
        {
            // Unfortunately this doesn't work right currently so vector transforms
            // will always be applied on the previous keyframe when on an empty frame
            //handleDrawingOnEmptyFrame();
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage == nullptr) { return; }

            // The image's own transform is only set on paint; an undo or redo step applies before that.
            vectorImage->applySelectionTransformation(selectMan->selectionTransform());
        }

        // The pixels now hold the transform: keep the selection on them with an identity transform.
        selectMan->setSelection(selectMan->mapToSelection(QPolygonF(selectMan->mySelectionRect())).boundingRect());
        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
    }

    updateFrame();
}

void ScribbleArea::cancelTransformedSelection()
{
    mCanvasPainter.ignoreTransformedSelection();

    auto selectMan = mEditor->select();
    if (selectMan->somethingSelected())
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer == nullptr) { return; }

        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            if (vectorImage != nullptr)
            {
                vectorImage->setSelectionTransformation(QTransform());
            }
        }

        mEditor->select()->setSelection(selectMan->mySelectionRect(), false);

        selectMan->resetSelectionProperties();
        mOriginalPolygonF = QPolygonF();

        mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
        updateFrame();
    }
}

void ScribbleArea::toggleThinLines()
{
    bool previousValue = mPrefs->isOn(SETTING::INVISIBLE_LINES);
    setEffect(SETTING::INVISIBLE_LINES, !previousValue);
}

void ScribbleArea::setLayerVisibility(LayerVisibility visibility)
{
    mLayerVisibility = visibility;
    mPrefs->set(SETTING::LAYER_VISIBILITY, static_cast<int>(mLayerVisibility));

    invalidateAllCache();
}

void ScribbleArea::increaseLayerVisibilityIndex()
{
    ++mLayerVisibility;
    mPrefs->set(SETTING::LAYER_VISIBILITY, static_cast<int>(mLayerVisibility));

    invalidateAllCache();
}

void ScribbleArea::decreaseLayerVisibilityIndex()
{
    --mLayerVisibility;
    mPrefs->set(SETTING::LAYER_VISIBILITY, static_cast<int>(mLayerVisibility));

    invalidateAllCache();
}

/************************************************************************************/
// tool handling

BaseTool* ScribbleArea::currentTool() const
{
    return editor()->tools()->currentTool();
}

CanvasHud ScribbleArea::canvasHud() const
{
    ViewManager* view = mEditor->view();
    CanvasHud hud;
    if (const StrokeTool* tool = mEditor->tools()->currentStrokeTool())
    {
        hud.ringRadius = tool->get(ToolProperty::Width) * 0.5 * view->scaling();
    }
    hud.zoomPercent = qRound(view->scaling() * 100);
    hud.rotation = view->rotation();
    return hud;
}

namespace
{
constexpr qreal kShiftAndTraceArm = 40; // screen px from the move handle up to the rotate handle
constexpr qreal kShiftAndTraceRadius = 7;
}

void ScribbleArea::setShiftAndTrace(bool on)
{
    mShiftAndTrace = on;
    clearShiftAndTraceOffsets();
    update();
}

void ScribbleArea::clearShiftAndTraceOffsets()
{
    mShiftAndTraceOffsets.clear();
    mShiftAndTraceDrag.frame = -1;
}

QVector<int> ScribbleArea::shiftAndTraceFrames() const
{
    QVector<int> frames;
    Layer* layer = mEditor->layers()->currentLayer();
    if (!mShiftAndTrace || mEditor->playback()->isPlaying() || layer == nullptr || !mEditor->object()->isLayerVisible(layer)) { return frames; }
    if (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR) { return frames; }
    for (const OnionSkinSubPainter::Neighbour& n : OnionSkinSubPainter().neighbours(layer, onionSkinOptions(), mEditor->currentFrame()))
    {
        frames.append(n.frame);
    }
    return frames;
}

QPointF ScribbleArea::shiftAndTraceHandle(int frame) const
{
    Layer* layer = mEditor->layers()->currentLayer();
    QRectF bounds;
    if (layer->type() == Layer::BITMAP)
    {
        if (BitmapImage* image = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(frame))
        {
            image->loadFile();
            bounds = image->bounds();
        }
    }
    else if (layer->type() == Layer::VECTOR)
    {
        if (VectorImage* image = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(frame))
        {
            for (int i = 0; i <= image->getLastCurveNumber(); ++i) { bounds |= image->curve(i).getBoundingRect(); }
        }
    }
    const QPointF centre = shiftAndTraceOffset(frame).map(layer->tweenStateAt(frame).transform().map(bounds.center()));
    return mEditor->view()->mapCanvasToScreen(centre);
}

QPointF ScribbleArea::shiftAndTraceRotateHandle(int frame) const
{
    return shiftAndTraceHandle(frame) - QPointF(0, kShiftAndTraceArm);
}

bool ScribbleArea::shiftAndTracePress(const PointerEvent* event)
{
    const QPointF pos = event->viewportPos();
    for (int frame : shiftAndTraceFrames())
    {
        const QPointF handle = shiftAndTraceHandle(frame);
        const bool rotate = QLineF(pos, shiftAndTraceRotateHandle(frame)).length() <= kShiftAndTraceRadius;
        if (!rotate && QLineF(pos, handle).length() > kShiftAndTraceRadius) { continue; }
        mShiftAndTraceDrag = { frame, rotate, event->canvasPos(), mEditor->view()->mapScreenToCanvas(handle), shiftAndTraceOffset(frame) };
        return true;
    }
    return false;
}

void ScribbleArea::shiftAndTraceDrag(const PointerEvent* event)
{
    const ShiftAndTraceDrag& d = mShiftAndTraceDrag;
    const QPointF p = event->canvasPos();
    QTransform delta;
    if (d.rotate)
    {
        const QPointF a = d.press - d.pivot;
        const QPointF b = p - d.pivot;
        delta.translate(d.pivot.x(), d.pivot.y());
        delta.rotate(qRadiansToDegrees(std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x())));
        delta.translate(-d.pivot.x(), -d.pivot.y());
    }
    else
    {
        delta = QTransform::fromTranslate(p.x() - d.press.x(), p.y() - d.press.y());
    }
    mShiftAndTraceOffsets[d.frame] = d.start * delta;
    update();
}

void ScribbleArea::paintShiftAndTraceHandles(QPainter& painter) const
{
    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(palette().color(QPalette::Highlight), 1.5));
    painter.setBrush(Qt::NoBrush);
    const QPointF r(0, kShiftAndTraceRadius);
    for (int frame : shiftAndTraceFrames())
    {
        const QPointF handle = shiftAndTraceHandle(frame);
        const QPointF rotate = shiftAndTraceRotateHandle(frame);
        // Rings only, so the lines under the centres stay visible.
        painter.drawEllipse(handle, kShiftAndTraceRadius, kShiftAndTraceRadius);
        painter.drawEllipse(rotate, kShiftAndTraceRadius, kShiftAndTraceRadius);
        painter.drawLine(handle - r, rotate + r);
    }
    painter.restore();
}

void ScribbleArea::paintCanvasHud(QPainter& painter) const
{
    const CanvasHud hud = canvasHud();
    // shortcut: a ring wider than the box is drawn at the box size; the canvas cursor shows the full size.
    constexpr qreal kMaxRing = 24;
    constexpr qreal kDial = 8;
    constexpr qreal kPad = 6;
    const QString text = tr("%1%  %2%3").arg(hud.zoomPercent).arg(qRound(hud.rotation)).arg(QChar(0x00B0));
    const QFontMetricsF fm(painter.font());
    const qreal ringBox = hud.ringRadius > 0 ? 2 * kMaxRing + kPad : 0;
    const qreal w = kPad + ringBox + 2 * kDial + kPad + fm.horizontalAdvance(text) + kPad;
    const qreal h = qMax(ringBox > 0 ? 2 * kMaxRing : 2 * kDial, fm.height()) + 2 * kPad;
    const QRectF box(12, height() - 12 - h, w, h);

    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 140));
    painter.drawRoundedRect(box, 6, 6);

    painter.setPen(QPen(Qt::white, 1));
    painter.setBrush(Qt::NoBrush);
    qreal x = box.left() + kPad;
    const qreal cy = box.center().y();
    if (ringBox > 0)
    {
        const qreal r = qMin(hud.ringRadius, kMaxRing);
        painter.drawEllipse(QPointF(x + kMaxRing, cy), r, r);
        x += ringBox;
    }
    // The needle points to canvas up; the view turns clockwise on screen.
    const QPointF c(x + kDial, cy);
    painter.drawEllipse(c, kDial, kDial);
    QLineF needle(c, c + QPointF(kDial, 0));
    needle.setAngle(90 - hud.rotation);
    painter.drawLine(needle);
    x += 2 * kDial + kPad;
    painter.drawText(QRectF(x, box.top(), box.right() - x, box.height()), Qt::AlignVCenter | Qt::AlignLeft, text);
    painter.restore();
}

void ScribbleArea::paintNote(QPainter& painter) const
{
    if (mNote.isEmpty()) { return; }
    constexpr qreal kPad = 8;
    const QFontMetricsF fm(painter.font());
    const qreal w = qMin(fm.horizontalAdvance(mNote) + 2 * kPad, width() - 24.0);
    const QRectF box((width() - w) / 2, 12, w, fm.height() + 2 * kPad);

    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QColor base = palette().color(QPalette::ToolTipBase);
    base.setAlpha(230);
    painter.setBrush(base);
    painter.drawRoundedRect(box, 6, 6);
    painter.setPen(palette().color(QPalette::ToolTipText));
    painter.drawText(box.adjusted(kPad, 0, -kPad, 0), Qt::AlignCenter, fm.elidedText(mNote, Qt::ElideRight, w - 2 * kPad));
    painter.restore();
}

namespace
{
// The toolbox tools; Camera is chosen by the camera layer.
constexpr ToolType kRadialTools[] = { PENCIL, ERASER, SELECT, MOVE, HAND, ROTATE, SMUDGE, PEN,
                                      POLYLINE, BUCKET, EYEDROPPER, BRUSH, SHAPE, LASSOFILL };
// shortcut: the picker shows the first presets only; a paged ring when users keep more.
constexpr int kMaxRadialPresets = 16;
}

void ScribbleArea::openToolMenu(QPointF center)
{
    if (isPointerInUse()) { return; }
    mRadialMenu = RadialMenu();
    mRadialMenu.kind = RadialMenu::Tools;
    mRadialMenu.center = center;
    for (ToolType t : kRadialTools)
    {
        mRadialMenu.labels << BaseTool::TypeName(t);
    }
    update();
}

void ScribbleArea::openPresetPicker(QPointF center, const QString& presetFolder)
{
    if (isPointerInUse() || !mEditor->tools()->currentStrokeTool()) { return; }
    const QString tool = BrushPresets::toolKey(mEditor->tools()->currentStrokeTool()->type());
    const QStringList names = BrushPresets(presetFolder).names(tool).mid(0, kMaxRadialPresets);
    if (names.isEmpty()) { return; }
    mPresetFolder = presetFolder;
    mRadialMenu = RadialMenu();
    mRadialMenu.kind = RadialMenu::Presets;
    mRadialMenu.center = center;
    mRadialMenu.labels = names;
    update();
}

void ScribbleArea::radialMenuEvent(QEvent::Type type, QPointF pos)
{
    mRadialMenu.hovered = MathUtils::radialSliceAt(pos - mRadialMenu.center, static_cast<int>(mRadialMenu.labels.size()),
                                        RadialMenu::kInner, RadialMenu::kOuter);
    update();
    if (type != QEvent::TabletRelease && type != QEvent::MouseButtonRelease) { return; }

    const RadialMenu menu = mRadialMenu;
    mRadialMenu = RadialMenu();
    if (menu.hovered < 0) { return; }
    ToolManager* tools = mEditor->tools();
    if (menu.kind == RadialMenu::Tools)
    {
        tools->setCurrentTool(kRadialTools[menu.hovered]);
    }
    else if (StrokeTool* tool = tools->currentStrokeTool())
    {
        if (BrushPresets(mPresetFolder).apply(menu.labels.at(menu.hovered), *tool))
        {
            emit tools->toolsReset(); // the tool options show the new values
        }
    }
}

void ScribbleArea::paintRadialMenu(QPainter& painter) const
{
    const RadialMenu& menu = mRadialMenu;
    const int n = static_cast<int>(menu.labels.size());
    if (menu.kind == RadialMenu::Closed || n == 0) { return; }

    const qreal step = 360.0 / n;
    const qreal mid = (RadialMenu::kInner + RadialMenu::kOuter) / 2;
    const QRectF outer(menu.center - QPointF(RadialMenu::kOuter, RadialMenu::kOuter),
                       QSizeF(2 * RadialMenu::kOuter, 2 * RadialMenu::kOuter));
    const QRectF inner(menu.center - QPointF(RadialMenu::kInner, RadialMenu::kInner),
                       QSizeF(2 * RadialMenu::kInner, 2 * RadialMenu::kInner));
    const QFontMetricsF fm(painter.font());
    const qreal labelWidth = qMax(2 * M_PI * mid / n - 4, 2 * RadialMenu::kInner);

    painter.save();
    painter.setWorldMatrixEnabled(false);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 255, 255, 90), 1));
    for (int i = 0; i < n; ++i)
    {
        // Qt angles run counter-clockwise from 3 o'clock; slice i is centred at i * step clockwise from up.
        const qreal start = 90 - i * step + step / 2;
        QPainterPath slice;
        slice.arcMoveTo(outer, start);
        slice.arcTo(outer, start, -step);
        slice.arcTo(inner, start - step, step);
        slice.closeSubpath();
        painter.setBrush(i == menu.hovered ? QColor(70, 130, 220, 220) : QColor(0, 0, 0, 170));
        painter.drawPath(slice);
    }

    painter.setPen(Qt::white);
    for (int i = 0; i < n; ++i)
    {
        const qreal a = qDegreesToRadians(i * step);
        const QPointF c = menu.center + QPointF(std::sin(a), -std::cos(a)) * mid;
        const QString text = fm.elidedText(menu.labels.at(i), Qt::ElideRight, labelWidth);
        painter.drawText(QRectF(c.x() - labelWidth / 2, c.y() - fm.height() / 2, labelWidth, fm.height()),
                         Qt::AlignCenter, text);
    }
    painter.restore();
}

void ScribbleArea::deleteSelection()
{
    auto selectMan = mEditor->select();
    if (selectMan->somethingSelected())      // there is something selected
    {
        Layer* layer = mEditor->layers()->currentLayer();
        if (layer == nullptr || mEditor->refusesEdit(layer)) { return; }

        const QString description = tr("Delete Selection", "Undo Step: clear the selection area.");
        // A Drawing added for this delete undoes with it in one step.
        Edit step = mEditor->edit(description);
        handleDrawingOnEmptyFrame();

        Edit drawing = mEditor->edit(layer, mEditor->currentFrame(), description);

        selectMan->clearCurves();
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = currentVectorImage(layer);
            Q_CHECK_PTR(vectorImage);
            vectorImage->deleteSelection();
        }
        else if (layer->type() == Layer::BITMAP)
        {
            BitmapImage* bitmapImage = currentBitmapImage(layer);
            Q_CHECK_PTR(bitmapImage);
            bitmapImage->clear(selectMan->mySelectionRect());
        }
        drawing.commit();
        step.commit();
        mEditor->setModified(mEditor->currentLayerIndex(), mEditor->currentFrame());
    }
}

void ScribbleArea::clearImage()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || mEditor->refusesEdit(layer)) { return; }

    if (layer->type() == Layer::VECTOR)
    {
        Edit edit = mEditor->edit(layer, mEditor->currentFrame(), tr("Clear Image", "Undo step text"));

        VectorImage* vectorImage = currentVectorImage(layer);
        if (vectorImage != nullptr)
        {
            vectorImage->clear();
        }
        mEditor->select()->clearCurves();
        mEditor->select()->clearVertices();
        edit.commit();
    }
    else if (layer->type() == Layer::BITMAP)
    {

        BitmapImage* bitmapImage = currentBitmapImage(layer);
        if (bitmapImage == nullptr) return;
        Edit edit = mEditor->edit(layer, mEditor->currentFrame(), tr("Clear Image", "Undo step text"));
        bitmapImage->clear();
        edit.commit();
    }
    else
    {
        return; // skip updates when nothing changes
    }
    mEditor->setModified(mEditor->layers()->currentLayerIndex(), mEditor->currentFrame());
}

void ScribbleArea::paletteColorChanged(QColor color)
{
    Q_UNUSED(color)

    for (int i = 0; i < mEditor->layers()->count(); i++)
    {
        Layer* layer = mEditor->layers()->getLayer(i);
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(mEditor->currentFrame());
            if (vectorImage != nullptr)
            {
                vectorImage->modification();
            }
        }
    }

    invalidateAllCache();
}
