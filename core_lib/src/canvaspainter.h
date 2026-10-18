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

#ifndef CANVASPAINTER_H
#define CANVASPAINTER_H

#include <functional>
#include <memory>
#include <QBitArray>
#include <QCache>
#include <QVector>
#include <QCoreApplication>
#include <QMap>
#include <QObject>
#include <QTransform>
#include <QPainter>
#include "log.h"
#include "pencildef.h"

#include "layer.h"

#include "onionskinpainteroptions.h"
#include "onionskinsubpainter.h"


class TiledBuffer;
class Tile;
class Object;
class BitmapImage;
class ViewManager;

struct CanvasPainterOptions
{
    bool  bAntiAlias = false;
    bool  bThinLines = false;
    bool  bOutlines = false;

    LayerVisibility eLayerVisibility = LayerVisibility::RELATED;
    float fLayerVisibilityThreshold = 0.f;
    bool bOnionSkinMultiLayer = false;
    // Cache the layers above the painted layer too (SETTING::FAST_RENDER_LAYERS_ABOVE).
    bool bFastLayersAbove = false;
    float scaling = 1.0f;
    QPainter::CompositionMode cmBufferBlendMode = QPainter::CompositionMode_SourceOver;
    OnionSkinPainterOptions mOnionSkinOptions;
};

// Composite of a range of layers, filled in 64 px cells; another key or view empties it.
struct LayerCellCache
{
    QBitArray cells; // one bit per cell, set when the cell is current
    QVector<qint64> key;
    QTransform view;
};

class CanvasPainter
{
    Q_DECLARE_TR_FUNCTIONS(CanvasPainter)
public:
    explicit CanvasPainter(QImage& canvas);
    virtual ~CanvasPainter();

    void reset();
    void setViewTransform(const QTransform view, const QTransform viewInverse);

    void setOnionSkinOptions(const OnionSkinPainterOptions& onionSkinOptions) { mOnionSkinPainterOptions = onionSkinOptions;}
    void setOptions(const CanvasPainterOptions& p) { mOptions = p; }
    /// Shift and Trace: canvas-space offset per onion frame of the current layer.
    void setShiftAndTraceOffsets(const QMap<int, QTransform>& offsets) { mShiftAndTraceOffsets = offsets; }
    void setTransformedSelection(QRect selection, QTransform transform);
    void ignoreTransformedSelection();

    void setPaintSettings(const Object* object, int currentLayer, int frame, TiledBuffer* tilledBuffer);
    void paint(const QRect& blitRect);
    void paintCached(const QRect& blitRect);
    void resetLayerCache();
    int onionFramesTinted() const { return mOnionFramesTinted; }

private:

    /**
     * CanvasPainter::initializePainter
     * Enriches the painter with a context and sets it's initial matrix.
     * @param painter The in/out painter
     * @param pixmap The paint device ie. a pixmap
     * @param blitRect The rect where the blitting will occur
     */
    void initializePainter(QPainter& painter, QPaintDevice& device, const QRect& blitRect);
    void paintPaper(QPainter& painter) const;

    void paintOnionSkinOnLayer(QPainter& painter, const QRect& blitRect, Layer* layer);
    void paintOnionSkin(QPainter& painter, const QRect& blitRect);

    void renderPostLayers(QPainter& painter, const QRect& blitRect);
    void renderPreLayers(QPainter& painter, const QRect& blitRect);
    void updateCells(LayerCellCache& cache, QImage& image, const QVector<qint64>& key, const QRect& blitRect,
                     void (CanvasPainter::*render)(QPainter&, const QRect&));
    /// Positions below are positions in mPaintOrder, not layer indexes.
    QVector<qint64> layersKey(int startPos, int endPos) const;
    QVector<qint64> onionKey() const;

    void paintCurrentFrame(QPainter& painter, const QRect& blitRect, int startPos, int endPos);
    /// Paints the visible layer at index: relative opacity, its key, the stroke buffer, the selection and the Rough tint.
    void paintLayer(QPainter& painter, const QRect& blitRect, int index);
    /// Positions painted with the current layer: its group's non-Rough layers when the group is translucent.
    QPair<int, int> currentRange() const;

    void paintTransformedSelection(QPainter& painter, BitmapImage* bitmapImage, const QRect& selection, const QTransform& keyView);

    void paintBitmapOnionSkinFrame(QPainter& painter, Layer* layer, int nFrame, bool colorize);
    void paintVectorOnionSkinFrame(QPainter& painter, Layer* layer, int nFrame, bool colorize);
    /// Draws the tinted onion frame cached under key, or renders, tints and caches it.
    /// bounds hold what render paints, in Drawing coordinates; pad is in device pixels.
    void paintOnionFrame(QPainter& painter, const Layer* layer, int nFrame, bool colorize, QVector<qint64> key,
                         const QRectF& bounds, int pad, const std::function<void(QPainter&)>& render);
    void tintOnionSkinFrame(QPainter& painter, QPainter& onionSkinPainter, int nFrame, bool colorize, qreal frameOpacity);
    /// The onion frame's canvas transform: its tween, then its Shift and Trace offset.
    QTransform onionTransform(const Layer* layer, int nFrame) const;

    bool isTileVisible(const Tile* tile, const QRect& blitRect) const;
    void paintCurrentBitmapFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer);
    void paintCurrentVectorFrame(QPainter& painter, const QRect& blitRect, Layer* layer, bool isCurrentLayer);
    /// Draws mCurrentLayerImage onto painter within blitRect, blended with mode.
    void drawLayerImage(QPainter& painter, const QRect& blitRect, BlendMode mode);

    CanvasPainterOptions mOptions;

    const Object* mObject = nullptr;
    QImage& mCanvas;
    QTransform mViewTransform;
    QTransform mViewInverse;

    int mCurrentLayerIndex = 0;
    int mFrameNumber = 0;
    TiledBuffer* mTiledBuffer = nullptr;

    QImage mScaledBitmap;

    // Handle selection transformation
    bool mRenderTransform = false;
    QRect mSelection;
    QTransform mSelectionTransform;
    // The selected pixels, reused while the image, its origin and the selection rect stay.
    QPixmap mSelectionPixmap;
    qint64 mSelectionPixmapKey = 0;
    QRect mSelectionPixmapRect;
    QPoint mSelectionPixmapOrigin;

    // Caches specifically for when drawing on the canvas
    QImage mPostLayersImage;
    QImage mPreLayersImage;
    QImage mCurrentLayerImage;
    QPixmap mOnionSkinPixmap;
    LayerCellCache mPreLayersCache;
    LayerCellCache mPostLayersCache; // used only with bFastLayersAbove
    bool mPostLayersImageCacheValid = false;
    QVector<int> mPaintOrder; // FrameRenderer::paintOrder with Rough layers

    // Tinted onion frames, cropped to their bounds, keyed by content and the view's scale and rotation.
    struct OnionFrame { QPixmap pixmap; QPoint pos; QPointF view; }; // pos holds for the view translation view
    QCache<QVector<qint64>, OnionFrame> mOnionCache{ 64 << 20 }; // cost in bytes
    int mOnionFramesTinted = 0;

    // There's a considerable amount of overhead in simply allocating a QPointF on the fly.
    // Since we just need to draw it at 0,0, we might as well make a const value for that purpose
    const QPointF mPointZero;


    OnionSkinSubPainter mOnionSkinSubPainter;
    OnionSkinPainterOptions mOnionSkinPainterOptions;
    QMap<int, QTransform> mShiftAndTraceOffsets;

    const static int OVERLAY_SAFE_CENTER_CROSS_SIZE = 25;
};

#endif // CANVASRENDERER_H
