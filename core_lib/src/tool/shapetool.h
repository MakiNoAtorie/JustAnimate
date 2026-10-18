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
#ifndef SHAPETOOL_H
#define SHAPETOOL_H

#include "brushtool.h"

/// Drags out a line, rectangle or ellipse, or clicks out a polygon. Bitmap shapes are dabbed with this tool's brush settings.
class ShapeTool : public BrushTool
{
    Q_OBJECT

public:
    enum Mode { LINE = 0, RECTANGLE = 1, ELLIPSE = 2, POLYGON = 3 };

    explicit ShapeTool(QObject* parent = nullptr);

    ToolType type() const override;
    void loadSettings() override;
    QCursor cursor() override;

    Mode shapeMode() const;

    /// The shape dragged from `from` to `to`, in canvas points; closed shapes end on their first point.
    /// Shift gives 45 degree lines, squares and circles; Alt makes `from` the centre.
    static QPolygonF outline(Mode mode, QPointF from, QPointF to, Qt::KeyboardModifiers modifiers);

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void pointerDoubleClickEvent(PointerEvent*) override;
    bool keyPressEvent(QKeyEvent* event) override;
    bool enteringThisTool() override;
    bool leavingThisTool() override;
    /// Drops a polygon that is not closed yet.
    void clearToolData() override;
    void paint(QPainter& painter, const QRect& blitRect) override;

private:
    void finishShape(const QPolygonF& shape, PointerEvent* event);
    void setOutline(const QPolygonF& outline);
    QRect outlineScreenRect() const;

    bool mDrawing = false;
    QPointF mFrom;
    QPolygonF mPolygon; // clicked polygon points, canvas
    QPolygonF mOutline; // canvas points of the live preview
};

#endif // SHAPETOOL_H
