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
#include "blendmode.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>

namespace
{

// round(x / 255), exact for 0 <= x <= 65535.
inline int div255(int x)
{
    x += 128;
    return (x + (x >> 8)) >> 8;
}

// s, d: premultiplied channels; sa, da: alphas. W3C separable blend formulas in premultiplied form;
// every numerator stays within 0..255 * 255.
inline int blendChannel(BlendMode mode, int s, int d, int sa, int da)
{
    switch (mode)
    {
    case BlendMode::Multiply: return div255(s * (255 - da) + d * (255 - sa) + s * d);
    case BlendMode::Screen: return s + d - div255(s * d);
    case BlendMode::Overlay:
    {
        const int b = 2 * d <= da ? 2 * s * d : sa * da - 2 * (da - d) * (sa - s);
        return div255(s * (255 - da) + d * (255 - sa) + b);
    }
    case BlendMode::Add: return qMin(255, s + d);
    default: return s + div255(d * (255 - sa));
    }
}

QImage grabDevice(QPaintDevice* device, const QRect& rect)
{
    if (device->devType() == QInternal::Image) { return static_cast<QImage*>(device)->copy(rect); }
    if (device->devType() == QInternal::Pixmap) { return static_cast<QPixmap*>(device)->copy(rect).toImage(); }
    return QImage();
}

} // namespace

void blendPixels(BlendMode mode, quint32* dst, const quint32* src, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const quint32 s = src[i];
        const quint32 d = dst[i];
        const int sa = int(s >> 24);
        const int da = int(d >> 24);
        const int a = mode == BlendMode::Add ? qMin(255, sa + da) : sa + da - div255(sa * da);
        quint32 out = quint32(a) << 24;
        for (int shift = 0; shift < 24; shift += 8)
        {
            out |= quint32(blendChannel(mode, int((s >> shift) & 0xff), int((d >> shift) & 0xff), sa, da)) << shift;
        }
        dst[i] = out;
    }
}

void drawBlended(QPainter& painter, const QImage& layer, const QPoint& pos, BlendMode mode)
{
    QImage src = layer.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage result = grabDevice(painter.device(), QRect(pos, src.size())).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (result.isNull() || result.size() != src.size()) { return; }

    if (painter.opacity() < 1.0)
    {
        QImage faded(src.size(), QImage::Format_ARGB32_Premultiplied);
        faded.fill(Qt::transparent);
        QPainter fader(&faded);
        fader.setOpacity(painter.opacity());
        fader.drawImage(0, 0, src);
        fader.end();
        src = faded;
    }
    for (int y = 0; y < src.height(); ++y)
    {
        blendPixels(mode, reinterpret_cast<quint32*>(result.scanLine(y)),
                    reinterpret_cast<const quint32*>(src.constScanLine(y)), src.width());
    }

    const qreal dpr = painter.device()->devicePixelRatioF();
    result.setDevicePixelRatio(dpr);
    painter.save();
    painter.resetTransform(); // also drops the window and viewport
    painter.setOpacity(1.0);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(QPointF(pos) / dpr, result);
    painter.restore();
}

void drawBlendedLayer(QPainter& painter, BlendMode mode, const std::function<void(QPainter&)>& paint)
{
    QPaintDevice* device = painter.device();
    // ponytail: a full device-size buffer per blend layer; clip to the layer bounds if export gets slow.
    QImage layer(device->width(), device->height(), QImage::Format_ARGB32_Premultiplied);
    layer.setDevicePixelRatio(device->devicePixelRatioF());
    layer.fill(Qt::transparent);
    QPainter layerPainter(&layer);
    layerPainter.setRenderHints(painter.renderHints());
    layerPainter.setWorldTransform(painter.combinedTransform());
    paint(layerPainter);
    layerPainter.end();
    layer.setDevicePixelRatio(1.0);
    drawBlended(painter, layer, QPoint(), mode);
}
