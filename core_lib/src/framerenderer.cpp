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

    auto paintLayer = [&](QPainter& target, Layer* layer)
    {
        // Rough layers show only on the canvas.
        if (!object.isLayerVisible(layer) || layer->isRough()) { return; }

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
        const TweenState tween = layer->tweenStateAt(frame);
        target.save();
        target.setOpacity(tween.opacity);
        target.setTransform(tween.transform(), true);
        if (layer->blendMode() == BlendMode::Normal) { paintKey(target); }
        else { drawBlendedLayer(target, layer->blendMode(), paintKey); }
        target.restore();
    };

    for (int i = 0; i < object.getLayerCount(); ++i)
    {
        const LayerGroup* group = object.findLayerGroup(object.getLayer(i)->groupId());
        if (group == nullptr || group->opacity >= 1.0)
        {
            paintLayer(painter, object.getLayer(i));
            continue;
        }
        const int first = i;
        i = object.layerGroupRange(group->id).second;
        if (!group->visible || group->opacity <= 0.0) { continue; }
        drawWithOpacity(painter, group->opacity, [&](QPainter& p) {
            for (int j = first; j <= i; ++j) { paintLayer(p, object.getLayer(j)); }
        });
    }
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
