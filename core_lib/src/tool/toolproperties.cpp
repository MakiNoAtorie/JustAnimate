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
#include "toolproperties.h"

#include <QCoreApplication>

#include "shapetool.h"
#include "tiledbuffer.h"

namespace
{

template <typename T> constexpr PropertyInfo::ValueType valueType();
template <> constexpr PropertyInfo::ValueType valueType<qreal>() { return PropertyInfo::REAL; }
template <> constexpr PropertyInfo::ValueType valueType<int>() { return PropertyInfo::INTEGER; }
template <> constexpr PropertyInfo::ValueType valueType<bool>() { return PropertyInfo::BOOL; }

// The id's value type sets the descriptor's type, so the two cannot disagree.
template <typename T>
ToolPropertyDescriptor row(ToolPropertyId<T> id, const char* key, double min, double max, double defaultValue,
                           ToolRowKind kind, const char* label, const char* disambiguation, QVector<ToolPropertyUse> uses, QVector<const char*> items = {})
{
    return { id.raw, key, valueType<T>(), min, max, defaultValue, label, disambiguation, kind, std::move(uses), std::move(items) };
}

// Older versions stored a StabilizationLevel; it maps to a smoothing mode and strength.
ToolPropertyDescriptor withLegacyLevel(ToolPropertyDescriptor d)
{
    d.legacyGroupKey = "LineStabilization";
    d.fromLegacy = [](ToolProperties& p, const QVariant& level) {
        const SmoothingPreset preset = InputSmoother::presetForLegacyLevel(level.toInt());
        p.set(ToolProperty::SmoothingMode, static_cast<int>(preset.mode));
        p.set(ToolProperty::SmoothingStrength, preset.strength);
    };
    return d;
}

QVector<ToolPropertyUse> rows(std::initializer_list<ToolType> tools, quint8 layers)
{
    QVector<ToolPropertyUse> uses;
    for (ToolType tool : tools) { uses.append({ tool, layers }); }
    return uses;
}

QVector<ToolPropertyDescriptor> makeDescriptors()
{
    using namespace ToolProperty;
    using K = ToolRowKind;
    const char* const opts = "Tool options";
    // Local: StrokeTool reads the table during static initialization.
    const std::initializer_list<ToolType> dabTools = { PENCIL, BRUSH, ERASER, SMUDGE, SHAPE };
    return {
        row(Width, "Width", 1, 200, 24, K::Slider, "Width", nullptr, {
            { PENCIL, BITMAP_ROW, 4.0, "pencilWidth" }, { BRUSH, BOTH_ROWS, {}, "brushWidth" },
            { ERASER, BOTH_ROWS, {}, "eraserWidth" }, { SMUDGE, BITMAP_ROW, {}, "smudgeWidth" },
            { PEN, BOTH_ROWS, 12.0, "penWidth" }, { POLYLINE, BOTH_ROWS, 8.0, "polylineWidth" }, { SHAPE, BOTH_ROWS } }),
        row(Feather, "Feather", 1, 99, 48, K::Slider, "Feather", nullptr, {
            { PENCIL, NO_ROW, 50.0 }, { BRUSH, BITMAP_ROW, {}, "brushFeather" }, { ERASER, BITMAP_ROW, {}, "eraserFeather" },
            { SMUDGE, BITMAP_ROW, {}, "smudgeFeather" }, { SHAPE, BITMAP_ROW } }),
        withLegacyLevel(row(ToolProperty::SmoothingMode, "SmoothingMode", int(::SmoothingMode::None), int(InputSmoother::LAST_MODE), int(::SmoothingMode::OneEuro),
            K::ComboBox, "Smoothing", nullptr, {
            { PENCIL, BOTH_ROWS, {}, "pencilLineStabilization" }, { BRUSH, BOTH_ROWS, {}, "brushLineStabilization" },
            { ERASER, BOTH_ROWS, double(::SmoothingMode::None), "stabilizerLevel" }, { SMUDGE, BITMAP_ROW },
            { PEN, BOTH_ROWS, {}, "penLineStablization" } })),
        row(PressureEnabled, "PressureEnabled", 0, 1, false, K::CheckBox, "Pressure", opts, {
            { PENCIL, BITMAP_ROW, true, "pencilPressure" }, { BRUSH, BOTH_ROWS, true, "brushPressure" },
            { ERASER, BOTH_ROWS, true, "eraserPressure" }, { SMUDGE, BITMAP_ROW }, { PEN, BOTH_ROWS, true, "penPressure" },
            { SHAPE, NO_ROW, true } }),
        row(InvisibilityEnabled, "InvisibilityEnabled", 0, 1, false, K::CheckBox, "Invisible", opts, {
            { BRUSH, VECTOR_ROW, {}, "brushInvisibility" }, { SHAPE, VECTOR_ROW } }),
        row(FeatherEnabled, "FeatherEnabled", 0, 1, false, K::CheckBox, "Use Feather", nullptr, {
            { BRUSH, NO_ROW, true }, { ERASER, BITMAP_ROW, true, "eraserUseFeather" }, { SHAPE, NO_ROW, true } }),
        row(AntiAliasingEnabled, "AntiAliasingEnabled", 0, 1, false, K::CheckBox, "Anti-Aliasing", "Brush AA", {
            { ERASER, BITMAP_ROW, true, "eraserAA" }, { PEN, BITMAP_ROW, true, "penAA" }, { POLYLINE, BITMAP_ROW, true, "brushAA" } }),
        row(FillContourEnabled, "FillContourEnabled", 0, 1, false, K::CheckBox, "Fill Contour", nullptr, {
            { PENCIL, VECTOR_ROW, {}, "FillContour" } }),
        // The smoothing mode's rows.
        row(SmoothingStrength, "SmoothingStrength", 0, 100, 50, K::Slider, "Smoothing strength", nullptr,
            rows({ PENCIL, BRUSH, ERASER, PEN }, BOTH_ROWS) << ToolPropertyUse{ SMUDGE, BITMAP_ROW }),
        row(Spacing, "Spacing", 1, 400, 12.5, K::Slider, "Spacing %", nullptr, {
            { PENCIL, BITMAP_ROW }, { BRUSH, BITMAP_ROW }, { ERASER, BITMAP_ROW },
            // About the 2 px step of the smudge loop before v0.5 at the default width.
            { SMUDGE, BITMAP_ROW, 10.0 }, { SHAPE, BITMAP_ROW } }),
        row(TipSquare, "TipSquare", 0, 1, false, K::CheckBox, "Square Tip", opts, rows(dabTools, BITMAP_ROW)),
        row(TipAspect, "TipAspect", 5, 100, 100, K::Slider, "Tip Aspect %", nullptr, rows(dabTools, BITMAP_ROW)),
        row(TipAngle, "TipAngle", -180, 180, 0, K::Slider, "Tip Angle", nullptr, rows(dabTools, BITMAP_ROW)),
        row(TipFollowDirection, "TipFollowDirection", 0, 1, false, K::CheckBox, "Follow Direction", opts, rows(dabTools, BITMAP_ROW)),
        row(Flow, "Flow", 1, 100, 100, K::Slider, "Flow %", nullptr, rows({ PENCIL, BRUSH, SMUDGE, SHAPE }, BITMAP_ROW)),
        row(Opacity, "Opacity", 1, 100, 100, K::Slider, "Opacity %", nullptr, rows({ PENCIL, BRUSH, SHAPE }, BITMAP_ROW)),
        row(WashEnabled, "WashEnabled", 0, 1, false, K::CheckBox, "Wash", nullptr, rows({ PENCIL, BRUSH, SHAPE }, BITMAP_ROW)),
        // A shape has no pen motion, so pen dynamics do not apply.
        row(SpeedToSize, "SpeedToSize", -100, 100, 0, K::SpinBox, "Speed to size", nullptr, rows({ PENCIL, BRUSH, ERASER, SMUDGE }, BITMAP_ROW)),
        row(TiltToSize, "TiltToSize", -100, 100, 0, K::SpinBox, "Tilt to size", nullptr, rows({ PENCIL, BRUSH, ERASER, SMUDGE }, BITMAP_ROW)),
        row(TiltToAngle, "TiltToAngle", -100, 100, 0, K::SpinBox, "Tilt to tip angle", nullptr, rows({ PENCIL, BRUSH, ERASER, SMUDGE }, BITMAP_ROW)),
        row(SizeJitter, "SizeJitter", 0, 100, 0, K::SpinBox, "Size jitter", nullptr, rows(dabTools, BITMAP_ROW)),
        row(Scatter, "Scatter", 0, 500, 0, K::SpinBox, "Scatter", nullptr, rows(dabTools, BITMAP_ROW)),
        row(AngleJitter, "AngleJitter", 0, 180, 0, K::SpinBox, "Angle jitter", nullptr, rows(dabTools, BITMAP_ROW)),
        row(DabCount, "DabCount", 1, 16, 1, K::SpinBox, "Dabs per step", nullptr, rows(dabTools, BITMAP_ROW)),
        row(FlowPerDab, "FlowPerDab", 0, 1, false, K::CheckBox, "Flow per dab (Classic)", nullptr, {}),
        row(TaperStart, "TaperStart", 0, 1000, 0, K::SpinBox, "Start taper", nullptr, rows(dabTools, BITMAP_ROW)),
        row(TaperEnd, "TaperEnd", 0, 1000, 0, K::SpinBox, "End taper", nullptr, rows(dabTools, BITMAP_ROW)),
        row(GrainMode, "GrainMode", 0, 2, 0, K::ComboBox, "Grain", nullptr, rows(dabTools, BITMAP_ROW),
            { QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Off"), QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Fixed to paper"),
              QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Moves with stroke") }),
        row(GrainScale, "GrainScale", 25, 800, 100, K::SpinBox, "Grain scale", nullptr, rows(dabTools, BITMAP_ROW)),
        row(GrainContrast, "GrainContrast", 0, 400, 100, K::SpinBox, "Grain contrast", nullptr, rows(dabTools, BITMAP_ROW)),
        row(GrainBrightness, "GrainBrightness", -100, 100, 0, K::SpinBox, "Grain brightness", nullptr, rows(dabTools, BITMAP_ROW)),
        row(SmudgeLength, "SmudgeLength", 0, 100, 50, K::SpinBox, "Smudge length", nullptr, rows({ SMUDGE }, BITMAP_ROW)),
        row(SmudgeRadius, "SmudgeRadius", 1, 100, 50, K::SpinBox, "Smudge sample radius", nullptr, rows({ SMUDGE }, BITMAP_ROW)),
        row(SmudgeColorRate, "SmudgeColorRate", 0, 100, 0, K::SpinBox, "Smudge colour rate", nullptr, rows({ SMUDGE }, BITMAP_ROW)),
        row(SmudgeMix, "SmudgeMix", 0, 1, 0, K::ComboBox, "Smudge mixing", nullptr, rows({ SMUDGE }, BITMAP_ROW),
            { QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Light"), QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Pigment") }),
        row(GrainTexture, "GrainTexture", 0, TiledBuffer::grainTextureCount - 1, 0, K::ComboBox, "Grain texture", nullptr, rows(dabTools, BITMAP_ROW)),
        row(ClosedPathEnabled, "ClosedPathEnabled", 0, 1, false, K::CheckBox, "Closed Path", nullptr, {
            { POLYLINE, BOTH_ROWS, {}, "closedPolylinePath" } }),
        row(BezierPathEnabled, "BezierPathEnabled", 0, 1, false, K::CheckBox, "B\xC3\xA9zier", opts, rows({ POLYLINE }, BITMAP_ROW)),
        row(ShapeMode, "ShapeMode", ShapeTool::LINE, ShapeTool::POLYGON, ShapeTool::RECTANGLE, K::ComboBox, "Shape", nullptr, rows({ SHAPE }, BOTH_ROWS),
            { QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Line"), QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Rectangle"),
              QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Ellipse"), QT_TRANSLATE_NOOP("StrokeOptionsWidget", "Polygon") }),
    };
}

} // namespace

const ToolPropertyUse* ToolPropertyDescriptor::use(ToolType tool) const
{
    for (const ToolPropertyUse& u : uses)
        if (u.tool == tool) { return &u; }
    return nullptr;
}

double ToolPropertyDescriptor::toolDefault(ToolType tool) const
{
    const ToolPropertyUse* u = use(tool);
    return u && u->defaultValue ? *u->defaultValue : defaultValue;
}

PropertyInfo ToolPropertyDescriptor::info(ToolType tool) const
{
    const double value = toolDefault(tool);
    switch (type)
    {
    case PropertyInfo::INTEGER: return PropertyInfo(int(min), int(max), int(value));
    case PropertyInfo::REAL: return PropertyInfo(qreal(min), qreal(max), qreal(value));
    case PropertyInfo::BOOL: return PropertyInfo(value != 0);
    case PropertyInfo::INVALID: break;
    }
    return PropertyInfo();
}

QString ToolPropertyDescriptor::text() const
{
    return QCoreApplication::translate("StrokeOptionsWidget", label, disambiguation);
}

QStringList ToolPropertyDescriptor::itemTexts() const
{
    QStringList texts;
    if (id == StrokeToolProperties::GRAIN_TEXTURE_VALUE)
    {
        for (int i = 0; i < TiledBuffer::grainTextureCount; ++i)
            texts << QCoreApplication::translate("GrainTexture", TiledBuffer::grainTextureName(i));
    }
    for (const char* item : items)
        texts << QCoreApplication::translate("StrokeOptionsWidget", item);
    return texts;
}

const QVector<ToolPropertyDescriptor>& ToolProperties::descriptors()
{
    static const QVector<ToolPropertyDescriptor> table = makeDescriptors();
    return table;
}

const ToolPropertyDescriptor* ToolProperties::descriptor(int id)
{
    for (const ToolPropertyDescriptor& d : descriptors())
        if (d.id == id) { return &d; }
    return nullptr;
}

void ToolProperties::loadTool(ToolType tool, const QString& toolIdentifier, QSettings& settings)
{
    QHash<int, PropertyInfo> info;
    for (const ToolPropertyDescriptor& d : descriptors())
    {
        // Every Stroke tool stores the shared StrokeToolProperties range.
        if (d.id >= StrokeToolProperties::END && !d.use(tool)) { continue; }
        info.insert(d.id, d.info(tool));
        mIdentifiers.insert(d.id, d.key);
    }
    insertProperties(info);
    loadFrom(toolIdentifier, settings);

    for (const ToolPropertyDescriptor& d : descriptors())
    {
        if (!d.legacyGroupKey || !mProps.contains(d.id)) { continue; }
        const QString oldKey = toolIdentifier + "/" + d.legacyGroupKey;
        if (settings.contains(oldKey) && !settings.contains(toolIdentifier + "/" + d.key))
        {
            if (d.fromLegacy) { d.fromLegacy(*this, settings.value(oldKey)); }
            else { setValue(mProps[d.id], settings.value(oldKey)); }
        }
    }
}

void ToolProperties::migrateLegacyKeys(ToolType tool, QSettings& settings)
{
    if (!requireMigration(settings, VERSION_1)) { return; }

    for (const ToolPropertyDescriptor& d : descriptors())
    {
        const ToolPropertyUse* use = d.use(tool);
        if (!use || !use->legacyKey || !mProps.contains(d.id)) { continue; }

        if (d.fromLegacy && settings.contains(use->legacyKey))
            d.fromLegacy(*this, settings.value(use->legacyKey));
        else
            setValue(mProps[d.id], settings.value(use->legacyKey, d.toolDefault(tool)));
        settings.remove(use->legacyKey);
    }
}

void ToolProperties::setValue(PropertyInfo& info, const QVariant& value)
{
    switch (info.type())
    {
    case PropertyInfo::INTEGER: info.setBaseValue(value.toInt()); break;
    case PropertyInfo::REAL: info.setBaseValue(value.toReal()); break;
    case PropertyInfo::BOOL: info.setBaseValue(value.toBool()); break;
    case PropertyInfo::INVALID: break;
    }
}
