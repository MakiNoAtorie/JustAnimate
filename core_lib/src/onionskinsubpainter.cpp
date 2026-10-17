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
#include "onionskinsubpainter.h"

#include <QPainter>
#include <cmath>

#include "keyframe.h"
#include "layer.h"
#include "onionskinpainteroptions.h"

namespace
{
/// Opacity of the onion frame at 0-based distance `position` out of `count` frames.
qreal onionOpacity(const OnionSkinPainterOptions& options, int position, int count)
{
    const qreal minOpacity = static_cast<qreal>(options.minOpacity / 100);
    const qreal maxOpacity = static_cast<qreal>(options.maxOpacity / 100);
    const qreal t = static_cast<qreal>(position) / count;
    qreal fall = t;
    if (options.falloff == OnionSkinFalloff::Gentle) { fall = t * t; }
    else if (options.falloff == OnionSkinFalloff::Steep) { fall = std::sqrt(t); }
    return maxOpacity - (maxOpacity - minOpacity) * fall;
}
}

OnionSkinSubPainter::OnionSkinSubPainter()
{
}

QVector<OnionSkinSubPainter::Neighbour> OnionSkinSubPainter::neighbours(const Layer* layer, const OnionSkinPainterOptions& options, int frameIndex) const
{
    QVector<Neighbour> result;
    if (!layer->onionSkin()) { return result; }

    // Marks belong to Drawings; camera keys always onion-skin.
    const bool hasDrawings = layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR;
    const MarkFilter filter = hasDrawings ? options.markFilter : MarkFilter();

    if (options.skinPrevFrames && frameIndex >= 1)
    {
        int onionFrameNumber = layer->getPreviousFrameNumber(frameIndex, options.isAbsolute, filter);
        KeyFrame* currentAbsoluteFrame = layer->getLastKeyFrameAtPosition(frameIndex);
        int currentAbsoluteFrameNumber = currentAbsoluteFrame ? currentAbsoluteFrame->pos() : -1;

        int onionPosition = 0;
        while (onionPosition < options.framesToSkinPrev && onionFrameNumber >= 1)
        {
            // In absolute mode the current absolute frame is not skinned.
            if (!options.isAbsolute || onionFrameNumber != currentAbsoluteFrameNumber) {
                result.append({ OnionSkinPaintState::PREV, onionFrameNumber, onionOpacity(options, onionPosition, options.framesToSkinPrev) });
                onionPosition++;
            }
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, options.isAbsolute, filter);
        }
    }

    if (options.skinNextFrames)
    {
        int onionFrameNumber = layer->getNextFrameNumber(frameIndex, options.isAbsolute, filter);
        for (int onionPosition = 0; onionPosition < options.framesToSkinNext && onionFrameNumber > 0; onionPosition++)
        {
            result.append({ OnionSkinPaintState::NEXT, onionFrameNumber, onionOpacity(options, onionPosition, options.framesToSkinNext) });
            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, options.isAbsolute, filter);
        }
    }
    return result;
}

void OnionSkinSubPainter::paint(QPainter& painter, const Layer* layer, const OnionSkinPainterOptions& options, int frameIndex, const std::function<void(OnionSkinPaintState, int)>& state) const
{
    if (!options.enabledWhilePlaying && options.isPlaying) { return; }
    if (!layer->visible() || layer->keyFrameCount() == 0) { return; }

    bool currentPainted = false;
    for (const Neighbour& n : neighbours(layer, options, frameIndex))
    {
        if (n.state == OnionSkinPaintState::NEXT && !currentPainted)
        {
            state(OnionSkinPaintState::CURRENT, frameIndex);
            currentPainted = true;
        }
        painter.setOpacity(n.opacity);
        state(n.state, n.frame);
    }
    if (!currentPainted) { state(OnionSkinPaintState::CURRENT, frameIndex); }
}
