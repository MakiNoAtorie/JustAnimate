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
#include "attributes.h"

#include <QCoreApplication>
#include <QDomElement>

#include "layerbitmap.h"

namespace
{

const std::vector<AttributeDescriptor> kTable = {
    { AttributeId::LockAlpha, AttributeScope::Layer, "lockAlpha", false,
      QT_TRANSLATE_NOOP("Attributes", "Lock Transparency"),
      [](Layer::LAYER_TYPE type) { return type == Layer::BITMAP; },
      [](const Layer* layer, const KeyFrame*) -> QVariant { return static_cast<const LayerBitmap*>(layer)->lockAlpha(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { static_cast<LayerBitmap*>(layer)->setLockAlpha(v.toBool()); } },
};

// Bool rows write "1" and read only "1" as true, as the file format had before the table.
void writeValue(QDomElement& elem, const char* name, const QVariant& value)
{
    if (value.typeId() == QMetaType::Bool) { elem.setAttribute(name, 1); }
}

QVariant readValue(const QDomElement& elem, const AttributeDescriptor& row)
{
    if (!elem.hasAttribute(row.xmlName)) { return row.defaultValue; }
    if (row.defaultValue.typeId() == QMetaType::Bool) { return elem.attribute(row.xmlName) == "1"; }
    return row.defaultValue;
}

bool isLayerRow(const AttributeDescriptor& row, const Layer& layer)
{
    return row.scope == AttributeScope::Layer && row.appliesTo(layer.type());
}

} // namespace

QString AttributeDescriptor::undoText() const
{
    return QCoreApplication::translate("Attributes", undoTextSource);
}

const std::vector<AttributeDescriptor>& attributeTable()
{
    return kTable;
}

const AttributeDescriptor& attribute(AttributeId id)
{
    return kTable[static_cast<size_t>(id)];
}

void writeLayerAttributes(const Layer& layer, QDomElement& elem)
{
    for (const AttributeDescriptor& row : kTable)
    {
        if (!isLayerRow(row, layer)) { continue; }
        const QVariant value = row.get(&layer);
        if (value != row.defaultValue) { writeValue(elem, row.xmlName, value); }
    }
}

void readLayerAttributes(Layer& layer, const QDomElement& elem)
{
    for (const AttributeDescriptor& row : kTable)
    {
        if (isLayerRow(row, layer)) { row.set(&layer, nullptr, readValue(elem, row)); }
    }
}

void copyLayerAttributes(const Layer& from, Layer& to)
{
    for (const AttributeDescriptor& row : kTable)
    {
        if (isLayerRow(row, from) && isLayerRow(row, to)) { row.set(&to, nullptr, row.get(&from)); }
    }
}
