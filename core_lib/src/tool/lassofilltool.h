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
#ifndef LASSOFILLTOOL_H
#define LASSOFILLTOOL_H

#include <QPolygonF>

#include "basetool.h"
#include "toolproperties.h"

/// Drag a freehand loop; on release it is filled with the front colour.
class LassoFillTool : public BaseTool
{
    Q_OBJECT

public:
    explicit LassoFillTool(QObject* parent = nullptr);

    ToolType type() const override { return LASSOFILL; }
    QCursor cursor() override;
    bool isActive() const override { return !mLoop.isEmpty(); }

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void paint(QPainter& painter, const QRect& blitRect) override;

private:
    void fill();
    void updateLoopRect();

    QPolygonF mLoop; // canvas points
};

#endif // LASSOFILLTOOL_H
