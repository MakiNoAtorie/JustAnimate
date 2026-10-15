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
#include "brushpresets.h"

#include <optional>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include "stroketool.h"
#include "toolproperties.h"

namespace
{

// Preset files are small; a larger file is not a preset.
constexpr qint64 MAX_PRESET_BYTES = 1 << 20;

std::optional<QJsonObject> readPreset(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;

    QJsonParseError error{};
    const QJsonDocument doc = file.size() > MAX_PRESET_BYTES ? QJsonDocument() : QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "Skipping corrupt brush preset" << path << error.errorString();
        return std::nullopt;
    }
    return doc.object();
}

}

BrushPresets::BrushPresets(const QString& folder) : mFolder(folder)
{
    if (mFolder.isEmpty())
        mFolder = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("brushpresets");
}

QStringList BrushPresets::names() const
{
    QStringList result;
    const QFileInfoList files = QDir(mFolder).entryInfoList({ "*.json" }, QDir::Files, QDir::Name);
    for (const QFileInfo& info : files)
    {
        const QString name = info.completeBaseName();
        if (isValidName(name) && readPreset(info.filePath()))
            result << name;
    }
    return result;
}

bool BrushPresets::save(const QString& name, StrokeTool& tool) const
{
    if (!isValidName(name) || !QDir().mkpath(mFolder))
        return false;

    const ToolProperties& properties = tool.toolProperties();
    QJsonObject settings;
    for (const auto& range : properties.typeRanges())
    {
        for (int key = range.first; key < range.second; ++key)
        {
            const PropertyInfo info = properties.getInfo(key);
            if (info.type() != PropertyInfo::INVALID)
                settings.insert(properties.identifier(key), QJsonValue::fromVariant(info.toVariant()));
        }
    }

    QSaveFile file(filePath(name));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QJsonObject preset{ { "settings", settings }, { "tipImage", tool.tipImagePath() } };
    file.write(QJsonDocument(preset).toJson());
    return file.commit();
}

bool BrushPresets::apply(const QString& name, StrokeTool& tool) const
{
    if (!isValidName(name))
        return false;
    const std::optional<QJsonObject> preset = readPreset(filePath(name));
    if (!preset)
        return false;

    const QJsonObject settings = preset->value("settings").toObject();
    ToolProperties& properties = tool.toolProperties();
    properties.restoreProperties();
    for (const auto& range : properties.typeRanges())
    {
        for (int key = range.first; key < range.second; ++key)
        {
            const PropertyInfo info = properties.getInfo(key);
            if (info.type() == PropertyInfo::INVALID)
                continue;
            const QJsonValue value = settings.value(properties.identifier(key));
            switch (info.type())
            {
            case PropertyInfo::INTEGER:
                if (value.isDouble())
                    properties.setBaseValue(key, PropertyInfo(static_cast<int>(qBound<double>(info.minInt(), value.toDouble(), info.maxInt()))));
                break;
            case PropertyInfo::REAL:
                if (value.isDouble())
                    properties.setBaseValue(key, PropertyInfo(static_cast<qreal>(value.toDouble())));
                break;
            case PropertyInfo::BOOL:
                if (value.isBool())
                    properties.setBaseValue(key, PropertyInfo(value.toBool()));
                break;
            case PropertyInfo::INVALID:
                break;
            }
        }
    }

    const QString tipImage = preset->value("tipImage").toString();
    if (!tool.setTipImage(tipImage).ok())
    {
        qWarning() << "Brush preset" << name << "uses the generated tip: cannot load" << tipImage;
        tool.setTipImage(QString());
    }
    return true;
}

QString BrushPresets::description(const QString& name) const
{
    if (!isValidName(name))
        return QString();
    const std::optional<QJsonObject> preset = readPreset(filePath(name));
    return preset ? preset->value("description").toString() : QString();
}

bool BrushPresets::rename(const QString& from, const QString& to) const
{
    if (!isValidName(from) || !isValidName(to) || QFile::exists(filePath(to)))
        return false;
    return QFile::rename(filePath(from), filePath(to));
}

bool BrushPresets::remove(const QString& name) const
{
    return isValidName(name) && QFile::remove(filePath(name));
}

bool BrushPresets::isValidName(const QString& name)
{
    static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
    if (name.trimmed() != name || name.isEmpty() || name.startsWith('.') || name.size() > 100)
        return false;
    for (const QChar c : name)
    {
        if (c.unicode() < 0x20 || forbidden.contains(c))
            return false;
    }
    return true;
}

QString BrushPresets::filePath(const QString& name) const
{
    return QDir(mFolder).filePath(name + ".json");
}
