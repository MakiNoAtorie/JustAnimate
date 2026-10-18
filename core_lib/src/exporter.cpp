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
#include <future>
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
#include "util.h"

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

// MovieExporter writes to a temporary file and renames it over the target only on success.
Status runMovie(const Object& object, const ExportRequest& r, const std::function<bool(int, int)>& progress)
{
    ExportMovieDesc desc;
    desc.strFileName = r.path;
    desc.startFrame = r.startFrame;
    desc.endFrame = r.endFrame;
    desc.fps = r.fps;
    desc.exportSize = r.size;
    desc.strCameraName = r.cameraName;
    desc.loop = r.loop;
    desc.alpha = r.transparency;
    desc.quality = r.quality;
    desc.gifColors = r.gifColors;
    desc.gifDither = r.gifDither;

    MovieExporter ex;
    // The overall fraction is the sub-task's start plus its share of the sub-task's length.
    float start = 0.f;
    float length = 0.f;
    auto report = [&](float f) { if (progress && !progress(int(f * 1000), 1000)) { ex.cancel(); } };
    const Status st = ex.run(&object, desc,
        [&](float f, float next) { start = f; length = qMax(0.f, next - f); report(f); },
        [&](float f) { report(start + f * length); },
        [](const QString&) {});
    if (st.ok() && r.skippedClips && !ex.skippedClips().isEmpty()) { r.skippedClips(ex.skippedClips()); }
    return st;
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
        targets << (r.kind == Kind::Movie ? r.path : withExtension(r.path, format.extension));
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
    if (r.kind == Kind::Movie) { return runMovie(object, r, progress); }

    // Each file is written to "<name>.tmp.<ext>" and renamed over its target only when the whole run succeeds.
    std::vector<std::pair<QString, QString>> written; // temporary, target
    auto discard = [&written] { for (const auto& w : written) { QFile::remove(w.first); } };
    // Runs on any thread: touches only its own files.
    auto encode = [name = format.name](const QImage& image, const QString& tmp, const QString& target) -> Status
    {
        QFile::remove(tmp);
        QImageWriter writer(tmp, name);
        if (writer.write(image)) { return Status::OK; }
        return failure("cannot write image", target, QString("%1 (code %2)").arg(writer.errorString()).arg(int(writer.error())));
    };
    auto temporary = [&written](const QString& target)
    {
        const QFileInfo info(target);
        written.emplace_back(info.dir().filePath(info.completeBaseName() + ".tmp." + info.suffix()), target);
        return written.back().first;
    };

    // Object is not thread-safe, so frames render here; one chunk encodes on the pool while the next renders.
    struct Job { QImage image; QString tmp; QString target; };
    std::vector<Job> chunk;
    std::future<Status> encoding;
    auto finishEncoding = [&encoding]() -> Status { return encoding.valid() ? encoding.get() : Status(Status::OK); };
    auto startEncoding = [&]() -> Status
    {
        const Status st = finishEncoding();
        if (!st.ok() || chunk.empty()) { return st; }
        encoding = std::async(std::launch::async, [jobs = std::move(chunk), &encode]() {
            std::vector<Status> results(jobs.size(), Status::OK);
            parallelFor(int(jobs.size()), [&](int k) { results[k] = encode(jobs[k].image, jobs[k].tmp, jobs[k].target); });
            for (const Status& s : results) { if (!s.ok()) { return s; } }
            return Status(Status::OK);
        });
        chunk.clear();
        return Status::OK;
    };
    auto fail = [&](const Status& st) { finishEncoding(); discard(); return st; };

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
            chunk.push_back({ image, temporary(targets[i]), targets[i] });
            if (int(chunk.size()) == parallelWorkerCount())
            {
                const Status st = startEncoding();
                if (!st.ok()) { return fail(st); }
            }
        }
        if (progress && !progress(i + 1, total)) { return fail(Status::CANCELED); }
    }
    Status encoded = startEncoding();
    if (encoded.ok()) { encoded = finishEncoding(); }
    if (!encoded.ok()) { return fail(encoded); }
    if (r.kind == Kind::SpriteSheet)
    {
        const Status st = encode(sheet, temporary(targets.front()), targets.front());
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
