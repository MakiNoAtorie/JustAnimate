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
#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

#include <vector>
#include <QString>
#include <QVariant>

#include "layer.h"

class QDomElement;
class KeyFrame;

enum class AttributeId
{
    LockAlpha,
};

enum class AttributeScope { Layer, Drawing, Project };

/// One layer, Drawing or project attribute. Save, load, duplicate and undo iterate these rows.
struct AttributeDescriptor
{
    AttributeId id;
    AttributeScope scope;
    const char* xmlName;
    QVariant defaultValue;
    const char* undoTextSource; // QT_TRANSLATE_NOOP("Attributes", ...)
    bool (*appliesToType)(Layer::LAYER_TYPE type);
    QVariant (*getValue)(const Layer* layer, const KeyFrame* key);
    void (*setValue)(Layer* layer, KeyFrame* key, const QVariant& value);

    bool appliesTo(Layer::LAYER_TYPE type) const { return appliesToType(type); }
    QString undoText() const;
    QVariant get(const Layer* layer, const KeyFrame* key = nullptr) const { return getValue(layer, key); }
    void set(Layer* layer, KeyFrame* key, const QVariant& value) const { setValue(layer, key, value); }
};

const std::vector<AttributeDescriptor>& attributeTable();
const AttributeDescriptor& attribute(AttributeId id);

/// Writes the layer's non-default layer-level attributes, in table order.
void writeLayerAttributes(const Layer& layer, QDomElement& elem);
/// A missing or bad value loads as the default.
void readLayerAttributes(Layer& layer, const QDomElement& elem);
void copyLayerAttributes(const Layer& from, Layer& to);

#endif // ATTRIBUTES_H
