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

#include <QFileInfo>
#include <QTextStream>

#include "editor.h"
#include "exporter.h"
#include "fileformat.h"
#include "layercamera.h"
#include "layermanager.h"
#include "object.h"
#include "pagewriter.h"
#include "paperpainter.h"
#include "playbackmanager.h"
#include "projectsession.h"

#include "commandlineexporter.h"

const auto qEndl = Qt::endl;

CommandLineExporter::CommandLineExporter(Editor *editor) :
    mEditor(editor),
    mOut(stdout, QIODevice::WriteOnly),
    mErr(stderr, QIODevice::WriteOnly)
{

}

Status CommandLineExporter::process(const QString &inputPath,
                                    const QStringList &outputPaths,
                                    const QString &camera,
                                    int width,
                                    int height,
                                    int startFrame,
                                    int endFrame,
                                    bool transparency,
                                    std::optional<PageSpec::Size> paper)
{
    LayerManager *layerManager = mEditor->layers();

    if(inputPath.isEmpty())
    {
        mErr << tr("Error: No input file specified. An input project file argument is required when output path(s) are specified.") << qEndl;
        return Status::FAIL;
    }

    Status s = ProjectSession(mEditor, nullptr).load(inputPath);
    if (!s.ok())
    {
        printError(s);
        return s;
    }

    const LayerCamera* cameraLayer = Exporter::camera(*mEditor->object(), camera);
    if (!camera.isEmpty() && cameraLayer && cameraLayer->name() != camera)
    {
        mErr << tr("Warning: the specified camera layer %1 was not found, ignoring.").arg(camera) << qEndl;
    }
    if (cameraLayer == nullptr)
    {
        mErr << tr("Error: the project has no camera layer.", "Command line error") << qEndl;
        return Status::FAIL;
    }

    if (width < 0)
    {
        width = cameraLayer->getViewRect().width();
    }
    if (height < 0)
    {
        height = cameraLayer->getViewRect().height();
    }
    QSize exportSize(width, height);

    Q_ASSERT(startFrame >= 1);
    if (endFrame < 0)
    {
        endFrame = layerManager->animationLength(endFrame < -1);
    }

    if (endFrame < startFrame)
    {
        mErr << tr("Error: Nothing to export. The frame range %1-%2 is empty.", "Command line error").arg(startFrame).arg(endFrame) << qEndl;
        return Status::INVALID_ARGUMENT;
    }

    Q_ASSERT(!outputPaths.empty());
    for (const QString& outputPath : outputPaths)
    {
        if (paper)
        {
            mOut << tr("Exporting on paper...", "Command line task progress") << qEndl;
            PageSpec spec;
            spec.size = *paper;
            const Status st = writePages(outputPath, spec, endFrame - startFrame + 1,
                                         paperFramePages(*mEditor->object(), *cameraLayer, startFrame));
            if (!st.ok())
            {
                printError(st);
                return st;
            }
            mOut << tr("Done.", "Command line task done") << qEndl;
            continue;
        }

        // Detect format
        QString format = detectFormatByFileNameExtension(outputPath);
        if (format.isNull())
        {
            mErr << tr("Warning: Output format is not specified or unsupported. Using PNG.", "Command line warning") << qEndl;
            format = "PNG";
        }

        ExportRequest request;
        request.cameraName = cameraLayer->name();
        request.startFrame = startFrame;
        request.endFrame = endFrame;
        request.size = exportSize;
        request.path = outputPath;
        if (isMovieFormat(format))
        {
            if (transparency)
            {
                mErr << tr("Warning: Transparency is not currently supported in movie files", "Command line warning") << qEndl;
            }
            mOut << tr("Exporting movie...", "Command line task progress") << qEndl;
            request.kind = ExportRequest::Kind::Movie;
            request.fps = mEditor->playback()->fps();
            request.skippedClips = [this](const QStringList& clips) {
                for (const QString& clip : clips)
                {
                    mErr << tr("Warning: sound clip left out, file is missing: %1", "Command line warning").arg(clip) << qEndl;
                }
            };
        }
        else
        {
            mOut << tr("Exporting image sequence...", "Command line task progress") << qEndl;
            request.kind = ExportRequest::Kind::Sequence;
            request.format = format;
            request.transparency = transparency;
        }
        const Status st = Exporter::run(*mEditor->object(), request);
        if (!st.ok())
        {
            printError(st);
            return st;
        }
        mOut << tr("Done.", "Command line task done") << qEndl;
    }

    return Status::OK;
}

void CommandLineExporter::printError(const Status &st)
{
    mErr << tr("Error: export failed (code %1).", "Command line error").arg(static_cast<int>(st.code())) << qEndl;
    if (st.code() == Status::ERROR_FFMPEG_NOT_FOUND) mErr << tr("ffmpeg was not found; it is needed for movie export.", "Command line error") << qEndl;
    if (!st.title().isEmpty()) mErr << st.title() << qEndl;
    if (!st.description().isEmpty()) mErr << st.description() << qEndl;
    const QString details = st.details().str();
    if (!details.isEmpty()) mErr << details << qEndl;
}
