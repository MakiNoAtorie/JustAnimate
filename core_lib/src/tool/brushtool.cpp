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

#include "brushtool.h"

#include <cmath>
#include <QPixmap>
#include <QPainter>
#include <QColor>

#include "beziercurve.h"
#include "vectorimage.h"
#include "editor.h"
#include "colormanager.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "selectionmanager.h"
#include "scribblearea.h"
#include "pointerevent.h"


BrushTool::BrushTool(QObject* parent) : DabTool(parent)
{
}

ToolType BrushTool::type() const
{
    return BRUSH;
}

void BrushTool::loadSettings()
{
    StrokeTool::loadSettings();

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
    mQuickSizingProperties.insert(Qt::ControlModifier, StrokeToolProperties::FEATHER_VALUE);
}

void BrushTool::drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip)
{
    mBuffer.dab(point, width, mFeather, mEditor->color()->frontColor(), opacity, false, tip);
}

QPen BrushTool::vectorPreviewPen()
{
    qreal pressure = (get(ToolProperty::PressureEnabled)) ? mCurrentPressure : 1;
    return QPen(mEditor->color()->frontColor(), get(ToolProperty::Width) * pressure, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

void BrushTool::finishVectorStroke(Layer*)
{
    commitVectorStroke(get(ToolProperty::Width), get(ToolProperty::Feather),
                       get(ToolProperty::InvisibilityEnabled), get(ToolProperty::PressureEnabled));
}
