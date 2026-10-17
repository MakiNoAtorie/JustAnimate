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
#ifndef CELLMENUS_H
#define CELLMENUS_H

#include <functional>
#include <QPixmap>
#include "cameraeasingtype.h"

class Editor;
class Layer;
class QMenu;
class QPoint;
class QString;

/// The right-click menus of a layer's cell and of a layer, shared by the timeline and the X-sheet.
/// They hold no widget state: each action edits through the Editor when it is chosen.
namespace CellMenus
{
/// The menu of layer's cell at frame: Tween, Mark and Unlink on a Drawing, markers on a sound layer,
/// the camera menu on a camera key. Makes layer current first. Shows nothing on other cells.
void showCellMenu(Editor* editor, Layer* layer, int frame, const QPoint& globalPos);

/// The layer menu: Lock Transparency, Rough, Storyboard, Blend Mode, Timesheet Column and groups.
void showLayerMenu(Editor* editor, Layer* layer, const QPoint& globalPos);

/// Add, rename, recolour, move or remove the marker at frame; layerId 0 is the scene's markers.
void showMarkerMenu(Editor* editor, int layerId, int frame, const QPoint& globalPos);

/// Adds a submenu of every easing type with its curve; checked is ticked, pick runs with the chosen type.
QMenu* addEasingMenu(QMenu* menu, const QString& title, CameraEasingType checked, const std::function<void(CameraEasingType)>& pick);

/** A small plot of the easing curve. */
QPixmap easingPreview(CameraEasingType easing);
}

#endif // CELLMENUS_H
