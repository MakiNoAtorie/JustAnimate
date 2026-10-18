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

#include <QCursor>
#include <QKeyEvent>
#include <QPixmap>
#include "scribblearea.h"
#include "viewmanager.h"
#include "preferencemanager.h"
#include "editor.h"
#include "toolmanager.h"
#include "mathutils.h"
#include "overlaymanager.h"
#include "layermanager.h"
#include "layercamera.h"
#include "tiledbuffer.h"
#include "beziercurve.h"
#include "vectorimage.h"
#include "colormanager.h"
#include "selectionmanager.h"

#include "canvascursorpainter.h"
#include "pencilsettings.h"

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

const qreal StrokeTool::FEATHER_MIN = ToolProperties::descriptor(StrokeToolProperties::FEATHER_VALUE)->min;
const qreal StrokeTool::FEATHER_MAX = ToolProperties::descriptor(StrokeToolProperties::FEATHER_VALUE)->max;

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
    mQuickSizingEnabled = mEditor->preference()->get<SETTING::QUICK_SIZING>();
    mOutlineStyle = mEditor->preference()->get<SETTING::CURSOR_STYLE>() != CURSOR_CROSS_ONLY;

    QSettings& settings = pencilSettings();
    toolProperties().loadTool(type(), typeName(), settings);
    toolProperties().migrateLegacyKeys(type(), settings);

    mPropertyUsed.clear();
    for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
    {
        const ToolPropertyUse* use = d.use(type());
        if (!use || use->layers == NO_ROW) { continue; }
        QSet<Layer::LAYER_TYPE>& layers = mPropertyUsed[d.id];
        if (use->layers & BITMAP_ROW) { layers.insert(Layer::BITMAP); }
        if (use->layers & VECTOR_ROW) { layers.insert(Layer::VECTOR); }
    }

    /// Given the way that we update preferences currently, this connection should not be removed
    /// when the tool is not active.
    connect(mEditor->preference(), &PreferenceManager::optionChanged, this, &StrokeTool::onPreferenceChanged);

    connect(&mWidthSizingTool, &RadialOffsetTool::offsetChanged, this, [=](qreal offset) {
        set(ToolProperty::Width, offset * 2.0);
    });

    connect(&mFeatherSizingTool, &RadialOffsetTool::offsetChanged, this, [=](qreal offset){
        const qreal inputMin = FEATHER_MIN;
        const qreal inputMax = get(ToolProperty::Width) * 0.5;
        const qreal outputMax = FEATHER_MAX;
        const qreal outputMin = inputMin;

        // We map the feather value to a value between the min width and max width
        const qreal mappedValue = MathUtils::map(offset, inputMin, inputMax, outputMax, outputMin);

        set(ToolProperty::Feather, mappedValue);
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
        mQuickSizingEnabled = mEditor->preference()->get<SETTING::QUICK_SIZING>();
    } else if (setting == SETTING::CURSOR_STYLE) {
        mOutlineStyle = mEditor->preference()->get<SETTING::CURSOR_STYLE>() != CURSOR_CROSS_ONLY;
        if (mEditor->tools()->currentTool() == this) { updateCanvasCursor(); }
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
    // A stroke whose release went unhandled (other device) never recorded its state.
    endStrokeEdit(false);
    // A Drawing added for this stroke undoes with the stroke in one step.
    mStrokeStep.emplace(mEditor->edit(typeName()));
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
    mStrokeEdit.emplace(mEditor->edit(mEditor->layers()->currentLayer(), mEditor->currentFrame(), typeName()));
    mBuffer.begin(mEditor->layers()->currentLayer(), mEditor->currentFrame(), bufferMode());

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

    commitBuffer();
    mEditor->setModified(mEditor->currentLayerIndex(), mEditor->currentFrame());

    endStrokeEdit(true);
}

void StrokeTool::drawScreenPath(const QPainterPath& path, const QPen& pen)
{
    mBuffer.tiles().drawPath(mEditor->view()->mapScreenToCanvas(path), pen, Qt::NoBrush, QPainter::CompositionMode_Source,
                             mEditor->preference()->get<SETTING::ANTIALIAS>());
}

void StrokeTool::commitBuffer()
{
    if (mStrokeEdit) { mStrokeEdit->willChange(mBuffer.tiles().bounds()); }
    const QRect changed = mBuffer.commit();
    // The caller's setModified repaints this area only; the margin covers edge pixels at any zoom.
    if (!changed.isEmpty())
    {
        mScribbleArea->setModifiedArea(mEditor->view()->mapCanvasToScreen(QRectF(changed.adjusted(-1, -1, 1, 1))).toAlignedRect());
    }
}

VectorImage* StrokeTool::commitVectorStroke(qreal width, qreal feather, bool invisible, bool variableWidth)
{
    if (mStrokePoints.empty()) { return nullptr; }

    const qreal scaling = mEditor->view()->scaling();
    BezierCurve curve(mStrokePoints, mStrokePressures, mScribbleArea->getCurveSmoothing() / scaling);
    curve.setWidth(width);
    curve.setFeather(feather);
    curve.setFilled(false);
    curve.setInvisibility(invisible);
    curve.setVariableWidth(variableWidth);
    curve.setColorNumber(mEditor->color()->frontColorNumber());

    VectorImage* image = mBuffer.commit(curve, scaling, false);
    if (image == nullptr) { return nullptr; } // the first frame can be deleted while drawing
    if (image->isAnyCurveSelected() || mEditor->select()->somethingSelected())
    {
        mEditor->deselectAll();
    }
    image->setSelected(image->getLastCurveNumber(), true);
    return image; // endStroke() marks the frame modified
}

void StrokeTool::endStrokeEdit(bool commit)
{
    if (commit && mStrokeEdit) { mStrokeEdit->commit(); }
    mStrokeEdit.reset();
    mStrokeStep.reset();
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
                mWidthSizingTool.setOffset(get(ToolProperty::Width) * 0.5);
                break;
            }
            case StrokeToolProperties::FEATHER_VALUE: {
                const qreal factor = 0.5;
                const qreal cursorRad = get(ToolProperty::Width) * factor;

                // Pull feather handle closer to center as feather increases
                const qreal featherWidthFactor = MathUtils::normalize(get(ToolProperty::Feather), FEATHER_MIN, FEATHER_MAX);
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
    mCanvasCursorEnabled = true;
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
    const qreal brushWidth = get(ToolProperty::Width);
    const qreal brushFeather = get(ToolProperty::Feather);

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


QCursor StrokeTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cross.png"), 10, 10);
}

QPainterPath StrokeTool::brushOutline(const QPointF& point) const
{
    const qreal r = get(ToolProperty::Width) * 0.5;
    const QRectF unit(-r, -r, 2 * r, 2 * r);
    QPainterPath shape;
    if (get(ToolProperty::TipSquare) && mTipImage.isNull()) { shape.addRect(unit); }
    else { shape.addEllipse(unit); }
    // Same frame as the dab mask: the tip angle turns counter-clockwise on screen, y points down.
    QTransform tip;
    tip.translate(point.x(), point.y());
    tip.rotate(-get(ToolProperty::TipAngle));
    tip.scale(1.0, get(ToolProperty::TipAspect) / 100.0);
    return tip.map(shape);
}

void StrokeTool::updateCanvasCursor()
{
    const QPointF point = mWidthSizingTool.isAdjusting() ? mWidthSizingTool.offsetPoint()
                        : mFeatherSizingTool.isAdjusting() ? mFeatherSizingTool.offsetPoint() : getCurrentPoint();
    const bool bigEnough = get(ToolProperty::Width) * mEditor->view()->scaling() >= OUTLINE_MIN_SCREEN_PX;
    const bool show = mCanvasCursorEnabled && bigEnough && mOutlineStyle;

    CanvasCursorPainterOptions widthOptions;
    widthOptions.shape = brushOutline(point);
    // The centre cross, 10 px on screen: quick-sizing moves the outline away from the hardware cross.
    const QPointF centre = mEditor->view()->getView().map(point);
    QPainterPath cross;
    cross.moveTo(centre - QPointF(5, 0));
    cross.lineTo(centre + QPointF(5, 0));
    cross.moveTo(centre - QPointF(0, 5));
    cross.lineTo(centre + QPointF(0, 5));
    widthOptions.shape.addPath(mEditor->view()->getViewInverse().map(cross));
    widthOptions.showCursor = show;

    CanvasCursorPainterOptions featherOptions;
    featherOptions.shape.addEllipse(cursorRect(StrokeToolProperties::FEATHER_VALUE, point));
    featherOptions.showCursor = show;

    mWidthCursorPainter.preparePainter(widthOptions);
    mFeatherCursorPainter.preparePainter(featherOptions);

    const QRect& dirtyRect = mWidthCursorPainter.dirtyRect();
    const QRect& updateRect = widthOptions.shape.boundingRect().toAlignedRect()
                              .united(featherOptions.shape.boundingRect().toAlignedRect());

    // Adjusted to account for some pixel bleeding outside the update rect
    mScribbleArea->update(mEditor->view()->getView().mapRect(updateRect.united(dirtyRect).adjusted(-2, -2, 2, 2)).adjusted(-2, -2, 2, 2));
    mWidthCursorPainter.clearDirty();
}

void StrokeTool::paint(QPainter& painter, const QRect& blitRect)
{
    painter.save();
    painter.setTransform(mEditor->view()->getView());

    if (get(ToolProperty::FeatherEnabled)) {
        mFeatherCursorPainter.paint(painter, blitRect);
    }

    mWidthCursorPainter.paint(painter, blitRect);

    painter.restore();
}

void StrokeTool::prepareInput(const QPointF& pressViewportPos)
{
    mInterpolator.setSmoothing(static_cast<SmoothingMode>(get(ToolProperty::SmoothingMode)), get(ToolProperty::SmoothingStrength));

    // Snap in viewport px, so the reach stays the same at any zoom.
    constexpr qreal kSnapTolerancePx = 16;
    QList<QLineF> guides;
    const OverlayManager* overlays = mEditor->overlays();
    if (overlays->isRulerEnabled())
    {
        const QLineF ruler = overlays->ruler();
        guides << QLineF(mEditor->view()->mapCanvasToScreen(ruler.p1()), mEditor->view()->mapCanvasToScreen(ruler.p2()));
    }
    const LayerCamera* camera = mEditor->layers()->getCameraLayerBelow(mEditor->currentLayerIndex());
    if (overlays->isPerspectiveSnapEnabled() && camera)
    {
        // Vanishing points are in camera space, as the overlay painter draws them.
        PreferenceManager* prefs = mEditor->preference();
        QList<QPointF> points;
        if (prefs->get<SETTING::OVERLAY_PERSPECTIVE1>()) { points << overlays->getSinglePerspectivePoint(); }
        if (prefs->get<SETTING::OVERLAY_PERSPECTIVE2>() || prefs->get<SETTING::OVERLAY_PERSPECTIVE3>())
        {
            points << overlays->getLeftPerspectivePoint() << overlays->getRightPerspectivePoint();
        }
        if (prefs->get<SETTING::OVERLAY_PERSPECTIVE3>()) { points << overlays->getMiddlePerspectivePoint(); }
        const QTransform toCanvas = camera->getViewAtFrame(mEditor->currentFrame()).inverted();
        for (const QPointF& point : points)
        {
            const QPointF vanishing = mEditor->view()->mapCanvasToScreen(toCanvas.map(point));
            if (QLineF(pressViewportPos, vanishing).length() >= 1) { guides << QLineF(pressViewportPos, vanishing); }
        }
    }
    mInterpolator.setSnapGuides(guides, kSnapTolerancePx);
}

void StrokeTool::setValue(int id, const QVariant& value)
{
    mProperties.setValue(id, value);
    emit propertyChanged(id);
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
    return Status::OK;
}
