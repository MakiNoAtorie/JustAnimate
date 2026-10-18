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

#include "movieexporter.h"
#include "exporter.h"
#include "framerenderer.h"

#include <ctime>
#include <vector>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QApplication>
#include <QStandardPaths>
#include <QThread>
#include <QtMath>
#include <QRegularExpression>

#include "object.h"
#include "layercamera.h"
#include "layersound.h"
#include "soundclip.h"
#include "util.h"

using Qt::SplitBehaviorFlags;

MovieExporter::MovieExporter()
{
}

MovieExporter::~MovieExporter()
{
}

/** Begin exporting the movie described by exportDesc.
 *
 * @param[in] obj An Object containing the animation to export.
 * @param[in] desc A structure containing all the export parameters.
 *            See ExportMovieDesc.
 * @param[out] majorProgress A function to update the major progress bar.
 *                          The major progress bar goes from 0-100% only
 *                          one time, representing the overall progress of
 *                          the export. The first float parameter is
 *                          the current progress %, and the second is
 *                          the desired progress when the next sub-task
 *                          completes. This should only be called at the
 *                          beginning of a subtask.
 * @param[out] minorProgress A function to update the minor progress bar.
 *                           The minor progress bar goes from 0-100% for
 *                           each sub-task of the exporting process.
 *                           It is up to minor progress to update the
 *                           major progress bar to reflect the sub-task
 *                           progress.
 * @param[out] progressMessage A function ot update the progres bar
 *                             message. The messages will describe
 *                             the current sub-task of the exporting
 *                             process.
 *
 * @return Returns Status:OK on success, or Status::FAIL on error.
 */
Status MovieExporter::run(const Object* obj,
                          const ExportMovieDesc& desc,
                          std::function<void(float, float)> majorProgress,
                          std::function<void(float)> minorProgress,
                          std::function<void(QString)> progressMessage)
{
    majorProgress(0.f, 0.03f);
    minorProgress(0.f);
    progressMessage(tr("Checking environment..."));

    clock_t t1 = clock();

    QString ffmpegPath = ffmpegLocation();
    qDebug() << ffmpegPath;
    if (!QFile::exists(ffmpegPath))
    {
#ifdef _WIN32
        qCritical() << "Please place ffmpeg.exe in " << ffmpegPath << " directory";
#else
        qCritical() << "Please place ffmpeg in " << ffmpegPath << " directory";
#endif
        return Status::ERROR_FFMPEG_NOT_FOUND;
    }

    STATUS_CHECK(checkInputParameters(desc))
    mDesc = desc;
    mSkippedClips.clear();

    qDebug() << "OutFile: " << mDesc.strFileName;

    // Setup temporary folder
    if (!mTempDir.isValid())
    {
        Status status = Status::FAIL;
        status.setTitle(tr("Error creating folder"));
        status.setDescription(tr("Unable to create a temporary folder, cannot export animation."));
        DebugDetails dd;
        dd << QString("Parent directory: ").append(QDir::tempPath())
           << QString("Error: ").append(mTempDir.errorString());
        status.setDetails(dd);
        return status;
    }

    mTempWorkDir = mTempDir.path();

    minorProgress(0.f);
    const Status result = writeThenReplace(desc.strFileName, [&](const QString& outFile) -> Status
    {
        if (desc.strFileName.endsWith("gif", Qt::CaseInsensitive))
        {
            majorProgress(0.03f, 1.f);
            progressMessage(tr("Generating GIF..."));
            minorProgress(0.f);
            return generateGif(obj, ffmpegPath, outFile, minorProgress);
        }
        majorProgress(0.03f, 0.25f);
        progressMessage(tr("Assembling audio..."));
        minorProgress(0.f);
        STATUS_CHECK(assembleAudio(obj, ffmpegPath, minorProgress));
        minorProgress(1.f);
        majorProgress(0.25f, 1.f);
        progressMessage(tr("Generating movie..."));
        return generateMovie(obj, ffmpegPath, outFile, minorProgress);
    });
    if (!result.ok()) { return result; }
    minorProgress(1.f);
    majorProgress(1.f, 1.f);
    progressMessage(tr("Done"));

    clock_t t2 = clock() - t1;
    qDebug("MOVIE = %.1f sec", static_cast<double>(t2 / CLOCKS_PER_SEC));

    return Status::OK;
}

Status MovieExporter::writeThenReplace(const QString& target, const std::function<Status(const QString&)>& write)
{
    const QFileInfo info(target);
    // Keep the extension: ffmpeg picks the output format from it.
    const QString tmp = info.dir().filePath(info.completeBaseName() + ".tmp." + info.suffix());
    QFile::remove(tmp);

    const Status st = write(tmp);
    if (!st.ok())
    {
        QFile::remove(tmp);
        return st;
    }

    std::error_code renameError;
    std::filesystem::rename(QFileInfo(tmp).filesystemFilePath(), info.filesystemFilePath(), renameError);
    if (renameError)
    {
        QFile::remove(tmp);
        DebugDetails dd;
        dd << QString("Error: Unable to rename %1 to %2: %3").arg(tmp, target, QString::fromStdString(renameError.message()));
        return Status(Status::FAIL, dd, tr("Something went wrong"),
                      tr("Failed to replace \"%1\". Please make sure you have sufficient permissions.").arg(target));
    }
    return st;
}

QImage MovieExporter::frameImage(const Object& obj, const ExportMovieDesc& desc, int frame)
{
    const LayerCamera* camera = Exporter::camera(obj, desc.strCameraName);
    const bool transparency = desc.alpha && !desc.strFileName.endsWith("gif", Qt::CaseInsensitive);
    return FrameRenderer::renderForExport(obj, frame, *camera, { desc.exportSize, transparency, true });
}

QString MovieExporter::error()
{
    return QString();
}

/** Combines all audio tracks in obj into a single file.
 *
 *  @param[in] obj
 *  @param[in] ffmpegPath
 *  @param[out] progress A function that takes one float argument
 *              (the percentage of the audio assembly complete) and
 *              may display the output to the user in any way it
 *              sees fit.
 *
 *  @return Returns the final status of the operation. Ok if successful,
 *          or safe if there was intentionally no output.
 */
Status MovieExporter::assembleAudio(const Object* obj,
                                    QString ffmpegPath,
                                    std::function<void(float)> progress)
{
    Q_ASSERT(mDesc.startFrame >= 0);
    Q_ASSERT(mDesc.endFrame >= mDesc.startFrame);

    QDir dir(mTempWorkDir);
    Q_ASSERT(dir.exists());

    QString tempAudioPath = QDir(mTempWorkDir).filePath("tmpaudio.wav");
    qDebug() << "TempAudio=" << tempAudioPath;

    if (mCanceled)
    {
        return Status::CANCELED;
    }

    const AudioMix mix = audioMix(obj, mDesc.startFrame, mDesc.endFrame, mDesc.fps, tempAudioPath);
    mSkippedClips = mix.skipped;
    if (mix.args.isEmpty()) return Status::SAFE;
    const QStringList& args = mix.args;

    STATUS_CHECK(MovieExporter::executeFFmpeg(ffmpegPath, args, [&progress, this] (int frame) { progress(frame / static_cast<float>(mDesc.endFrame - mDesc.startFrame)); return !mCanceled; }))
    qDebug() << "audio file: " + tempAudioPath;

    return Status::OK;
}

MovieExporter::AudioMix MovieExporter::audioMix(const Object* obj, int startFrame, int endFrame, int fps, const QString& outPath)
{
    AudioMix mix;
    std::vector< SoundClip* > allSoundClips;

    std::vector< LayerSound* > allSoundLayers = obj->getLayersByType<LayerSound>();
    for (LayerSound* layer : allSoundLayers)
    {
        if (!layer->visible()) { continue; }
        layer->foreachKeyFrame([&](KeyFrame* key)
        {
            SoundClip* clip = static_cast<SoundClip*>(key);
            if (clip->fileName().isEmpty()) { return; }
            if (clip->isFileMissing())
            {
                mix.skipped << layer->clipLabel(key);
                return;
            }
            allSoundClips.push_back(clip);
        });
    }

    if (allSoundClips.empty()) return mix;

    int clipCount = 0;

    QString filterComplex, amergeInput, panChannelLayout;
    QStringList& args = mix.args;

    int wholeLen = qCeil(endFrame * 44100.0 / fps);
    for (auto clip : allSoundClips)
    {
        // Add sound file as input
        args << "-i" << clip->fileName();

        // Offset the sound to its correct position
        // See https://superuser.com/questions/716320/ffmpeg-placing-audio-at-specific-location
        filterComplex += QString("[%1:a:0] aformat=sample_fmts=fltp:sample_rates=44100:channel_layouts=mono,volume=1,adelay=%2S|%2S,apad=whole_len=%3[ad%1];")
                    .arg(clipCount).arg(qRound(44100.0 * (clip->pos() - 1) / fps)).arg(wholeLen);
        amergeInput += QString("[ad%1]").arg(clipCount);
        panChannelLayout += QString("c%1+").arg(clipCount);

        clipCount++;
    }
    // Remove final '+'
    panChannelLayout.chop(1);
    // Output arguments
    // Mix audio
    args << "-filter_complex";
    if (clipCount == 1)
    {
        // If there is only one sound clip there is no need to use amerge
        // Prior to ffmpeg 3.2, amerge does not support inputs=1
        filterComplex.chop(1); // Remove final semicolon since there are no more filters added after
        args << filterComplex << "-map" << amergeInput;
    }
    else {
        args << QString("%1%2 amerge=inputs=%3, pan=mono|c0=%4 [out]")
                .arg(filterComplex).arg(amergeInput).arg(clipCount).arg(panChannelLayout);
        args << "-map" << "[out]";
    }
    // Convert audio file: 44100Hz sampling rate, stereo, signed 16 bit little endian
    // Supported audio file types: wav, mp3, ogg... ( all file types supported by ffmpeg )
    args << "-ar" << "44100" << "-acodec" << "pcm_s16le" << "-ac" << "2" << "-y";
    // Trim audio
    args << "-ss" << QString::number((startFrame - 1) / static_cast<double>(fps));
    args << "-to" << QString::number(endFrame / static_cast<double>(fps));
    // Output path
    args << outPath;

    return mix;
}

/** Exports obj to a movie image at strOut using FFmpeg.
 *
 *  @param[in]  obj An Object containing the animation to export.
 *  @param[in]  ffmpegPath The path to the FFmpeg binary.
 *  @param[in]  strOutputFile The output path. Should end with .gif.
 *  @param[out] progress A function that takes one float argument
 *              (the percentage of the gif generation complete) and
 *              may display the output to the user in any way it
 *              sees fit.
 *
 *  The movie formats supported by this operation are any file
 *  formats that the referenced FFmpeg binary supports and that have
 *  the required features (ex. video and audio support)
 *
 *  @return Returns the final status of the operation (ok or canceled)
 */
Status MovieExporter::generateMovie(
        const Object* obj,
        QString ffmpegPath,
        QString strOutputFile,
        std::function<void(float)> progress)
{
    if (mCanceled)
    {
        return Status::CANCELED;
    }

    // Frame generation setup

    int frameStart = mDesc.startFrame;
    int frameEnd = mDesc.endFrame;
    const QSize exportSize = mDesc.exportSize;
    bool loop = mDesc.loop;
    int currentFrame = frameStart;

    // Build FFmpeg command

    //int exportFps = mDesc.videoFps;
    const QString tempAudioPath = QDir(mTempWorkDir).filePath("tmpaudio.wav");

    QStringList args = {"-f", "rawvideo", "-pixel_format", "bgra"};
    args << "-video_size" << QString("%1x%2").arg(exportSize.width()).arg(exportSize.height());
    args << "-framerate" << QString::number(mDesc.fps);

    //args << "-r" << QString::number(exportFps);
    args << "-i" << "-";
    args << "-threads" << (QThread::idealThreadCount() == 1 ? "0" : QString::number(QThread::idealThreadCount()));

    if (QFile::exists(tempAudioPath))
    {
        args << "-i" << tempAudioPath;
    }

    if (strOutputFile.endsWith(".apng", Qt::CaseInsensitive))
    {
        args << "-plays" << (loop ? "0" : "1");
    }

    if (strOutputFile.endsWith(".mp4", Qt::CaseInsensitive))
    {
        args << "-pix_fmt" << "yuv420p";
    }
    args << qualityArgs(strOutputFile, mDesc.quality);

    if (strOutputFile.endsWith(".avi", Qt::CaseInsensitive))
    {
        args << "-q:v" << "5";
    }

    args << "-y";
    args << strOutputFile;

    // Run FFmpeg command

    Status status = executeFFMpegPipe(ffmpegPath, args, progress, [&](QImage& imageToExport)
    {
        if(currentFrame > frameEnd)
        {
            return false;
        }

        imageToExport = frameImage(*obj, mDesc, currentFrame);
        currentFrame++;
        return true;
    });
    STATUS_CHECK(status);

    return Status::OK;
}

/** Exports obj to a gif image at strOut using FFmpeg.
 *
 *  @param[in]  obj An Object containing the animation to export.
 *  @param[in]  ffmpegPath The path to the FFmpeg binary.
 *  @param[in]  strOut The output path. Should end with .gif.
 *  @param[out] progress A function that takes one float argument
 *              (the percentage of the gif generation complete) and
 *              may display the output to the user in any way it
 *              sees fit.
 *
 *  @return Returns the final status of the operation (ok or canceled)
 */
Status MovieExporter::generateGif(
        const Object* obj,
        QString ffmpegPath,
        QString strOut,
        std::function<void(float)> progress)
{

    if (mCanceled)
    {
        return Status::CANCELED;
    }

    // Frame generation setup

    int frameStart = mDesc.startFrame;
    int frameEnd = mDesc.endFrame;
    const QSize exportSize = mDesc.exportSize;
    bool loop = mDesc.loop;
    int currentFrame = frameStart;

    // Build FFmpeg command

    QStringList args = {"-f", "rawvideo", "-pixel_format", "bgra"};
    args << "-video_size" << QString("%1x%2").arg(exportSize.width()).arg(exportSize.height());
    args << "-framerate" << QString::number(mDesc.fps);

    args << "-i" << "-";

    args << "-y";

    args << "-filter_complex" << gifFilter(mDesc.gifColors, mDesc.gifDither);

    args << "-loop" << (loop ? "0" : "-1");
    args << strOut;

    // Run FFmpeg command

    Status status = executeFFMpegPipe(ffmpegPath, args, progress, [&](QImage& imageToExport)
    {
        if(currentFrame > frameEnd)
        {
            return false;
        }

        imageToExport = frameImage(*obj, mDesc, currentFrame);
        currentFrame++;
        return true;
    });
    STATUS_CHECK(status);

    return Status::OK;
}

/** Runs the specified command (should be ffmpeg) and allows for progress feedback.
 *
 *  @param[in]  cmd A string containing the command to execute
 *  @param[in]  args A string list containing the arguments to
 *              pass to the command
 *  @param[out] progress A function that takes one float argument
 *              (the percentage of the ffmpeg operation complete) and
 *              may display the output to the user in any way it
 *              sees fit.
 *
 *  executeFFMpeg does not allow for writing direct input, the only
 *  input through the "-i" argument to specify input files on the disk.
 *
 *  @return Returns Status::OK if everything went well, and Status::FAIL
 *  and error is detected (usually a non-zero exit code for ffmpeg).
 */
QStringList MovieExporter::qualityArgs(const QString& path, MovieQuality quality)
{
    const int i = static_cast<int>(quality);
    if (path.endsWith(".mp4", Qt::CaseInsensitive))
    {
        static const char* crf[] = { "28", "23", "18" };
        return { "-crf", crf[i] };
    }
    if (path.endsWith(".webm", Qt::CaseInsensitive))
    {
        static const char* crf[] = { "40", "32", "24" };
        return { "-crf", crf[i], "-b:v", "0" };
    }
    return {};
}

QString MovieExporter::gifFilter(int colors, bool dither)
{
    return QString("[0:v]palettegen=max_colors=%1 [p]; [0:v][p] paletteuse=dither=%2")
        .arg(qBound(3, colors, 256)).arg(dither ? "sierra2_4a" : "none");
}

Status MovieExporter::executeFFmpeg(const QString& cmd, const QStringList& args, std::function<bool(int)> progress)
{
    qDebug() << cmd;

    QProcess ffmpeg;
    ffmpeg.setReadChannel(QProcess::StandardOutput);
    // FFmpeg writes to stderr only for some reason, so we just read both channels together
    ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
    ffmpeg.start(cmd, args);

    Status status = Status::OK;
    DebugDetails dd;
    dd << QStringLiteral("Command: %1 %2").arg(cmd).arg(args.join(' '));
    if (ffmpeg.waitForStarted())
    {
        while(ffmpeg.state() == QProcess::Running)
        {
            if(!ffmpeg.waitForReadyRead()) break;

            QString output(ffmpeg.readAll());
            QStringList sList = output.split(QRegularExpression("[\r\n]"), SplitBehaviorFlags::SkipEmptyParts);
            for (const QString& s : sList)
            {
                qDebug() << "[ffmpeg]" << s;
                dd << s;
            }

            if(output.startsWith("frame="))
            {
                QString frame = output.mid(6, output.indexOf(' '));

                bool shouldContinue = progress(frame.toInt());
                if (!shouldContinue)
                {
                    ffmpeg.terminate();
                    ffmpeg.waitForFinished(3000);
                    if (ffmpeg.state() == QProcess::Running) ffmpeg.kill();
                    ffmpeg.waitForFinished();
                    return Status::CANCELED;
                }
            }
        }

        QString output(ffmpeg.readAll());
        QStringList sList = output.split(QRegularExpression("[\r\n]"), SplitBehaviorFlags::SkipEmptyParts);
        for (const QString& s : sList)
        {
            qDebug() << "[ffmpeg]" << s;
            dd << s;
        }

        if(ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0)
        {
            status = Status::FAIL;
            status.setTitle(tr("Something went wrong"));
            status.setDescription(tr("Looks like our video backend did not exit normally. Your movie may not have exported correctly. Please try again and report this if it persists."));
            dd << QString("Exit status: ").append(QProcess::NormalExit ? "NormalExit": "CrashExit")
               << QString("Exit code: %1").arg(ffmpeg.exitCode());
            status.setDetails(dd);
            return status;
        }
    }
    else
    {
        qDebug() << "ERROR: Could not execute FFmpeg.";
        status = Status::FAIL;
        status.setTitle(tr("Something went wrong"));
        status.setDescription(tr("Couldn't start the video backend, please try again."));
        status.setDetails(dd);
    }
    return status;
}

/** Runs the specified command (should be ffmpeg), and lets
 *  writeFrame pipe data into it 1 frame at a time.
 *
 *  @param[in]  cmd A string containing the command to execute
 *  @param[in]  args A string list containing the arguments to
 *              pass to the command
 *  @param[out] progress A function that takes one float argument
 *              (the percentage of the ffmpeg operation complete) and
 *              may display the output to the user in any way it
 *              sees fit.
 *  @param[in]  nextFrame A function that renders the next frame into
 *              its argument and returns true, or returns false when
 *              there are no more frames.
 *
 *  At most MAX_QUEUED_FRAMES frames wait unwritten in the stdin buffer:
 *  past that, the renderer waits for ffmpeg to read instead of queueing.
 *
 *  @return Returns Status::OK if everything went well, and Status::FAIL
 *  and error is detected (usually a non-zero exit code for ffmpeg).
 */
Status MovieExporter::executeFFMpegPipe(const QString& cmd, const QStringList& args, std::function<void(float)> progress, std::function<bool(QImage&)> nextFrame)
{
    qDebug() << cmd;

    // Frames render on this thread (Object is not thread-safe); a writer thread owns ffmpeg and drains the pipe meanwhile.
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<QImage> queue; // guarded by mutex, as are lastFrame and finished
    bool lastFrame = false;
    bool finished = false;
    std::atomic<bool> stop{ false };
    std::atomic<int> lastFrameProcessed{ 0 };
    qint64 peak = 0;
    Status status = Status::OK;

    std::thread writer([&] {
        QProcess ffmpeg;
        ffmpeg.setReadChannel(QProcess::StandardOutput);
        // FFmpeg writes to stderr only for some reason, so we just read both channels together
        ffmpeg.setProcessChannelMode(QProcess::MergedChannels);
        ffmpeg.start(cmd, args);

        DebugDetails dd;
        dd << QStringLiteral("Command: %1 %2").arg(cmd).arg(args.join(' '));
        auto readOutput = [&](int msecs)
        {
            if (msecs >= 0 && !ffmpeg.waitForReadyRead(msecs)) { return; }
            const QString output(ffmpeg.readAll());
            for (const QString& s : output.split(QRegularExpression("[\r\n]"), SplitBehaviorFlags::SkipEmptyParts))
            {
                qDebug() << "[ffmpeg]" << s;
                dd << s;
            }
            if (output.startsWith("frame=")) { lastFrameProcessed = output.mid(6, output.indexOf(' ')).toInt(); }
        };

        if (!ffmpeg.waitForStarted())
        {
            qDebug() << "ERROR: Could not execute FFmpeg.";
            status = Status::FAIL;
            status.setTitle(tr("Something went wrong"));
            status.setDescription(tr("Couldn't start the video backend, please try again."));
            status.setDetails(dd);
        }
        else
        {
            bool writing = true;
            while (ffmpeg.state() == QProcess::Running && !stop)
            {
                if (writing && ffmpeg.bytesToWrite() == 0)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    if (queue.empty() && !lastFrame) { changed.wait_for(lock, std::chrono::milliseconds(10)); }
                    if (!queue.empty())
                    {
                        const QImage frame = std::move(queue.front());
                        queue.pop_front();
                        const qint64 queued = qint64(queue.size()) * frame.sizeInBytes();
                        lock.unlock();
                        changed.notify_all();
                        ffmpeg.write(reinterpret_cast<const char*>(frame.constBits()), frame.sizeInBytes());
                        peak = qMax(peak, queued + ffmpeg.bytesToWrite());
                    }
                    else if (lastFrame)
                    {
                        lock.unlock();
                        ffmpeg.closeWriteChannel();
                        writing = false;
                    }
                }
                else if (writing)
                {
                    ffmpeg.waitForBytesWritten(10);
                }
                readOutput(writing ? 0 : 10);
            }

            if (stop)
            {
                ffmpeg.terminate();
                if (ffmpeg.state() == QProcess::Running) ffmpeg.kill();
            }
            else
            {
                readOutput(-1);
                if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0)
                {
                    status = Status::FAIL;
                    status.setTitle(tr("Something went wrong"));
                    status.setDescription(tr("Looks like our video backend did not exit normally. Your movie may not have exported correctly. Please try again and report this if it persists."));
                    dd << QString("Exit status: ").append(QProcess::NormalExit ? "NormalExit": "CrashExit")
                       << QString("Exit code: %1").arg(ffmpeg.exitCode());
                    status.setDetails(dd);
                }
            }
        }
        std::lock_guard<std::mutex> lock(mutex);
        finished = true;
        changed.notify_all();
    });

    // At most MAX_QUEUED_FRAMES frames wait: MAX_QUEUED_FRAMES - 1 here, one in ffmpeg's stdin buffer.
    int framesGenerated = 0;
    const float frameCount = qMax(1, mDesc.endFrame - mDesc.startFrame);
    bool more = true;
    std::unique_lock<std::mutex> lock(mutex);
    while (!finished)
    {
        if (mCanceled) { stop = true; break; }
        if (more && int(queue.size()) < MAX_QUEUED_FRAMES - 1)
        {
            lock.unlock();
            QImage frame;
            more = nextFrame(frame);
            lock.lock();
            if (more) { queue.push_back(std::move(frame)); ++framesGenerated; } else { lastFrame = true; }
            changed.notify_all();
        }
        else
        {
            changed.wait_for(lock, std::chrono::milliseconds(10));
        }
        lock.unlock();
        progress((framesGenerated + lastFrameProcessed) / frameCount / 2);
        lock.lock();
    }
    lock.unlock();
    writer.join();
    mPeakQueuedBytes = peak;
    return stop ? Status(Status::CANCELED) : status;
}

Status MovieExporter::checkInputParameters(const ExportMovieDesc& desc)
{
    bool b = true;
    b &= (!desc.strFileName.isEmpty());
    b &= (desc.startFrame > 0);
    b &= (desc.endFrame >= desc.startFrame);
    b &= (desc.fps > 0);
    b &= (!desc.strCameraName.isEmpty());

    return b ? Status::OK : Status::INVALID_ARGUMENT;
}
