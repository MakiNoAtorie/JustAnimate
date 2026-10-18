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

/// Table order; the values index the table.
enum class AttributeId
{
    Name,
    Visibility,
    OnionSkin,
    BlendMode,
    GroupId, // set through LayerManager::commitLayerArrangement: a group is a run of adjacent layers
    Rough,
    LockAll,
    Timesheet, // a TimesheetColumn as int
    Storyboard,
    LockAlpha,
    Mark, // a Mark as int
    Tween, // a KeyTween
    PanelFields,
    LinkedTo, // the link id; saved as the linkedTo file name by Layer::writeLinks
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
    bool alwaysWritten = false; // else written only when not the default
    QString (*toXmlText)(const QVariant& value) = nullptr; // nullptr: by the value type
    QVariant (*fromXmlText)(const QString& text) = nullptr; // nullptr: by the value type; invalid loads the default
    void (*writeXml)(QDomElement& elem, const QVariant& value) = nullptr; // a row with several XML attributes; nullptr: one, xmlName
    QVariant (*readXml)(const QDomElement& elem) = nullptr; // invalid: the row is not read from this element
    const char* (*undoTextSourceFor)(const QVariant& after) = nullptr; // nullptr: undoTextSource for every value

    bool appliesTo(Layer::LAYER_TYPE type) const { return appliesToType(type); }
    /// The undo text of setting the row to `after`; an invalid `after` gives the row's base text.
    QString undoText(const QVariant& after = QVariant()) const;
    QVariant get(const Layer* layer, const KeyFrame* key = nullptr) const { return getValue(layer, key); }
    void set(Layer* layer, KeyFrame* key, const QVariant& value) const { setValue(layer, key, value); }
};

const std::vector<AttributeDescriptor>& attributeTable();
const AttributeDescriptor& attribute(AttributeId id);

/// Writes the layer's layer-level attributes. QDom sorts them by name.
void writeLayerAttributes(const Layer& layer, QDomElement& elem);
/// A missing or bad value loads as the default.
void readLayerAttributes(Layer& layer, const QDomElement& elem);
void copyLayerAttributes(const Layer& from, Layer& to);
/// Writes the key's Drawing-level attributes on its <image> element, except linked-to.
void writeKeyAttributes(const Layer& layer, const KeyFrame& key, QDomElement& image);
/// A missing or bad value loads as the default. Linked-to is read by Layer::readLinks.
void readKeyAttributes(const Layer& layer, KeyFrame* key, const QDomElement& image);
void copyKeyAttributes(const KeyFrame& from, KeyFrame& to);

#endif // ATTRIBUTES_H
