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
#include "framerenderer.h"

#include <functional>
#include <QPainter>

#include "bitmapimage.h"
#include "blendmode.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layervector.h"
#include "object.h"
#include "vectorimage.h"

namespace FrameRenderer
{
QVector<int> paintOrder(const Object& object, bool rough)
{
    QVector<int> roughLayers;
    QVector<int> others;
    for (int i = 0; i < object.getLayerCount(); ++i)
    {
        if (!object.getLayer(i)->isRough()) { others.append(i); }
        else if (rough) { roughLayers.append(i); }
    }
    return roughLayers + others;
}

void paintLayers(const Object& object, const QVector<int>& order, int first, int last, QPainter& painter,
                 const LayerPainter& paintLayer)
{
    for (int p = first; p <= last; ++p)
    {
        Layer* layer = object.getLayer(order[p]);
        const LayerGroup* group = object.findLayerGroup(layer->groupId());
        if (group == nullptr || group->opacity >= 1.0)
        {
            if (object.isLayerVisible(layer)) { paintLayer(painter, order[p]); }
            continue;
        }
        if (layer->isRough())
        {
            if (group->visible && object.isLayerVisible(layer)) { painter.setOpacity(group->opacity); paintLayer(painter, order[p]); }
            painter.setOpacity(1.0);
            continue;
        }
        const int start = p;
        p = qMin(groupRange(object, order, p).second, last);
        if (!group->visible || group->opacity <= 0.0) { continue; }
        drawWithOpacity(painter, group->opacity, [&](QPainter& groupPainter) {
            for (int g = start; g <= p; ++g)
            {
                groupPainter.setOpacity(1.0);
                if (object.isLayerVisible(object.getLayer(order[g]))) { paintLayer(groupPainter, order[g]); }
            }
        });
    }
}

QPair<int, int> groupRange(const Object& object, const QVector<int>& order, int pos)
{
    const Layer* layer = object.getLayer(order[pos]);
    const LayerGroup* group = object.findLayerGroup(layer->groupId());
    if (group == nullptr || group->opacity >= 1.0 || layer->isRough()) { return { pos, pos }; }
    auto inGroup = [&](int p) {
        const Layer* l = object.getLayer(order[p]);
        return l->groupId() == group->id && !l->isRough();
    };
    int first = pos;
    int last = pos;
    while (first > 0 && inGroup(first - 1)) { --first; }
    while (last < order.size() - 1 && inGroup(last + 1)) { ++last; }
    return { first, last };
}

QVector<qint64> layersKey(const Object& object, int frame, const QVector<int>& order, int first, int last)
{
    QVector<qint64> key;
    for (int p = first; p <= last; ++p)
    {
        Layer* layer = object.getLayer(order[p]);
        KeyFrame* keyFrame = layer->getLastKeyFrameAtPosition(frame);
        const LayerGroup* group = object.findLayerGroup(layer->groupId());
        const bool visible = object.isLayerVisible(layer);
        key << qint64(quintptr(layer)) << visible << int(layer->blendMode()) << layer->isRough() << qint64(quintptr(keyFrame))
            << qRound64((group ? group->opacity : 1.0) * 1e6);
        if (!visible || keyFrame == nullptr) { continue; }
        if (layer->type() == Layer::BITMAP)
        {
            auto* image = static_cast<BitmapImage*>(keyFrame);
            const QPoint topLeft = image->topLeft();
            // QImage::cacheKey changes on every write to the image.
            key << topLeft.x() << topLeft.y() << image->image()->cacheKey();
        }
        const TweenState tween = layer->tweenStateAt(frame);
        key << qRound64(tween.tx * 1e6) << qRound64(tween.ty * 1e6) << qRound64(tween.rot * 1e6) << qRound64(tween.scale * 1e6)
            << qRound64(tween.opacity * 1e6);
    }
    return key;
}

TweenState applyTween(QPainter& painter, const Layer* layer, int frame, qreal fade)
{
    const TweenState tween = layer->tweenStateAt(frame);
    painter.setOpacity(tween.opacity - fade);
    painter.setTransform(tween.transform(), true);
    return tween;
}

void paint(const Object& object, int frame, QPainter& painter, const Options& options)
{
    object.updateActiveFrames(frame);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (options.background)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white); // palette-exempt: paper colour of the frame
        painter.setWorldMatrixEnabled(false);
        painter.drawRect(QRect(0, 0, painter.device()->width(), painter.device()->height()));
        painter.setWorldMatrixEnabled(true);
    }

    auto paintLayer = [&](QPainter& target, int index)
    {
        Layer* layer = object.getLayer(index);
        std::function<void(QPainter&)> paintKey;
        if (layer->type() == Layer::BITMAP)
        {
            BitmapImage* bitmap = static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(frame);
            if (bitmap)
            {
                paintKey = [bitmap](QPainter& p) { bitmap->paintImage(p); };
            }
        }
        if (layer->type() == Layer::VECTOR)
        {
            VectorImage* vec = static_cast<LayerVector*>(layer)->getLastVectorImageAtFrame(frame);
            if (vec)
            {
                const bool antialiasing = options.antialiasing;
                paintKey = [&object, vec, antialiasing](QPainter& p) { vec->paintImage(p, object, false, false, antialiasing); };
            }
        }
        if (!paintKey) { return; }
        target.save();
        applyTween(target, layer, frame);
        if (layer->blendMode() == BlendMode::Normal) { paintKey(target); }
        else { drawBlendedLayer(target, layer->blendMode(), paintKey); }
        target.restore();
    };

    // Rough layers show only on the canvas.
    const QVector<int> order = paintOrder(object, false);
    paintLayers(object, order, 0, order.size() - 1, painter, paintLayer);
}

QImage renderForExport(const Object& object, int frame, const QTransform& view, QSize cameraSize, const ExportOptions& options)
{
    QImage image(options.size, QImage::Format_ARGB32_Premultiplied);
    QColor bgColor = Qt::white; // palette-exempt: paper colour of the export
    if (options.transparency) { bgColor.setAlpha(0); }
    image.fill(bgColor);

    QTransform centralizeCamera;
    centralizeCamera.translate(cameraSize.width() / 2, cameraSize.height() / 2);

    QPainter painter(&image);
    painter.setWorldTransform(view * centralizeCamera);
    painter.setWindow(QRect(0, 0, cameraSize.width(), cameraSize.height()));

    paint(object, frame, painter, { options.antialiasing, false });
    return image;
}

QImage renderForExport(const Object& object, int frame, const LayerCamera& camera, const ExportOptions& options)
{
    return renderForExport(object, frame, camera.getViewAtFrame(frame), camera.getViewSize(), options);
}
}
