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
#include <cstdlib>
#include <memory>
#include <QAction>
#include <QDomDocument>
#include <QIcon>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextStream>

#include "filemanager.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "canvaspainter.h"
#include "layercamera.h"
#include "layermanager.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"

namespace
{

/// Layer names bottom first, with "*" after a group member.
QString arrangement(const Object* object)
{
    QStringList names;
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        const Layer* layer = object->getLayer(i);
        names << layer->name() + (layer->groupId() ? "*" : "");
    }
    return names.join(" ");
}

/// Camera, then bitmap layers A B C D bottom to top.
void addLayers(Object* object)
{
    object->addNewCameraLayer()->setName("Cam");
    for (const char* name : { "A", "B", "C", "D" }) { object->addNewBitmapLayer()->setName(name); }
}

} // namespace

TEST_CASE("Layer groups survive save and load", "[LayerGroup]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("groups.pclx");
    std::unique_ptr<Object> object(new Object);
    object->init();
    addLayers(object.get());
    object->addLayerGroup(LayerGroup{ 7, "Hair", true, false, 0.5 });
    object->getLayer(2)->setGroupId(7);
    object->getLayer(3)->setGroupId(7);

    FileManager fm;
    REQUIRE(fm.save(object.get(), path).ok());
    std::unique_ptr<Object> loaded(fm.load(path));
    REQUIRE(loaded != nullptr);

    REQUIRE(loaded->layerGroups().size() == 1);
    const LayerGroup group = loaded->layerGroups().first();
    CHECK(group.id == 7);
    CHECK(group.name == "Hair");
    CHECK(group.collapsed);
    CHECK_FALSE(group.visible);
    CHECK(group.opacity == Approx(0.5));
    CHECK(arrangement(loaded.get()) == "Cam A B* C* D");
}

TEST_CASE("A file without layer groups loads and saves unchanged", "[LayerGroup]")
{
    QTemporaryFile file;
    REQUIRE(file.open());
    QTextStream(&file) << "<!DOCTYPE PencilDocument><document><object>"
                          "<layer name='A' id='1' visibility='1' type='1'></layer>"
                          "<layer name='B' id='2' visibility='1' type='1' group='9'></layer>"
                          "</object></document>";
    file.close();

    FileManager fm;
    std::unique_ptr<Object> object(fm.load(file.fileName()));
    REQUIRE(object != nullptr);
    CHECK(object->layerGroups().isEmpty());
    CHECK(arrangement(object.get()) == "A B Camera Layer"); // a member of a missing group loads ungrouped

    QDomDocument doc;
    const QDomElement saved = object->saveXML(doc);
    CHECK(saved.elementsByTagName("layerGroup").isEmpty());
    const QDomNodeList layers = saved.elementsByTagName("layer");
    for (int i = 0; i < layers.size(); ++i) { CHECK_FALSE(layers.at(i).toElement().hasAttribute("group")); }
}

TEST_CASE("Group edits and reorders undo and redo", "[LayerGroup]")
{
    TestSession session;
    addLayers(session.object);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());
    const Object* object = editor->object();
    auto layer = [&](const char* name) { return object->findLayerByName(name); };

    const int group = layers->createLayerGroup(layer("B"));
    REQUIRE(group != 0);
    CHECK(arrangement(object) == "Cam A B* C D");
    REQUIRE(editor->undoRedo()->hasUnsavedChanges());

    layers->addLayerToGroup(layer("D"), group); // moves next to the group
    CHECK(arrangement(object) == "Cam A B* D* C");

    undo->trigger();
    CHECK(arrangement(object) == "Cam A B* C D");
    redo->trigger();
    CHECK(arrangement(object) == "Cam A B* D* C");

    layers->removeLayerFromGroup(layer("B")); // leaves the group on the outside
    CHECK(arrangement(object) == "Cam A B D* C");
    undo->trigger();
    CHECK(arrangement(object) == "Cam A B* D* C");

    undo->trigger();
    undo->trigger();
    CHECK(arrangement(object) == "Cam A B C D");
    CHECK(object->layerGroups().isEmpty());
    redo->trigger();
    CHECK(object->layerGroups().size() == 1);
    CHECK(arrangement(object) == "Cam A B* C D");
}

TEST_CASE("Reordering keeps group members together", "[LayerGroup]")
{
    TestSession session;
    addLayers(session.object);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    const Object* object = editor->object();
    auto layer = [&](const char* name) { return object->findLayerByName(name); };

    const int group = layers->createLayerGroup(layer("B"));
    layers->addLayerToGroup(layer("C"), group);
    REQUIRE(arrangement(object) == "Cam A B* C* D");

    SECTION("A member moves only inside its group")
    {
        REQUIRE(layers->moveLayer(2, 4));
        CHECK(arrangement(object) == "Cam A C* B* D");
        undo->trigger();
        CHECK(arrangement(object) == "Cam A B* C* D");
    }
    SECTION("A layer moves over the group, not into it")
    {
        REQUIRE(layers->moveLayer(1, 2)); // A above B
        CHECK(arrangement(object) == "Cam B* C* A D");
        REQUIRE(layers->moveLayer(4, 2)); // D below C
        CHECK(arrangement(object) == "Cam D B* C* A");
    }
    SECTION("A collapsed group moves as a block")
    {
        layers->setLayerGroupCollapsed(group, true);
        CHECK(object->isLayerFolded(2));
        CHECK_FALSE(object->isLayerFolded(3)); // the top member keeps its row
        REQUIRE(layers->moveLayer(3, 4)); // the group above D
        CHECK(arrangement(object) == "Cam A D B* C*");
        undo->trigger();
        CHECK(arrangement(object) == "Cam A B* C* D");
    }
    SECTION("The bottom camera layer stays at the bottom")
    {
        CHECK_FALSE(layers->moveLayer(0, 2));
        REQUIRE(layers->moveLayer(4, 0));
        CHECK(arrangement(object) == "Cam D A B* C*");
    }
}

namespace
{

/// Bitmap layers: empty "A", then group members "B" (red) and "C" (blue) that overlap at the centre.
std::unique_ptr<Object> groupOfTwo(qreal opacity)
{
    std::unique_ptr<Object> object(new Object);
    object->init();
    object->addNewBitmapLayer()->setName("A");
    const QRectF rects[] = { QRectF(-20, -20, 30, 30), QRectF(-10, -10, 30, 30) };
    const QColor colors[] = { Qt::red, Qt::blue };
    for (int i = 0; i < 2; ++i)
    {
        LayerBitmap* layer = object->addNewBitmapLayer();
        layer->setGroupId(1);
        layer->addNewKeyFrameAt(1);
        layer->getBitmapImageAtFrame(1)->drawRect(rects[i], Qt::NoPen, QBrush(colors[i]), QPainter::CompositionMode_Source, false);
    }
    object->addLayerGroup(LayerGroup{ 1, "G", false, true, opacity });
    return object;
}

QImage canvasImage(const Object* object, int paintedLayer, LayerVisibility visibility = LayerVisibility::ALL)
{
    QPixmap canvas(60, 60);
    CanvasPainter painter(canvas);
    const QTransform view = QTransform::fromTranslate(30, 30);
    painter.setViewTransform(view, view.inverted());
    CanvasPainterOptions options;
    options.eLayerVisibility = visibility;
    painter.setOptions(options);
    painter.setPaintSettings(object, paintedLayer, 1, nullptr);
    painter.paint(QRect(0, 0, 60, 60));
    return canvas.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QImage exportImage(const Object* object)
{
    QImage image(60, 60, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setWorldTransform(QTransform::fromTranslate(30, 30));
    object->paintImage(painter, 1, false, true);
    painter.end();
    return image;
}

QImage faded(const QImage& image, qreal opacity)
{
    QImage out(image.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setOpacity(opacity);
    painter.drawImage(0, 0, image);
    return out;
}

/// Largest channel difference between two premultiplied images of one size.
int maxDiff(const QImage& a, const QImage& b)
{
    int diff = 0;
    for (int y = 0; y < a.height(); ++y)
    {
        for (int x = 0; x < a.width(); ++x)
        {
            const QRgb p = a.pixel(x, y), q = b.pixel(x, y);
            diff = std::max({ diff, std::abs(qRed(p) - qRed(q)), std::abs(qGreen(p) - qGreen(q)), std::abs(qBlue(p) - qBlue(q)),
                              std::abs(qAlpha(p) - qAlpha(q)) });
        }
    }
    return diff;
}

} // namespace

TEST_CASE("A hidden group hides its layers on canvas and export", "[LayerGroup]")
{
    auto object = groupOfTwo(1.0);
    object->findLayerGroup(1)->visible = false;
    for (int painted = 0; painted < 3; ++painted)
    {
        for (LayerVisibility visibility : { LayerVisibility::ALL, LayerVisibility::RELATED, LayerVisibility::CURRENTONLY })
        {
            INFO("painted layer " << painted << " visibility " << int(visibility));
            CHECK(qAlpha(canvasImage(object.get(), painted, visibility).pixel(30, 30)) == 0);
        }
    }
    CHECK(qAlpha(exportImage(object.get()).pixel(30, 30)) == 0);
}

TEST_CASE("Group opacity fades the composite of its layers", "[LayerGroup]")
{
    auto opaque = groupOfTwo(1.0);
    auto half = groupOfTwo(0.5);
    for (int painted = 0; painted < 3; ++painted)
    {
        INFO("painted layer " << painted);
        CHECK(maxDiff(canvasImage(half.get(), painted), faded(canvasImage(opaque.get(), painted), 0.5)) <= 1);
    }
    CHECK(maxDiff(exportImage(half.get()), faded(exportImage(opaque.get()), 0.5)) <= 1);
    // The red member does not show through the blue one
    CHECK(qRed(exportImage(half.get()).pixel(30, 30)) == 0);
}

TEST_CASE("Group visibility and opacity changes undo and redo", "[LayerGroup]")
{
    TestSession session;
    addLayers(session.object);
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());
    const int id = layers->createLayerGroup(editor->object()->findLayerByName("B"));
    auto group = [&] { return editor->object()->findLayerGroup(id); };

    layers->setLayerGroupVisible(id, false);
    CHECK_FALSE(group()->visible);
    layers->setLayerGroupOpacity(id, 0.25);
    CHECK(group()->opacity == Approx(0.25));

    undo->trigger();
    CHECK(group()->opacity == Approx(1.0));
    CHECK_FALSE(group()->visible);
    undo->trigger();
    CHECK(group()->visible);
    redo->trigger();
    redo->trigger();
    CHECK_FALSE(group()->visible);
    CHECK(group()->opacity == Approx(0.25));
}