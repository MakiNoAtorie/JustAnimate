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
#include "exporter.h"

#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainter>

#include "framerenderer.h"
#include "layer.h"
#include "layercamera.h"
#include "object.h"

namespace
{
struct ImageFormat
{
    const char* name;      // QImageWriter format
    const char* extension; // with the dot
    bool alpha;
};

// Unknown formats are PNG.
ImageFormat imageFormat(const QString& format)
{
    const QString f = format.toUpper();
    if (f == "JPG" || f == "JPEG") { return { "JPG", ".jpg", false }; }
    if (f == "TIFF" || f == "TIF") { return { "TIFF", ".tiff", true }; }
    if (f == "BMP") { return { "BMP", ".bmp", false }; }
    if (f == "WEBP") { return { "WEBP", ".webp", true }; }
    return { "PNG", ".png", true };
}

QString withExtension(const QString& path, const QString& extension)
{
    return path.endsWith(extension, Qt::CaseInsensitive) ? path : path + extension;
}

Status failure(const QString& what, const QString& path, const QString& error)
{
    DebugDetails dd;
    dd << QString("Exporter: %1").arg(what);
    dd << QString("&nbsp;&nbsp;filePath: ").append(path);
    dd << QString("&nbsp;&nbsp;Error: ").append(error);
    return Status(Status::FAIL, dd);
}
}

const LayerCamera* Exporter::camera(const Object& object, const QString& name)
{
    if (auto camera = static_cast<const LayerCamera*>(object.findLayerByName(name, Layer::CAMERA))) { return camera; }
    const auto cameras = object.getLayersByType<LayerCamera>();
    return cameras.empty() ? nullptr : cameras.front();
}

Status Exporter::run(const Object& object, const ExportRequest& r, const std::function<bool(int, int)>& progress)
{
    const LayerCamera* cam = camera(object, r.cameraName);
    if (cam == nullptr) { return failure("no camera layer", r.path, r.cameraName); }

    using Kind = ExportRequest::Kind;
    const ImageFormat format = r.kind == Kind::SpriteSheet ? imageFormat("PNG") : imageFormat(r.format);
    const FrameRenderer::ExportOptions options{ r.size, r.transparency && format.alpha, true };

    std::vector<int> frames;
    const Layer* keyLayer = r.keyframesOnly ? object.findLayerByName(r.keyLayerName) : nullptr;
    for (int f = r.startFrame; f <= (r.kind == Kind::Image ? r.startFrame : r.endFrame); ++f)
    {
        if (keyLayer == nullptr || keyLayer->keyExists(f)) { frames.push_back(f); }
    }

    QStringList targets;
    if (r.kind == Kind::Sequence)
    {
        QString prefix = r.path;
        if (prefix.endsWith(format.extension, Qt::CaseInsensitive)) { prefix.chop(int(strlen(format.extension))); }
        for (int f : frames) { targets << prefix + QString("%1").arg(f, 4, 10, QChar('0')) + format.extension; }
    }
    else
    {
        targets << withExtension(r.path, format.extension);
    }

    if (r.confirmOverwrite)
    {
        for (const QString& t : targets)
        {
            if (QFile::exists(t))
            {
                if (!r.confirmOverwrite(t)) { return Status::CANCELED; }
                break;
            }
        }
    }

    // Each file is written to "<name>.tmp.<ext>" and renamed over its target only when the whole run succeeds.
    std::vector<std::pair<QString, QString>> written; // temporary, target
    auto discard = [&written] { for (const auto& w : written) { QFile::remove(w.first); } };
    auto write = [&](const QImage& image, const QString& target) -> Status
    {
        const QFileInfo info(target);
        const QString tmp = info.dir().filePath(info.completeBaseName() + ".tmp." + info.suffix());
        QFile::remove(tmp);
        written.emplace_back(tmp, target);
        QImageWriter writer(tmp, format.name);
        if (writer.write(image)) { return Status::OK; }
        return failure("cannot write image", target, QString("%1 (code %2)").arg(writer.errorString()).arg(int(writer.error())));
    };

    const int total = int(frames.size());
    QImage sheet;
    Object::SpriteSheetGrid grid;
    if (r.kind == Kind::SpriteSheet)
    {
        grid = Object::spriteSheetGrid(total, r.columns);
        sheet = QImage(r.size.width() * grid.columns, r.size.height() * grid.rows, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(options.transparency ? QColor(Qt::transparent) : QColor(Qt::white)); // palette-exempt: export background
    }
    for (int i = 0; i < total; ++i)
    {
        const QImage image = FrameRenderer::renderForExport(object, frames[i], *cam, options);
        if (r.kind == Kind::SpriteSheet)
        {
            QPainter(&sheet).drawImage(grid.cell(i, r.size).topLeft(), image);
        }
        else
        {
            const Status st = write(image, targets[i]);
            if (!st.ok()) { discard(); return st; }
        }
        if (progress && !progress(i + 1, total)) { discard(); return Status::CANCELED; }
    }
    if (r.kind == Kind::SpriteSheet)
    {
        const Status st = write(sheet, targets.front());
        if (!st.ok()) { discard(); return st; }
    }

    for (size_t i = 0; i < written.size(); ++i)
    {
        std::error_code error;
        std::filesystem::rename(QFileInfo(written[i].first).filesystemFilePath(), QFileInfo(written[i].second).filesystemFilePath(), error);
        if (error)
        {
            // shortcut: frames renamed before the failure stay; a rename failure after every write succeeded is rare.
            for (size_t j = i; j < written.size(); ++j) { QFile::remove(written[j].first); }
            return failure("cannot replace file", written[i].second, QString::fromStdString(error.message()));
        }
    }
    return Status::OK;
}
