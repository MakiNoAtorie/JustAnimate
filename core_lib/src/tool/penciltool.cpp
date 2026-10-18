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
#include "penciltool.h"

#include <QPixmap>
#include "pointerevent.h"

#include "layermanager.h"
#include "colormanager.h"
#include "viewmanager.h"
#include "preferencemanager.h"
#include "selectionmanager.h"

#include "editor.h"
#include "scribblearea.h"
#include "layervector.h"
#include "vectorimage.h"


PencilTool::PencilTool(QObject* parent) : DabTool(parent)
{
}

void PencilTool::loadSettings()
{
    StrokeTool::loadSettings();
    // The pencil keeps its feather fixed.
    toolProperties().set(ToolProperty::Feather, ToolProperties::descriptor(StrokeToolProperties::FEATHER_VALUE)->toolDefault(PENCIL));

    mQuickSizingProperties.insert(Qt::ShiftModifier, StrokeToolProperties::WIDTH_VALUE);
}

void PencilTool::strokeStarted()
{
    // note: why are we doing this on device press event?
    if (mEditor->layers()->currentLayer()->type() == Layer::VECTOR && !mEditor->preference()->get<SETTING::INVISIBLE_LINES>())
    {
        mScribbleArea->toggleThinLines();
    }
}

void PencilTool::drawDab(QPointF point, qreal width, qreal opacity, const BrushTip& tip)
{
    mBuffer.dab(point, width, get(ToolProperty::Feather), mEditor->color()->frontColor(), opacity, false, tip);
}

QPen PencilTool::vectorPreviewPen()
{
    return QPen(mEditor->color()->frontColor(), 1, Qt::DotLine, Qt::RoundCap, Qt::RoundJoin);
}

void PencilTool::finishVectorStroke(Layer*)
{
    VectorImage* image = commitVectorStroke(0, 0, true, false);
    if (image && get(ToolProperty::FillContourEnabled))
    {
        image->fillContour(mStrokePoints, mEditor->color()->frontColorNumber());
    }
}
