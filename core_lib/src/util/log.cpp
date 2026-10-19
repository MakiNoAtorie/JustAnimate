
#include "log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <cstring>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

Q_LOGGING_CATEGORY(logCanvasPainter, "core.canvasPainter");
Q_LOGGING_CATEGORY(logFileManager, "core.FileManager");

void initCategoryLogging()
{
    const QString logRules =
        "*.debug=false\n"
        "default.debug=true\n"
        "core.canvasPainter.debug=false\n"
        "core.fileManager.debug=false";

    QLoggingCategory::setFilterRules(logRules);
}

namespace
{
const char kLogPattern[] = "session-*.log";
constexpr int kMaxPath = 4096;

QMutex gLogMutex;
QFile gLogFile;
QtMessageHandler gPreviousHandler = nullptr;

// Native path, fixed at start, for writeCrashLine.
#ifdef Q_OS_WIN
wchar_t gCrashPath[kMaxPath] = {};
#else
char gCrashPath[kMaxPath] = {};
#endif

const char* levelName(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg: return "debug";
    case QtInfoMsg: return "info";
    case QtWarningMsg: return "warning";
    case QtCriticalMsg: return "critical";
    case QtFatalMsg: return "fatal";
    }
    return "?";
}

void writeLine(const QString& line)
{
    gLogFile.write(line.toUtf8() + '\n');
}

void sessionMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    // A message raised while writing must not take the lock again.
    static thread_local bool inHandler = false;
    if (!inHandler)
    {
        inHandler = true;
        QMutexLocker lock(&gLogMutex);
        if (gLogFile.isOpen())
        {
            writeLine(QString("%1 %2 %3: %4")
                      .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"),
                           levelName(type),
                           context.category ? context.category : "default",
                           msg));
            // Warnings and worse reach the OS at once, so a crash keeps them.
            if (type != QtDebugMsg && type != QtInfoMsg) { gLogFile.flush(); }
        }
        inHandler = false;
    }
    if (gPreviousHandler) { gPreviousHandler(type, context, msg); }
}

QStringList sessionLogs(const QDir& dir)
{
    return dir.entryList({ kLogPattern }, QDir::Files, QDir::Name);
}
} // namespace

QString startSessionLog(const QString& dirPath, const QStringList& header, int keep)
{
    stopSessionLog();
    QDir dir(dirPath);
    if (!dir.mkpath(".")) { return QString(); }

    const QString name = QString("session-%1-%2.log")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz"))
        .arg(QCoreApplication::applicationPid());
    const QString path = dir.filePath(name);
    {
        QMutexLocker lock(&gLogMutex);
        gLogFile.setFileName(path);
        if (!gLogFile.open(QIODevice::WriteOnly)) { return QString(); }
        for (const QString& line : header) { writeLine(line); }
        writeLine(QString());
        gLogFile.flush();

#ifdef Q_OS_WIN
        const QString native = QDir::toNativeSeparators(path);
        if (native.size() < kMaxPath) { gCrashPath[native.toWCharArray(gCrashPath)] = L'\0'; }
#else
        const QByteArray native = QFile::encodeName(path);
        if (native.size() < kMaxPath) { std::memcpy(gCrashPath, native.constData(), static_cast<size_t>(native.size()) + 1); }
#endif
    }

    const QStringList logs = sessionLogs(dir);
    for (int i = 0; i < logs.size() - qMax(keep, 1); ++i) { dir.remove(logs[i]); }

    gPreviousHandler = qInstallMessageHandler(sessionMessageHandler);
    return path;
}

void stopSessionLog()
{
    if (gPreviousHandler)
    {
        qInstallMessageHandler(gPreviousHandler);
        gPreviousHandler = nullptr;
    }
    QMutexLocker lock(&gLogMutex);
    gCrashPath[0] = 0;
    gLogFile.close();
}

QString sessionLogPath()
{
    QMutexLocker lock(&gLogMutex);
    return gLogFile.isOpen() ? gLogFile.fileName() : QString();
}

bool exportSessionLogs(const QString& target)
{
    QString current;
    {
        QMutexLocker lock(&gLogMutex);
        if (!gLogFile.isOpen()) { return false; }
        gLogFile.flush();
        current = gLogFile.fileName();
    }
    const QDir dir = QFileInfo(current).absoluteDir();
    const QStringList logs = sessionLogs(dir);
    const int at = logs.indexOf(QFileInfo(current).fileName());

    QFile out(target);
    if (!out.open(QIODevice::WriteOnly)) { return false; }
    auto append = [&out](const QString& path, const QByteArray& title)
    {
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly)) { return; }
        out.write("===== " + title + ": " + QFile::encodeName(QFileInfo(path).fileName()) + " =====\n");
        out.write(in.readAll());
        out.write("\n");
    };
    append(current, "This session");
    if (at > 0) { append(dir.filePath(logs[at - 1]), "Previous session"); }
    return out.error() == QFileDevice::NoError;
}

void writeCrashLine()
{
    static const char line[] = "crashed\n";
    if (gCrashPath[0] == 0) { return; }
#ifdef Q_OS_WIN
    HANDLE file = CreateFileW(gCrashPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) { return; }
    DWORD written = 0;
    WriteFile(file, line, sizeof(line) - 1, &written, nullptr);
    CloseHandle(file);
#else
    const int file = open(gCrashPath, O_WRONLY | O_APPEND | O_CLOEXEC);
    if (file < 0) { return; }
    const ssize_t written = write(file, line, sizeof(line) - 1);
    (void)written;
    close(file);
#endif
}
