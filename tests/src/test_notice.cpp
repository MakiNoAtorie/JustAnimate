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
#include <QTimer>
#include <QTabletEvent>

#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "mainwindow2.h"
#include "preferencemanager.h"
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

TEST_CASE("The canvas note takes its colours from the palette", "[Notice]")
{
    NoticeSession n;
    QPalette palette = n.s.area->palette();
    palette.setColor(QPalette::ToolTipBase, QColor(0, 255, 0));
    n.s.area->setPalette(palette);
    n.s.area->show();
    n.s.editor->notice("Note");
    const QImage shot = n.s.area->grab().toImage();
    const QFontMetricsF fm(n.s.area->font());
    const qreal w = fm.horizontalAdvance("Note") + 16;
    const QColor box = shot.pixelColor(qRound((n.s.area->width() - w) / 2 + 3), qRound(12 + (fm.height() + 16) / 2));
    CHECK(box.green() > box.red() + 100);
    CHECK(box.green() > box.blue() + 100);
}

// Needs MainWindow2: the notice is its needSave slot and names its Save action's key.
TEST_CASE("Autosave count on an untitled project shows a notice and no dialog", "[Notice]")
{
    const SettingsGuard guard({ SETTING_AUTO_SAVE, SETTING_AUTO_SAVE_NUMBER, SETTING_AUTO_SAVE_REMINDER });
    MainWindow2 window;
    Editor* editor = window.findChild<Editor*>();
    REQUIRE(editor);
    REQUIRE(editor->object()->filePath().isEmpty());
    editor->preference()->set<SETTING::AUTO_SAVE>(true);
    editor->preference()->set<SETTING::AUTO_SAVE_NUMBER>(1);
    editor->resetAutoSaveCounter();

    QStringList notices;
    QObject::connect(editor, &Editor::statusMessage, &window, [&](const QString& t) { notices << t; });
    bool dialogOpened = false;
    QTimer::singleShot(0, [&]
    {
        if (QWidget* modal = QApplication::activeModalWidget())
        {
            dialogOpened = true;
            modal->close();
        }
    });

    auto* layer = static_cast<LayerBitmap*>(editor->layers()->currentLayer());
    REQUIRE(layer->type() == Layer::BITMAP);
    auto modify = [&]
    {
        REQUIRE(layer->addNewKeyFrameAt(7));
        editor->scrubTo(7);
        editor->removeKey();
        QCoreApplication::processEvents();
    };
    QAction* save = window.findChild<QAction*>("actionSave");
    REQUIRE(save);

    SECTION("Reminder on names the Save key")
    {
        save->setShortcut(QKeySequence("Ctrl+Shift+F9"));
        modify();
        CHECK_FALSE(dialogOpened);
        CHECK(notices.contains(QString("Unsaved project: press %1").arg(QKeySequence("Ctrl+Shift+F9").toString(QKeySequence::NativeText))));
    }
    SECTION("Reminder on with Save unbound")
    {
        save->setShortcut(QKeySequence());
        modify();
        CHECK(notices.contains("Unsaved project: save it to keep autosaving"));
    }
    SECTION("Reminder off shows nothing and keeps autosave by count")
    {
        editor->preference()->set<SETTING::AUTO_SAVE_REMINDER>(false);
        modify();
        CHECK(notices.isEmpty());
        CHECK(editor->preference()->get<SETTING::AUTO_SAVE>());
    }
}
