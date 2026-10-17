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

#ifndef COMMANDLINEEXPORTER_H
#define COMMANDLINEEXPORTER_H

#include <optional>
#include <QTextStream>
#include "pagewriter.h"
#include "pencilerror.h"

class Editor;
class LayerCamera;

/**
 * Handles command line export jobs
 */
class CommandLineExporter : QObject
{
    Q_OBJECT

public:
    /**
     * Creates a new exporter instance.
     *
     * @param editor The Editor instance to be used by the exporter
     */
    explicit CommandLineExporter(Editor *editor);

    /**
     * Exports a Pencil2D file according to the specified options.
     *
     * @param inputPath Path of the file to export
     * @param outputPaths At least one export destination file path
     * @param camera Name of the camera layer to use, or an empty string to select one automatically
     * @param width Width to use for export, or a negative value to use the dimensions from the camera layer
     * @param height Height to use for export, or a negative value to use the dimensions from the camera layer
     * @param startFrame First frame to include in the export(s)
     * @param endFrame Last frame to include in the export(s) or -1 to use the last keyframe or -2 to use the last
     *                 keyframe including sound clips
     * @param transparency Whether to export with transparency
     * @param paper The paper size to export on (PDF or PNG pages), or none
     * @return Status::OK if every export succeeded, otherwise the first error
     */
    Status process(const QString &inputPath,
                   const QStringList &outputPaths,
                   const QString &camera,
                   int width,
                   int height,
                   int startFrame,
                   int endFrame,
                   bool transparency,
                   std::optional<PageSpec::Size> paper = std::nullopt);

private:
    Editor *mEditor;
    QTextStream mOut;
    QTextStream mErr;
    Status exportMovie(const QString &outputPath,
                       const LayerCamera *cameraLayer,
                       const QSize &exportSize,
                       int startFrame,
                       int endFrame,
                       bool transparency);
    Status exportImageSequence(const QString &outputPath,
                               const QString &format,
                               const LayerCamera *cameraLayer,
                               const QSize &exportSize,
                               int startFrame,
                               int endFrame,
                               bool transparency);
    void printError(const Status &st);
};

#endif // COMMANDLINEEXPORTER_H
