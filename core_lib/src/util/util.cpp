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
#include "util.h"
#include "appinfo.h"
#include <QAbstractSpinBox>
#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSemaphore>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>

QString installedManualIndex(const QString& appDir)
{
#if defined(Q_OS_MACOS)
    return appDir + "/../Resources/manual/index.html";
#elif defined(Q_OS_WIN)
    return appDir + "/manual/index.html";
#else
    return appDir + "/../share/justanimate/manual/index.html";
#endif
}

QUrl manualUrl(const QString& appDir)
{
    const QString index = installedManualIndex(appDir);
    if (QFileInfo::exists(index)) {
        return QUrl::fromLocalFile(QDir::cleanPath(index));
    }
    return QUrl(APP_ONLINE_MANUAL_URL);
}

static inline bool clipLineToEdge(qreal& t0, qreal& t1, qreal p, qreal q)
{
    if (p < 0) { // Line entering the clipping window
        t0 = qMax(t0, q / p);
        return t0 < t1;
    }
    if (p > 0) { // Line leaving the clipping window
        t1 = qMin(t1, q / p);
        return t0 < t1;
    }
    return q >= 0;
}

QLineF clipLine(const QLineF& line, const QRect& clip, qreal t0, qreal t1)
{
    int left = clip.left(), right = left + clip.width(), top = clip.top(), bottom = top + clip.height();
    qreal x1 = line.x1(), x2 = line.x2(), dx = line.dx(), y1 = line.y1(), y2 = line.y2(), dy = line.dy();

    if ((t0 == 0 && t1 == 1 && ((x1 < left && x2 < left) ||
                                (x1 > right && x2 > right) ||
                                (y1 < top && y2 < top) ||
                                (y1 > bottom && y2 > bottom))) ||
        !clipLineToEdge(t0, t1, -dx, x1 - left) ||
        !clipLineToEdge(t0, t1,  dx, right - x1) ||
        !clipLineToEdge(t0, t1, -dy, y1 - top) ||
        !clipLineToEdge(t0, t1,  dy, bottom - y1)) {
        return {};
    }

    Q_ASSERT(t0 < t1);
    return {line.x1() + line.dx() * t0,
            line.y1() + line.dy() * t0,
            line.x1() + line.dx() * t1,
            line.y1() + line.dy() * t1};
}

void clearFocusOnFinished(QAbstractSpinBox *spinBox)
{
    QObject::connect(spinBox, &QAbstractSpinBox::editingFinished, spinBox, &QAbstractSpinBox::clearFocus);
}

QString ffprobeLocation()
{
#ifdef _WIN32
    return QApplication::applicationDirPath() + "/plugins/ffprobe.exe";
#elif __APPLE__
    return QApplication::applicationDirPath() + "/plugins/ffprobe";
#else
    QString ffprobePath = QStandardPaths::findExecutable(
        "ffprobe",
        QStringList()
        << QApplication::applicationDirPath() + "/plugins"
        << QApplication::applicationDirPath() + "/../plugins" // linuxdeployqt in FHS-like mode
    );
    if (!ffprobePath.isEmpty())
    {
        return ffprobePath;
    }
    return QStandardPaths::findExecutable("ffprobe"); // ffprobe is a standalone project.
#endif
}

QString ffmpegLocation()
{
#ifdef _WIN32
    return QApplication::applicationDirPath() + "/plugins/ffmpeg.exe";
#elif __APPLE__
    return QApplication::applicationDirPath() + "/plugins/ffmpeg";
#else
    QString ffmpegPath = QStandardPaths::findExecutable(
        "ffmpeg",
        QStringList()
        << QApplication::applicationDirPath() + "/plugins"
        << QApplication::applicationDirPath() + "/../plugins" // linuxdeployqt in FHS-like mode
    );
    if (!ffmpegPath.isEmpty())
    {
        return ffmpegPath;
    }
    return QStandardPaths::findExecutable("ffmpeg"); // ffmpeg is a standalone project.
#endif
}

int parallelWorkerCount()
{
    return qBound(1, QThread::idealThreadCount(), 8);
}

void parallelFor(int count, const std::function<void(int)>& fn)
{
    if (count == 1)
    {
        fn(0); // no thread hop for serial callers
        return;
    }
    QSemaphore done;
    for (int i = 0; i < count; ++i)
        workerPool()->start([&fn, &done, i] { fn(i); done.release(); });
    done.acquire(count);
}

QThreadPool* workerPool()
{
    // Shared so threads are reused between calls. Do not use waitForDone: in Qt 6 it joins and deletes every thread.
    static QThreadPool pool;
    pool.setMaxThreadCount(parallelWorkerCount());
    return &pool;
}

quint64 imageSize(const QImage& img)
{
    return img.sizeInBytes();
}

QString formatSecondsFrames(int frames, int fps)
{
    if (fps <= 0) { return QString("0+%1").arg(frames); }
    return QString("%1+%2").arg(frames / fps).arg(frames % fps);
}

int parseSecondsFrames(const QString& text, int fps)
{
    const QStringList parts = text.trimmed().split('+');
    bool ok = false;
    const int first = parts.value(0).toInt(&ok);
    if (!ok || parts.size() > 2) { return 0; }
    if (parts.size() == 1) { return qMax(0, first); }
    const int frames = parts[1].toInt(&ok);
    return ok && first >= 0 && frames >= 0 ? first * qMax(1, fps) + frames : 0;
}

QString uniqueString(int len)
{
    static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    const int alphanumLen = sizeof(alphanum);

    if (len > 128) len = 128;

    char s[128 + 1];
    for (int i = 0; i < len; ++i)
    {
        // Thread-safe and seeded per process: rand() repeats across processes started in the same second.
        s[i] = alphanum[QRandomGenerator::global()->bounded(alphanumLen - 1)];
    }
    s[len] = 0;
    return QString::fromUtf8(s);
}

QString closestCanonicalPath(const QString& path)
{
    QString origPath = QDir(path).absolutePath();

    // Iterate up the path until an existing file/directory is found
    QFileInfo existingSubpath(origPath);
    // Symlinks must be checked for separately because exists checks if the target of the symlink exists, not the symlink itself
    while (!existingSubpath.isRoot() && !existingSubpath.exists() && !existingSubpath.isSymbolicLink())
    {
        // Move up one directory logically
        existingSubpath.setFile(existingSubpath.dir().absolutePath());
    }

    // Resolve symlinks for all existing parts of the path
    QString canonicalPath = existingSubpath.canonicalFilePath();
    if (canonicalPath.isEmpty())
    {
        // This can happen if there is a dangling symlink in the path
        return QString();
    }

    // Combine existing canonical path with non-existing path segment
    QString finalPath = QDir(canonicalPath).filePath(QDir(existingSubpath.absoluteFilePath()).relativeFilePath(origPath));

    return QDir(finalPath).absolutePath();
}

QString validateDataPath(const QString& filePath, const QString& dataDirPath)
{
    return DataPathValidator(dataDirPath).validate(filePath);
}

DataPathValidator::DataPathValidator(const QString& dataDirPath)
    : mDataDirPath(dataDirPath), mCanonicalDataDirPath(closestCanonicalPath(dataDirPath))
{
}

QString DataPathValidator::validate(const QString& filePath) const
{
    // Make sure src path is relative
    if (!QFileInfo(filePath).isRelative()) return QString();

    // A plain file name can leave the data dir only if it is a link itself.
    // This skips the canonical lookup, which costs about 0.2 ms per file on Windows.
    static const QRegularExpression notPlainName(QStringLiteral("[/\\\\:]"));
    if (!mCanonicalDataDirPath.isEmpty() && !filePath.isEmpty() &&
        filePath != QLatin1String(".") && filePath != QLatin1String("..") &&
        !filePath.contains(notPlainName))
    {
        const QString plainPath = QDir(mCanonicalDataDirPath).filePath(filePath);
        if (!QFileInfo(plainPath).isSymLink()) return plainPath;
    }

    // Get canonical path of data dir and file for comparison
    QString canonicalDataDirPath = mCanonicalDataDirPath;
    QString canonicalFilePath = closestCanonicalPath(QDir(mDataDirPath).filePath(filePath));

    // Bail out if either canonical path could not be resolved (e.g. dangling symlinks)
    if (canonicalDataDirPath.isEmpty() || canonicalFilePath.isEmpty())
    {
        qWarning() << "validateDataPath: failed to resolve canonical path for:" << filePath;
        return QString();
    }

    // Ensure the data dir path ends with a separator so that a prefix match
    // cannot falsely succeed against a sibling directory with a similar name
    // (e.g. /tmp/data matching /tmp/dataevil/...)
    if (!canonicalDataDirPath.endsWith('/'))
        canonicalDataDirPath.append('/');

    // Use case-insensitive comparison on filesystems that are case-insensitive
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    if (canonicalFilePath.startsWith(canonicalDataDirPath, cs))
    {
        return canonicalFilePath;
    }

    // If canonicalFilePath does not start with the canonicalDataDirPath, then symlinks or '..' have made
    // the file resolve outside of the data directory and the file should not be loaded.
    qWarning() << "validateDataPath: rejected path outside data directory:" << filePath;
    return QString();
}
