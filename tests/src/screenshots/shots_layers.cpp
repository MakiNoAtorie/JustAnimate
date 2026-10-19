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
#include "catch.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QMenu>
#include <QToolButton>

#include "attributes.h"
#include "cellmenus.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "screenshots.h"

namespace
{
// Opens the layer cell menu for `layer`, saves it (and the Blend Mode submenu when `blendName` is set), then closes it.
void shootLayerMenu(Editor* editor, Layer* layer, const QString& menuName, const QString& blendName)
{
    whenMenuOpens([=](QMenu* menu) {
        saveShot(menu, menuName);
        if (!blendName.isEmpty())
        {
            for (QAction* a : menu->actions())
            {
                if (a->menu() && a->text() == "Blend Mode")
                {
                    a->menu()->popup(menu->pos() + QPoint(menu->width(), menu->actionGeometry(a).y()));
                    saveShot(a->menu(), blendName);
                    a->menu()->close();
                }
            }
        }
    });
    CellMenus::showLayerMenu(editor, layer, QPoint(300, 300));
}
} // namespace


// Shots for docs/manual/*-layers.md. Each SECTION saves images/layers-<shot>.png.
TEST_CASE("Manual screenshots: Layers", "[.screenshots]")
{
    ShotWindow shot;
    LayerManager* layers = shot.editor->layers();

    SECTION("timeline")
    {
        saveShot(tallTimeline(&shot.window), "layers-timeline");
    }

    SECTION("add-menu")
    {
        QToolButton* add = nullptr;
        for (auto* b : tallTimeline(&shot.window)->findChildren<QToolButton*>())
        {
            if (b->toolTip() == "Add Layer") { add = b; }
        }
        REQUIRE(add);
        saveMenu(add->menu(), "layers-add-menu");
    }

    SECTION("rename")
    {
        layers->setCurrentLayer(layerNamed(shot.editor, "Voice"));
        actionNamed(&shot.window, "actionRename_Layer")->trigger();
        saveShot(tallTimeline(&shot.window), "layers-rename");
    }

    SECTION("group")
    {
        saveShot(tallTimeline(&shot.window), "layers-group");
    }

    SECTION("group-collapsed")
    {
        const int group = layerNamed(shot.editor, "Line")->groupId();
        layers->setLayerGroupCollapsed(group, true);
        layers->setLayerGroupOpacity(group, 0.5);
        saveShot(tallTimeline(&shot.window), "layers-group-collapsed");
    }

    SECTION("layer-menu")
    {
        shootLayerMenu(shot.editor, layerNamed(shot.editor, "Line"), "layers-layer-menu", "layers-blend-modes");
    }

    SECTION("lock-transparency")
    {
        auto* background = static_cast<LayerBitmap*>(layerNamed(shot.editor, "Background"));
        layers->setAttribute(background, AttributeId::LockAlpha, true);
        saveShot(tallTimeline(&shot.window), "layers-lock-transparency");
    }

    SECTION("rough")
    {
        layers->setRough(layerNamed(shot.editor, "Rough"), true);
        saveShot(&shot.window, "layers-rough");
    }

    SECTION("solo")
    {
        layers->setCurrentLayer(layerNamed(shot.editor, "Rough"));
        layers->toggleSolo(shot.editor->currentLayerIndex());
        saveShot(&shot.window, "layers-solo");
    }

    SECTION("lock")
    {
        layers->setAttribute(layerNamed(shot.editor, "Rough"), AttributeId::LockAll, true);
        saveShot(tallTimeline(&shot.window), "layers-lock");
    }

    SECTION("locked-notice")
    {
        layers->setAttribute(layerNamed(shot.editor, "Rough"), AttributeId::LockAll, true);
        REQUIRE(shot.editor->refusesEdit(layers->currentLayer()));
        saveShot(&shot.window, "layers-locked-notice");
    }

    SECTION("storyboard")
    {
        layers->setStoryboard(layerNamed(shot.editor, "Background"), true);
        saveShot(floatPanel(&shot.window, "Storyboard", QSize(640, 220)), "layers-storyboard");
    }
}
