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

#include "layerbitmap.h"
#include "bitmapimage.h"
#include "buckettool.h"
#include "util.h"
#include "colormanager.h"
#include "editor.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "stroketool.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "viewmanager.h"

#include <memory>
#include <QDir>
#include <QApplication>
#include <QDomElement>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QTemporaryDir>

TEST_CASE("Load bitmap layer from XML")
{
    std::unique_ptr<Layer> bitmapLayer(new LayerBitmap(1));
    QTemporaryDir dataDir;
    REQUIRE(dataDir.isValid());
    QDomDocument doc;
    doc.setContent(QString("<layer id='1' name='Bitmap Layer' visibility='1'></layer>"));
    QDomElement layerElem = doc.documentElement();
    ProgressCallback nullCallback = []() {};

    auto createFrame = [&layerElem, &doc](QString src = "001.001.png", int frame = 1, int topLeftX = 0, int topLeftY = 0)
    {
        QDomElement frameElem = doc.createElement("image");
        frameElem.setAttribute("src", src);
        frameElem.setAttribute("frame", frame);
        frameElem.setAttribute("topLeftX", topLeftX);
        frameElem.setAttribute("topLeftY", topLeftY);
        layerElem.appendChild(frameElem);
    };

    SECTION("No frames")
    {
        bitmapLayer->loadDomElement(layerElem, dataDir.path(), []() {});

        REQUIRE(bitmapLayer->keyFrameCount() == 0);
    }

    SECTION("Single frame")
    {
        createFrame("001.001.png", 1, 0, 0);

        bitmapLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(bitmapLayer->keyFrameCount() == 1);
        BitmapImage* frame = static_cast<BitmapImage*>(bitmapLayer->getKeyFrameAt(1));
        REQUIRE(frame != nullptr);
        REQUIRE(frame->top() == 0);
        REQUIRE(frame->left() == 0);
        REQUIRE(closestCanonicalPath(frame->fileName()) == closestCanonicalPath(dataDir.filePath("001.001.png")));
    }

    SECTION("Multiple frames")
    {
        createFrame("001.001.png", 1);
        createFrame("001.002.png", 2);

        bitmapLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(bitmapLayer->keyFrameCount() == 2);
        for (int i = 1; i <= 2; i++)
        {
            BitmapImage* frame = static_cast<BitmapImage*>(bitmapLayer->getKeyFrameAt(i));
            REQUIRE(frame != nullptr);
            REQUIRE(frame->top() == 0);
            REQUIRE(frame->left() == 0);
            REQUIRE(closestCanonicalPath(frame->fileName()) == closestCanonicalPath(dataDir.filePath(QString("001.%1.png").arg(QString::number(i), 3, QChar('0')))));
        }
    }

    SECTION("Frame with absolute src")
    {
        createFrame(QDir(dataDir.filePath("001.001.png")).absolutePath());

        bitmapLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(bitmapLayer->keyFrameCount() == 0);
    }

    SECTION("Frame src outside of data dir")
    {
        QTemporaryDir otherDir;
        createFrame(QDir(dataDir.path()).relativeFilePath(QDir(otherDir.filePath("001.001.png")).absolutePath()));

        bitmapLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(bitmapLayer->keyFrameCount() == 0);
    }

    SECTION("Frame src nested in data dir")
    {
        createFrame("subdir/001.001.png");

        bitmapLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(bitmapLayer->keyFrameCount() == 1);
        BitmapImage* frame = static_cast<BitmapImage*>(bitmapLayer->getKeyFrameAt(1));
        REQUIRE(frame != nullptr);
        REQUIRE(frame->top() == 0);
        REQUIRE(frame->left() == 0);
        REQUIRE(closestCanonicalPath(frame->fileName()) == closestCanonicalPath(dataDir.filePath("subdir/001.001.png")));
    }
}

TEST_CASE("LayerBitmap - lock transparency round-trips through XML")
{
    LayerBitmap layer(1);
    QDomDocument doc;
    CHECK_FALSE(layer.createDomElement(doc).hasAttribute("lockAlpha"));

    layer.setLockAlpha(true);
    const QDomElement elem = layer.createDomElement(doc);
    CHECK(elem.attribute("lockAlpha") == "1");

    LayerBitmap loaded(2);
    loaded.loadDomElement(elem, QString(), []() {});
    CHECK(loaded.lockAlpha());
}

namespace
{
void sendTablet(ScribbleArea* area, QEvent::Type type, QPointF pos)
{
    const Qt::MouseButtons buttons = type == QEvent::TabletRelease ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   1.0, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    QApplication::sendEvent(area, &e);
}

void lockStroke(ScribbleArea* area, QPointF from, QPointF to)
{
    sendTablet(area, QEvent::TabletPress, from);
    for (int i = 1; i <= 10; ++i) sendTablet(area, QEvent::TabletMove, from + (to - from) * i / 10.0);
    sendTablet(area, QEvent::TabletRelease, to);
}
}

TEST_CASE("LayerBitmap - painting with lock transparency never changes alpha")
{
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(1);
    editor->color()->setFrontColor(Qt::blue);

    // Opaque red left of canvas x = 0, transparent right of it.
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    image->drawRect(QRectF(-100, -50, 100, 100), Qt::NoPen, QBrush(Qt::red), QPainter::CompositionMode_Source, false);
    layer->setLockAlpha(true);
    auto drag = [&](QPointF from, QPointF to) {
        lockStroke(area, editor->view()->mapCanvasToScreen(from), editor->view()->mapCanvasToScreen(to));
    };

    for (ToolType tool : { PENCIL, PEN, BRUSH, ERASER })
    {
        INFO("tool " << tool);
        editor->tools()->setCurrentTool(tool);
        const ToolSettingsGuard guard(editor->tools()->currentTool());
        static_cast<StrokeTool*>(editor->tools()->currentTool())->setWidth(10);
        drag(QPointF(-30, 0), QPointF(30, 0));
        CHECK(qAlpha(image->pixel(QPoint(-15, 0))) == 255);
        CHECK(qAlpha(image->pixel(QPoint(15, 0))) == 0);
    }
    CHECK(qBlue(image->pixel(QPoint(-15, 0))) > 0);

    editor->tools()->setCurrentTool(BUCKET);
    {
        const ToolSettingsGuard guard(editor->tools()->currentTool());
        auto* bucket = static_cast<BucketTool*>(editor->tools()->currentTool());
        for (int mode : { 0, 1, 2 })
        {
            INFO("bucket fill mode " << mode);
            bucket->setFillMode(mode);
            const QPointF at = editor->view()->mapCanvasToScreen(QPointF(15, 0));
            sendTablet(area, QEvent::TabletPress, at);
            sendTablet(area, QEvent::TabletRelease, at);
            CHECK(qAlpha(image->pixel(QPoint(15, 0))) == 0);
            CHECK(qAlpha(image->pixel(QPoint(-15, 0))) == 255);
        }
    }

    editor->tools()->setCurrentTool(LASSOFILL);
    const QPolygonF loop({ QPointF(-30, -20), QPointF(30, -20), QPointF(30, 20), QPointF(-30, 20) });
    sendTablet(area, QEvent::TabletPress, editor->view()->mapCanvasToScreen(loop[0]));
    for (QPointF p : loop) sendTablet(area, QEvent::TabletMove, editor->view()->mapCanvasToScreen(p));
    sendTablet(area, QEvent::TabletRelease, editor->view()->mapCanvasToScreen(loop[0]));
    CHECK(qAlpha(image->pixel(QPoint(15, 0))) == 0);
    CHECK(qAlpha(image->pixel(QPoint(-15, 0))) == 255);
}
