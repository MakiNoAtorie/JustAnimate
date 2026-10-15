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

#ifndef ERASERTOOL_H
#define ERASERTOOL_H

#include "dabtool.h"

class EraserTool : public DabTool
{
    Q_OBJECT

public:
    explicit EraserTool(QObject* parent = nullptr);
    ToolType type() const override;

    ToolProperties& toolProperties() override { return mSettings.toolProperties(); }
    const StrokeToolProperties& strokeToolProperties() const override { return mSettings; }

    void loadSettings() override;
    QCursor cursor() override;

    void removeVectorPaint();
    void strokeMoved() override;

protected:
    void drawDab(QPointF point, qreal width, qreal opacity) override;
    QPen vectorPreviewPen() override;
    void finishVectorStroke(Layer* layer) override;
    bool showsPrediction() const override { return false; }

    StrokeToolProperties mSettings;
};

#endif // ERASERTOOL_H
