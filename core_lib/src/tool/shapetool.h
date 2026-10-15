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

/// Drags out a line, rectangle or ellipse. Bitmap shapes are dabbed with this tool's brush settings.
class ShapeTool : public BrushTool
{
    Q_OBJECT

public:
    enum Mode { LINE = 0, RECTANGLE = 1, ELLIPSE = 2 };

    explicit ShapeTool(QObject* parent = nullptr);

    ToolType type() const override;
    void loadSettings() override;
    QCursor cursor() override;

    int shapeMode() const;
    void setShapeMode(int mode);

    /// The shape dragged from `from` to `to`, in canvas points; closed shapes end on their first point.
    /// Shift gives 45 degree lines, squares and circles; Alt makes `from` the centre.
    static QPolygonF outline(int mode, QPointF from, QPointF to, Qt::KeyboardModifiers modifiers);

    void pointerPressEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    bool keyPressEvent(QKeyEvent* event) override;
    void paint(QPainter& painter, const QRect& blitRect) override;

signals:
    void shapeModeChanged(int mode);

private:
    void setOutline(const QPolygonF& outline);
    QRect outlineScreenRect() const;

    bool mDrawing = false;
    QPointF mFrom;
    QPolygonF mOutline; // canvas points of the live preview
};

#endif // SHAPETOOL_H
