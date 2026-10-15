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
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "miniz.h"
#include "stroketool.h"
#include "tiledbuffer.h"
#include "toolproperties.h"
#include "util.h"

namespace
{

// Preset files are small; a larger file is not a preset.
constexpr qint64 MAX_PRESET_BYTES = 1 << 20;
// Bounds a tip file in a .jabrush before it is unpacked to memory.
constexpr qint64 MAX_TIP_BYTES = 64 << 20;

// A .jabrush holds preset.json and at most one tip entry of this form.
const QRegularExpression& tipEntryPattern()
{
    static const QRegularExpression re(QStringLiteral(R"(^tip\.[A-Za-z0-9]{1,5}$)"));
    return re;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(bytes);
    return file.commit();
}

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

bool BrushPresets::save(const QString& name, StrokeTool& tool, const QString& description) const
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

    QJsonObject preset{ { "settings", settings }, { "tipImage", tool.tipImagePath() } };
    if (!tool.pressureCurve().isEmpty())
        preset.insert("pressureCurve", tool.pressureCurve());
    if (!description.isEmpty())
        preset.insert("description", description);
    return writeFile(filePath(name), QJsonDocument(preset).toJson());
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
    tool.setPressureCurve(preset->value("pressureCurve").toString());
    return true;
}

QString BrushPresets::description(const QString& name) const
{
    if (!isValidName(name))
        return QString();
    const std::optional<QJsonObject> preset = readPreset(filePath(name));
    return preset ? preset->value("description").toString() : QString();
}

bool BrushPresets::addClassic() const
{
    if (QFile::exists(filePath("Classic")))
        return true;
    if (!QDir().mkpath(mFolder))
        return false;
    // Missing keys give each tool's default; only these differ from the defaults.
    const QJsonObject settings{ { "Flow", 50 }, { "FlowPerDab", true }, { "PressureEnabled", true } };
    QSaveFile file(filePath("Classic"));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(QJsonObject{ { "settings", settings }, { "tipImage", "" } }).toJson());
    return file.commit();
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

Status BrushPresets::exportPreset(const QString& name, const QString& fileName) const
{
    const Status failed(Status::FAIL, QObject::tr("Cannot export the brush preset %1 to %2.").arg(name, fileName));
    const std::optional<QJsonObject> preset = isValidName(name) ? readPreset(filePath(name)) : std::nullopt;
    if (!preset)
        return failed;

    QJsonObject out{ { "name", name }, { "settings", preset->value("settings") }, { "tipImage", QString() },
                     { "pressureCurve", preset->value("pressureCurve").toString() },
                     { "description", preset->value("description").toString() } };
    QByteArray tipEntry, tipData;
    const QString tipPath = preset->value("tipImage").toString();
    if (!tipPath.isEmpty())
    {
        tipEntry = "tip." + QFileInfo(tipPath).suffix().toLower().toUtf8();
        QFile tip(tipPath);
        if (!tipEntryPattern().match(QString::fromUtf8(tipEntry)).hasMatch() || !tip.open(QIODevice::ReadOnly) || tip.size() > MAX_TIP_BYTES)
            return failed;
        tipData = tip.readAll();
        out["tipImage"] = QString::fromUtf8(tipEntry);
    }
    const QByteArray json = QJsonDocument(out).toJson();

    mz_zip_archive mz;
    mz_zip_zero_struct(&mz);
    if (!mz_zip_writer_init_heap(&mz, 0, 0))
        return failed;
    void* buffer = nullptr;
    size_t size = 0;
    bool ok = mz_zip_writer_add_mem(&mz, "preset.json", json.constData(), size_t(json.size()), MZ_BEST_SPEED);
    if (ok && !tipEntry.isEmpty())
        ok = mz_zip_writer_add_mem(&mz, tipEntry.constData(), tipData.constData(), size_t(tipData.size()), MZ_BEST_SPEED);
    ok = ok && mz_zip_writer_finalize_heap_archive(&mz, &buffer, &size);
    mz_zip_writer_end(&mz);
    const QByteArray zip = ok ? QByteArray(static_cast<const char*>(buffer), qsizetype(size)) : QByteArray();
    mz_free(buffer);
    if (!ok || !writeFile(fileName, zip))
        return failed;
    return Status::OK;
}

Status BrushPresets::importPreset(const QString& fileName) const
{
    const Status bad(Status::FAIL, QObject::tr("This file is not a valid brush preset: %1").arg(fileName));

    // Read and check every entry in memory; nothing is written until the whole file passes.
    mz_zip_archive mz;
    mz_zip_zero_struct(&mz);
    if (!mz_zip_reader_init_file(&mz, fileName.toUtf8().constData(), 0))
        return bad;
    OnScopeExit(mz_zip_reader_end(&mz));

    bool hasPreset = false;
    QByteArray json, tipData;
    QString tipEntry;
    const mz_uint count = mz_zip_reader_get_num_files(&mz);
    for (mz_uint i = 0; i < count; ++i)
    {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&mz, i, &stat) || stat.m_is_directory)
            return bad;
        const QString entry = QString::fromUtf8(stat.m_filename);
        const bool isPreset = entry == QLatin1String("preset.json");
        if (isPreset ? hasPreset : (!tipEntry.isEmpty() || !tipEntryPattern().match(entry).hasMatch()))
            return bad; // unknown, duplicate or path-escaping entry
        if (stat.m_uncomp_size > mz_uint64(isPreset ? MAX_PRESET_BYTES : MAX_TIP_BYTES))
            return bad;
        QByteArray data(qsizetype(stat.m_uncomp_size), Qt::Uninitialized);
        if (!mz_zip_reader_extract_to_mem(&mz, i, data.data(), size_t(data.size()), 0))
            return bad;
        if (isPreset)
        {
            hasPreset = true;
            json = data;
        }
        else
        {
            tipEntry = entry;
            tipData = data;
        }
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    const QJsonObject preset = doc.object();
    const QString name = preset.value("name").toString();
    if (!hasPreset || error.error != QJsonParseError::NoError || !doc.isObject() || !isValidName(name) ||
        preset.value("tipImage").toString() != tipEntry)
        return bad;
    if (QFile::exists(filePath(name)))
        return Status(Status::FAIL, QObject::tr("A brush preset named %1 exists.").arg(name));

    // The tip loader reads files, so the tip is checked from a temporary copy.
    QString tipPath;
    if (!tipEntry.isEmpty())
    {
        QTemporaryDir temp;
        QImage mask;
        if (!temp.isValid() || !writeFile(temp.filePath(tipEntry), tipData) || !loadBrushTipImage(temp.filePath(tipEntry), mask).ok())
            return bad;
        tipPath = QDir(mFolder).filePath("tips/" + name + "." + QFileInfo(tipEntry).suffix());
    }

    const QJsonObject stored{ { "settings", preset.value("settings").toObject() }, { "tipImage", tipPath },
                              { "pressureCurve", preset.value("pressureCurve").toString() },
                              { "description", preset.value("description").toString() } };
    if (!QDir().mkpath(QDir(mFolder).filePath("tips")) ||
        (!tipPath.isEmpty() && !writeFile(tipPath, tipData)) ||
        !writeFile(filePath(name), QJsonDocument(stored).toJson()))
        return Status(Status::FAIL, QObject::tr("Cannot write the brush preset %1.").arg(name));
    return Status::OK;
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
