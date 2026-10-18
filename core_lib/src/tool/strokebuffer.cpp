/*

JustAnimate - Traditional Animation Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "strokebuffer.h"

#include "bitmapimage.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "vectorimage.h"

namespace
{
LayerBitmap* bitmapLayer(Layer* layer)
{
    return layer && layer->type() == Layer::BITMAP ? static_cast<LayerBitmap*>(layer) : nullptr;
}
}

void StrokeBuffer::begin(Layer* layer, int frame, Mode mode)
{
    mTiles.clear();
    mLayer = layer;
    mFrame = frame;
    mMode = mode;
}

void StrokeBuffer::dab(QPointF center, qreal width, qreal feather, QColor color, qreal opacity, bool antialiasing,
                       const BrushTip& tip, const SmudgeSettings& smudge)
{
    if (mMode != Mode::Smudge)
    {
        mTiles.drawDab(center, width, feather, color, opacity, antialiasing, tip);
        return;
    }
    if (!mTiles.isSmudging())
    {
        LayerBitmap* layer = bitmapLayer(mLayer);
        BitmapImage* image = layer ? layer->getLastBitmapImageAtFrame(mFrame) : nullptr;
        if (image == nullptr) { return; } // the first frame can be deleted while drawing
        // Dabs land in canvas space, so the source is the key as drawn there.
        BitmapImage source = image->mapped(layer->tweenStateAt(mFrame).transform());
        mTiles.setSmudgeSource(*source.image(), source.topLeft());
    }
    mTiles.smudgeDab(center, width, feather, color, opacity, antialiasing, tip, smudge);
}

QPainter::CompositionMode StrokeBuffer::compositionMode() const
{
    const LayerBitmap* layer = bitmapLayer(mLayer);
    const bool lockAlpha = layer && layer->lockAlpha();
    switch (mMode)
    {
    case Mode::Erase:
        return lockAlpha ? QPainter::CompositionMode_Destination : QPainter::CompositionMode_DestinationOut;
    // Smudge tiles hold the layer pixels they change.
    case Mode::Smudge:
        return lockAlpha ? QPainter::CompositionMode_SourceAtop : QPainter::CompositionMode_Source;
    default:
        return lockAlpha ? QPainter::CompositionMode_SourceAtop : QPainter::CompositionMode_SourceOver;
    }
}

QRect StrokeBuffer::commit()
{
    QRect changed;
    LayerBitmap* layer = bitmapLayer(mLayer);
    BitmapImage* image = layer ? layer->getLastBitmapImageAtFrame(mFrame) : nullptr;
    if (image != nullptr)
    {
        changed = mTiles.bounds();
        // The stroke is drawn in canvas space; map it into the space of the key.
        image->paste(&mTiles, compositionMode(), layer->tweenStateAt(mFrame).transform().inverted());
        layer->setModified(mFrame, true);
    }
    clear();
    return changed;
}

VectorImage* StrokeBuffer::commit(BezierCurve curve, qreal scaling, bool interacts)
{
    LayerVector* layer = mLayer && mLayer->type() == Layer::VECTOR ? static_cast<LayerVector*>(mLayer) : nullptr;
    VectorImage* image = layer ? layer->getLastVectorImageAtFrame(mFrame) : nullptr;
    if (image != nullptr)
    {
        const QTransform toKey = layer->tweenStateAt(mFrame).transform().inverted();
        if (!toKey.isIdentity())
        {
            // BezierCurve::transform moves only selected vertices; a new curve is not selected.
            curve.setSelected(true);
            curve.transform(toKey);
            curve.setSelected(false);
        }
        image->addCurve(curve, scaling, interacts);
        layer->setModified(mFrame, true);
    }
    clear();
    return image;
}

void StrokeBuffer::clear()
{
    mTiles.clear();
    mLayer = nullptr; // the layer can go once the stroke ends
}
