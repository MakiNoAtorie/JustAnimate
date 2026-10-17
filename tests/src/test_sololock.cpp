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
#include <QApplication>
#include <QDomDocument>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTabletEvent>

#include "bitmapimage.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layersound.h"
#include "layervector.h"
#include "object.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "timeline.h"
#include "timelinecells.h"
#include "toolmanager.h"
#include "viewmanager.h"

namespace
{
QList<int> keyPositions(const Layer* layer)
{
    QList<int> positions;
    layer->foreachKeyFrame([&](const KeyFrame* key) { positions.prepend(key->pos()); });
    return positions;
}

void sendTablet(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = type == QEvent::TabletRelease ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}
} // namespace

TEST_CASE("Lock-all is saved in the project", "[SoloLock]")
{
    QDomDocument doc;
    LayerVector saved(1);
    REQUIRE_FALSE(saved.lockAll());
    REQUIRE_FALSE(saved.createBaseDomElement(doc).hasAttribute("lockAll"));

    saved.setLockAll(true);
    const QDomElement elem = saved.createBaseDomElement(doc);
    CHECK(elem.attribute("lockAll") == "1");
    LayerVector loaded(2);
    loaded.loadBaseDomElement(elem);
    CHECK(loaded.lockAll());

    loaded.loadBaseDomElement(LayerVector(3).createBaseDomElement(doc));
    CHECK_FALSE(loaded.lockAll());
}

TEST_CASE("Duplicate Layer keeps lock-all", "[SoloLock]")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->setLockAll(true);
    REQUIRE(s.start());
    CHECK(s.editor->layers()->duplicateLayer(layer, "Copy")->lockAll());
}

TEST_CASE("Alt+click on an eye solos the layer and a second Alt+click restores", "[SoloLock]")
{
    TestSession s;
    LayerBitmap* a = s.object->addNewBitmapLayer();
    LayerVector* b = s.object->addNewVectorLayer();
    LayerBitmap* c = s.object->addNewBitmapLayer();
    LayerSound* sound = s.object->addNewSoundLayer();
    b->setVisible(false);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    TimeLine timeline(nullptr);
    timeline.setEditor(editor);
    timeline.initUI();
    timeline.resize(800, 300);
    timeline.grab();
    // The layer names sit left of the tracks.
    const QList<TimeLineCells*> cells = timeline.findChildren<TimeLineCells*>();
    TimeLineCells* names = *std::min_element(cells.cbegin(), cells.cend(), [&](TimeLineCells* x, TimeLineCells* y) {
        return x->mapTo(&timeline, QPoint()).x() < y->mapTo(&timeline, QPoint()).x(); });
    auto altClickEye = [&](int layerIndex) {
        const int row = s.object->getLayerCount() - 1 - layerIndex;
        const QPointF pos(5, TimeLineCells::getOffsetY() + row * names->getLayerHeight() + names->getLayerHeight() / 2);
        QMouseEvent press(QEvent::MouseButtonPress, pos, names->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton, Qt::AltModifier);
        QApplication::sendEvent(names, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, names->mapToGlobal(pos), Qt::LeftButton, Qt::NoButton, Qt::AltModifier);
        QApplication::sendEvent(names, &release);
    };
    auto visibility = [&] { return QList<bool>{ a->visible(), b->visible(), c->visible(), sound->visible() }; };

    altClickEye(0);
    CHECK(visibility() == QList<bool>({ true, false, false, true })); // sound is not drawn, so it keeps playing

    altClickEye(1); // solo moves to b; the saved visibility stays the one from before the first solo
    CHECK(visibility() == QList<bool>({ false, true, false, true }));

    altClickEye(1);
    CHECK(visibility() == QList<bool>({ true, false, true, true }));
}

TEST_CASE("A lock-all layer refuses edits with a status message", "[SoloLock]")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(3);
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    image->drawRect(QRectF(-50, -50, 100, 100), Qt::NoPen, QBrush(Qt::red), QPainter::CompositionMode_Source, false);
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    int messages = 0;
    QObject::connect(editor, &Editor::statusMessage, [&](const QString& text) { CHECK_FALSE(text.isEmpty()); ++messages; });
    layer->setLockAll(true);

    auto refused = [&](const char* what, const std::function<void()>& edit) {
        INFO(what);
        const int before = messages;
        edit();
        CHECK(messages > before);
        CHECK(keyPositions(layer) == QList<int>({ 1, 3 }));
        CHECK(image->pixel(QPoint(0, 0)) == qRgb(255, 0, 0));
    };
    refused("new key", [&] { CHECK(editor->addNewKey() == nullptr); });
    refused("remove key", [&] { editor->removeKey(); });
    refused("clear frame", [&] { editor->clearCurrentFrame(); });
    refused("exposure", [&] { CHECK_FALSE(editor->layers()->changeExposure(1, 1)); });
    refused("insert frame", [&] { CHECK_FALSE(editor->layers()->rippleInsertFrame()); });
    refused("delete frame", [&] { CHECK_FALSE(editor->layers()->rippleRemoveFrame()); });
    layer->setFrameSelected(3, true);
    refused("move frames", [&] { CHECK_FALSE(editor->layers()->moveSelectedFrames(2)); });
    refused("remove frames", [&] { editor->removeSelectedKeyFrames("Remove"); });
    refused("set exposure", [&] { editor->layers()->setExposureForSelectedFrames(2, false); });
    layer->deselectAll();
    refused("eraser stroke", [&] {
        editor->tools()->setCurrentTool(ERASER);
        const QPointF from = editor->view()->mapCanvasToScreen(QPointF(-20, 0));
        const QPointF to = editor->view()->mapCanvasToScreen(QPointF(20, 0));
        sendTablet(area, QEvent::TabletPress, from);
        sendTablet(area, QEvent::TabletMove, (from + to) / 2);
        sendTablet(area, QEvent::TabletMove, to);
        sendTablet(area, QEvent::TabletRelease, to);
    });

    layer->setLockAll(false);
    CHECK(editor->addNewKey() != nullptr); // the guard is the lock, not the layer
}
