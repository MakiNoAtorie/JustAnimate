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

#include "stroketool.h"

#include <QKeyEvent>
#include "scribblearea.h"
#include "viewmanager.h"
#include "preferencemanager.h"
#include "editor.h"
#include "toolmanager.h"
#include "mathutils.h"
#include "tiledbuffer.h"

#include "canvascursorpainter.h"

#ifdef Q_OS_MAC
extern "C" {
    void detectWhichOSX();
    void disableCoalescing();
    void enableCoalescing();
}
#else
extern "C" {
    void detectWhichOSX() {}
    void disableCoalescing() {}
    void enableCoalescing() {}
}
#endif

const qreal StrokeTool::FEATHER_MIN = 1.;
const qreal StrokeTool::FEATHER_MAX = 99.;
const qreal StrokeTool::WIDTH_MIN = 1.;
const qreal StrokeTool::WIDTH_MAX = 200.;

// ---- shared static variables ---- ( only one instance for all the tools )
bool StrokeTool::mQuickSizingEnabled = false;

StrokeTool::StrokeTool(QObject* parent) : BaseTool(parent)
{
    detectWhichOSX();
}

StrokeTool::~StrokeTool()
{
}

void StrokeTool::loadSettings()
{
    mQuickSizingEnabled = mEditor->preference()->isOn(SETTING::QUICK_SIZING);
    mCanvasCursorEnabled = mEditor->preference()->isOn(SETTING::CANVAS_CURSOR);

    QSettings pencilSettings(PENCIL2D, PENCIL2D);
    QHash<int, PropertyInfo> info;
    info[StrokeToolProperties::WIDTH_VALUE] = { WIDTH_MIN, WIDTH_MAX, 24.0 };
    info[StrokeToolProperties::FEATHER_VALUE] = { FEATHER_MIN, FEATHER_MAX, 48.0 };
    info[StrokeToolProperties::SPACING_VALUE] = { SPACING_MIN, SPACING_MAX, 12.5 };
    info[StrokeToolProperties::TIP_SQUARE_ENABLED] = false;
    info[StrokeToolProperties::TIP_ASPECT_VALUE] = { TIP_ASPECT_MIN, 100.0, 100.0 };
    info[StrokeToolProperties::TIP_ANGLE_VALUE] = { -180.0, 180.0, 0.0 };
    info[StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED] = false;
    info[StrokeToolProperties::FLOW_VALUE] = { 1.0, 100.0, 100.0 };
    info[StrokeToolProperties::OPACITY_VALUE] = { 1.0, 100.0, 100.0 };
    info[StrokeToolProperties::WASH_ENABLED] = false;
    info[StrokeToolProperties::SPEED_TO_SIZE_VALUE] = { -100, 100, 0 };
    info[StrokeToolProperties::TILT_TO_SIZE_VALUE] = { -100, 100, 0 };
    info[StrokeToolProperties::TILT_TO_ANGLE_VALUE] = { -100, 100, 0 };
    info[StrokeToolProperties::SIZE_JITTER_VALUE] = { 0, 100, 0 };
    info[StrokeToolProperties::SCATTER_VALUE] = { 0, 500, 0 };
    info[StrokeToolProperties::ANGLE_JITTER_VALUE] = { 0, 180, 0 };
    info[StrokeToolProperties::DAB_COUNT_VALUE] = { 1, 16, 1 };
    info[StrokeToolProperties::FLOW_PER_DAB_ENABLED] = false;
    info[StrokeToolProperties::TAPER_START_VALUE] = { 0, 1000, 0 };
    info[StrokeToolProperties::TAPER_END_VALUE] = { 0, 1000, 0 };
    info[StrokeToolProperties::FEATHER_ENABLED] = false;
    info[StrokeToolProperties::PRESSURE_ENABLED] = false;
    info[StrokeToolProperties::INVISIBILITY_ENABLED] = false;
    info[StrokeToolProperties::SMOOTHING_MODE] = smoothingModeInfo(SmoothingMode::OneEuro);
    info[StrokeToolProperties::SMOOTHING_STRENGTH] = { 0, 100, 50 };
    info[StrokeToolProperties::ANTI_ALIASING_ENABLED] = false;
    info[StrokeToolProperties::FILLCONTOUR_ENABLED] = false;

    toolProperties().insertProperties(info);
    toolProperties().loadFrom(typeName(), pencilSettings);

    /// Given the way that we update preferences currently, this connection should not be removed
    /// when the tool is not active.
    connect(mEditor->preference(), &PreferenceManager::optionChanged, this, &StrokeTool::onPreferenceChanged);

    connect(&mWidthSizingTool, &RadialOffsetTool::offsetChanged, this, [=](qreal offset) {
        setWidth(offset * 2.0);
    });

    connect(&mFeatherSizingTool, &RadialOffsetTool::offsetChanged, this, [=](qreal offset){
        const qreal inputMin = FEATHER_MIN;
        const qreal inputMax = strokeToolProperties().width() * 0.5;
        const qreal outputMax = FEATHER_MAX;
        const qreal outputMin = inputMin;

        // We map the feather value to a value between the min width and max width
        const qreal mappedValue = MathUtils::map(offset, inputMin, inputMax, outputMax, outputMin);

        setFeather(mappedValue);
    });
}

bool StrokeTool::enteringThisTool()
{
    mActiveConnections.append(connect(mEditor->view(), &ViewManager::viewChanged, this, &StrokeTool::onViewUpdated));
    return true;
}

bool StrokeTool::leavingThisTool()
{
    return BaseTool::leavingThisTool();
}

void StrokeTool::onPreferenceChanged(SETTING setting)
{
    if (setting == SETTING::QUICK_SIZING) {
        mQuickSizingEnabled = mEditor->preference()->isOn(setting);
    } else if (setting == SETTING::CANVAS_CURSOR) {
        mCanvasCursorEnabled = mEditor->preference()->isOn(setting);
    }
}

void StrokeTool::onViewUpdated()
{
    updateCanvasCursor();
}

QPointF StrokeTool::getCurrentPressPixel() const
{
    return mInterpolator.getCurrentPressPixel();
}

QPointF StrokeTool::getCurrentPressPoint() const
{
    return mEditor->view()->mapScreenToCanvas(mInterpolator.getCurrentPressPixel());
}

QPointF StrokeTool::getCurrentPixel() const
{
    return mInterpolator.getCurrentPixel();
}

QPointF StrokeTool::getCurrentPoint() const
{
    return mEditor->view()->mapScreenToCanvas(getCurrentPixel());
}

QPointF StrokeTool::getLastPixel() const
{
    return mInterpolator.getLastPixel();
}

QPointF StrokeTool::getLastPoint() const
{
    return mEditor->view()->mapScreenToCanvas(getLastPixel());
}

void StrokeTool::startStroke(PointerEvent::InputType inputType)
{
    if (emptyFrameActionEnabled())
    {
        mScribbleArea->handleDrawingOnEmptyFrame();
    }

    mFirstDraw = true;
    mLastPixel = getCurrentPixel();

    mStrokePoints.clear();

    //Experimental
    mStrokePoints << mEditor->view()->mapScreenToCanvas(mLastPixel);

    // A tap never moves, so take the pressure from the press, not the previous stroke (A17).
    mCurrentPressure = mInterpolator.getPressure();
    mStrokePressures.clear();
    mStrokePressures << mCurrentPressure;

    mCurrentInputType = inputType;
    mUndoSaveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);

    disableCoalescing();
}

bool StrokeTool::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Alt:
        if (mEditor->tools()->setTemporaryTool(EYEDROPPER, {}, Qt::AltModifier))
        {
            return true;
        }
        break;
    case Qt::Key_Space:
        if (mEditor->tools()->setTemporaryTool(HAND, Qt::Key_Space, Qt::NoModifier))
        {
            return true;
        }
        break;
    }
    return BaseTool::keyPressEvent(event);
}

bool StrokeTool::emptyFrameActionEnabled()
{
    return true;
}

void StrokeTool::endStroke()
{
    mStrokePoints.clear();
    mStrokePressures.clear();

    enableCoalescing();

    mEditor->setModified(mEditor->currentLayerIndex(), mEditor->currentFrame());
    mScribbleArea->endStroke();

    mEditor->undoRedo()->record(mUndoSaveStateId, typeName());
}

void StrokeTool::drawStroke()
{
    QPointF pixel = getCurrentPixel();
    if (pixel != mLastPixel || !mFirstDraw)
    {
        mStrokePoints << mEditor->view()->mapScreenToCanvas(getLastPixel());
        mStrokePressures << mInterpolator.getPressure();
    }
    else
    {
        mFirstDraw = false;
    }
}

bool StrokeTool::handleQuickSizing(PointerEvent* event)
{
    if (!mQuickSizingEnabled) { return false; }

    if (!mQuickSizingProperties.contains(event->modifiers())) {
        mWidthSizingTool.stopAdjusting();
        mFeatherSizingTool.stopAdjusting();
        return false;
    }

    StrokeToolProperties::Type setting = static_cast<StrokeToolProperties::Type>(mQuickSizingProperties[event->modifiers()]);
    if (event->eventType() == PointerEvent::Press) {
        switch (setting) {
            case StrokeToolProperties::WIDTH_VALUE: {
                mWidthSizingTool.setOffset(strokeToolProperties().width() * 0.5);
                break;
            }
            case StrokeToolProperties::FEATHER_VALUE: {
                const qreal factor = 0.5;
                const qreal cursorRad = strokeToolProperties().width() * factor;

                // Pull feather handle closer to center as feather increases
                const qreal featherWidthFactor = MathUtils::normalize(strokeToolProperties().feather(), FEATHER_MIN, FEATHER_MAX);
                const qreal offset = (cursorRad * featherWidthFactor);
                mFeatherSizingTool.setOffset(offset);
                break;
            }
            default: break;
        }
    }

    switch (setting) {
        case StrokeToolProperties::WIDTH_VALUE: {
            mWidthSizingTool.pointerEvent(event);
            break;
        }
        case StrokeToolProperties::FEATHER_VALUE: {
            mFeatherSizingTool.pointerEvent(event);
            break;
        }
        default: break;
    }

    updateCanvasCursor();
    return true;
}

void StrokeTool::pointerPressEvent(PointerEvent*)
{
    updateCanvasCursor();
}

void StrokeTool::pointerMoveEvent(PointerEvent*)
{
    updateCanvasCursor();
}

void StrokeTool::pointerReleaseEvent(PointerEvent*)
{
    updateCanvasCursor();
}

bool StrokeTool::enterEvent(QEnterEvent*)
{
    mCanvasCursorEnabled = mEditor->preference()->isOn(SETTING::CANVAS_CURSOR);
    return true;
}

bool StrokeTool::leaveEvent(QEvent*)
{
    if (isActive())
    {
        return false;
    }

    mCanvasCursorEnabled = false;
    updateCanvasCursor();
    return true;
}


QRectF StrokeTool::cursorRect(StrokeToolProperties::Type settingType, const QPointF& point)
{
    const qreal brushWidth = strokeToolProperties().width();
    const qreal brushFeather = strokeToolProperties().feather();

    const QPointF& cursorPos = point;
    const qreal cursorRad = brushWidth * 0.5;
    const QPointF& widthCursorTopLeft = QPointF(cursorPos.x() - cursorRad, cursorPos.y() - cursorRad);

    const QRectF widthCircleRect = QRectF(widthCursorTopLeft, QSizeF(brushWidth, brushWidth));
    if (settingType == StrokeToolProperties::WIDTH_VALUE) {
        return widthCircleRect;
    } else if (settingType == StrokeToolProperties::FEATHER_VALUE) {
        const qreal featherWidthFactor =  MathUtils::normalize(brushFeather, FEATHER_MIN, FEATHER_MAX);
        QRectF featherRect = QRectF(widthCircleRect.center().x() - (cursorRad * featherWidthFactor),
                                     widthCircleRect.center().y() - (cursorRad * featherWidthFactor),
                                     brushWidth * featherWidthFactor,
                                     brushWidth * featherWidthFactor);

        // Adjust the feather rect so it doesn't colide with the width rect;
        // as this cancels out both circles when painted
        return featherRect.adjusted(2, 2, -2, -2);
    }

    return QRectF();
}


void StrokeTool::updateCanvasCursor()
{
    CanvasCursorPainterOptions widthOptions;
    widthOptions.circleRect = cursorRect(StrokeToolProperties::WIDTH_VALUE, mWidthSizingTool.isAdjusting() ? mWidthSizingTool.offsetPoint() : getCurrentPoint());
    widthOptions.showCursor = mCanvasCursorEnabled;
    widthOptions.showCross = true;

    CanvasCursorPainterOptions featherOptions;
    featherOptions.circleRect = cursorRect(StrokeToolProperties::FEATHER_VALUE, mFeatherSizingTool.isAdjusting() ? mFeatherSizingTool.offsetPoint() : getCurrentPoint());
    featherOptions.showCursor = mCanvasCursorEnabled;
    featherOptions.showCross = false;

    if (mFeatherSizingTool.isAdjusting()) {
        widthOptions.circleRect = cursorRect(StrokeToolProperties::WIDTH_VALUE, mFeatherSizingTool.offsetPoint());
    } else if (mWidthSizingTool.isAdjusting()) {
        featherOptions.circleRect = cursorRect(StrokeToolProperties::FEATHER_VALUE, mWidthSizingTool.offsetPoint());
    }

    mWidthCursorPainter.preparePainter(widthOptions);
    mFeatherCursorPainter.preparePainter(featherOptions);

    const QRect& dirtyRect = mWidthCursorPainter.dirtyRect();

    // We know that the width rect is bigger than the feather rect
    // so we don't need to change this
    const QRect& updateRect = widthOptions.circleRect.toAlignedRect();

    // Adjusted to account for some pixel bleeding outside the update rect
    mScribbleArea->update(mEditor->view()->getView().mapRect(updateRect.united(dirtyRect).adjusted(-2, -2, 2, 2)).adjusted(-2, -2, 2, 2));
    mWidthCursorPainter.clearDirty();
}

void StrokeTool::paint(QPainter& painter, const QRect& blitRect)
{
    painter.save();
    painter.setTransform(mEditor->view()->getView());

    if (strokeToolProperties().featherEnabled()) {
        mFeatherCursorPainter.paint(painter, blitRect);
    }

    mWidthCursorPainter.paint(painter, blitRect);

    painter.restore();
}

PropertyInfo StrokeTool::smoothingModeInfo(SmoothingMode defaultMode)
{
    return { static_cast<int>(SmoothingMode::None), static_cast<int>(InputSmoother::LAST_MODE), static_cast<int>(defaultMode) };
}

void StrokeTool::setLegacySmoothing(int level)
{
    const SmoothingPreset preset = InputSmoother::presetForLegacyLevel(level);
    toolProperties().setBaseValue(StrokeToolProperties::SMOOTHING_MODE, static_cast<int>(preset.mode));
    toolProperties().setBaseValue(StrokeToolProperties::SMOOTHING_STRENGTH, preset.strength);
}

void StrokeTool::migrateLineStabilization(const QSettings& settings)
{
    const QString oldKey = typeName() + "/LineStabilization";
    if (settings.contains(oldKey) && !settings.contains(typeName() + "/SmoothingMode"))
    {
        setLegacySmoothing(settings.value(oldKey).toInt());
    }
}

void StrokeTool::applySmoothing()
{
    mInterpolator.setSmoothing(strokeToolProperties().smoothingMode(), strokeToolProperties().smoothingStrength());
}

void StrokeTool::setSmoothingMode(int mode)
{
    toolProperties().setBaseValue(StrokeToolProperties::SMOOTHING_MODE, mode);
    emit smoothingModeChanged(mode);
}

void StrokeTool::setSmoothingStrength(int strength)
{
    toolProperties().setBaseValue(StrokeToolProperties::SMOOTHING_STRENGTH, strength);
    emit smoothingStrengthChanged(strength);
}

void StrokeTool::setFeatherEnabled(bool enabled)
{
    toolProperties().setBaseValue(StrokeToolProperties::FEATHER_ENABLED, enabled);
    emit featherEnabledChanged(enabled);
}

void StrokeTool::setSpacing(qreal percent)
{
    toolProperties().setBaseValue(StrokeToolProperties::SPACING_VALUE, percent);
    emit spacingChanged(strokeToolProperties().spacing());
}

void StrokeTool::setFlow(qreal percent)
{
    toolProperties().setBaseValue(StrokeToolProperties::FLOW_VALUE, percent);
    emit flowChanged(strokeToolProperties().flow());
}

void StrokeTool::setStrokeOpacity(qreal percent)
{
    toolProperties().setBaseValue(StrokeToolProperties::OPACITY_VALUE, percent);
    emit strokeOpacityChanged(strokeToolProperties().opacity());
}

void StrokeTool::setWashEnabled(bool enabled)
{
    toolProperties().setBaseValue(StrokeToolProperties::WASH_ENABLED, enabled);
    emit washEnabledChanged(enabled);
}

void StrokeTool::setDynamic(int type, int value)
{
    toolProperties().setBaseValue(type, value);
    emit dynamicsChanged();
}

void StrokeTool::setTipSquare(bool square)
{
    toolProperties().setBaseValue(StrokeToolProperties::TIP_SQUARE_ENABLED, square);
    emit tipChanged();
}

void StrokeTool::setTipAspect(qreal percent)
{
    toolProperties().setBaseValue(StrokeToolProperties::TIP_ASPECT_VALUE, percent);
    emit tipChanged();
}

void StrokeTool::setTipAngle(qreal degrees)
{
    toolProperties().setBaseValue(StrokeToolProperties::TIP_ANGLE_VALUE, degrees);
    emit tipChanged();
}

void StrokeTool::setTipFollowsDirection(bool follow)
{
    toolProperties().setBaseValue(StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED, follow);
    emit tipChanged();
}

Status StrokeTool::setTipImage(const QString& fileName)
{
    QImage mask;
    if (!fileName.isEmpty())
    {
        const Status st = loadBrushTipImage(fileName, mask);
        if (!st.ok()) return st;
    }
    mTipImage = mask;
    mTipImagePath = fileName;
    emit tipChanged();
    return Status::OK;
}

void StrokeTool::setFeather(qreal feather)
{
    toolProperties().setBaseValue(StrokeToolProperties::FEATHER_VALUE, feather);
    emit featherChanged(strokeToolProperties().feather());
}

void StrokeTool::setWidth(qreal width)
{
    toolProperties().setBaseValue(StrokeToolProperties::WIDTH_VALUE, width);
    emit widthChanged(strokeToolProperties().width());
}

void StrokeTool::setPressureEnabled(bool enabled)
{
    toolProperties().setBaseValue(StrokeToolProperties::PRESSURE_ENABLED, enabled);
    emit pressureEnabledChanged(enabled);
}

void StrokeTool::setFillContourEnabled(bool enabled)
{
    toolProperties().setBaseValue(StrokeToolProperties::FILLCONTOUR_ENABLED, enabled);
    emit fillContourEnabledChanged(enabled);
}

void StrokeTool::setAntiAliasingEnabled(bool enabled)
{
    toolProperties().setBaseValue(StrokeToolProperties::ANTI_ALIASING_ENABLED, enabled);
    emit antiAliasingEnabledChanged(enabled);
}

void StrokeTool::setStrokeInvisibleEnabled(bool enabled)
{
    toolProperties().setBaseValue(StrokeToolProperties::INVISIBILITY_ENABLED, enabled);
    emit invisibleStrokeEnabledChanged(enabled);
}
