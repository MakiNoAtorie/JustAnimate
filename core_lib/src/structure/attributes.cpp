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

#include <iterator>
#include <QCoreApplication>
#include <QDomElement>

#include "layerbitmap.h"

namespace
{

bool anyLayer(Layer::LAYER_TYPE) { return true; }

// Codes in TimesheetColumn order; Default and Dialogue have none.
const char* const kTimesheetCodes[] = { "", "KA", "KB", "KC", "KD", "KE", "KF", "",
                                        "IA", "IB", "IC", "ID", "IE", "IF", "CAM" };

// Rows of one scope are in the order their XML attributes are written.
const std::vector<AttributeDescriptor> kTable = {
    { AttributeId::Name, AttributeScope::Layer, "name", QString("untitled"),
      QT_TRANSLATE_NOOP("Attributes", "Rename Layer"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return layer->name(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setName(v.toString()); }, true },
    { AttributeId::Visibility, AttributeScope::Layer, "visibility", true,
      QT_TRANSLATE_NOOP("Attributes", "Layer Visibility"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return layer->visible(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setVisible(v.toBool()); }, true },
    { AttributeId::OnionSkin, AttributeScope::Layer, "onion", true,
      QT_TRANSLATE_NOOP("Attributes", "Layer Onion Skin"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return layer->onionSkin(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setOnionSkin(v.toBool()); } },
    { AttributeId::BlendMode, AttributeScope::Layer, "blend", static_cast<int>(BlendMode::Normal),
      QT_TRANSLATE_NOOP("Attributes", "Blend Mode"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return static_cast<int>(layer->blendMode()); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setBlendMode(static_cast<BlendMode>(v.toInt())); },
      false, nullptr,
      [](const QString& s) -> QVariant { const int v = s.toInt(); return v >= 0 && v < kBlendModeCount ? QVariant(v) : QVariant(); } },
    { AttributeId::GroupId, AttributeScope::Layer, "group", 0,
      QT_TRANSLATE_NOOP("Attributes", "Layer Group"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return layer->groupId(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setGroupId(v.toInt()); } },
    { AttributeId::Rough, AttributeScope::Layer, "rough", false,
      QT_TRANSLATE_NOOP("Attributes", "Rough Layer"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return layer->isRough(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setRough(v.toBool()); } },
    { AttributeId::LockAll, AttributeScope::Layer, "lockAll", false,
      QT_TRANSLATE_NOOP("Attributes", "Lock Layer"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return layer->lockAll(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setLockAll(v.toBool()); } },
    { AttributeId::Timesheet, AttributeScope::Layer, "timesheet", static_cast<int>(TimesheetColumn::Default),
      QT_TRANSLATE_NOOP("Attributes", "Timesheet Column"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return static_cast<int>(layer->timesheetColumn()); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setTimesheetColumn(static_cast<TimesheetColumn>(v.toInt())); },
      false,
      [](const QVariant& v) { return QString(QLatin1String(kTimesheetCodes[v.toInt()])); },
      [](const QString& s) -> QVariant {
          for (int i = 1; i < static_cast<int>(std::size(kTimesheetCodes)); ++i)
          {
              if (!s.isEmpty() && s == QLatin1String(kTimesheetCodes[i])) { return i; }
          }
          return QVariant();
      } },
    { AttributeId::Storyboard, AttributeScope::Layer, "storyboard", false,
      QT_TRANSLATE_NOOP("Attributes", "Storyboard Layer"), anyLayer,
      [](const Layer* layer, const KeyFrame*) -> QVariant { return layer->isStoryboard(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { layer->setStoryboard(v.toBool()); } },
    { AttributeId::LockAlpha, AttributeScope::Layer, "lockAlpha", false,
      QT_TRANSLATE_NOOP("Attributes", "Lock Transparency"),
      [](Layer::LAYER_TYPE type) { return type == Layer::BITMAP; },
      [](const Layer* layer, const KeyFrame*) -> QVariant { return static_cast<const LayerBitmap*>(layer)->lockAlpha(); },
      [](Layer* layer, KeyFrame*, const QVariant& v) { static_cast<LayerBitmap*>(layer)->setLockAlpha(v.toBool()); } },
};

QString toXml(const AttributeDescriptor& row, const QVariant& value)
{
    if (row.toXmlText) { return row.toXmlText(value); }
    if (value.typeId() == QMetaType::Bool) { return value.toBool() ? "1" : "0"; }
    return value.toString();
}

// A bool reads "1" or "0", an int any number; anything else loads as the default.
QVariant readValue(const QDomElement& elem, const AttributeDescriptor& row)
{
    if (!elem.hasAttribute(row.xmlName)) { return row.defaultValue; }
    const QString text = elem.attribute(row.xmlName);
    QVariant value;
    if (row.fromXmlText) { value = row.fromXmlText(text); }
    else if (row.defaultValue.typeId() == QMetaType::Bool) { if (text == "1" || text == "0") { value = text == "1"; } }
    else if (row.defaultValue.typeId() == QMetaType::Int) { bool ok = false; const int v = text.toInt(&ok); if (ok) { value = v; } }
    else { value = text; }
    return value.isValid() ? value : row.defaultValue;
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
        if (!row.alwaysWritten && value == row.defaultValue) { continue; }
        const QString text = toXml(row, value);
        // Dialogue has no Timesheet code: like Default, it writes nothing.
        if (row.alwaysWritten || !text.isEmpty()) { elem.setAttribute(row.xmlName, text); }
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
