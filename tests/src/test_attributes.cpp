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
#include <QPainter>
#include <QTemporaryDir>

#include "attributes.h"
#include "bitmapimage.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "layersound.h"
#include "layervector.h"
#include "object.h"
#include "testfixture.h"
#include "undoredomanager.h"

namespace
{

Layer* addLayer(Object* object, Layer::LAYER_TYPE type)
{
    switch (type)
    {
    case Layer::BITMAP: return object->addNewBitmapLayer();
    case Layer::VECTOR: return object->addNewVectorLayer();
    case Layer::SOUND: return object->addNewSoundLayer();
    case Layer::CAMERA: return object->addNewCameraLayer();
    default: return nullptr;
    }
}

std::unique_ptr<Layer> freshLayer(Layer::LAYER_TYPE type)
{
    switch (type)
    {
    case Layer::BITMAP: return std::make_unique<LayerBitmap>(99);
    case Layer::VECTOR: return std::make_unique<LayerVector>(99);
    case Layer::SOUND: return std::make_unique<LayerSound>(99);
    case Layer::CAMERA: return std::make_unique<LayerCamera>(99);
    default: return nullptr;
    }
}

// A value other than `current`. A new row with a new value type fails here until the test learns it.
QVariant otherValue(const AttributeDescriptor& row, const QVariant& current)
{
    switch (row.defaultValue.typeId())
    {
    case QMetaType::Bool: return !current.toBool();
    case QMetaType::Int: return current.toInt() + 1;
    case QMetaType::QString: return current.toString() + "x";
    default: break;
    }
    if (current.canConvert<KeyTween>())
    {
        KeyTween t = current.value<KeyTween>();
        t.on = !t.on;
        t.easing = CameraEasingType::INQUAD;
        t.state.tx += 1.5;
        t.state.scale *= 2;
        return QVariant::fromValue(t);
    }
    if (current.canConvert<PanelFields>())
    {
        PanelFields p = current.value<PanelFields>();
        p.scene += "1";
        p.notes += "a & <b>";
        return QVariant::fromValue(p);
    }
    return QVariant();
}

} // namespace

TEST_CASE("The table has a row for every layer attribute", "[Attributes]")
{
    for (AttributeId id : { AttributeId::Name, AttributeId::Visibility, AttributeId::OnionSkin, AttributeId::BlendMode,
                            AttributeId::GroupId, AttributeId::Rough, AttributeId::LockAll, AttributeId::Timesheet,
                            AttributeId::Storyboard, AttributeId::LockAlpha })
    {
        CHECK(attribute(id).id == id);
        CHECK(attribute(id).scope == AttributeScope::Layer);
    }
}

TEST_CASE("Every layer attribute row round-trips through set save load duplicate undo and redo", "[Attributes]")
{
    for (const AttributeDescriptor& row : attributeTable())
    {
        REQUIRE(attribute(row.id).id == row.id);
        if (row.scope != AttributeScope::Layer) { continue; }
        for (Layer::LAYER_TYPE type : { Layer::BITMAP, Layer::VECTOR, Layer::SOUND, Layer::CAMERA })
        {
            if (!row.appliesTo(type)) { continue; }
            DYNAMIC_SECTION(row.xmlName << " on layer type " << type)
            {
                TestSession session;
                Layer* layer = addLayer(session.object, type);
                REQUIRE(layer != nullptr);
                session.start();
                Editor* editor = session.editor.get();
                QObject parent;
                QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
                QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());

                const QVariant start = row.get(layer);
                const QVariant value = otherValue(row, start);
                REQUIRE(value.isValid());

                QDomDocument doc;
                CHECK(layer->createBaseDomElement(doc).hasAttribute(row.xmlName) == row.alwaysWritten);

                editor->layers()->setAttribute(layer, row.id, value);
                CHECK(row.get(layer) == value);
                CHECK(editor->undoRedo()->hasUnsavedChanges());
                CHECK(undo->text().contains(row.undoText()));

                // The base element: an unsaved Drawing has no file for the full layer element.
                const QDomElement elem = layer->createBaseDomElement(doc);
                CHECK(elem.hasAttribute(row.xmlName));
                std::unique_ptr<Layer> loaded = freshLayer(type);
                loaded->loadBaseDomElement(elem);
                CHECK(row.get(loaded.get()) == value);

                Layer* copy = editor->layers()->duplicateLayer(layer, "copy");
                REQUIRE(copy != nullptr);
                // Duplicate Layer names the copy itself.
                CHECK(row.get(copy) == (row.id == AttributeId::Name ? QVariant("copy") : value));
                undo->trigger(); // the duplicate

                undo->trigger();
                CHECK(row.get(layer) == start);
                redo->trigger();
                CHECK(row.get(layer) == value);
            }
        }
    }
}

TEST_CASE("The table has a row for every Drawing attribute", "[Attributes]")
{
    for (AttributeId id : { AttributeId::Mark, AttributeId::Tween, AttributeId::PanelFields, AttributeId::LinkedTo })
    {
        CHECK(attribute(id).id == id);
        CHECK(attribute(id).scope == AttributeScope::Drawing);
    }
}

TEST_CASE("Every Drawing attribute row round-trips through set save load key copy undo and redo", "[Attributes]")
{
    for (const AttributeDescriptor& row : attributeTable())
    {
        if (row.scope != AttributeScope::Drawing) { continue; }
        for (Layer::LAYER_TYPE type : { Layer::BITMAP, Layer::VECTOR, Layer::SOUND, Layer::CAMERA })
        {
            if (!row.appliesTo(type)) { continue; }
            DYNAMIC_SECTION(row.xmlName << " on layer type " << type)
            {
                QTemporaryDir dir;
                REQUIRE(dir.isValid());
                TestSession session;
                Layer* layer = addLayer(session.object, type);
                REQUIRE(layer != nullptr);
                layer->addNewKeyFrameAt(1);
                layer->addNewKeyFrameAt(3);
                if (type == Layer::BITMAP)
                {
                    for (int f : { 1, 3 })
                    {
                        static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(f)->drawRect(
                            QRectF(0, 0, 4, 4), Qt::NoPen, QBrush(Qt::black), QPainter::CompositionMode_Source, false);
                    }
                }
                // A linked-to value needs a Drawing to link to.
                layer->getKeyFrameAt(1)->setLinkId(1);
                session.start();
                Editor* editor = session.editor.get();
                QObject parent;
                QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
                QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());
                KeyFrame* key = layer->getKeyFrameAt(3);

                const QVariant start = row.get(layer, key);
                CHECK(start == row.defaultValue);
                const QVariant value = otherValue(row, start);
                REQUIRE(value.isValid());

                // Frame 4 is a hold of the Drawing at 3.
                CHECK(editor->layers()->setKeyAttribute(layer, 4, row.id, value));
                CHECK(row.get(layer, key) == value);
                // The undo texts from before the table.
                const QHash<AttributeId, QString> oldText = { { AttributeId::Mark, "Mark" }, { AttributeId::Tween, "Tween" },
                    { AttributeId::PanelFields, "Panel Field" }, { AttributeId::LinkedTo, "Unlink Drawing" } };
                CHECK(undo->text().endsWith(" " + oldText.value(row.id, "?")));

                const QString path = dir.filePath("drawing.pclx");
                FileManager fm;
                REQUIRE(fm.save(session.object, path).ok());
                std::unique_ptr<Object> loaded(fm.load(path));
                REQUIRE(loaded != nullptr);
                const Layer* loadedLayer = loaded->findLayerById(layer->id());
                REQUIRE(loadedLayer != nullptr);
                CHECK(row.get(loadedLayer, loadedLayer->getKeyFrameAt(3)) == value);

                std::unique_ptr<KeyFrame> copy(key->clone());
                CHECK(row.get(layer, copy.get()) == value);

                undo->trigger();
                CHECK(row.get(layer, key) == start);
                redo->trigger();
                CHECK(row.get(layer, key) == value);
            }
        }
    }
}

// Qt 6.8 QDom writes attributes sorted by name; Qt 6.4 and 6.5 write them in hash order, so compare them sorted.
TEST_CASE("Layer attributes save byte-identical", "[Attributes]")
{
    auto saved = [](const Layer& layer) {
        QDomDocument doc;
        const QDomNamedNodeMap attributes = layer.createDomElement(doc).attributes();
        QStringList parts;
        for (int i = 0; i < attributes.count(); ++i)
        {
            const QDomAttr attribute = attributes.item(i).toAttr();
            parts << QString("%1=\"%2\"").arg(attribute.name(), attribute.value());
        }
        parts.sort();
        return ("<layer " + parts.join(' ') + "/>").toStdString();
    };
    LayerBitmap layer(7);
    layer.setName("Ink");
    CHECK(saved(layer) == "<layer id=\"7\" name=\"Ink\" type=\"1\" visibility=\"1\"/>");

    layer.setVisible(false);
    layer.setOnionSkin(false);
    layer.setBlendMode(BlendMode::Multiply);
    layer.setGroupId(3);
    layer.setRough(true);
    layer.setLockAll(true);
    layer.setTimesheetColumn(TimesheetColumn::InbetweenB);
    layer.setStoryboard(true);
    layer.setLockAlpha(true);
    CHECK(saved(layer) == "<layer blend=\"1\" group=\"3\" id=\"7\" lockAll=\"1\" lockAlpha=\"1\" name=\"Ink\""
                          " onion=\"0\" rough=\"1\" storyboard=\"1\" timesheet=\"IB\" type=\"1\" visibility=\"0\"/>");
}

TEST_CASE("Rename and lock-all are one undo step each", "[Attributes]")
{
    TestSession session;
    Layer* layer = session.object->addNewBitmapLayer();
    layer->setName("A");
    session.start();
    Editor* editor = session.editor.get();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());

    REQUIRE(editor->layers()->renameLayer(layer, "B") == Status::OK);
    editor->layers()->setAttribute(layer, AttributeId::LockAll, true);
    CHECK(layer->name() == "B");
    CHECK(layer->lockAll());

    undo->trigger();
    CHECK_FALSE(layer->lockAll());
    CHECK(layer->name() == "B");
    undo->trigger();
    CHECK(layer->name() == "A");
}

TEST_CASE("Duplicate Layer keeps the copy in the original's group", "[Attributes]")
{
    TestSession session;
    Layer* grouped = session.object->addNewBitmapLayer();
    session.object->addNewVectorLayer();
    session.start();
    Editor* editor = session.editor.get();
    LayerManager* layers = editor->layers();
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redo = editor->undoRedo()->createRedoAction(&parent, QIcon());
    const int group = layers->createLayerGroup(grouped);
    REQUIRE(group != 0);
    const int index = session.object->layerGroupRange(group).first;

    Layer* copy = layers->duplicateLayer(grouped, "copy");
    REQUIRE(copy != nullptr);
    CHECK(copy->name() == "copy");
    CHECK(copy->groupId() == group);
    CHECK(session.object->getLayer(index + 1) == copy);
    CHECK(session.object->layerGroupRange(group) == qMakePair(index, index + 1));

    undo->trigger();
    CHECK(layers->findLayerById(copy->id()) == nullptr);
    CHECK(session.object->layerGroupRange(group) == qMakePair(index, index));
    redo->trigger();
    CHECK(session.object->layerGroupRange(group) == qMakePair(index, index + 1));
}

TEST_CASE("A Mark edit keeps the playhead and the Drawing", "[Attributes]")
{
    TestSession session;
    LayerBitmap* layer = session.object->addNewBitmapLayer();
    layer->addNewKeyFrameAt(1);
    layer->addNewKeyFrameAt(5);
    layer->getBitmapImageAtFrame(1)->drawRect(QRectF(0, 0, 4, 4), Qt::NoPen, QBrush(Qt::black),
                                              QPainter::CompositionMode_Source, false);
    session.start();
    Editor* editor = session.editor.get();
    editor->layers()->setCurrentLayer(layer);
    editor->scrubTo(5);
    QObject parent;
    QAction* undo = editor->undoRedo()->createUndoAction(&parent, QIcon());
    int scrubs = 0;
    QObject::connect(editor, &Editor::scrubbed, &parent, [&scrubs] { ++scrubs; });
    BitmapImage* key = layer->getBitmapImageAtFrame(1);
    const QImage pixels = key->image()->copy();

    editor->layers()->setKeyMark(1, Mark::Key);
    CHECK(key->mark() == Mark::Key);
    undo->trigger();
    CHECK(scrubs == 0);
    CHECK(editor->currentFrame() == 5);
    // The undo restored the value on the same Drawing, not a clone.
    REQUIRE(layer->getBitmapImageAtFrame(1) == key);
    CHECK(key->mark() == Mark::None);
    CHECK(*key->image() == pixels);

    // Undo texts read as before the table.
    editor->layers()->setKeyMark(1, Mark::Key);
    CHECK(undo->text().endsWith(" Mark"));
    CHECK_FALSE(undo->text().contains("Clear"));
    editor->layers()->setKeyMark(1, Mark::None);
    CHECK(undo->text().contains("Clear Mark"));
}

// Qt 6.4 and 6.5 QDom write attributes in hash order, so compare each <image> sorted.
TEST_CASE("Drawing attributes save byte-identical", "[Attributes]")
{
    LayerBitmap layer(7);
    layer.addNewKeyFrameAt(1);
    layer.addNewKeyFrameAt(3);
    KeyFrame* key = layer.getKeyFrameAt(1);
    key->setMark(Mark::Breakdown);
    key->setTween(true);
    key->setTweenEasing(CameraEasingType::INQUAD);
    key->setTweenState(TweenState{ 1.5, -2, 30, 0.25, 0.5 });
    key->setPanelFields({ "1", "12", "Pan & go", "\"Hi\"" });
    key->setLinkId(4);
    layer.getKeyFrameAt(3)->setLinkId(4);
    QDomDocument doc;
    const QDomElement layerElem = layer.createDomElement(doc);
    QStringList images;
    for (QDomElement image = layerElem.firstChildElement(); !image.isNull(); image = image.nextSiblingElement())
    {
        const QDomNamedNodeMap attributes = image.attributes();
        QStringList parts;
        for (int i = 0; i < attributes.count(); ++i)
        {
            const QDomAttr attribute = attributes.item(i).toAttr();
            parts << QString("%1=\"%2\"").arg(attribute.name(), attribute.value());
        }
        parts.sort();
        images << "<" + image.tagName() + " " + parts.join(' ') + "/>";
    }
    CHECK(images.join("").toStdString() ==
          "<image frame=\"3\" linkedTo=\"007.001.png\" opacity=\"1\" src=\"007.003.png\" topLeftX=\"0\" topLeftY=\"0\"/>"
          "<image audio=\"\"Hi\"\" cut=\"12\" easing=\"1\" frame=\"1\" mark=\"breakdown\" notes=\"Pan & go\""
          " opacity=\"0.5\" rot=\"30\" scale=\"0.25\" scene=\"1\" src=\"007.001.png\" topLeftX=\"0\" topLeftY=\"0\""
          " tween=\"1\" tx=\"1.5\" ty=\"-2\"/>");
}

TEST_CASE("Setting an attribute to its current value records nothing", "[Attributes]")
{
    TestSession session;
    Layer* layer = session.object->addNewBitmapLayer();
    session.start();
    Editor* editor = session.editor.get();

    editor->layers()->setAttribute(layer, AttributeId::LockAlpha, false);
    CHECK_FALSE(editor->undoRedo()->hasUnsavedChanges());
}
