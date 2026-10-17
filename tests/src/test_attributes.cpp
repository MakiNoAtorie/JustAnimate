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

// A new row with a new value type fails here until the test learns it.
QVariant nonDefault(const AttributeDescriptor& row)
{
    if (row.defaultValue.typeId() == QMetaType::Bool) { return !row.defaultValue.toBool(); }
    return QVariant();
}

} // namespace

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

                const QVariant value = nonDefault(row);
                REQUIRE(value.isValid());
                REQUIRE(row.get(layer) == row.defaultValue);

                QDomDocument doc;
                CHECK_FALSE(layer->createDomElement(doc).hasAttribute(row.xmlName));

                editor->layers()->setAttribute(layer, row.id, value);
                CHECK(row.get(layer) == value);
                CHECK(editor->undoRedo()->hasUnsavedChanges());
                CHECK(undo->text().contains(row.undoText()));

                const QDomElement elem = layer->createDomElement(doc);
                CHECK(elem.hasAttribute(row.xmlName));
                std::unique_ptr<Layer> loaded = freshLayer(type);
                loaded->loadDomElement(elem, QString(), []() {});
                CHECK(row.get(loaded.get()) == value);

                Layer* copy = editor->layers()->duplicateLayer(layer, "copy");
                REQUIRE(copy != nullptr);
                CHECK(row.get(copy) == value);
                undo->trigger(); // the duplicate

                undo->trigger();
                CHECK(row.get(layer) == row.defaultValue);
                redo->trigger();
                CHECK(row.get(layer) == value);
            }
        }
    }
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
