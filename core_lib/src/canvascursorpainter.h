/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef CANVASCURSORPAINTER_H
#define CANVASCURSORPAINTER_H

#include <QPainterPath>
#include <QPen>

class QPainter;

struct CanvasCursorPainterOptions
{
    QPainterPath shape; // in canvas coordinates
    bool showCursor = false;
};

class CanvasCursorPainter
{

public:
    CanvasCursorPainter();
    void paint(QPainter& painter, const QRect& blitRect);

    void preparePainter(const CanvasCursorPainterOptions& painterOptions);

    const QRect dirtyRect() { return mDirtyRect; }
    bool isDirty() const { return mIsDirty; }
    void clearDirty();

private:

    void setupPen();

    void paintOutline(QPainter& painter, const QRect& blitRect);

    CanvasCursorPainterOptions mOptions;
    QRect mDirtyRect;
    bool mIsDirty = false;

    QPen mCursorPen;
};

#endif // CANVASCURSORPAINTER_H
