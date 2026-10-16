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

#include "rotateviewtool.h"

#include <QPixmap>

#include "editor.h"
#include "pointerevent.h"
#include "scribblearea.h"
#include "viewmanager.h"

RotateViewTool::RotateViewTool(QObject* parent) : BaseTool(parent)
{
}

QCursor RotateViewTool::cursor()
{
    return QCursor(QPixmap(":icons/general/cursor-rotate.svg"));
}

void RotateViewTool::pointerPressEvent(PointerEvent* event)
{
    mLastPixel = event->viewportPos();
}

void RotateViewTool::pointerMoveEvent(PointerEvent* event)
{
    if (event->buttons() == Qt::NoButton) { return; }

    const QPointF centre(mScribbleArea->width() / 2, mScribbleArea->height() / 2);
    mEditor->view()->rotateByDrag(centre, mLastPixel, event->viewportPos());
    mLastPixel = event->viewportPos();
}
