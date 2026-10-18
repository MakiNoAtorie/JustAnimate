/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef MOVIEEXPORTER_H
#define MOVIEEXPORTER_H

#include <functional>
#include <QCoreApplication>
#include <QString>
#include <QSize>
#include <QTemporaryDir>
#include "pencilerror.h"

class Object;
class QImage;

enum class MovieQuality { Low, Medium, High };

struct ExportMovieDesc
{
    QString strFileName;
    int     startFrame = 0;
    int     endFrame = 0;
    //int     videoFps   = 30;
    int     fps = 12;
    QSize   exportSize{ 0, 0 };
    QString strCameraName;
    bool loop = false;
    bool alpha = false;
    MovieQuality quality = MovieQuality::Medium;
    int gifColors = 256; // 3..256
    bool gifDither = true;
};

class MovieExporter
{
    Q_DECLARE_TR_FUNCTIONS(MovieExporter)
public:
    MovieExporter();
    ~MovieExporter();

    Status run(const Object* obj,
               const ExportMovieDesc& desc,
               std::function<void(float, float)> majorProgress,
               std::function<void(float)> minorProgress,
               std::function<void(QString)> progressMessage);
    QString error();
    // The frame as the export adapter renders it through desc's camera; GIF frames are opaque.
    static QImage frameImage(const Object& obj, const ExportMovieDesc& desc, int frame);
    // "<layer> frame <n>" of each sound clip the last run left out because its file is missing.
    QStringList skippedClips() const { return mSkippedClips; }

    struct AudioMix { QStringList args; QStringList skipped; };
    // ffmpeg args that mix the visible sound clips into outPath; args is empty when no clip has a file.
    static AudioMix audioMix(const Object* obj, int startFrame, int endFrame, int fps, const QString& outPath);

    void cancel() { mCanceled = true; }

    // The maximum number of sound frames that can be successfully rendered
    static const int MAX_SOUND_FRAMES = 63;

    // Calls write with a temporary path next to target (same extension) and renames it over
    // target only if write succeeds; otherwise removes it and leaves target unchanged.
    static Status writeThenReplace(const QString& target, const std::function<Status(const QString&)>& write);
    // True if path is free or confirm() accepts overwriting it.
    static bool mayOverwrite(const QString& path, const std::function<bool()>& confirm);

    // Extra ffmpeg output args for the quality preset of an MP4 or WebM file; empty for other types.
    static QStringList qualityArgs(const QString& path, MovieQuality quality);
    // The ffmpeg -filter_complex graph for GIF export.
    static QString gifFilter(int colors, bool dither);

    static Status executeFFmpeg(const QString& cmd, const QStringList& args, std::function<bool(int)> progress);

    // Most frames left unwritten in ffmpeg's stdin buffer; the renderer waits beyond this.
    static const int MAX_QUEUED_FRAMES = 4;
    // Pipes frames from nextFrame (false when done) into ffmpeg's stdin.
    Status executeFFMpegPipe(const QString& cmd, const QStringList& args, std::function<void(float)> progress, std::function<bool(QImage&)> nextFrame);
    // Peak unwritten stdin bytes of the last executeFFMpegPipe run.
    qint64 peakQueuedBytes() const { return mPeakQueuedBytes; }
private:
    Status assembleAudio(const Object* obj, QString ffmpegPath, std::function<void(float)> progress);
    Status generateMovie(const Object *obj, QString ffmpegPath, QString strOutputFile, std::function<void(float)> progress);
    Status generateGif(const Object *obj, QString ffmpeg, QString strOut, std::function<void(float)>  progress);

    Status checkInputParameters(const ExportMovieDesc&);

private:
    QTemporaryDir mTempDir;
    QString mTempWorkDir;
    ExportMovieDesc mDesc;
    QStringList mSkippedClips;
    bool mCanceled = false;
    qint64 mPeakQueuedBytes = 0;
};

#endif // MOVIEEXPORTER_H
