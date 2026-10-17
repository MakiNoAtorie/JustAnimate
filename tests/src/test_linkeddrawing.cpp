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

#include <algorithm>
#include <memory>
#include <QAbstractScrollArea>
#include <QAction>
#include <QApplication>
#include <QDomDocument>
#include <QIcon>
#include <QMouseEvent>
#include <QTemporaryDir>

#include "bitmapimage.h"
#include "editor.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "testfixture.h"
#include "timeline.h"
#include "timelinecells.h"
#include "undoredomanager.h"
#include "xsheet.h"

namespace
{

// Paints a black square on the Drawing shown at `frame` the way a tool does: one keyframe undo record.
void paintSquare(Editor* editor, LayerBitmap* layer, int frame, QRect square)
{
    editor->scrubTo(frame);
    const SAVESTATE_ID state = editor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    layer->getLastBitmapImageAtFrame(frame)->drawRect(square, Qt::NoPen, QBrush(Qt::black),
                                                      QPainter::CompositionMode_SourceOver, false);
    editor->undoRedo()->record(state, "Paint");
}

QRgb pixelAt(LayerBitmap* layer, int frame, QPoint p)
{
    return layer->getBitmapImageAtFrame(frame)->pixel(p);
}

void altDrag(QWidget* target, QPoint from, QPoint to)
{
    const auto send = [target](QEvent::Type type, QPoint pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, QPointF(pos), QPointF(target->mapToGlobal(pos)), Qt::LeftButton, buttons, Qt::AltModifier);
        QApplication::sendEvent(target, &e);
    };
    send(QEvent::MouseButtonPress, from, Qt::LeftButton);
    send(QEvent::MouseMove, to, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, to, Qt::NoButton);
}

bool containsColour(const QImage& image, const QRect& area, QColor colour)
{
    for (int y = area.top(); y <= area.bottom(); ++y)
    {
        for (int x = area.left(); x <= area.right(); ++x)
        {
            if (image.pixelColor(x, y).rgb() == colour.rgb()) { return true; }
        }
    }
    return false;
}

} // namespace

TEST_CASE("A stroke on a Linked Drawing appears on every copy and one undo restores all", "[LinkedDrawing]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());
    paintSquare(editor, layer, 1, QRect(0, 0, 4, 4));

    REQUIRE(editor->layers()->linkDrawing(1, 5));
    REQUIRE(layer->keyExists(5));
    CHECK(layer->isLinked(layer->getKeyFrameAt(1)));
    CHECK(layer->isLinked(layer->getKeyFrameAt(5)));
    CHECK(qAlpha(pixelAt(layer, 5, QPoint(1, 1))) == 255);

    // Paint on the copy, from a hold of it.
    paintSquare(editor, layer, 6, QRect(10, 10, 4, 4));
    CHECK(qAlpha(pixelAt(layer, 1, QPoint(11, 11))) == 255);
    CHECK(qAlpha(pixelAt(layer, 5, QPoint(11, 11))) == 255);

    undo->trigger();
    CHECK(qAlpha(pixelAt(layer, 1, QPoint(11, 11))) == 0);
    CHECK(qAlpha(pixelAt(layer, 5, QPoint(11, 11))) == 0);
    CHECK(qAlpha(pixelAt(layer, 1, QPoint(1, 1))) == 255);

    redo->trigger();
    CHECK(qAlpha(pixelAt(layer, 1, QPoint(11, 11))) == 255);
    CHECK(qAlpha(pixelAt(layer, 5, QPoint(11, 11))) == 255);

    // Undo of the link removes the copy and leaves the source unlinked.
    undo->trigger();
    undo->trigger();
    CHECK_FALSE(layer->keyExists(5));
    CHECK_FALSE(layer->isLinked(layer->getKeyFrameAt(1)));
}

TEST_CASE("Linking refuses an occupied frame and a layer without Drawings", "[LinkedDrawing]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(3);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    CHECK_FALSE(editor->layers()->linkDrawing(1, 3));
    CHECK_FALSE(editor->layers()->linkDrawing(1, 1));
    CHECK_FALSE(layer->isLinked(layer->getKeyFrameAt(1)));

    layer->setLockAll(true);
    CHECK_FALSE(editor->layers()->linkDrawing(1, 5));
    CHECK_FALSE(layer->keyExists(5));
}

TEST_CASE("Unlink makes one copy independent and undo links it again", "[LinkedDrawing]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    REQUIRE(editor->layers()->linkDrawing(1, 3));
    REQUIRE(editor->layers()->linkDrawing(3, 5));
    paintSquare(editor, layer, 1, QRect(0, 0, 4, 4));
    REQUIRE(qAlpha(pixelAt(layer, 5, QPoint(1, 1))) == 255);

    editor->scrubTo(1);
    REQUIRE(editor->layers()->unlinkDrawing(4)); // a hold of the copy at 3
    CHECK(editor->currentFrame() == 1);
    CHECK_FALSE(layer->isLinked(layer->getKeyFrameAt(3)));
    CHECK(layer->isLinked(layer->getKeyFrameAt(1)));
    CHECK(qAlpha(pixelAt(layer, 3, QPoint(1, 1))) == 255);

    paintSquare(editor, layer, 3, QRect(10, 10, 4, 4));
    CHECK(qAlpha(pixelAt(layer, 3, QPoint(11, 11))) == 255);
    CHECK(qAlpha(pixelAt(layer, 1, QPoint(11, 11))) == 0);
    CHECK(qAlpha(pixelAt(layer, 5, QPoint(11, 11))) == 0);
    paintSquare(editor, layer, 5, QRect(20, 20, 4, 4));
    CHECK(qAlpha(pixelAt(layer, 1, QPoint(21, 21))) == 255);
    CHECK(qAlpha(pixelAt(layer, 3, QPoint(21, 21))) == 0);

    undo->trigger();
    undo->trigger();
    undo->trigger();
    CHECK(layer->isLinked(layer->getKeyFrameAt(3)));
}

TEST_CASE("Linked Drawings save as full copies with linkedTo and load linked again", "[LinkedDrawing]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("linked.pclx");
    int bitmapId = 0;
    int vectorId = 0;
    {
        TestSession session;
        LayerBitmap* bitmap = session.object->addNewBitmapLayer();
        bitmap->addNewKeyFrameAt(1);
        bitmap->addNewKeyFrameAt(2);
        LayerVector* vector = session.object->addNewVectorLayer();
        vector->addNewKeyFrameAt(1);
        bitmapId = bitmap->id();
        vectorId = vector->id();
        REQUIRE(session.start());
        Editor* editor = session.editor.get();
        editor->layers()->setCurrentLayer(bitmap);
        paintSquare(editor, bitmap, 1, QRect(0, 0, 4, 4));
        REQUIRE(editor->layers()->linkDrawing(1, 4));
        REQUIRE(editor->layers()->linkDrawing(1, 6));
        // An older program edits one copy only: the file holds copies that disagree.
        bitmap->getBitmapImageAtFrame(6)->drawRect(QRect(20, 20, 4, 4), Qt::NoPen, QBrush(Qt::black),
                                                   QPainter::CompositionMode_SourceOver, false);
        editor->layers()->setCurrentLayer(vector);
        REQUIRE(editor->layers()->linkDrawing(1, 3));
        FileManager fm;
        REQUIRE(fm.save(session.object, path).ok());
    }

    FileManager fm;
    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);
    Layer* bitmap = loaded->findLayerById(bitmapId);
    CHECK(bitmap->linkedKeys(bitmap->getKeyFrameAt(1)) == QList<KeyFrame*>({ bitmap->getKeyFrameAt(6), bitmap->getKeyFrameAt(4) }));
    CHECK_FALSE(bitmap->isLinked(bitmap->getKeyFrameAt(2)));
    // Each copy is a full image, so a program that ignores linkedTo still shows it.
    auto* copy = static_cast<BitmapImage*>(bitmap->getKeyFrameAt(6));
    copy->loadFile();
    CHECK(qAlpha(copy->pixel(QPoint(1, 1))) == 255);
    // On load the copies join the source's image.
    CHECK(qAlpha(copy->pixel(QPoint(21, 21))) == 0);
    Layer* vector = loaded->findLayerById(vectorId);
    CHECK(vector->isLinked(vector->getKeyFrameAt(3)));

    // The saved XML: the first copy has no linkedTo; the others name its src.
    QDomDocument doc;
    const QDomElement layerElem = bitmap->createDomElement(doc);
    QHash<int, QString> linkedTo;
    for (QDomElement image = layerElem.firstChildElement("image"); !image.isNull(); image = image.nextSiblingElement("image"))
    {
        linkedTo.insert(image.attribute("frame").toInt(), image.attribute("linkedTo"));
    }
    const QString src = QString::asprintf("%03d.%03d.png", bitmapId, 1);
    CHECK(linkedTo == QHash<int, QString>({ { 1, QString() }, { 2, QString() }, { 4, src }, { 6, src } }));
}

TEST_CASE("A keyframe record syncs the Linked Drawings of its own layer", "[LinkedDrawing]")
{
    TestSession session;
    LayerBitmap* linked = session.object->addNewBitmapLayer();
    LayerBitmap* other = session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(linked);
    REQUIRE(editor->layers()->linkDrawing(1, 3));
    editor->layers()->setCurrentLayer(other);
    editor->scrubTo(1);

    const SAVESTATE_ID state = editor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY, linked);
    linked->getBitmapImageAtFrame(1)->drawRect(QRect(0, 0, 4, 4), Qt::NoPen, QBrush(Qt::black),
                                               QPainter::CompositionMode_SourceOver, false);
    editor->undoRedo()->record(state, "Paint");
    CHECK(qAlpha(pixelAt(linked, 3, QPoint(1, 1))) == 255);
}

TEST_CASE("Images without linkedTo or with a bad linkedTo load as independent copies", "[LinkedDrawing]")
{
    QDomDocument doc;
    QDomElement layerElem = doc.createElement("layer");
    const auto addImage = [&](int frame, const QString& src, const QString& linkedTo) {
        QDomElement image = doc.createElement("image");
        image.setAttribute("frame", frame);
        image.setAttribute("src", src);
        if (!linkedTo.isNull()) { image.setAttribute("linkedTo", linkedTo); }
        layerElem.appendChild(image);
    };
    addImage(1, "001.001.png", QString());
    addImage(2, "001.002.png", QString());           // linkedTo removed
    addImage(3, "001.003.png", "001.003.png");       // itself
    addImage(4, "001.004.png", "missing.png");       // no such image
    addImage(5, "001.005.png", "001.001.png");
    QTemporaryDir dir; // bitmaps load lazily: the files need not exist
    REQUIRE(dir.isValid());
    LayerBitmap loaded(1);
    loaded.loadDomElement(layerElem, dir.path(), [] {});
    REQUIRE(loaded.keyFrameCount() == 5);
    for (int frame : { 2, 3, 4 }) { CHECK_FALSE(loaded.isLinked(loaded.getKeyFrameAt(frame))); }
    CHECK(loaded.linkedKeys(loaded.getKeyFrameAt(5)) == QList<KeyFrame*>({ loaded.getKeyFrameAt(1) }));
}

TEST_CASE("Alt-drag in the timeline and the X-sheet makes a Linked Drawing, marked in its cell", "[LinkedDrawing]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer(); // the top row and the first column
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(3);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(9);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());

    TimeLine timeline(nullptr);
    timeline.setEditor(editor);
    timeline.initUI();
    timeline.resize(800, 300);
    timeline.grab(); // lays out the timeline
    const QList<TimeLineCells*> cells = timeline.findChildren<TimeLineCells*>();
    TimeLineCells* tracks = *std::max_element(cells.cbegin(), cells.cend(), [&](TimeLineCells* a, TimeLineCells* b) {
        return a->mapTo(&timeline, QPoint()).x() < b->mapTo(&timeline, QPoint()).x(); });
    const int size = tracks->getFrameSize();
    const int y = TimeLineCells::getOffsetY() + tracks->getLayerHeight() / 2;
    const auto frameX = [&](int frame) { return TimeLineCells::getOffsetX() + (frame - 1) * size + size / 2; };

    altDrag(tracks, QPoint(frameX(1), y), QPoint(frameX(5), y));
    REQUIRE(layer->keyExists(5));
    CHECK(layer->linkedKeys(layer->getKeyFrameAt(5)) == QList<KeyFrame*>({ layer->getKeyFrameAt(1) }));

    tracks->clearCache();
    const QImage image = tracks->grab().toImage();
    const QColor highlight = tracks->palette().color(QPalette::Highlight);
    const auto cell = [&](int frame) {
        return QRect(TimeLineCells::getOffsetX() + (frame - 1) * size, TimeLineCells::getOffsetY(), size, tracks->getLayerHeight());
    };
    CHECK(containsColour(image, cell(5), highlight));
    CHECK(containsColour(image, cell(1), highlight));
    CHECK_FALSE(containsColour(image, cell(3), highlight));

    // Alt-drag onto a Drawing changes nothing.
    altDrag(tracks, QPoint(frameX(1), y), QPoint(frameX(3), y));
    CHECK_FALSE(layer->isLinked(layer->getKeyFrameAt(3)));

    XSheet sheet(nullptr);
    sheet.setEditor(editor);
    sheet.initUI();
    sheet.resize(400, 600);
    sheet.grab();
    QWidget* sheetCells = sheet.findChild<QAbstractScrollArea*>()->viewport();
    // A hold of the Drawing at 3 drags that Drawing.
    altDrag(sheetCells, sheet.cellRect(0, 4).center(), sheet.cellRect(0, 7).center());
    REQUIRE(layer->keyExists(7));
    CHECK(layer->linkedKeys(layer->getKeyFrameAt(7)) == QList<KeyFrame*>({ layer->getKeyFrameAt(3) }));
    const QList<Layer::Run> runs = layer->runs(9);
    QList<bool> linked;
    for (const Layer::Run& run : runs) { linked << run.linked; }
    CHECK(linked == QList<bool>({ true, true, true, true }));

    undo->trigger();
    undo->trigger();
    CHECK(layer->keyFrameCount() == 2);
}
