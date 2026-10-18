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

#include "attributes.h"
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
    default: return QVariant();
    }
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

TEST_CASE("Setting an attribute to its current value records nothing", "[Attributes]")
{
    TestSession session;
    Layer* layer = session.object->addNewBitmapLayer();
    session.start();
    Editor* editor = session.editor.get();

    editor->layers()->setAttribute(layer, AttributeId::LockAlpha, false);
    CHECK_FALSE(editor->undoRedo()->hasUnsavedChanges());
}
