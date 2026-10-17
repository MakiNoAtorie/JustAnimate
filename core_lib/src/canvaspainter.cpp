/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "canvaspainter.h"

#include <QtMath>

#include "object.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layercamera.h"
#include "objectdata.h"
#include "paperpainter.h"
#include "bitmapimage.h"
#include "tile.h"
#include "tiledbuffer.h"
#include "vectorimage.h"

#include "painterutils.h"
#include "blendmode.h"

namespace
{
const int kCell = 64; // below-composite cache cell, in logical px
}

CanvasPainter::CanvasPainter(QPixmap& canvas) : mCanvas(canvas)
{
    reset();
}

CanvasPainter::~CanvasPainter()
{
}

void CanvasPainter::reset()
{
    mPostLayersPixmap = QPixmap(mCanvas.size());
    mPreLayersPixmap = QPixmap(mCanvas.size());
    mCurrentLayerPixmap = QPixmap(mCanvas.size());
    mOnionSkinPixmap = QPixmap(mCanvas.size());
    mPreLayersPixmap.fill(Qt::transparent);
    mPreLayersCache.cells.clear();
    mPostLayersCache.cells.clear();
    mOnionCache.clear(); // the tint covers the whole canvas size
    mCanvas.fill(Qt::transparent);
    mCurrentLayerPixmap.fill(Qt::transparent);
    mPostLayersPixmap.fill(Qt::transparent);
    mOnionSkinPixmap.fill(Qt::transparent);
    mCurrentLayerPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mPreLayersPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mPostLayersPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
    mOnionSkinPixmap.setDevicePixelRatio(mCanvas.devicePixelRatioF());
}

void CanvasPainter::setViewTransform(const QTransform view, const QTransform viewInverse)
{
    if (mViewTransform != view || mViewInverse != viewInverse) {
        mViewTransform = view;
        mViewInverse = viewInverse;
    }
}

void CanvasPainter::setTransformedSelection(QRect selection, QTransform transform)
{
    // Make sure that the selection is not empty
    if (selection.width() > 0 && selection.height() > 0)
    {
        mSelection = selection;
        mSelectionTransform = transform;
        mRenderTransform = true;
    }
    else
    {
        // Otherwise we shouldn't be in transformation mode
        ignoreTransformedSelection();
    }
}

void CanvasPainter::ignoreTransformedSelection()
{
    mRenderTransform = false;
    mSelectionTransform.reset();
    mSelection = QRect();
}

void CanvasPainter::paintCached(const QRect& blitRect)
{
    mPaintOrder = paintOrder();
    const QPair<int, int> range = currentRange();
    updateCells(mPreLayersCache, mPreLayersPixmap, layersKey(0, range.first - 1), blitRect, &CanvasPainter::renderPreLayers);

    QPainter mainPainter;
    initializePainter(mainPainter, mCanvas, blitRect);
    paintPaper(mainPainter);
    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPreLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    paintCurrentFrame(mainPainter, blitRect, range.first, range.second);

    // A blend layer needs every layer below it as backdrop, so the layers above paint straight onto the canvas.
    bool blendAbove = false;
    for (int i = range.second + 1; i < mObject->getLayerCount(); ++i)
    {
        blendAbove |= mObject->getLayer(mPaintOrder[i])->blendMode() != BlendMode::Normal;
    }

    if (blendAbove)
    {
        renderPostLayers(mainPainter, blitRect);
        return;
    }
    if (mOptions.bFastLayersAbove)
    {
        updateCells(mPostLayersCache, mPostLayersPixmap, layersKey(range.second + 1, mObject->getLayerCount() - 1),
                    blitRect, &CanvasPainter::renderPostLayers);
    }
    else if (!mPostLayersPixmapCacheValid)
    {
        QPainter postLayerPainter;
        initializePainter(postLayerPainter, mPostLayersPixmap, blitRect);
        renderPostLayers(postLayerPainter, blitRect);
        postLayerPainter.end();
        mPostLayersPixmapCacheValid = true;
    }

    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPostLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);
}

// Under every layer, around the frame of the camera layer the canvas shows.
void CanvasPainter::paintPaper(QPainter& painter) const
{
    const PaperTemplate& paper = mObject->data()->paperTemplate();
    if (!paper.on) { return; }
    const Layer* camera = mObject->getLayerBelow(mCurrentLayerIndex, Layer::CAMERA);
    if (camera == nullptr) { camera = mObject->getLayerBelow(mObject->getLayerCount() - 1, Layer::CAMERA); }
    if (camera == nullptr) { return; }
    const auto* cameraLayer = static_cast<const LayerCamera*>(camera);
    // The sheet moves with the camera frame on this frame, as the camera border does.
    painter.save();
    painter.setWorldTransform(cameraLayer->getViewAtFrame(mFrameNumber).inverted(), true);
    paintPaperTemplate(painter, cameraLayer->getViewRect(), paper);
    painter.restore();
}

void CanvasPainter::resetLayerCache()
{
    mPreLayersCache.cells.fill(false);
    mPostLayersCache.cells.fill(false);
    mPostLayersPixmapCacheValid = false;
}

// Identifies what positions startPos..endPos show on this frame; another key empties the cache.
QVector<qint64> CanvasPainter::layersKey(int startPos, int endPos) const
{
    QVector<qint64> key{ qint64(quintptr(mObject)), mCurrentLayerIndex, mFrameNumber };
    for (auto it = mShiftAndTraceOffsets.cbegin(); it != mShiftAndTraceOffsets.cend(); ++it)
    {
        const QTransform& t = it.value();
        key << it.key() << qRound64(t.m11() * 1e6) << qRound64(t.m12() * 1e6) << qRound64(t.m21() * 1e6)
            << qRound64(t.m22() * 1e6) << qRound64(t.dx() * 1e6) << qRound64(t.dy() * 1e6);
    }
    for (int i = startPos; i <= endPos; ++i)
    {
        Layer* layer = mObject->getLayer(mPaintOrder[i]);
        KeyFrame* frame = layer->getLastKeyFrameAtPosition(mFrameNumber);
        const LayerGroup* group = mObject->findLayerGroup(layer->groupId());
        const bool visible = mObject->isLayerVisible(layer);
        key << qint64(quintptr(layer)) << visible << int(layer->blendMode()) << layer->isRough() << qint64(quintptr(frame))
            << qRound64((group ? group->opacity : 1.0) * 1e6);
        if (!visible || frame == nullptr) { continue; }
        if (layer->type() == Layer::BITMAP)
        {
            auto* image = static_cast<BitmapImage*>(frame);
            const QPoint topLeft = image->topLeft();
            // QImage::cacheKey changes on every write to the image.
            key << topLeft.x() << topLeft.y() << image->image()->cacheKey();
        }
        const TweenState tween = layer->tweenStateAt(mFrameNumber);
        key << qRound64(tween.tx * 1e6) << qRound64(tween.ty * 1e6) << qRound64(tween.rot * 1e6) << qRound64(tween.scale * 1e6)
            << qRound64(tween.opacity * 1e6);
    }
    return key;
}

// Renders into the stale 64 px cells of pixmap that blitRect touches.
void CanvasPainter::updateCells(LayerCellCache& cache, QPixmap& pixmap, const QVector<qint64>& key, const QRect& blitRect,
                                void (CanvasPainter::*render)(QPainter&, const QRect&))
{
    const QSize size = pixmap.deviceIndependentSize().toSize();
    const int cols = (size.width() + kCell - 1) / kCell;
    const int cells = cols * ((size.height() + kCell - 1) / kCell);
    if (cache.cells.size() != cells || key != cache.key || mViewTransform != cache.view)
    {
        cache.cells = QBitArray(cells);
        cache.key = key;
        cache.view = mViewTransform;
    }

    const QRect area = blitRect & QRect(QPoint(), size);
    if (area.isEmpty()) { return; }
    QRect stale;
    for (int y = area.top() / kCell; y <= area.bottom() / kCell; ++y)
    {
        for (int x = area.left() / kCell; x <= area.right() / kCell; ++x)
        {
            if (cache.cells.testBit(y * cols + x)) { continue; }
            cache.cells.setBit(y * cols + x);
            stale |= QRect(x * kCell, y * kCell, kCell, kCell);
        }
    }
    if (stale.isEmpty()) { return; }

    // shortcut: repaints the bounding rect of the stale cells; clip to a QRegion if scattered cells cost too much.
    QPainter painter;
    initializePainter(painter, pixmap, stale);
    (this->*render)(painter, stale);
}

void CanvasPainter::initializePainter(QPainter& painter, QPaintDevice& device, const QRect& blitRect)
{
    painter.begin(&device);

    // Only draw inside the clipped rectangle
    painter.setClipRect(blitRect);

    // Clear the area that's about to be painted again, to avoid painting on top of existing pixels
    // causing artifacts.
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(blitRect, Qt::transparent);

    // Surface has been cleared and is ready to be painted on
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setWorldMatrixEnabled(true);
    painter.setWorldTransform(mViewTransform);
}

void CanvasPainter::renderPreLayers(QPainter& painter, const QRect& blitRect)
{
    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, 0, currentRange().first - 1);
    }

    paintOnionSkin(painter, blitRect);
    painter.setOpacity(1.0);
}

void CanvasPainter::renderPostLayers(QPainter& painter, const QRect& blitRect)
{
    if (mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY || mObject->getLayer(mCurrentLayerIndex)->type() == Layer::CAMERA)
    {
        paintCurrentFrame(painter, blitRect, currentRange().second + 1, mObject->getLayerCount() - 1);
    }
}

void CanvasPainter::setPaintSettings(const Object* object, int currentLayer, int frame, TiledBuffer* tiledBuffer)
{
    Q_ASSERT(object);
    mObject = object;

    CANVASPAINTER_LOG("Set CurrentLayerIndex = %d", currentLayer);
    mCurrentLayerIndex = currentLayer;
    mFrameNumber = frame;
    mTiledBuffer = tiledBuffer;
}

void CanvasPainter::paint(const QRect& blitRect)
{
    mPostLayersPixmapCacheValid = false;
    paintCached(blitRect);
}

void CanvasPainter::paintOnionSkinOnLayer(QPainter& painter, const QRect& blitRect, Layer* layer)
{
    if (!mObject->isLayerVisible(layer)) { return; }
    mOnionSkinSubPainter.paint(painter, layer, mOnionSkinPainterOptions, mFrameNumber, [&] (OnionSkinPaintState state, int onionFrameNumber) {
        if (state == OnionSkinPaintState::PREV) {
            switch (layer->type())
            {
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizePrevFrames); break; }
            case Layer::VECTOR: { paintVectorOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizePrevFrames); break; }
            default: break;
            }
        }
        if (state == OnionSkinPaintState::NEXT) {
            switch (layer->type())
            {
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizeNextFrames); break; }
            case Layer::VECTOR: { paintVectorOnionSkinFrame(painter, blitRect, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizeNextFrames); break; }
            default: break;
            }
        }
    });
}

void CanvasPainter::paintOnionSkin(QPainter& painter, const QRect& blitRect)
{
    if (!mOptions.bOnionSkinMultiLayer || mOptions.eLayerVisibility == LayerVisibility::CURRENTONLY) {
        Layer* layer = mObject->getLayer(mCurrentLayerIndex);
        paintOnionSkinOnLayer(painter, blitRect, layer);
    } else {
        for (int i = 0; i < mObject->getLayerCount(); i++) {
            Layer* layer = mObject->getLayer(i);
            if (layer == nullptr) { continue; }

            paintOnionSkinOnLayer(painter, blitRect, layer);
        }
    }
}

void CanvasPainter::paintBitmapOnionSkinFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int nFrame, bool colorize)
{
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);

    BitmapImage* bitmapImage = bitmapLayer->getBitmapImageAtFrame(nFrame);

    if (bitmapImage == nullptr) { return; }
    bitmapImage->loadFile(); // Critical! force the BitmapImage to load the image

    Q_UNUSED(blitRect) // cached whole; the painter clips
    if (mOnionCacheView != mViewTransform)
    {
        mOnionCache.clear();
        mOnionCacheView = mViewTransform;
    }
    // Everything tintOnionSkinFrame reads; QImage::cacheKey changes on every write to the image.
    QColor tint = Qt::transparent;
    if (nFrame < mFrameNumber) { tint = mOnionSkinPainterOptions.prevColor; }
    else if (nFrame > mFrameNumber) { tint = mOnionSkinPainterOptions.nextColor; }
    const TweenState tween = layer->tweenStateAt(nFrame);
    const qreal opacity = tween.opacity - (1.0 - painter.opacity());
    const QPoint topLeft = bitmapImage->topLeft();
    const QTransform canvasTransform = onionTransform(layer, nFrame);
    const QVector<qint64> key{ bitmapImage->image()->cacheKey(), topLeft.x(), topLeft.y(), colorize,
                               colorize ? qint64(tint.rgba64()) : 0, colorize ? qRound64(opacity * 1e6) : 0,
                               qRound64(canvasTransform.m11() * 1e6), qRound64(canvasTransform.m12() * 1e6),
                               qRound64(canvasTransform.m21() * 1e6), qRound64(canvasTransform.m22() * 1e6),
                               qRound64(canvasTransform.dx() * 1e6), qRound64(canvasTransform.dy() * 1e6) };

    OnionFrame frame;
    if (const OnionFrame* cached = mOnionCache.object(key))
    {
        frame = *cached;
    }
    else
    {
        // Outside the image the onion pixmap stays transparent, so only its bounds are rendered.
        const QRect canvasRect(QPoint(), mOnionSkinPixmap.deviceIndependentSize().toSize());
        const QTransform world = canvasTransform * mViewTransform;
        const QRect r = world.mapRect(QRectF(bitmapImage->bounds())).toAlignedRect().adjusted(-1, -1, 1, 1) & canvasRect;
        if (!r.isEmpty())
        {
            QPainter onionSkinPainter;
            initializePainter(onionSkinPainter, mOnionSkinPixmap, r);
            onionSkinPainter.setWorldTransform(world);
            onionSkinPainter.drawImage(topLeft, *bitmapImage->image());
            tintOnionSkinFrame(painter, onionSkinPainter, nFrame, colorize, tween.opacity);
            onionSkinPainter.end();
            const qreal dpr = mOnionSkinPixmap.devicePixelRatio();
            frame = { mOnionSkinPixmap.copy(QRectF(QPointF(r.topLeft()) * dpr, QSizeF(r.size()) * dpr).toAlignedRect()), r.topLeft() };
            ++mOnionFramesTinted;
        }
        mOnionCache.insert(key, new OnionFrame(frame), qMax(1, frame.pixmap.width() * frame.pixmap.height() * 4));
    }
    painter.setWorldMatrixEnabled(false);
    if (!frame.pixmap.isNull()) { painter.drawPixmap(frame.pos, frame.pixmap); }
}

void CanvasPainter::paintVectorOnionSkinFrame(QPainter& painter, const QRect& blitRect, Layer* layer, int nFrame, bool colorize)
{
    LayerVector* vectorLayer = static_cast<LayerVector*>(layer);

    CANVASPAINTER_LOG("Paint Onion skin vector, Frame = %d", nFrame);
    VectorImage* vectorImage = vectorLayer->getVectorImageAtFrame(nFrame);
    if (vectorImage == nullptr) { return; }

    QPainter onionSkinPainter;
    initializePainter(onionSkinPainter, mOnionSkinPixmap, blitRect);

    const TweenState tween = layer->tweenStateAt(nFrame);
    onionSkinPainter.setWorldTransform(onionTransform(layer, nFrame) * mViewTransform);
    vectorImage->paintImage(onionSkinPainter, *mObject, mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias);
    tintOnionSkinFrame(painter, onionSkinPainter, nFrame, colorize, tween.opacity);
    painter.drawPixmap(mPointZero, mOnionSkinPixmap);
}

QTransform CanvasPainter::onionTransform(const Layer* layer, int nFrame) const
{
    const QTransform tween = layer->tweenStateAt(nFrame).transform();
    if (layer != mObject->getLayer(mCurrentLayerIndex)) { return tween; }
    return tween * mShiftAndTraceOffsets.value(nFrame);
}

void CanvasPainter::tintOnionSkinFrame(QPainter& painter, QPainter& onionSkinPainter, int nFrame, bool colorize, qreal frameOpacity)
{
    // Don't transform the image here as we used the viewTransform in the image output
    painter.setWorldMatrixEnabled(false);
    // Remember to adjust overall opacity based on opacity value from image
    onionSkinPainter.setOpacity(frameOpacity - (1.0-painter.opacity()));
    if (colorize)
    {
        QColor colorBrush = Qt::transparent; //no color for the current frame

        if (nFrame < mFrameNumber)
        {
            colorBrush = mOnionSkinPainterOptions.prevColor;
        }
        else if (nFrame > mFrameNumber)
        {
            colorBrush = mOnionSkinPainterOptions.nextColor;
        }
        onionSkinPainter.setWorldMatrixEnabled(false);

        onionSkinPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        onionSkinPainter.setBrush(colorBrush);
        onionSkinPainter.drawRect(painter.viewport());
    }
}

// Recolours the painted layer pixels in one faded blue, keeping their coverage.
static void tintRough(QPainter& layerPainter, const QRect& blitRect)
{
    layerPainter.setWorldMatrixEnabled(false);
    layerPainter.setOpacity(1.0);
    layerPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    layerPainter.fillRect(blitRect, QColor(107, 143, 201, 96));
}

void CanvasPainter::paintCurrentBitmapFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer)
{
    LayerBitmap* bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* paintedImage = bitmapLayer->getLastBitmapImageAtFrame(mFrameNumber);

    if (paintedImage == nullptr) { return; }
    paintedImage->loadFile(); // Critical! force the BitmapImage to load the image

    const bool isDrawing = mTiledBuffer && !mTiledBuffer->bounds().isEmpty();

    QPainter currentBitmapPainter;
    initializePainter(currentBitmapPainter, mCurrentLayerPixmap, blitRect);

    painter.setWorldMatrixEnabled(false);

    const TweenState tween = layer->tweenStateAt(mFrameNumber);
    currentBitmapPainter.setOpacity(tween.opacity - (1.0-painter.opacity()));
    currentBitmapPainter.setWorldTransform(tween.transform() * mViewTransform);
    currentBitmapPainter.drawImage(paintedImage->topLeft(), *paintedImage->image());
    currentBitmapPainter.setWorldTransform(mViewTransform);

    if (isCurrentLayer && isDrawing)
    {
        currentBitmapPainter.setCompositionMode(mOptions.cmBufferBlendMode);
        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            currentBitmapPainter.drawImage(tile->posF(), tile->image());
        }
    }

    // We do not wish to draw selection transformations on anything but the current layer
    Q_ASSERT(!isDrawing || mSelectionTransform.isIdentity());
    if (isCurrentLayer && mRenderTransform && !isDrawing) {
        paintTransformedSelection(currentBitmapPainter, paintedImage, mSelection, tween.transform() * mViewTransform);
    }

    if (layer->isRough()) { tintRough(currentBitmapPainter, blitRect); }
    drawLayerPixmap(painter, blitRect, layer->blendMode());
}

void CanvasPainter::paintCurrentVectorFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer)
{
    LayerVector* vectorLayer = static_cast<LayerVector*>(layer);
    VectorImage* vectorImage = vectorLayer->getLastVectorImageAtFrame(mFrameNumber);
    if (vectorImage == nullptr)
    {
        return;
    }

    QPainter currentVectorPainter;
    initializePainter(currentVectorPainter, mCurrentLayerPixmap, blitRect);

    const bool isDrawing = mTiledBuffer->isValid();

    if (mRenderTransform) {
        vectorImage->setSelectionTransformation(mSelectionTransform);
    }

    // Paint existing vector image to the painter
    // Remember to adjust opacity based on additional opacity value from the keyframe
    const TweenState tween = layer->tweenStateAt(mFrameNumber);
    currentVectorPainter.setOpacity(tween.opacity - (1.0-painter.opacity()));
    currentVectorPainter.setWorldTransform(tween.transform() * mViewTransform);
    vectorImage->paintImage(currentVectorPainter, *mObject, mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias);
    currentVectorPainter.setWorldTransform(mViewTransform);

    if (isCurrentLayer && isDrawing) {
        currentVectorPainter.setCompositionMode(mOptions.cmBufferBlendMode);

        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            currentVectorPainter.drawImage(tile->posF(), tile->image());
        }
    }

    // Don't transform the image here as we used the viewTransform in the image output
    painter.setWorldMatrixEnabled(false);
    painter.setTransform(QTransform());

    if (layer->isRough()) { tintRough(currentVectorPainter, blitRect); }
    drawLayerPixmap(painter, blitRect, layer->blendMode());
}

void CanvasPainter::drawLayerPixmap(QPainter& painter, const QRect& blitRect, BlendMode mode)
{
    if (mode == BlendMode::Normal)
    {
        painter.drawPixmap(mPointZero, mCurrentLayerPixmap);
        return;
    }
    const qreal dpr = mCurrentLayerPixmap.devicePixelRatioF();
    const QRect rect = QRectF(QPointF(blitRect.topLeft()) * dpr, QSizeF(blitRect.size()) * dpr).toAlignedRect()
                       & mCurrentLayerPixmap.rect();
    if (!rect.isEmpty()) { drawBlended(painter, mCurrentLayerPixmap.copy(rect).toImage(), rect.topLeft(), mode); }
}

void CanvasPainter::paintTransformedSelection(QPainter& painter, BitmapImage* bitmapImage, const QRect& selection, const QTransform& keyView) const
{
    // Make sure there is something selected
    if (selection.width() == 0 && selection.height() == 0)
        return;

    QPixmap transformedPixmap = QPixmap(mSelection.size());
    transformedPixmap.fill(Qt::transparent);

    QPainter imagePainter(&transformedPixmap);
    imagePainter.translate(-selection.topLeft());
    imagePainter.drawImage(bitmapImage->topLeft(), *bitmapImage->image());
    imagePainter.end();

    painter.save();

    painter.setTransform(keyView);

    // Clear the painted area to make it look like the content has been erased
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(selection, QColor(255,255,255,255));
    painter.restore();

    // Multiply the selection and view matrix to get proper rotation and scale values
    // Now the image origin will be topleft
    painter.setTransform(mSelectionTransform * keyView);

    // Draw the selection image separately and on top
    painter.drawPixmap(selection, transformedPixmap);
    painter.restore();
}

/** Paints layers within the specified range for the current frame.
 *
 *  @param painter The painter to paint to
 *  @param startLayer The first layer to paint (inclusive)
 *  @param endLayer The last layer to paint (inclusive)
 */
void CanvasPainter::paintCurrentFrame(QPainter& painter, const QRect& blitRect, int startPos, int endPos)
{
    painter.setOpacity(1.0);

    for (int p = startPos; p <= endPos; ++p)
    {
        const Layer* layer = mObject->getLayer(mPaintOrder[p]);
        const LayerGroup* group = mObject->findLayerGroup(layer->groupId());
        if (group == nullptr || group->opacity >= 1.0)
        {
            paintLayer(painter, blitRect, mPaintOrder[p]);
            continue;
        }
        if (layer->isRough())
        {
            // A Rough layer paints apart from its group, so it fades on its own.
            if (group->visible) { painter.setOpacity(group->opacity); paintLayer(painter, blitRect, mPaintOrder[p]); }
            painter.setOpacity(1.0);
            continue;
        }
        // The group's non-Rough layers are next to each other in paint order.
        const int first = p;
        while (p < endPos && mObject->getLayer(mPaintOrder[p + 1])->groupId() == group->id) { ++p; }
        if (group->visible && group->opacity > 0.0) { paintGroup(painter, blitRect, first, p, group->opacity); }
    }
}

void CanvasPainter::paintLayer(QPainter& painter, const QRect& blitRect, int index)
{
    Layer* layer = mObject->getLayer(index);
    if (!mObject->isLayerVisible(layer)) { return; }

    if (mOptions.eLayerVisibility == LayerVisibility::RELATED && mObject->getLayer(mCurrentLayerIndex)->type() != Layer::CAMERA)
    {
        painter.setOpacity(calculateRelativeOpacityForLayer(mCurrentLayerIndex, index, mOptions.fLayerVisibilityThreshold));
    }
    const bool isCurrentLayer = mCurrentLayerIndex == index;

    CANVASPAINTER_LOG("  Render Layer[%d] %s", index, layer->name());
    switch (layer->type())
    {
    case Layer::BITMAP: { paintCurrentBitmapFrame(painter, blitRect, layer, isCurrentLayer); break; }
    case Layer::VECTOR: { paintCurrentVectorFrame(painter, blitRect, layer, isCurrentLayer); break; }
    default: break;
    }
}

void CanvasPainter::paintGroup(QPainter& painter, const QRect& blitRect, int first, int last, qreal opacity)
{
    drawWithOpacity(painter, opacity, [&](QPainter& groupPainter) {
        for (int p = first; p <= last; ++p)
        {
            groupPainter.setOpacity(1.0);
            paintLayer(groupPainter, blitRect, mPaintOrder[p]);
        }
    });
}

QPair<int, int> CanvasPainter::currentRange() const
{
    const Layer* layer = mObject->getLayer(mCurrentLayerIndex);
    const LayerGroup* group = mObject->findLayerGroup(layer->groupId());
    const bool currentOnly = mOptions.eLayerVisibility == LayerVisibility::CURRENTONLY && layer->type() != Layer::CAMERA;
    int first = mPaintOrder.indexOf(mCurrentLayerIndex);
    int last = first;
    if (group == nullptr || group->opacity >= 1.0 || currentOnly || layer->isRough()) { return { first, last }; }
    auto inGroup = [&](int p) {
        const Layer* l = mObject->getLayer(mPaintOrder[p]);
        return l->groupId() == group->id && !l->isRough();
    };
    while (first > 0 && inGroup(first - 1)) { --first; }
    while (last < mPaintOrder.size() - 1 && inGroup(last + 1)) { ++last; }
    return { first, last };
}

QVector<int> CanvasPainter::paintOrder() const
{
    QVector<int> rough;
    QVector<int> others;
    for (int i = 0; i < mObject->getLayerCount(); ++i)
    {
        (mObject->getLayer(i)->isRough() ? rough : others).append(i);
    }
    return rough + others;
}
