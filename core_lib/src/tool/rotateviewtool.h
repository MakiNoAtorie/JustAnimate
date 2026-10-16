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

#ifndef ROTATEVIEWTOOL_H
#define ROTATEVIEWTOOL_H

#include "basetool.h"
#include "toolproperties.h"


/// A View tool: a drag turns the View about the canvas area centre. It never edits a Drawing.
class RotateViewTool : public BaseTool
{
    Q_OBJECT
public:
    explicit RotateViewTool(QObject* parent = nullptr);

    ToolType type() const override { return ROTATE; }
    ToolProperties& toolProperties() override { return mSettings; }
    void loadSettings() override {}
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override {}

private:
    QPointF mLastPixel;
    ToolProperties mSettings;
};

#endif // ROTATEVIEWTOOL_H
