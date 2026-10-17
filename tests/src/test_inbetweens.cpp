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
#include <QIcon>
#include <QMenu>

#include "cameraeasingtype.h"
#include "cellmenus.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "preferencemanager.h"
#include "testfixture.h"
#include "undoredomanager.h"
#include "vectorimage.h"

namespace
{
void addLine(LayerVector* layer, int pos, const QList<QPointF>& points)
{
    if (!layer->keyExists(pos)) { layer->addNewKeyFrameAt(pos); }
    BezierCurve curve(points, false);
    layer->getVectorImageAtFrame(pos)->addCurve(curve, 1.0, false);
}
}

TEST_CASE("Create inbetweens fills the empty frames between two vector Drawings", "[Inbetweens]")
{
    TestSession session;
    LayerVector* layer = session.object->addNewVectorLayer();
    // Two vertices at frame 1, three at frame 5: the first stroke is split to match.
    addLine(layer, 1, { QPointF(0, 0), QPointF(10, 0) });
    addLine(layer, 5, { QPointF(0, 10), QPointF(5, 10), QPointF(10, 10) });
    layer->addNewKeyFrameAt(4); // a Drawing already there stays
    layer->getKeyFrameAt(1)->setLinkId(7); // inbetweens are never linked
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    layer->setFrameSelected(1, true);
    layer->setFrameSelected(5, true);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    REQUIRE(editor->layers()->createInbetweens(CameraEasingType::LINEAR).ok());

    REQUIRE(layer->keyFrameCount() == 5);
    for (int pos : { 2, 3 })
    {
        REQUIRE(layer->keyExists(pos));
        CHECK(layer->getKeyFrameAt(pos)->mark() == Mark::Inbetween);
        CHECK(layer->getKeyFrameAt(pos)->linkId() == 0);
    }
    CHECK(layer->getKeyFrameAt(1)->mark() == Mark::None);
    CHECK(layer->getKeyFrameAt(4)->mark() == Mark::None);
    CHECK(layer->getVectorImageAtFrame(4)->isEmpty());

    // Frame 3 is halfway from 1 to 5. The split puts the new vertex at the middle of the first cubic, (8.75, 0).
    BezierCurve& mid = layer->getVectorImageAtFrame(3)->curve(0);
    REQUIRE(mid.getVertexSize() == 2);
    CHECK(mid.getVertex(-1) == QPointF(0, 5));
    CHECK(mid.getVertex(0) == QPointF(6.875, 5));
    CHECK(mid.getVertex(1) == QPointF(10, 5));
    CHECK(layer->getVectorImageAtFrame(2)->curve(0).getVertex(-1) == QPointF(0, 2.5));

    undo->trigger();
    CHECK(layer->keyFrameCount() == 3);
    CHECK_FALSE(layer->keyExists(2));
    CHECK_FALSE(layer->keyExists(3));
    CHECK_FALSE(undo->isEnabled());

    redo->trigger();
    REQUIRE(layer->keyFrameCount() == 5);
    CHECK(layer->getKeyFrameAt(3)->mark() == Mark::Inbetween);
    CHECK(layer->getVectorImageAtFrame(3)->curve(0).getVertex(0) == QPointF(6.875, 5));
}

TEST_CASE("Create inbetweens follows the easing", "[Inbetweens]")
{
    TestSession session;
    LayerVector* layer = session.object->addNewVectorLayer();
    addLine(layer, 1, { QPointF(0, 0), QPointF(10, 0) });
    addLine(layer, 5, { QPointF(0, 16), QPointF(10, 16) });
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    layer->setFrameSelected(1, true);
    layer->setFrameSelected(5, true);

    REQUIRE(editor->layers()->createInbetweens(CameraEasingType::INQUAD).ok());

    // Halfway in time is a quarter of the way in space.
    CHECK(layer->getVectorImageAtFrame(3)->curve(0).getVertex(-1) == QPointF(0, 4));
}

TEST_CASE("Create inbetweens refuses different stroke counts and bitmap layers", "[Inbetweens]")
{
    TestSession session;
    LayerVector* vector = session.object->addNewVectorLayer();
    addLine(vector, 1, { QPointF(0, 0), QPointF(10, 0) });
    addLine(vector, 5, { QPointF(0, 10), QPointF(10, 10) });
    addLine(vector, 5, { QPointF(0, 20), QPointF(10, 20) });
    LayerBitmap* bitmap = session.object->addNewBitmapLayer();
    bitmap->addNewKeyFrameAt(1);
    bitmap->addNewKeyFrameAt(5);
    session.start();
    Editor* editor = session.editor.get();
    QObject parent;
    int messages = 0;
    QObject::connect(editor, &Editor::statusMessage, &parent, [&messages](const QString&) { ++messages; });
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());

    editor->layers()->setCurrentLayer(vector);
    vector->setFrameSelected(1, true);
    vector->setFrameSelected(5, true);
    CHECK_FALSE(editor->layers()->createInbetweens(CameraEasingType::LINEAR).ok());
    CHECK(messages == 1);
    CHECK(vector->keyFrameCount() == 2);

    editor->layers()->setCurrentLayer(bitmap);
    bitmap->setFrameSelected(1, true);
    bitmap->setFrameSelected(5, true);
    CHECK_FALSE(editor->layers()->createInbetweens(CameraEasingType::LINEAR).ok());
    CHECK(messages == 2);
    CHECK(bitmap->keyFrameCount() == 2);
    CHECK_FALSE(undo->isEnabled());
}

TEST_CASE("Create inbetweens stores the chosen easing and preselects it next time", "[Inbetweens]")
{
    TestSession session;
    session.start();
    Editor* editor = session.editor.get();
    editor->preference()->set(SETTING::INBETWEEN_EASING, static_cast<int>(CameraEasingType::LINEAR));

    QMenu first;
    CameraEasingType picked = CameraEasingType::LINEAR;
    CellMenus::fillInbetweenEasingMenu(&first, editor, [&picked](CameraEasingType type) { picked = type; });
    REQUIRE(first.activeAction() == first.actions().at(0));
    first.actions().at(static_cast<int>(CameraEasingType::OUTQUAD))->trigger();
    CHECK(picked == CameraEasingType::OUTQUAD);
    CHECK(editor->preference()->getInt(SETTING::INBETWEEN_EASING) == static_cast<int>(CameraEasingType::OUTQUAD));

    QMenu second;
    CellMenus::fillInbetweenEasingMenu(&second, editor, [](CameraEasingType) {});
    QAction* active = second.activeAction();
    REQUIRE(active != nullptr);
    CHECK(second.actions().indexOf(active) == static_cast<int>(CameraEasingType::OUTQUAD));
    CHECK(active->isChecked());
}
