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
#ifndef ZOOMTOOL_H
#define ZOOMTOOL_H

#include "basetool.h"
#include "toolproperties.h"

/// A View tool: a click zooms in at the pointer, Alt+click zooms out, a horizontal drag zooms about the press point.
class ZoomTool : public BaseTool
{
    Q_OBJECT
public:
    explicit ZoomTool(QObject* parent = nullptr);

    ToolType type() const override { return ZOOM; }
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;

private:
    QPointF mPressPixel;
    QPointF mAnchor; // View offset of the press point
    qreal mPressScale = 1;
    bool mDragged = false;
    bool mZoomOut = false;
};

#endif // ZOOMTOOL_H
