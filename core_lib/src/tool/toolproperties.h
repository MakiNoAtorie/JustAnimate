/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef TOOLPROPERTIES_H
#define TOOLPROPERTIES_H

#include <QHash>
#include <QSettings>
#include <QDebug>
#include <QVector>
#include <QPair>
#include <QStringList>
#include <optional>
#include <type_traits>

#include "pencildef.h"
#include "inputsmoother.h"

struct ToolProperties;

struct PropertyInfo
{
    enum ValueType {
        INTEGER,
        REAL,
        BOOL,
        INVALID
    };

    PropertyInfo() {
        mValueType = INVALID;
        mBaseValue = -1;
    }
    PropertyInfo(int min, int max, int defaultValue)
        : mValueType(INTEGER) {
        mMinValue = min;
        mMaxValue = max;
        mDefaultValue = defaultValue;
        setBaseValue(defaultValue);
    }
    PropertyInfo(qreal min, qreal max, qreal defaultValue)
        : mValueType(REAL) {
        mMinValue = min;
        mMaxValue = max;
        mDefaultValue = defaultValue;
        setBaseValue(defaultValue);
    }

    PropertyInfo(bool base, bool defaultValue)
        : mValueType(BOOL) {
        mMinValue = false;
        mMaxValue = true;
        mDefaultValue = defaultValue;
        setBaseValue(base);
    }

    PropertyInfo(bool base) : PropertyInfo(base, base) {}
    PropertyInfo(int base) : PropertyInfo(base, base, base) {}
    PropertyInfo(qreal base) : PropertyInfo(base, base, base) {}

    void setBaseValue(int newValue) {
        Q_ASSERT(mValueType == INTEGER);
        mBaseValue = qBound(mMinValue.toInt(), newValue, mMaxValue.toInt());
    }

    void setBaseValue(qreal newValue) {
        Q_ASSERT(mValueType == REAL);
        mBaseValue = qBound(mMinValue.toReal(), newValue, mMaxValue.toReal());
    }

    void setBaseValue(bool newValue) {
        Q_ASSERT(mValueType == BOOL);
        mBaseValue = newValue;
    }

    QVariant toVariant() const {
        switch (mValueType) {
            case INTEGER:
                return mBaseValue;
            case REAL:
                return mBaseValue;
            case BOOL:
                return mBaseValue;
            case INVALID:
                return "INVALID";
        }
        return "INVALID";
    }

    int intValue() const {
        if (mValueType != INTEGER) {
            Q_ASSERT(false);
            return -1;
        }

        return mBaseValue.toInt();
    }

    qreal realValue() const {
        if (mValueType != REAL) {
            Q_ASSERT(false);
            return -1.0;
        }

        return mBaseValue.toReal();
    }

    bool boolValue() const {
        if (mValueType != BOOL) {
            Q_ASSERT(false);
            return false;
        }
        return mBaseValue.toBool();
    }

    qreal minReal() const {
        if (mValueType != REAL) {
            return -1.0;
        }
        return mMinValue.toReal();
    }

    qreal maxReal() const {
        if (mValueType != REAL) {
            return -1.0;
        }

        return mMaxValue.toReal();
    }

    int minInt() const {
        if (mValueType != INTEGER) {
            return -1;
        }
        return mMinValue.toInt();
    }

    int maxInt() const {
        if (mValueType != INTEGER) {
            return -1;
        }

        return mMaxValue.toInt();
    }

    void resetBaseValue() {
        switch (mValueType) {
        case INTEGER:
            mBaseValue = mDefaultValue;
            break;
        case REAL:
            mBaseValue = mDefaultValue;
            break;
        case BOOL:
            mBaseValue = mDefaultValue;
            break;
        case INVALID:
            break;
        }
    }

    /// Returns the default value as an real, otherwise -1 if it hasn't been specified or the type doesn't match
    int defaultInt() {
        if (mValueType != INTEGER) {
            return -1;
        }
        return mDefaultValue.toInt();
    }

    /// Returns the default value as an real, otherwise -1.0 if it hasn't been specified or the type doesn't match
    qreal defaultReal() {
        if (mValueType != REAL) {
            return -1.0;
        }
        return mDefaultValue.toReal();
    }

    /// Returns the default value as an bool, otherwise false if it hasn't been specified or the type doesn't match
    bool defaultBool() {
        if (mValueType != BOOL) {
            return false;
        }
        return mDefaultValue.toBool();
    }

    ValueType type() const { return mValueType; }

private:
    ValueType mValueType;
    QVariant mBaseValue;
    QVariant mMinValue;
    QVariant mMaxValue;
    QVariant mDefaultValue;
};

/// A property id that carries its value type, so a get or set with the wrong type does not compile.
template <typename T>
struct ToolPropertyId { int raw; };

/// The values a property of type T takes: T, an int for a real, an enum for an int.
template <typename T, typename V>
inline constexpr bool kToolPropertyAccepts = std::is_same_v<T, V> || (std::is_same_v<T, qreal> && std::is_same_v<V, int>)
                                             || (std::is_same_v<T, int> && std::is_enum_v<V>);

enum class ToolRowKind { Slider, SpinBox, CheckBox, ComboBox };

/// Layer types that show a property's Tool settings row.
enum ToolRowLayers : quint8 { NO_ROW = 0, BITMAP_ROW = 1, VECTOR_ROW = 2, BOTH_ROWS = BITMAP_ROW | VECTOR_ROW };

struct ToolPropertyUse
{
    ToolType tool;
    quint8 layers = NO_ROW;                 // ToolRowLayers
    std::optional<double> defaultValue;     // empty: the descriptor's default
    const char* legacyKey = nullptr;        // top-level key that migrateLegacyKeys reads once
};

/// One Stroke tool property: the only home of its settings key, range and defaults.
struct ToolPropertyDescriptor
{
    int id;                         // a StrokeToolProperties, PolylineToolProperties or ShapeToolProperties Type
    const char* key;                // key in the tool's settings group and in preset JSON
    PropertyInfo::ValueType type;
    double min;
    double max;
    double defaultValue;
    const char* label;              // translate in the "StrokeOptionsWidget" context
    const char* disambiguation;
    ToolRowKind row;
    QVector<ToolPropertyUse> uses;  // tools that show a row or have their own default or legacy key
    QVector<const char*> items;     // ComboBox item labels from min up, in the "StrokeOptionsWidget" context
    const char* legacyGroupKey = nullptr;   // older key in the tool's group, read while key is not stored yet
    void (*fromLegacy)(ToolProperties&, const QVariant&) = nullptr; // maps an old stored value; nullptr: stored as is

    const ToolPropertyUse* use(ToolType tool) const;
    double toolDefault(ToolType tool) const;
    PropertyInfo info(ToolType tool) const;
    QString text() const;               // the translated label
    QStringList itemTexts() const;      // the translated ComboBox items; empty when the row has no item names
};

struct ToolProperties
{
    enum Version {
        NOT_SET = 0,
        VERSION_1 = 1,
        VERSION_2,
        VERSION_3
    };

    ~ToolProperties() {}

    /* Inserts properties into the ToolSetting model for loading and saving */
    void insertProperties(const QHash<int, PropertyInfo>& properties) {
        mProps.insert(properties);
    }

    /*  Loads properties for the given tool from the input QSetting

        If an existing value is found in QSettings then that will be the BaseValue, otherwise
        it'll use use the default value.

        @param toolIdentifier The identifier for the tool. This is later used to look up settings, so make sure it's consistent
        @param settings The QSettings instance that contains the settings to load properties from
    */
    void loadFrom(const QString& toolIdentifier, QSettings& settings) {
        mIdentifier = toolIdentifier;
        settings.beginGroup(mIdentifier);

        Q_ASSERT(mProps.count() > 0);
        for (auto it = mProps.begin(); it != mProps.end(); ++it) {

            PropertyInfo& info = it.value();
            const QString& settingName = identifier(it.key());
            loadProperty(settingName, info, settings);
        }
        settings.endGroup();
    }

    /* Store all tool property changes into the input QSetting instance */
    void storeTo(QSettings& settings) {
        settings.setValue(mVersionKey, mVersion);
        settings.beginGroup(mIdentifier);

        for (auto it = mProps.begin(); it != mProps.end(); ++it) {

            QString propertyId = identifier(it.key());
            if (it.value().type() == PropertyInfo::INVALID) {
                Q_ASSERT_X(false, __func__, "Wrong state, expected a value type INTEGER|REAL|BOOL but got INVALID. You've probably misconfigured the property. "
                                            "Ensure the property has been setup correctly and try again.");
                continue;
            }
            settings.setValue(propertyId, it.value().toVariant());
        }
        settings.endGroup();
        settings.sync();
    }

    void setVersion(Version version) { mVersion = version; }

    PropertyInfo getInfo(int rawPropertyType) const
    {
        return mProps[rawPropertyType];
    }

    void restoreProperties() {
        for (auto it = mProps.begin(); it != mProps.end(); ++it) {
            it.value().resetBaseValue();
        }
    }

    /* Checks whether keys referred to in settings needs to be migrated from the input version */
    bool requireMigration(QSettings& settings, ToolProperties::Version version) {

        if (hasLegacySettings(settings) && !settings.contains(mVersionKey)) {
            // Let's assume we're dealing with an existing user
            return true;
        }

        int migrationNumber = static_cast<int>(version);

        return settings.contains(mVersionKey) && migrationNumber < mVersion && migrationNumber > settings.value(mVersionKey).toInt();
    }

    bool hasLegacySettings(QSettings& settings) const {
        // Crude check for existing settings...
        return settings.contains("brushWidth") || settings.contains("pencilWidth") || settings.contains("penWidth");
    }

    bool isValidType(int rawType) const {
        QString rangeCheck;
        for (const auto& r : mTypeRanges) {

            #ifdef QT_DEBUG
                rangeCheck += QString("[%1...%2]").arg(r.first).arg(r.second);
            #endif
            if (rawType >= r.first && rawType < r.second) {
                return true;
            }
        }
        qWarning() << __FUNCTION__ << ":" << QString("Expected a valid type in range of %1 but instead got: %2. Make sure the input value matches one of the ranges of the tool setting").arg(rangeCheck).arg(rawType);
        return false;
    }

    void insertIdentifiers(const QHash<int, QString>& identifiers) {
        this->mIdentifiers.insert(identifiers);
    }

    QString identifier(int rawKey) const {
        auto it = mIdentifiers.find(rawKey);
        Q_ASSERT_X(it != mIdentifiers.end(),
                   "ToolSettings::identifier",
                   QString("No identifier matching the key %1 found").arg(rawKey).toUtf8().constData());
        return it.value();
    }

    void setRanges(const QVector<QPair<int, int>>& ranges) { mTypeRanges = ranges; }
    void addRange(QPair<int, int> range) { mTypeRanges.append(range); }
    const QVector<QPair<int, int>>& typeRanges() const { return mTypeRanges; }

    static const QVector<ToolPropertyDescriptor>& descriptors();
    static const ToolPropertyDescriptor* descriptor(int id);

    /* Registers every descriptor the Stroke tool stores, with that tool's default, then loads them from its settings group */
    void loadTool(ToolType tool, const QString& toolIdentifier, QSettings& settings);

    /* Reads the tool's old top-level keys once, before VERSION_1, and removes them */
    void migrateLegacyKeys(ToolType tool, QSettings& settings);

    /* Returns the descriptor default when the property is not registered */
    template <typename T>
    T get(ToolPropertyId<T> id) const
    {
        const auto it = mProps.constFind(id.raw);
        if (it != mProps.cend() && it->type() != PropertyInfo::INVALID) { return it->toVariant().template value<T>(); }
        const ToolPropertyDescriptor* d = descriptor(id.raw);
        return d ? static_cast<T>(d->defaultValue) : T{};
    }

    /* Clamps to the property's range; ignores a property that is not registered */
    template <typename T, typename V, typename = std::enable_if_t<kToolPropertyAccepts<T, V>>>
    void set(ToolPropertyId<T> id, V value)
    {
        const auto it = mProps.find(id.raw);
        if (it != mProps.end() && it->type() != PropertyInfo::INVALID) { it->setBaseValue(static_cast<T>(value)); }
    }

    bool has(int id) const { const auto it = mProps.constFind(id); return it != mProps.cend() && it->type() != PropertyInfo::INVALID; }
    /* The value of a registered property; an invalid QVariant when it is not registered */
    QVariant value(int id) const { return has(id) ? mProps.value(id).toVariant() : QVariant(); }
    /* Converts to the property's type and clamps to its range; ignores a property that is not registered */
    void setValue(int id, const QVariant& value) { if (has(id)) { setValue(mProps[id], value); } }

private:
    static void setValue(PropertyInfo& info, const QVariant& value);

    void loadProperty(const QString& settingName, PropertyInfo& info, const QSettings& settings) {
        switch (info.type()) {
            case PropertyInfo::INTEGER: {
                QVariant value = settings.value(settingName, info.defaultInt());
                info.setBaseValue(value.toInt());
                break;
            }
            case PropertyInfo::REAL: {
                QVariant value = settings.value(settingName, info.defaultReal());
                info.setBaseValue(value.toReal());
                break;
            }
            case PropertyInfo::BOOL: {
                QVariant value = settings.value(settingName, info.defaultBool());
                info.setBaseValue(value.toBool());
                break;
            }
            case PropertyInfo::INVALID: {
                Q_ASSERT_X(false, __func__, "Wrong state, expected a value type but got INVALID. You've probably misconfigured the property. "
                                            "Ensure the property has been setup correctly and try again.");
                break;
            }
        }
    }

    // The list of ranges that are valid for the given tool. ToolProperties can inherit its parents cases as well
    // eg. PolyLineTool uses both StrokeSettings range as well as it's own
    QVector<QPair<int, int>> mTypeRanges;
    QHash<int, QString> mIdentifiers;
    QHash<int, PropertyInfo> mProps;

    QString mIdentifier = "undefined";
    Version mVersion = VERSION_1;
    QString mVersionKey = "ToolProperties_Version";
};

struct ToolPropertiesBase {

    virtual ~ToolPropertiesBase() {}

    virtual ToolProperties& toolProperties() = 0;
    virtual PropertyInfo getInfo(int rawPropertyType) const = 0;
};

// Stroke tool property ids; StrokeTool holds their values in its ToolProperties.
struct StrokeToolProperties
{
    enum Type {
        START               = 100,
        WIDTH_VALUE         = START,

        FEATHER_VALUE       = 101,
        SMOOTHING_MODE      = 102,
        PRESSURE_ENABLED         = 103,
        INVISIBILITY_ENABLED     = 104,
        FEATHER_ENABLED          = 105,
        ANTI_ALIASING_ENABLED    = 106,
        FILLCONTOUR_ENABLED      = 107,
        SMOOTHING_STRENGTH       = 108,
        SPACING_VALUE            = 109,
        TIP_SQUARE_ENABLED       = 110,
        TIP_ASPECT_VALUE         = 111,
        TIP_ANGLE_VALUE          = 112,
        TIP_FOLLOW_DIRECTION_ENABLED = 113,
        FLOW_VALUE               = 114,
        OPACITY_VALUE            = 115,
        WASH_ENABLED             = 116,
        SPEED_TO_SIZE_VALUE      = 117,
        TILT_TO_SIZE_VALUE       = 118,
        TILT_TO_ANGLE_VALUE      = 119,
        SIZE_JITTER_VALUE        = 120,
        SCATTER_VALUE            = 121,
        ANGLE_JITTER_VALUE       = 122,
        DAB_COUNT_VALUE          = 123,
        FLOW_PER_DAB_ENABLED     = 124,
        TAPER_START_VALUE        = 125,
        TAPER_END_VALUE          = 126,
        GRAIN_MODE_VALUE         = 127,
        GRAIN_SCALE_VALUE        = 128,
        GRAIN_CONTRAST_VALUE     = 129,
        GRAIN_BRIGHTNESS_VALUE   = 130,
        SMUDGE_LENGTH_VALUE      = 131,
        SMUDGE_RADIUS_VALUE      = 132,
        SMUDGE_COLOR_RATE_VALUE  = 133,
        SMUDGE_MIX_VALUE         = 134,
        GRAIN_TEXTURE_VALUE      = 135,

        END                = 199,
    };
};

struct PolylineToolProperties
{
    enum Type {
        START               = 200,

        CLOSEDPATH_ENABLED  = START,
        BEZIERPATH_ENABLED  = 201,

        END                 = 299,
    };
};

struct BucketToolProperties: public ToolPropertiesBase
{
    enum Type {
        START                           = 300,
        FILLTHICKNESS_VALUE             = START,

        COLORTOLERANCE_VALUE            = 301,
        FILLEXPAND_VALUE                = 302,
        FILLLAYERREFERENCEMODE_VALUE    = 303,
        FILLMODE_VALUE                  = 304,
        COLORTOLERANCE_ENABLED          = 305,
        FILLEXPAND_ENABLED              = 306,
        FILLCLOSEGAPS_VALUE             = 307,

        END                             = 399,
    };

    BucketToolProperties() {
        mToolProperties.setRanges({ { START, END } });

        mToolProperties.insertIdentifiers({
            { FILLTHICKNESS_VALUE,          "FillThickness"},
            { COLORTOLERANCE_VALUE,         "ColorTolerance"},
            { COLORTOLERANCE_ENABLED,       "ColorToleranceEnabled"},
            { FILLEXPAND_VALUE,             "FillExpand"},
            { FILLEXPAND_ENABLED,           "FillExpandEnabled"},
            { COLORTOLERANCE_ENABLED,       "ColorToleranceEnabled"},
            { FILLLAYERREFERENCEMODE_VALUE, "FillReferenceMode"},
            { FILLMODE_VALUE,               "FillMode"},
            { FILLCLOSEGAPS_VALUE,          "FillCloseGaps"}
        });
    }

    ToolProperties& toolProperties() override { return mToolProperties; }

    PropertyInfo getInfo(int rawPropertyType) const override {
        return mToolProperties.getInfo(rawPropertyType);
    }

    qreal fillThickness() const { return getInfo(FILLTHICKNESS_VALUE).realValue(); }
    int tolerance() const { return getInfo(COLORTOLERANCE_VALUE).intValue(); }
    int fillExpandAmount() const { return getInfo(FILLEXPAND_VALUE).intValue(); }
    int fillReferenceMode() const { return getInfo(FILLLAYERREFERENCEMODE_VALUE).intValue(); }
    int fillMode() const { return getInfo(FILLMODE_VALUE).intValue(); }
    int fillCloseGaps() const { const PropertyInfo i = getInfo(FILLCLOSEGAPS_VALUE); return i.type() == PropertyInfo::INTEGER ? i.intValue() : 0; }
    bool colorToleranceEnabled() const { return getInfo(COLORTOLERANCE_ENABLED).boolValue(); }
    bool fillExpandEnabled() const { return getInfo(FILLEXPAND_ENABLED).boolValue(); }

private:
    ToolProperties mToolProperties;
};

struct CameraToolProperties: public ToolPropertiesBase
{
    enum Type {
        START               = 400,
        SHOWPATH_ENABLED    = START,

        PATH_DOTCOLOR_TYPE  = 401,

        END                 = 499,
    };

    CameraToolProperties() {
        mToolProperties.setRanges({ { START, END }});

        mToolProperties.insertIdentifiers({
            { SHOWPATH_ENABLED,     "ShowPathEnabled"},
            { PATH_DOTCOLOR_TYPE,   "PathDotColorType"},
        });
    }

    ToolProperties& toolProperties() override { return mToolProperties; }

    PropertyInfo getInfo(int rawPropertyType) const override {
        return mToolProperties.getInfo(rawPropertyType);
    }

    bool showPathEnabled() const { return getInfo(SHOWPATH_ENABLED).boolValue(); }
    DotColorType dotColorType() const { return static_cast<DotColorType>(getInfo(PATH_DOTCOLOR_TYPE).intValue()); }

private:
    ToolProperties mToolProperties;
};

// Used by both select and move tool
struct TransformToolProperties: public ToolPropertiesBase
{
    enum Type {
        START                        = 500,
        SHOWSELECTIONINFO_ENABLED    = START,
        ANTI_ALIASING_ENABLED        = 501,
        KEYTRANSFORM_ENABLED         = 502, // Move tool: drags write the current Drawing's key transform
        END                          = 599,
    };

    TransformToolProperties() {
        mProperties.setRanges({ { START, END } });

        mProperties.insertIdentifiers({
            { SHOWSELECTIONINFO_ENABLED,    "ShowSelectionInfoEnabled" },
            { ANTI_ALIASING_ENABLED,        "AntiAliasingEnabled" },
            { KEYTRANSFORM_ENABLED,         "KeyTransformEnabled" }
        });
    }

    ToolProperties& toolProperties() override { return mProperties; }

    PropertyInfo getInfo(int rawPropertyType) const override {
        return mProperties.getInfo(rawPropertyType);
    }

    bool showSelectionInfoEnabled() const { return getInfo(SHOWSELECTIONINFO_ENABLED).boolValue(); }
    bool antiAliasingEnabled() const { return getInfo(ANTI_ALIASING_ENABLED).boolValue(); }
    bool keyTransformEnabled() const { const PropertyInfo i = getInfo(KEYTRANSFORM_ENABLED); return i.type() == PropertyInfo::BOOL && i.boolValue(); }

private:
    ToolProperties mProperties;
};

// The shape tool keeps these in its StrokeTool properties, next to the brush settings.
struct ShapeToolProperties
{
    enum Type {
        START               = 600,
        SHAPEMODE_VALUE     = START, // 0 line, 1 rectangle, 2 ellipse, 3 polygon
        END                 = 699,
    };
};

// Typed ids for ToolProperties::get and set.
namespace ToolProperty
{
using S = StrokeToolProperties;
inline constexpr ToolPropertyId<qreal> Width{ S::WIDTH_VALUE };
inline constexpr ToolPropertyId<qreal> Feather{ S::FEATHER_VALUE };
inline constexpr ToolPropertyId<int> SmoothingMode{ S::SMOOTHING_MODE };
inline constexpr ToolPropertyId<bool> PressureEnabled{ S::PRESSURE_ENABLED };
inline constexpr ToolPropertyId<bool> InvisibilityEnabled{ S::INVISIBILITY_ENABLED };
inline constexpr ToolPropertyId<bool> FeatherEnabled{ S::FEATHER_ENABLED };
inline constexpr ToolPropertyId<bool> AntiAliasingEnabled{ S::ANTI_ALIASING_ENABLED };
inline constexpr ToolPropertyId<bool> FillContourEnabled{ S::FILLCONTOUR_ENABLED };
inline constexpr ToolPropertyId<int> SmoothingStrength{ S::SMOOTHING_STRENGTH };
inline constexpr ToolPropertyId<qreal> Spacing{ S::SPACING_VALUE };
inline constexpr ToolPropertyId<bool> TipSquare{ S::TIP_SQUARE_ENABLED };
inline constexpr ToolPropertyId<qreal> TipAspect{ S::TIP_ASPECT_VALUE };
inline constexpr ToolPropertyId<qreal> TipAngle{ S::TIP_ANGLE_VALUE };
inline constexpr ToolPropertyId<bool> TipFollowDirection{ S::TIP_FOLLOW_DIRECTION_ENABLED };
inline constexpr ToolPropertyId<qreal> Flow{ S::FLOW_VALUE };
inline constexpr ToolPropertyId<qreal> Opacity{ S::OPACITY_VALUE };
inline constexpr ToolPropertyId<bool> WashEnabled{ S::WASH_ENABLED };
inline constexpr ToolPropertyId<int> SpeedToSize{ S::SPEED_TO_SIZE_VALUE };
inline constexpr ToolPropertyId<int> TiltToSize{ S::TILT_TO_SIZE_VALUE };
inline constexpr ToolPropertyId<int> TiltToAngle{ S::TILT_TO_ANGLE_VALUE };
inline constexpr ToolPropertyId<int> SizeJitter{ S::SIZE_JITTER_VALUE };
inline constexpr ToolPropertyId<int> Scatter{ S::SCATTER_VALUE };
inline constexpr ToolPropertyId<int> AngleJitter{ S::ANGLE_JITTER_VALUE };
inline constexpr ToolPropertyId<int> DabCount{ S::DAB_COUNT_VALUE };
inline constexpr ToolPropertyId<bool> FlowPerDab{ S::FLOW_PER_DAB_ENABLED };
inline constexpr ToolPropertyId<int> TaperStart{ S::TAPER_START_VALUE };
inline constexpr ToolPropertyId<int> TaperEnd{ S::TAPER_END_VALUE };
inline constexpr ToolPropertyId<int> GrainMode{ S::GRAIN_MODE_VALUE };
inline constexpr ToolPropertyId<int> GrainScale{ S::GRAIN_SCALE_VALUE };
inline constexpr ToolPropertyId<int> GrainContrast{ S::GRAIN_CONTRAST_VALUE };
inline constexpr ToolPropertyId<int> GrainBrightness{ S::GRAIN_BRIGHTNESS_VALUE };
inline constexpr ToolPropertyId<int> SmudgeLength{ S::SMUDGE_LENGTH_VALUE };
inline constexpr ToolPropertyId<int> SmudgeRadius{ S::SMUDGE_RADIUS_VALUE };
inline constexpr ToolPropertyId<int> SmudgeColorRate{ S::SMUDGE_COLOR_RATE_VALUE };
inline constexpr ToolPropertyId<int> SmudgeMix{ S::SMUDGE_MIX_VALUE };
inline constexpr ToolPropertyId<int> GrainTexture{ S::GRAIN_TEXTURE_VALUE };
inline constexpr ToolPropertyId<bool> ClosedPathEnabled{ PolylineToolProperties::CLOSEDPATH_ENABLED };
inline constexpr ToolPropertyId<bool> BezierPathEnabled{ PolylineToolProperties::BEZIERPATH_ENABLED };
inline constexpr ToolPropertyId<int> ShapeMode{ ShapeToolProperties::SHAPEMODE_VALUE };
}

#endif // TOOLPROPERTIES_H
