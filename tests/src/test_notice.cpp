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

#include <QApplication>
#include <QPointingDevice>
#include <QTabletEvent>

#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "viewmanager.h"

namespace
{
void sendTablet(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = type == QEvent::TabletRelease ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

struct NoticeSession
{
    TestSession s;
    LayerBitmap* layer = nullptr;
    QStringList notices;
    QList<int> refusedLayers;

    NoticeSession()
    {
        layer = s.object->addNewBitmapLayer();
        layer->setName("Ink");
        REQUIRE(s.start());
        s.area->init();
        s.area->resize(800, 600);
        s.editor->layers()->setCurrentLayer(layer);
        QObject::connect(s.editor.get(), &Editor::statusMessage, s.area.get(), [this](const QString& text) { notices << text; });
        QObject::connect(s.editor.get(), &Editor::layerRefused, s.area.get(), [this](int id) { refusedLayers << id; });
    }
};
} // namespace

TEST_CASE("A stroke on a hidden layer shows one notice and no dialog", "[Notice]")
{
    NoticeSession n;
    n.layer->setVisible(false);
    n.s.editor->tools()->setCurrentTool(PENCIL);
    const QPointF from = n.s.editor->view()->mapCanvasToScreen(QPointF(-20, 0));
    const QPointF to = n.s.editor->view()->mapCanvasToScreen(QPointF(20, 0));
    sendTablet(n.s.area.get(), QEvent::TabletPress, from);
    sendTablet(n.s.area.get(), QEvent::TabletMove, to);
    sendTablet(n.s.area.get(), QEvent::TabletRelease, to);
    QCoreApplication::processEvents();

    REQUIRE(n.notices.size() == 1);
    CHECK(n.notices.first().contains("Ink"));
    CHECK(n.refusedLayers == QList<int>({ n.layer->id() }));
    CHECK(QApplication::activeModalWidget() == nullptr);
    CHECK(n.s.area->note() == n.notices.first()); // the canvas shows it without the status bar
}

TEST_CASE("Remove drawing on a frame with no Drawing keeps the playhead", "[Notice]")
{
    NoticeSession n;
    n.layer->addNewKeyFrameAt(3);
    n.s.editor->scrubTo(5);
    n.s.editor->removeKey();

    CHECK(n.s.editor->currentFrame() == 5);
    CHECK(n.notices.size() == 1);
    CHECK(n.layer->keyFrameCount() == 2);
}

TEST_CASE("Ripple edits and Mark refuse through the notice", "[Notice]")
{
    NoticeSession n;
    SECTION("Ripple insert on a hidden layer")
    {
        n.layer->setVisible(false);
        CHECK_FALSE(n.s.editor->layers()->rippleInsertFrame());
        CHECK(n.refusedLayers == QList<int>({ n.layer->id() }));
    }
    SECTION("Mark on a frame with no Drawing")
    {
        n.layer->addNewKeyFrameAt(5);
        REQUIRE(n.layer->removeKeyFrame(1));
        n.s.editor->layers()->setKeyMark(2, Mark::Key);
        CHECK(n.notices.size() == 1);
    }
    SECTION("Lock-all flashes the lock")
    {
        n.layer->setLockAll(true);
        CHECK(n.s.editor->addNewKey() == nullptr);
        CHECK(n.refusedLayers == QList<int>({ n.layer->id() }));
        CHECK(n.s.area->note() == n.notices.first());
    }
}
