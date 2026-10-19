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
#include <QMainWindow>
#include <QMenu>
#include <QTimer>
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
int indexNamed(Editor* editor, const QString& name)
{
    for (int i = 0; i < editor->object()->getLayerCount(); ++i)
    {
        if (editor->object()->getLayer(i)->name() == name) { return i; }
    }
    FAIL("No layer named " << name.toStdString());
    return -1;
}

Layer* layerNamed(Editor* editor, const QString& name) { return editor->object()->getLayer(indexNamed(editor, name)); }

QAction* actionNamed(QWidget* window, const QString& text)
{
    for (QAction* a : window->findChildren<QAction*>())
    {
        if (a->text() == text) { return a; }
    }
    return nullptr;
}

QDockWidget* panel(QWidget* window, const char* objectName)
{
    auto* dock = window->findChild<QDockWidget*>(objectName);
    REQUIRE(dock);
    return dock;
}

// Makes the Timeline tall enough to show every layer row.
QDockWidget* tallTimeline(QWidget* window)
{
    QDockWidget* dock = panel(window, "TimeLine");
    dock->raise();
    static_cast<QMainWindow*>(window)->resizeDocks({ dock }, { 300 }, Qt::Vertical);
    QApplication::processEvents();
    return dock;
}

// Opens the layer cell menu for `layer`, saves it (and the Blend Mode submenu when `blendName` is set), then closes it.
void shootLayerMenu(Editor* editor, Layer* layer, const QString& menuName, const QString& blendName)
{
    QTimer::singleShot(0, [=] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        REQUIRE(menu);
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
        menu->close();
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
        add->menu()->popup(add->mapToGlobal(QPoint(0, add->height())));
        saveShot(add->menu(), "layers-add-menu");
        add->menu()->close();
    }

    SECTION("rename")
    {
        layers->setCurrentLayer(layerNamed(shot.editor, "Voice"));
        QAction* rename = actionNamed(&shot.window, "Rename Layer");
        REQUIRE(rename);
        rename->trigger();
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
        layers->toggleSolo(indexNamed(shot.editor, "Rough"));
        saveShot(&shot.window, "layers-solo");
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
        QDockWidget* board = panel(&shot.window, "Storyboard");
        board->setFloating(true);
        board->resize(640, 220);
        board->show();
        saveShot(board, "layers-storyboard");
    }
}
