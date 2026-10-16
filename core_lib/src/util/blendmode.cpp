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

// SSE2 is the x86-64 baseline: no runtime check. Other targets use the scalar path.
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define BLEND_SSE2
#include <emmintrin.h>
#endif

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

#ifdef BLEND_SSE2
// One channel per 16-bit lane. Lane maths wraps modulo 2^16, so it gives the exact scalar result
// whenever the true numerator is within 0..65407: true for valid premultiplied pixels.

// round(x / 255), exact for 0 <= x <= 65407.
inline __m128i div255(__m128i x)
{
    return _mm_mulhi_epu16(_mm_add_epi16(x, _mm_set1_epi16(128)), _mm_set1_epi16(257));
}

inline __m128i alphas(__m128i p)
{
    return _mm_shufflehi_epi16(_mm_shufflelo_epi16(p, 0xff), 0xff);
}

// blendChannel on 8 lanes. The alpha lanes get the alpha formula, as in the scalar path.
template <BlendMode M>
__m128i blendLanes(__m128i s, __m128i d)
{
    const __m128i sa = alphas(s);
    const __m128i da = alphas(d);
    const __m128i full = _mm_set1_epi16(255);
    if constexpr (M == BlendMode::Normal)
    {
        return _mm_add_epi16(s, div255(_mm_mullo_epi16(d, _mm_sub_epi16(full, sa))));
    }
    else if constexpr (M == BlendMode::Screen)
    {
        return _mm_sub_epi16(_mm_add_epi16(s, d), div255(_mm_mullo_epi16(s, d)));
    }
    else
    {
        const __m128i base = _mm_add_epi16(_mm_mullo_epi16(s, _mm_sub_epi16(full, da)),
                                           _mm_mullo_epi16(d, _mm_sub_epi16(full, sa)));
        __m128i b = _mm_mullo_epi16(s, d);
        if constexpr (M == BlendMode::Overlay)
        {
            const __m128i high = _mm_sub_epi16(_mm_mullo_epi16(sa, da),
                                               _mm_slli_epi16(_mm_mullo_epi16(_mm_sub_epi16(da, d), _mm_sub_epi16(sa, s)), 1));
            const __m128i useHigh = _mm_cmpgt_epi16(_mm_slli_epi16(d, 1), da);
            b = _mm_or_si128(_mm_and_si128(useHigh, high), _mm_andnot_si128(useHigh, _mm_slli_epi16(b, 1)));
        }
        return div255(_mm_add_epi16(base, b));
    }
}

// Blends 4 pixels per step; returns the number of pixels done.
template <BlendMode M>
int blendSse2(quint32* dst, const quint32* src, int count)
{
    int i = 0;
    for (; i + 4 <= count; i += 4)
    {
        const __m128i s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        const __m128i d = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + i));
        __m128i out;
        if constexpr (M == BlendMode::Add)
        {
            out = _mm_adds_epu8(s, d);
        }
        else
        {
            const __m128i zero = _mm_setzero_si128();
            out = _mm_packus_epi16(blendLanes<M>(_mm_unpacklo_epi8(s, zero), _mm_unpacklo_epi8(d, zero)),
                                   blendLanes<M>(_mm_unpackhi_epi8(s, zero), _mm_unpackhi_epi8(d, zero)));
        }
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), out);
    }
    return i;
}
#endif

QImage grabDevice(QPaintDevice* device, const QRect& rect)
{
    if (device->devType() == QInternal::Image) { return static_cast<QImage*>(device)->copy(rect); }
    if (device->devType() == QInternal::Pixmap) { return static_cast<QPixmap*>(device)->copy(rect).toImage(); }
    return QImage();
}

} // namespace

void blendPixels(BlendMode mode, quint32* dst, const quint32* src, int count)
{
    int done = 0;
#ifdef BLEND_SSE2
    switch (mode)
    {
    case BlendMode::Multiply: done = blendSse2<BlendMode::Multiply>(dst, src, count); break;
    case BlendMode::Screen: done = blendSse2<BlendMode::Screen>(dst, src, count); break;
    case BlendMode::Overlay: done = blendSse2<BlendMode::Overlay>(dst, src, count); break;
    case BlendMode::Add: done = blendSse2<BlendMode::Add>(dst, src, count); break;
    default: done = blendSse2<BlendMode::Normal>(dst, src, count); break;
    }
#endif
    blendPixelsScalar(mode, dst + done, src + done, count - done);
}

void blendPixelsScalar(BlendMode mode, quint32* dst, const quint32* src, int count)
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

void drawWithOpacity(QPainter& painter, qreal opacity, const std::function<void(QPainter&)>& paint)
{
    QPaintDevice* device = painter.device();
    QImage copy = grabDevice(device, QRect(0, 0, device->width(), device->height())).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (copy.isNull()) { return; }
    copy.setDevicePixelRatio(device->devicePixelRatioF());
    QPainter copyPainter(&copy);
    copyPainter.setRenderHints(painter.renderHints());
    copyPainter.setWorldTransform(painter.combinedTransform());
    paint(copyPainter);
    copyPainter.end();

    painter.save();
    painter.resetTransform();
    // Source with an opacity interpolates between the source and the device pixel.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setOpacity(opacity);
    painter.drawImage(QPointF(), copy);
    painter.restore();
}

void drawBlendedLayer(QPainter& painter, BlendMode mode, const std::function<void(QPainter&)>& paint)
{
    QPaintDevice* device = painter.device();
    // shortcut: a full device-size buffer per blend layer; clip to the layer bounds if export gets slow.
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
