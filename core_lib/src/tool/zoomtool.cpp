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
#include "zoomtool.h"

#include <cmath>

#include "editor.h"
#include "pointerevent.h"
#include "viewmanager.h"

namespace
{
constexpr qreal kClickFactor = 2;
constexpr qreal kDragThreshold = 4;      // screen px before a press becomes a drag
constexpr qreal kDragRate = 0.01;        // e-fold of the scale per px, so 100 px is about 2.7x
}

ZoomTool::ZoomTool(QObject* parent) : BaseTool(parent)
{
}

QCursor ZoomTool::cursor()
{
    return QCursor(Qt::SizeHorCursor);
}

void ZoomTool::pointerPressEvent(PointerEvent* event)
{
    mPressPixel = event->viewportPos();
    mAnchor = mEditor->view()->mapScreenToCanvas(mPressPixel);
    mPressScale = mEditor->view()->scaling();
    mZoomOut = event->modifiers().testFlag(Qt::AltModifier);
    mDragged = false;
}

void ZoomTool::pointerMoveEvent(PointerEvent* event)
{
    if (event->buttons() == Qt::NoButton) { return; }
    const qreal dx = event->viewportPos().x() - mPressPixel.x();
    if (!mDragged && std::abs(dx) < kDragThreshold) { return; }
    mDragged = true;
    mEditor->view()->scaleAtOffset(mPressScale * std::exp(dx * kDragRate), mAnchor);
}

void ZoomTool::pointerReleaseEvent(PointerEvent*)
{
    if (mDragged) { return; }
    const qreal factor = mZoomOut ? 1 / kClickFactor : kClickFactor;
    mEditor->view()->scaleAtOffset(mPressScale * factor, mAnchor);
}
