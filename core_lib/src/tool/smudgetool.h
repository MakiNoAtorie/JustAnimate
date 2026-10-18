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

#ifndef SMUDGETOOL_H
#define SMUDGETOOL_H

#include "dabtool.h"

/// Smudge preset of the brush engine on bitmap layers; moves curve vertices on vector layers.
class SmudgeTool : public DabTool
{
    Q_OBJECT
public:
    explicit SmudgeTool(QObject* parent = nullptr);

    ToolType type() const override;

    void loadSettings() override;

    void pointerPressEvent(PointerEvent*) override;
    void pointerReleaseEvent(PointerEvent*) override;
    void pointerMoveEvent(PointerEvent*) override;

protected:
    bool emptyFrameActionEnabled() override;
    void drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip) override;
    QPen vectorPreviewPen() override { return QPen(); }
    void finishVectorStroke(Layer*) override {}
    // The prediction line shows the front colour, which a smudge does not paint.
    bool showsPrediction() const override { return false; }
    StrokeBuffer::Mode bufferMode() const override { return StrokeBuffer::Mode::Smudge; }

private:
    QPointF offsetFromPressPos();
};

#endif // SMUDGETOOL_H
