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

#include <memory>
#include <QAction>
#include <QDomDocument>
#include <QIcon>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "editor.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"

TEST_CASE("Setting and clearing a Mark are one undo step each", "[Mark]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(5);
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

    // Frame 3 is a hold of the Drawing at 1.
    editor->layers()->setKeyMark(3, Mark::Key);
    REQUIRE(layer->getKeyFrameAt(1)->mark() == Mark::Key);
    editor->layers()->setKeyMark(5, Mark::Breakdown);
    REQUIRE(layer->getKeyFrameAt(5)->mark() == Mark::Breakdown);
    editor->layers()->setKeyMark(1, Mark::None);
    REQUIRE(layer->getKeyFrameAt(1)->mark() == Mark::None);

    undo->trigger();
    REQUIRE(layer->getKeyFrameAt(1)->mark() == Mark::Key);
    undo->trigger();
    REQUIRE(layer->getKeyFrameAt(5)->mark() == Mark::None);
    REQUIRE(layer->getKeyFrameAt(1)->mark() == Mark::Key);
    undo->trigger();
    REQUIRE(layer->getKeyFrameAt(1)->mark() == Mark::None);
    REQUIRE_FALSE(undo->isEnabled());

    redo->trigger();
    redo->trigger();
    REQUIRE(layer->getKeyFrameAt(1)->mark() == Mark::Key);
    REQUIRE(layer->getKeyFrameAt(5)->mark() == Mark::Breakdown);
}

TEST_CASE("Setting the Mark a Drawing already has records no undo step", "[Mark]")
{
    TestSession session;
    LayerVector* layer = session.object->addNewVectorLayer();
    layer->addNewKeyFrameAt(1);
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());

    editor->layers()->setKeyMark(1, Mark::None);
    REQUIRE_FALSE(undo->isEnabled());
    editor->layers()->setKeyMark(1, Mark::Inbetween);
    REQUIRE(layer->getKeyFrameAt(1)->mark() == Mark::Inbetween);
    REQUIRE(undo->isEnabled());
}

TEST_CASE("Marks round-trip through save and load", "[Mark]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("mark.pclx");
    FileManager fm;
    std::unique_ptr<Object> object(new Object);
    object->init();
    LayerVector* vector = object->addNewVectorLayer();
    vector->addNewKeyFrameAt(1);
    vector->addNewKeyFrameAt(2);
    vector->addNewKeyFrameAt(3);
    vector->getKeyFrameAt(1)->setMark(Mark::Key);
    vector->getKeyFrameAt(2)->setMark(Mark::Inbetween);
    LayerBitmap* bitmap = object->addNewBitmapLayer();
    bitmap->addNewKeyFrameAt(1);
    bitmap->getBitmapImageAtFrame(1)->drawRect(QRectF(0, 0, 4, 4), Qt::NoPen, QBrush(Qt::black),
                                               QPainter::CompositionMode_Source, false);
    bitmap->getKeyFrameAt(1)->setMark(Mark::Breakdown);

    REQUIRE(fm.save(object.get(), path).ok());
    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);

    const Layer* v = loaded->findLayerById(vector->id());
    REQUIRE(v->getKeyFrameAt(1)->mark() == Mark::Key);
    REQUIRE(v->getKeyFrameAt(2)->mark() == Mark::Inbetween);
    REQUIRE(v->getKeyFrameAt(3)->mark() == Mark::None);
    REQUIRE(loaded->findLayerById(bitmap->id())->getKeyFrameAt(1)->mark() == Mark::Breakdown);
}

TEST_CASE("A file without marks loads and a bad mark loads as none", "[Mark]")
{
    QDomDocument doc;
    LayerBitmap saved(1);
    saved.addNewKeyFrameAt(1);
    REQUIRE_FALSE(saved.createDomElement(doc).firstChildElement("image").hasAttribute("mark"));

    QDomElement layerElem = doc.createElement("layer");
    QDomElement image = doc.createElement("image");
    image.setAttribute("frame", 1);
    layerElem.appendChild(image);
    QDomElement bad = doc.createElement("image");
    bad.setAttribute("frame", 2);
    bad.setAttribute("mark", "extreme");
    layerElem.appendChild(bad);
    LayerVector loaded(2);
    loaded.loadDomElement(layerElem, QString(), [] {});
    REQUIRE(loaded.getKeyFrameAt(1)->mark() == Mark::None);
    REQUIRE(loaded.getKeyFrameAt(2)->mark() == Mark::None);
}

TEST_CASE("A Mark filter passes only Drawings with a Mark in the set", "[Mark]")
{
    KeyFrame none, key, breakdown, inbetween;
    key.setMark(Mark::Key);
    breakdown.setMark(Mark::Breakdown);
    inbetween.setMark(Mark::Inbetween);

    MarkFilter filter;
    REQUIRE(filter.isEmpty());
    for (const KeyFrame* k : { &none, &key, &breakdown, &inbetween }) { CHECK(filter.passes(k)); }

    filter.set(Mark::Key, true);
    filter.set(Mark::Breakdown, true);
    REQUIRE(filter.contains(Mark::Key));
    CHECK_FALSE(filter.passes(&none));
    CHECK(filter.passes(&key));
    CHECK(filter.passes(&breakdown));
    CHECK_FALSE(filter.passes(&inbetween));

    filter.set(Mark::Breakdown, false);
    CHECK_FALSE(filter.passes(&breakdown));
    CHECK(filter.passes(&key));
}
