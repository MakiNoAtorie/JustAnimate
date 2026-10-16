/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "catch.hpp"

#include "editor.h"
#include "object.h"
#include "layermanager.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "layervector.h"
#include "scribblearea.h"
#include "erasertool.h"

TEST_CASE("EraserTool - Erasing on a vector layer before its first keyframe does nothing")
{
    TestSession session;
    Object* obj = session.object;
    LayerVector* layer = obj->addNewVectorLayer();
    layer->addNewKeyFrameAt(5);
    layer->removeKeyFrame(1);

    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    REQUIRE(layer->getLastKeyFrameAtPosition(1) == nullptr);

    EraserTool* eraser = static_cast<EraserTool*>(editor->tools()->getTool(ERASER));
    eraser->drawStroke();
    eraser->strokeMoved();
    eraser->removeVectorPaint();

    REQUIRE(layer->keyFrameCount() == 1);
    REQUIRE_FALSE(layer->keyExists(1));
}

TEST_CASE("ToolManager - Tablet eraser end emits toolChanged once per flip")
{
    TestSession session;
    session.start();
    ToolManager* tools = session.editor->tools();
    int changes = 0;
    QObject::connect(tools, &ToolManager::toolChanged, [&changes](ToolType) { ++changes; });

    tools->tabletSwitchToEraser();
    tools->tabletSwitchToEraser();
    REQUIRE(changes == 1);

    tools->tabletRestorePrevTool();
    tools->tabletRestorePrevTool();
    REQUIRE(changes == 2);
}

TEST_CASE("ToolManager - A temporary tool beats the tablet eraser end")
{
    TestSession session;
    session.start();
    ToolManager* tools = session.editor->tools();
    int changes = 0;
    QObject::connect(tools, &ToolManager::toolChanged, [&changes](ToolType) { ++changes; });

    tools->setTemporaryTool(HAND, Qt::MiddleButton);
    const int afterTemporary = changes;
    tools->tabletSwitchToEraser();
    REQUIRE(changes == afterTemporary);
}
