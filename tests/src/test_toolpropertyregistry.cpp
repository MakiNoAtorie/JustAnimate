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

#include <type_traits>
#include <QSettings>

#include <QAbstractSlider>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSpinBox>
#include <QTemporaryDir>

#include "brushpresets.h"
#include "inputsmoother.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layervector.h"
#include "object.h"
#include "pencilsettings.h"
#include "spinslider.h"
#include "stroketool.h"
#include "strokeoptionswidget.h"
#include "testfixture.h"
#include "toolmanager.h"
#include "toolproperties.h"

namespace
{

template <typename Id, typename V, typename = void>
struct CanSet : std::false_type {};
template <typename Id, typename V>
struct CanSet<Id, V, std::void_t<decltype(std::declval<ToolProperties&>().set(std::declval<Id>(), std::declval<V>()))>> : std::true_type {};

static_assert(CanSet<decltype(ToolProperty::Width), qreal>::value);
static_assert(!CanSet<decltype(ToolProperty::Width), bool>::value);
static_assert(!CanSet<decltype(ToolProperty::PressureEnabled), int>::value);
static_assert(!CanSet<decltype(ToolProperty::DabCount), qreal>::value);
static_assert(std::is_same_v<decltype(std::declval<const ToolProperties&>().get(ToolProperty::Width)), qreal>);
static_assert(std::is_same_v<decltype(std::declval<const ToolProperties&>().get(ToolProperty::DabCount)), int>);
static_assert(std::is_same_v<decltype(std::declval<const ToolProperties&>().get(ToolProperty::PressureEnabled)), bool>);

const ToolType kStrokeTools[] = { PENCIL, ERASER, SMUDGE, PEN, POLYLINE, BRUSH, SHAPE };
const QString kGroup = "ToolPropertyRegistryTest";

/// Puts every key in the store back as it was at scope exit.
class StoreGuard
{
public:
    StoreGuard()
    {
        QSettings& settings = pencilSettings();
        for (const QString& key : settings.allKeys()) { mSaved.insert(key, settings.value(key)); }
    }
    ~StoreGuard()
    {
        QSettings& settings = pencilSettings();
        settings.clear();
        for (auto it = mSaved.cbegin(); it != mSaved.cend(); ++it) { settings.setValue(it.key(), it.value()); }
    }
    Q_DISABLE_COPY(StoreGuard)

private:
    QHash<QString, QVariant> mSaved;
};

double valueOf(const ToolProperties& p, int id) { return p.getInfo(id).toVariant().toDouble(); }


bool storedBy(const ToolPropertyDescriptor& d, ToolType tool)
{
    return d.id < StrokeToolProperties::END || d.use(tool);
}

} // namespace

TEST_CASE("Tool property registry - an old LineStabilization level in the tool group maps to smoothing once", "[ToolPropertyRegistry]")
{
    const StoreGuard guard;
    QSettings& settings = pencilSettings();
    const SmoothingPreset simple = InputSmoother::presetForLegacyLevel(StabilizationLevel::SIMPLE);

    settings.clear();
    settings.setValue(kGroup + "/LineStabilization", StabilizationLevel::SIMPLE);
    ToolProperties old;
    old.loadTool(PEN, kGroup, settings);
    CHECK(old.get(ToolProperty::SmoothingMode) == static_cast<int>(simple.mode));
    CHECK(old.get(ToolProperty::SmoothingStrength) == simple.strength);

    // A stored SmoothingMode wins over the old key.
    settings.setValue(kGroup + "/SmoothingMode", static_cast<int>(SmoothingMode::None));
    ToolProperties current;
    current.loadTool(PEN, kGroup, settings);
    CHECK(current.get(ToolProperty::SmoothingMode) == static_cast<int>(SmoothingMode::None));
}

TEST_CASE("Tool property registry - every descriptor loads its default, migrates its legacy key and clamps to its range", "[ToolPropertyRegistry]")
{
    const StoreGuard guard;
    QSettings& settings = pencilSettings();

    REQUIRE(ToolProperties::descriptors().size() == 39);

    for (ToolType tool : kStrokeTools)
    {
        settings.clear();
        ToolProperties p;
        p.loadTool(tool, kGroup, settings);
        for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
        {
            CAPTURE(tool, d.key);
            if (!storedBy(d, tool))
            {
                CHECK(p.getInfo(d.id).type() == PropertyInfo::INVALID);
                continue;
            }
            CHECK(p.getInfo(d.id).type() == d.type);
            CHECK(valueOf(p, d.id) == d.toolDefault(tool));
        }

        // Out-of-range stored values load clamped.
        for (bool high : { true, false })
        {
            settings.clear();
            for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
                if (d.type != PropertyInfo::BOOL)
                    settings.setValue(kGroup + "/" + d.key, high ? d.max + 1000 : d.min - 1000);
            ToolProperties clamped;
            clamped.loadTool(tool, kGroup, settings);
            for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
            {
                CAPTURE(tool, d.key, high);
                if (storedBy(d, tool) && d.type != PropertyInfo::BOOL)
                    CHECK(valueOf(clamped, d.id) == (high ? d.max : d.min));
            }
        }

        // An existing user's old top-level keys migrate once and go.
        settings.clear();
        settings.setValue("pencilWidth", 3); // marks the store as legacy
        const ToolPropertyDescriptor* smoothing = nullptr;
        QHash<int, double> written;
        for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
        {
            const ToolPropertyUse* use = d.use(tool);
            if (!use || !use->legacyKey) continue;
            if (d.id == StrokeToolProperties::SMOOTHING_MODE)
            {
                smoothing = &d;
                settings.setValue(use->legacyKey, StabilizationLevel::SIMPLE);
                continue;
            }
            const double def = d.toolDefault(tool);
            const double v = d.type == PropertyInfo::BOOL ? double(!def) : (def == d.min ? d.max : d.min);
            settings.setValue(use->legacyKey, v);
            written.insert(d.id, v);
        }
        ToolProperties migrated;
        migrated.loadTool(tool, kGroup, settings);
        migrated.migrateLegacyKeys(tool, settings);
        for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
        {
            const ToolPropertyUse* use = d.use(tool);
            if (!use || !use->legacyKey) continue;
            CAPTURE(tool, d.key, use->legacyKey);
            CHECK_FALSE(settings.contains(use->legacyKey));
            if (written.contains(d.id))
                CHECK(valueOf(migrated, d.id) == written.value(d.id));
        }
        if (smoothing)
        {
            const SmoothingPreset simple = InputSmoother::presetForLegacyLevel(StabilizationLevel::SIMPLE);
            CHECK(migrated.get(ToolProperty::SmoothingMode) == static_cast<int>(simple.mode));
            CHECK(migrated.get(ToolProperty::SmoothingStrength) == simple.strength);
        }
    }

    // Typed set clamps to the descriptor range.
    settings.clear();
    ToolProperties p;
    p.loadTool(BRUSH, kGroup, settings);
    p.set(ToolProperty::Width, 1e6);
    CHECK(p.get(ToolProperty::Width) == ToolProperties::descriptor(StrokeToolProperties::WIDTH_VALUE)->max);
    p.set(ToolProperty::DabCount, -5);
    CHECK(p.get(ToolProperty::DabCount) == 1);
    p.set(ToolProperty::PressureEnabled, false);
    CHECK_FALSE(p.get(ToolProperty::PressureEnabled));

    // An unregistered property reads as its descriptor default.
    const ToolProperties empty;
    CHECK(empty.get(ToolProperty::Width) == ToolProperties::descriptor(StrokeToolProperties::WIDTH_VALUE)->defaultValue);
}

TEST_CASE("Tool property registry - the Tool settings Panel shows one row per applicable descriptor with its range", "[ToolPropertyRegistry]")
{
    TestSession s;
    LayerBitmap* bitmap = s.object->addNewBitmapLayer();
    LayerVector* vector = s.object->addNewVectorLayer();
    REQUIRE(s.start());
    StrokeOptionsWidget panel(s.editor.get());

    for (ToolType tool : kStrokeTools)
    {
        for (Layer* layer : { static_cast<Layer*>(bitmap), static_cast<Layer*>(vector) })
        {
            s.editor->layers()->setCurrentLayer(layer);
            s.editor->tools()->setCurrentTool(tool);
            panel.updateUI();
            const quint8 bit = layer->type() == Layer::BITMAP ? BITMAP_ROW : VECTOR_ROW;

            QMultiHash<int, QWidget*> rows;
            for (QWidget* w : panel.findChildren<QWidget*>())
                if (w->property("toolProperty").isValid() && w->isVisibleTo(&panel))
                    rows.insert(w->property("toolProperty").toInt(), w);

            for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
            {
                CAPTURE(tool, layer->type(), d.key);
                const ToolPropertyUse* use = d.use(tool);
                CHECK(rows.contains(d.id) == (use && (use->layers & bit)));
                for (QWidget* w : rows.values(d.id))
                {
                    CAPTURE(w->metaObject()->className());
                    if (auto* slider = qobject_cast<SpinSlider*>(w)) { CHECK(slider->minimum() == d.min); CHECK(slider->maximum() == d.max); }
                    else if (auto* spin = qobject_cast<QDoubleSpinBox*>(w)) { CHECK(spin->minimum() == d.min); CHECK(spin->maximum() == d.max); }
                    else if (auto* box = qobject_cast<QSpinBox*>(w)) { CHECK(box->minimum() == d.min); CHECK(box->maximum() == d.max); }
                    else if (auto* strength = qobject_cast<QAbstractSlider*>(w)) { CHECK(strength->minimum() == d.min); CHECK(strength->maximum() == d.max); }
                    else if (auto* combo = qobject_cast<QComboBox*>(w)) { CHECK(combo->count() == int(d.max - d.min) + 1); }
                }
            }
        }
    }
}

TEST_CASE("Tool property registry - a preset saves every stored property and applies it back", "[ToolPropertyRegistry]")
{
    QTemporaryDir dir;
    TestSession s;
    REQUIRE(s.start());
    auto* brush = static_cast<StrokeTool*>(s.editor->tools()->getTool(BRUSH));
    const ToolSettingsGuard guard(brush);

    // Every stored property off its default: bools flipped, numbers at the range end away from the default.
    QHash<int, QVariant> changed;
    for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
    {
        if (!brush->properties().has(d.id)) continue;
        const double def = d.toolDefault(BRUSH);
        const double end = def == d.max ? d.min : d.max;
        const QVariant v = d.type == PropertyInfo::BOOL ? QVariant(def == 0)
                         : d.type == PropertyInfo::INTEGER ? QVariant(int(end)) : QVariant(end);
        brush->setValue(d.id, v);
        changed.insert(d.id, v);
    }
    REQUIRE(changed.size() > 30);

    const BrushPresets presets(dir.path());
    REQUIRE(presets.save("All", *brush));
    QFile file(dir.filePath("All.json"));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonObject settings = QJsonDocument::fromJson(file.readAll()).object().value("settings").toObject();
    CHECK(settings.size() == changed.size());

    brush->toolProperties().restoreProperties();
    REQUIRE(presets.apply("All", *brush));
    for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
    {
        if (!changed.contains(d.id)) continue;
        CAPTURE(d.key);
        const QJsonValue saved = settings.value(d.key);
        CHECK((d.type == PropertyInfo::BOOL ? saved.isBool() : saved.isDouble()));
        CHECK(brush->properties().value(d.id) == changed.value(d.id));
    }
}
