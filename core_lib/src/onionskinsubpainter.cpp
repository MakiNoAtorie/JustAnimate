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

void OnionSkinSubPainter::paint(QPainter& painter, const Layer* layer, const OnionSkinPainterOptions& options, int frameIndex, const std::function<void(OnionSkinPaintState, int)>& state) const
{
    if (!options.enabledWhilePlaying && options.isPlaying) { return; }

    if (layer->visible() == false)
        return;

    if (layer->keyFrameCount() == 0)
        return;

    if (layer->onionSkin() && options.skinPrevFrames && frameIndex >= 1)
    {
        // Paint onion skin before current frame.
        int onionFrameNumber = layer->getPreviousFrameNumber(frameIndex, options.isAbsolute);
        KeyFrame* currentAbsoluteFrame = layer->getLastKeyFrameAtPosition(frameIndex);
        int currentAbsoluteFrameNumber = currentAbsoluteFrame ? currentAbsoluteFrame->pos() : -1;

        int onionPosition = 0;
        while (onionPosition < options.framesToSkinPrev)
        {
            // We've gone below the first frame, stop iterating
            if (onionFrameNumber < 1) {
                break;
            }
            painter.setOpacity(onionOpacity(options, onionPosition, options.framesToSkinPrev));

            // When in absolute mode, we don't skin the current absolute frame
            // otherwise, if absolute is off and the current frame is in range, will be painted
            if (!options.isAbsolute || onionFrameNumber != currentAbsoluteFrameNumber) {
                state(OnionSkinPaintState::PREV, onionFrameNumber);
                onionPosition++;
            }

            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, options.isAbsolute);
        }
    }

    state(OnionSkinPaintState::CURRENT, frameIndex);

    if (layer->onionSkin() && options.skinNextFrames)
    {
        // Paint onion skin after current frame.
        int onionFrameNumber = layer->getNextFrameNumber(frameIndex, options.isAbsolute);
        int onionPosition = 0;

        while (onionPosition < options.framesToSkinNext && onionFrameNumber > 0)
        {
            painter.setOpacity(onionOpacity(options, onionPosition, options.framesToSkinNext));

            state(OnionSkinPaintState::NEXT, onionFrameNumber);

            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, options.isAbsolute);
            onionPosition++;
        }
    }
}
