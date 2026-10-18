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

#include "framerenderer.h"

#include <QtMath>

#include "object.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layercamera.h"
#include "objectdata.h"
#include "paperpainter.h"
#include "stageprofiler.h"
#include "bitmapimage.h"
#include "tile.h"
#include "tiledbuffer.h"
#include "vectorimage.h"

#include "painterutils.h"
#include "blendmode.h"

namespace
{
const int kCell = 64; // below-composite cache cell, in logical px

// Vector Drawings paint with the palette colours.
qint64 paletteKey(const Object& object)
{
    size_t hash = 0;
    for (int i = 0; i < object.getColorCount(); ++i) { hash = qHashMulti(hash, quint64(object.getColor(i).color.rgba64())); }
    return qint64(hash);
}
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
    mSelectionPixmap = QPixmap();
}

void CanvasPainter::paintCached(const QRect& blitRect)
{
    mPaintOrder = FrameRenderer::paintOrder(*mObject, true);
    const QPair<int, int> range = currentRange();
    updateCells(mPreLayersCache, mPreLayersPixmap, layersKey(0, range.first - 1) + onionKey(), blitRect, &CanvasPainter::renderPreLayers);

    QPainter mainPainter;
    initializePainter(mainPainter, mCanvas, blitRect);
    paintPaper(mainPainter);
    mainPainter.setWorldMatrixEnabled(false);
    mainPainter.drawPixmap(mPointZero, mPreLayersPixmap);
    mainPainter.setWorldMatrixEnabled(true);

    paintCurrentFrame(mainPainter, blitRect, range.first, range.second);

    // The layers above paint straight onto the canvas when one of them blends.
    if (FrameRenderer::blendsAbove(*mObject, mPaintOrder, range.second))
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
    return key + FrameRenderer::layersKey(*mObject, mFrameNumber, mPaintOrder, startPos, endPos);
}

// Identifies the onion frames renderPreLayers paints; a changed onion frame empties the cache.
QVector<qint64> CanvasPainter::onionKey() const
{
    QVector<qint64> key;
    bool vector = false;
    const bool allLayers = mOptions.bOnionSkinMultiLayer && mOptions.eLayerVisibility != LayerVisibility::CURRENTONLY;
    for (int i = 0; i < mObject->getLayerCount(); ++i)
    {
        Layer* layer = mObject->getLayer(i);
        if ((!allLayers && i != mCurrentLayerIndex) || (layer->type() != Layer::BITMAP && layer->type() != Layer::VECTOR)) { continue; }
        key << i << mObject->isLayerVisible(layer);
        for (const OnionSkinSubPainter::Neighbour& n : mOnionSkinSubPainter.neighbours(layer, mOnionSkinPainterOptions, mFrameNumber))
        {
            KeyFrame* image = layer->getKeyFrameAt(n.frame);
            if (image == nullptr) { continue; }
            key << n.frame;
            if (layer->type() == Layer::BITMAP)
            {
                auto* bitmap = static_cast<BitmapImage*>(image);
                key << bitmap->topLeft().x() << bitmap->topLeft().y() << bitmap->image()->cacheKey();
            }
            else
            {
                key << image->stamp();
                vector = true;
            }
            const QTransform t = onionTransform(layer, n.frame);
            const qreal opacity = layer->tweenStateAt(n.frame).opacity;
            key << qRound64(t.m11() * 1e6) << qRound64(t.m12() * 1e6) << qRound64(t.m21() * 1e6) << qRound64(t.m22() * 1e6)
                << qRound64(t.dx() * 1e6) << qRound64(t.dy() * 1e6) << qRound64(opacity * 1e6);
        }
    }
    if (vector) { key << paletteKey(*mObject); }
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
    StageScope stage(StageProfiler::CellRender);

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
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizePrevFrames); break; }
            case Layer::VECTOR: { paintVectorOnionSkinFrame(painter, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizePrevFrames); break; }
            default: break;
            }
        }
        if (state == OnionSkinPaintState::NEXT) {
            switch (layer->type())
            {
            case Layer::BITMAP: { paintBitmapOnionSkinFrame(painter, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizeNextFrames); break; }
            case Layer::VECTOR: { paintVectorOnionSkinFrame(painter, layer, onionFrameNumber, mOnionSkinPainterOptions.colorizeNextFrames); break; }
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

void CanvasPainter::paintBitmapOnionSkinFrame(QPainter& painter, Layer* layer, int nFrame, bool colorize)
{
    BitmapImage* bitmapImage = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(nFrame);
    if (bitmapImage == nullptr) { return; }
    bitmapImage->loadFile(); // Critical! force the BitmapImage to load the image

    // QImage::cacheKey changes on every write to the image.
    const QPoint topLeft = bitmapImage->topLeft();
    const QVector<qint64> key{ bitmapImage->image()->cacheKey(), topLeft.x(), topLeft.y() };
    paintOnionFrame(painter, layer, nFrame, colorize, key, bitmapImage->bounds(), 1,
                    [&](QPainter& onionSkinPainter) { onionSkinPainter.drawImage(topLeft, *bitmapImage->image()); });
}

void CanvasPainter::paintVectorOnionSkinFrame(QPainter& painter, Layer* layer, int nFrame, bool colorize)
{
    CANVASPAINTER_LOG("Paint Onion skin vector, Frame = %d", nFrame);
    VectorImage* vectorImage = static_cast<LayerVector*>(layer)->getVectorImageAtFrame(nFrame);
    if (vectorImage == nullptr) { return; }

    // One entry more than a bitmap key, so the two never match. Pens of a fixed device width reach 2 px past paintBounds.
    const QVector<qint64> key{ vectorImage->stamp(), paletteKey(*mObject), mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias };
    paintOnionFrame(painter, layer, nFrame, colorize, key, vectorImage->paintBounds(), 2, [&](QPainter& onionSkinPainter) {
        vectorImage->paintImage(onionSkinPainter, *mObject, mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias);
    });
}

void CanvasPainter::paintOnionFrame(QPainter& painter, const Layer* layer, int nFrame, bool colorize, QVector<qint64> key,
                                    const QRectF& bounds, int pad, const std::function<void(QPainter&)>& render)
{
    // Everything the tint reads.
    QColor tint = Qt::transparent;
    if (nFrame < mFrameNumber) { tint = mOnionSkinPainterOptions.prevColor; }
    else if (nFrame > mFrameNumber) { tint = mOnionSkinPainterOptions.nextColor; }
    const TweenState tween = layer->tweenStateAt(nFrame);
    const qreal opacity = tween.opacity - (1.0 - painter.opacity());
    const QTransform canvasTransform = onionTransform(layer, nFrame);
    // The view's translation is not in the key: a pan moves the cached pixmap by whole pixels.
    key << colorize << (colorize ? qint64(tint.rgba64()) : 0) << (colorize ? qRound64(opacity * 1e6) : 0)
        << qRound64(canvasTransform.m11() * 1e6) << qRound64(canvasTransform.m12() * 1e6)
        << qRound64(canvasTransform.m21() * 1e6) << qRound64(canvasTransform.m22() * 1e6)
        << qRound64(canvasTransform.dx() * 1e6) << qRound64(canvasTransform.dy() * 1e6)
        << qRound64(mViewTransform.m11() * 1e6) << qRound64(mViewTransform.m12() * 1e6)
        << qRound64(mViewTransform.m21() * 1e6) << qRound64(mViewTransform.m22() * 1e6);

    painter.setWorldMatrixEnabled(false);
    const QRect canvasRect(QPoint(), mOnionSkinPixmap.deviceIndependentSize().toSize());
    const QTransform world = canvasTransform * mViewTransform;
    const QRect shown = world.mapRect(bounds).toAlignedRect().adjusted(-pad, -pad, pad, pad) & canvasRect;
    if (shown.isEmpty()) { return; }

    // A pan by exactly whole pixels moves the cached pixmap. Only unrotated draws shift pixel for pixel, and the tint
    // outlines the canvas edges, so a frame that touches an edge is tinted again.
    const QPointF view(mViewTransform.dx(), mViewTransform.dy());
    if (const OnionFrame* cached = mOnionCache.object(key))
    {
        const QPointF moved = view - cached->view;
        const QPoint step = moved.toPoint();
        const QRect drawn(cached->pos, cached->pixmap.deviceIndependentSize().toSize());
        const QRect inner = canvasRect.adjusted(1, 1, -1, -1);
        const bool shifts = world.m12() == 0 && world.m21() == 0 && moved == QPointF(step)
                            && inner.contains(drawn) && inner.contains(shown);
        const QRect at = drawn.translated(step);
        if ((moved.isNull() || shifts) && at.contains(shown))
        {
            painter.drawPixmap(at.topLeft(), cached->pixmap);
            return;
        }
    }

    StageScope stage(StageProfiler::OnionTint);
    // Outside the Drawing the onion pixmap stays transparent, so only its bounds are rendered.
    QPainter onionSkinPainter;
    initializePainter(onionSkinPainter, mOnionSkinPixmap, shown);
    onionSkinPainter.setWorldTransform(world);
    render(onionSkinPainter);
    tintOnionSkinFrame(painter, onionSkinPainter, nFrame, colorize, tween.opacity);
    onionSkinPainter.end();
    const qreal dpr = mOnionSkinPixmap.devicePixelRatio();
    const QPixmap pixmap = mOnionSkinPixmap.copy(QRectF(QPointF(shown.topLeft()) * dpr, QSizeF(shown.size()) * dpr).toAlignedRect());
    ++mOnionFramesTinted;
    mOnionCache.insert(key, new OnionFrame{ pixmap, shown.topLeft(), view }, qMax(1, pixmap.width() * pixmap.height() * 4));
    painter.drawPixmap(shown.topLeft(), pixmap);
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

// A tile outside the blit rect would be clipped whole; the margin covers smooth resampling.
bool CanvasPainter::isTileVisible(const Tile* tile, const QRect& blitRect) const
{
    return mViewTransform.mapRect(QRectF(tile->bounds())).toAlignedRect().adjusted(-1, -1, 1, 1).intersects(blitRect);
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

    const TweenState tween = FrameRenderer::applyTween(currentBitmapPainter, layer, mFrameNumber, 1.0 - painter.opacity());
    currentBitmapPainter.drawImage(paintedImage->topLeft(), *paintedImage->image());
    currentBitmapPainter.setWorldTransform(mViewTransform);

    if (isCurrentLayer && isDrawing)
    {
        currentBitmapPainter.setCompositionMode(mOptions.cmBufferBlendMode);
        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            if (!isTileVisible(tile, blitRect)) { continue; }
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

    const bool isDrawing = mTiledBuffer && mTiledBuffer->isValid();

    if (mRenderTransform) {
        vectorImage->setSelectionTransformation(mSelectionTransform);
    }

    // Paint existing vector image to the painter
    // Remember to adjust opacity based on additional opacity value from the keyframe
    FrameRenderer::applyTween(currentVectorPainter, layer, mFrameNumber, 1.0 - painter.opacity());
    vectorImage->paintImage(currentVectorPainter, *mObject, mOptions.bOutlines, mOptions.bThinLines, mOptions.bAntiAlias);
    currentVectorPainter.setWorldTransform(mViewTransform);

    if (isCurrentLayer && isDrawing) {
        currentVectorPainter.setCompositionMode(mOptions.cmBufferBlendMode);

        const auto tiles = mTiledBuffer->tiles();
        for (const Tile* tile : tiles) {
            if (!isTileVisible(tile, blitRect)) { continue; }
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

void CanvasPainter::paintTransformedSelection(QPainter& painter, BitmapImage* bitmapImage, const QRect& selection, const QTransform& keyView)
{
    // Make sure there is something selected
    if (selection.width() == 0 && selection.height() == 0)
        return;

    const QPoint origin = bitmapImage->topLeft(); // crops first, which can replace the image
    const qint64 key = bitmapImage->image()->cacheKey();
    if (mSelectionPixmap.isNull() || key != mSelectionPixmapKey || selection != mSelectionPixmapRect
        || origin != mSelectionPixmapOrigin)
    {
        mSelectionPixmap = QPixmap(selection.size());
        mSelectionPixmap.fill(Qt::transparent);

        QPainter imagePainter(&mSelectionPixmap);
        imagePainter.translate(-selection.topLeft());
        imagePainter.drawImage(origin, *bitmapImage->image());
        imagePainter.end();
        mSelectionPixmapKey = key;
        mSelectionPixmapRect = selection;
        mSelectionPixmapOrigin = origin;
    }

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
    painter.drawPixmap(selection, mSelectionPixmap);
    painter.restore();
}

void CanvasPainter::paintCurrentFrame(QPainter& painter, const QRect& blitRect, int startPos, int endPos)
{
    painter.setOpacity(1.0);
    FrameRenderer::paintLayers(*mObject, mPaintOrder, startPos, endPos, painter,
                               [&](QPainter& target, int index) { paintLayer(target, blitRect, index); });
}

void CanvasPainter::paintLayer(QPainter& painter, const QRect& blitRect, int index)
{
    Layer* layer = mObject->getLayer(index);
    if (mOptions.eLayerVisibility == LayerVisibility::RELATED && mObject->getLayer(mCurrentLayerIndex)->type() != Layer::CAMERA)
    {
        painter.setOpacity(calculateRelativeOpacityForLayer(mCurrentLayerIndex, index, mOptions.fLayerVisibilityThreshold));
    }
    const bool isCurrentLayer = mCurrentLayerIndex == index;

    CANVASPAINTER_LOG("  Render Layer[%d] %s", index, layer->name());
    switch (layer->type())
    {
    case Layer::BITMAP: { StageScope stage(StageProfiler::LayerDraw); paintCurrentBitmapFrame(painter, blitRect, layer, isCurrentLayer); break; }
    case Layer::VECTOR: { StageScope stage(StageProfiler::LayerDraw); paintCurrentVectorFrame(painter, blitRect, layer, isCurrentLayer); break; }
    default: break;
    }
}

QPair<int, int> CanvasPainter::currentRange() const
{
    const int pos = mPaintOrder.indexOf(mCurrentLayerIndex);
    const bool currentOnly = mOptions.eLayerVisibility == LayerVisibility::CURRENTONLY
                             && mObject->getLayer(mCurrentLayerIndex)->type() != Layer::CAMERA;
    if (currentOnly) { return { pos, pos }; }
    return FrameRenderer::groupRange(*mObject, mPaintOrder, pos);
}
