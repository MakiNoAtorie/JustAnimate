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

#include "basetool.h"

#include <array>
#include "editor.h"
#include "viewmanager.h"
#include "scribblearea.h"
#include "strokeinterpolator.h"
#include "pointerevent.h"
#include "layermanager.h"
#include "keyframe.h"
#include "beziercurve.h"
#include "pencilsettings.h"

QString BaseTool::TypeName(ToolType type)
{
    static std::array<QString, TOOL_TYPE_COUNT> map;

    if (map[0].isEmpty())
    {
        map[PENCIL] = tr("Pencil");
        map[ERASER] = tr("Eraser");
        map[SELECT] = tr("Select");
        map[MOVE] = tr("Move");
        map[HAND] = tr("Hand");
        map[SMUDGE] = tr("Smudge");
        map[PEN] = tr("Pen");
        map[POLYLINE] = tr("Polyline");
        map[BUCKET] = tr("Bucket");
        map[EYEDROPPER] = tr("Eyedropper");
        map[BRUSH] = tr("Brush");
        map[CAMERA] = tr("Camera");
        map[SHAPE] = tr("Shape");
        map[LASSOFILL] = tr("Lasso Fill");
        map[ROTATE] = tr("Rotate View");
        map[ZOOM] = tr("Zoom");
    }
    return map.at(type);
}

BaseTool::BaseTool(QObject* parent) : QObject(parent)
{
}

BaseTool::~BaseTool() {}

void BaseTool::initialize(Editor* editor)
{
    Q_ASSERT(editor);
    mEditor = editor;
    mScribbleArea = editor->getScribbleArea();
    Q_ASSERT(mScribbleArea);
    loadSettings();
}

void BaseTool::saveSettings()
{
    QSettings& storedSettings = pencilSettings();
    toolProperties().storeTo(storedSettings);
}

void BaseTool::resetSettings()
{
    toolProperties().restoreProperties();
}

bool BaseTool::isPropertyEnabled(int rawType)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (!currentLayer) {
        return false;
    }

    return mPropertyUsed[rawType].contains(currentLayer->type());
}

QCursor BaseTool::cursor()
{
    return Qt::ArrowCursor;
}

bool BaseTool::leavingThisTool()
{
   for (auto& connection : mActiveConnections) {
       disconnect(connection);
       mActiveConnections.removeOne(connection);
   }

   saveSettings();

   return true;
}

void BaseTool::pointerPressEvent(PointerEvent* event)
{
    event->accept();
}

void BaseTool::pointerMoveEvent(PointerEvent* event)
{
    event->accept();
}

void BaseTool::pointerReleaseEvent(PointerEvent* event)
{
    event->accept();
}

void BaseTool::pointerDoubleClickEvent(PointerEvent* event)
{
    pointerPressEvent(event);
}

/**
 * @brief BaseTool::isDrawingTool - A drawing tool is anything that applies something to the canvas.
 * SELECT and MOVE does not count here because they modify already applied content.
 * @return true if not a drawing tool and false otherwise
 */
bool BaseTool::isDrawingTool()
{
    if (type() == ToolType::HAND || type() == ToolType::ROTATE || type() == ToolType::ZOOM || type() == ToolType::MOVE || type() == ToolType::CAMERA || type() == ToolType::SELECT )
    {
        return false;
    }
    return true;
}

bool BaseTool::isActive() const
{
    return false;
}

void BaseTool::mapCurveToKey(BezierCurve& curve) const
{
    const QTransform toKey = canvasToKey();
    if (toKey.isIdentity()) { return; }
    // BezierCurve::transform moves only selected vertices; a new curve is not selected.
    curve.setSelected(true);
    curve.transform(toKey);
    curve.setSelected(false);
}

QTransform BaseTool::canvasToKey() const
{
    const Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) { return QTransform(); }
    return layer->tweenStateAt(mEditor->currentFrame()).transform().inverted();
}
