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
#include "catch.hpp"

#include <csignal>
#include <cstdio>
#include <memory>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include "emergencysave.h"
#include "fileformat.h"
#include "filemanager.h"
#include "object.h"
#include "bitmapimage.h"
#include "layerbitmap.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

// Not instrumented: UBSan and ASan report a null store and exit the process,
// so the kernel never raises the signal that the handler is there for.
#if defined(__GNUC__)
__attribute__((no_sanitize("address"))) __attribute__((no_sanitize("undefined")))
#endif
static void writeToNull()
{
    int* volatile null = nullptr;
    *null = 1;
}

static QString savedProjectPath(const QString& workingDir)
{
    return QDir::temp().filePath(QDir(workingDir).dirName() + "-saved.pclx");
}

/// Child half of the crash test, run as `pencil2d_tests --crash-after-autosave`
/// (see main.cpp): autosave once, print the working folder, then crash.
/// With `--crash-after-save` a manual save comes between the two.
int crashAfterAutosave(bool thenSave)
{
    EmergencySave::install();

    Object* obj = new Object; // leaked on purpose: the crash must leave its working folder
    obj->init();
    obj->addNewCameraLayer();
    LayerBitmap* layer = obj->addNewBitmapLayer();
    layer->setName("Ink");
    BitmapImage red(QRect(0, 0, 8, 8), Qt::red);
    layer->getBitmapImageAtFrame(1)->paste(&red);

    FileManager fm;
    if (!fm.writeToWorkingFolder(obj).ok())
        return 2;
    if (thenSave && !fm.save(obj, savedProjectPath(obj->workingDir())).ok())
        return 2;

    std::fputs(qPrintable(obj->workingDir()), stdout);
    std::fflush(stdout);

#ifdef Q_OS_WIN
    SetErrorMode(SEM_NOGPFAULTERRORBOX); // no crash dialog in unattended runs
#endif
    // A real fault, not raise(): on Windows raise() skips the exception filter.
    writeToNull();
    return 3; // not reached
}

TEST_CASE("A crash after an autosave leaves it recoverable on next start", "[EmergencySave]")
{
    QProcess child;
    child.setProcessChannelMode(QProcess::ForwardedErrorChannel); // show a sanitizer report from the child
    child.start(QCoreApplication::applicationFilePath(), { "--crash-after-autosave" });
    REQUIRE(child.waitForFinished(60000));
    // Crashed: a signal on POSIX, a non-zero exit code on Windows.
    REQUIRE((child.exitStatus() == QProcess::CrashExit || child.exitCode() != 0));
    REQUIRE(child.exitCode() != 2);
#ifndef Q_OS_WIN
    // An exit code means something ended the process before the fault.
    REQUIRE(child.exitStatus() == QProcess::CrashExit);
#endif

    const QString workingDir = QString::fromLocal8Bit(child.readAllStandardOutput()).trimmed();
    REQUIRE_FALSE(workingDir.isEmpty());
    const QString projectName = QDir(workingDir).dirName();

    // The temp folder is wiped before the next start (reboot, OS cleanup).
    REQUIRE(QDir(workingDir).removeRecursively());

    FileManager fm;
    QString recoverPath;
    for (const QString& path : fm.searchForUnsavedProjects())
        if (QDir(path).dirName() == projectName)
            recoverPath = path;
    REQUIRE_FALSE(recoverPath.isEmpty());

    std::unique_ptr<Object> recovered(fm.recoverUnsavedProject(recoverPath));
    REQUIRE(fm.error().ok());
    REQUIRE(recovered != nullptr);

    LayerBitmap* recoveredLayer = nullptr;
    for (int i = 0; i < recovered->getLayerCount(); ++i)
        if (recovered->getLayer(i)->type() == Layer::BITMAP)
            recoveredLayer = static_cast<LayerBitmap*>(recovered->getLayer(i));
    REQUIRE(recoveredLayer != nullptr);
    REQUIRE(recoveredLayer->name() == "Ink");
    BitmapImage* frame = recoveredLayer->getBitmapImageAtFrame(1);
    REQUIRE(frame != nullptr);
    REQUIRE(frame->image()->pixel(4, 4) == qRgba(255, 0, 0, 255));

    // Nothing is offered twice: the published copy was taken in.
    QStringList again;
    for (const QString& path : fm.searchForUnsavedProjects())
        again << QDir(path).dirName();
    REQUIRE(again.count(projectName) == 1);
}

TEST_CASE("A crash after a manual save publishes no older autosave", "[EmergencySave]")
{
    QProcess child;
    child.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    child.start(QCoreApplication::applicationFilePath(), { "--crash-after-save" });
    REQUIRE(child.waitForFinished(60000));
    REQUIRE((child.exitStatus() == QProcess::CrashExit || child.exitCode() != 0));
    REQUIRE(child.exitCode() != 2);

    const QString workingDir = QString::fromLocal8Bit(child.readAllStandardOutput()).trimmed();
    REQUIRE_FALSE(workingDir.isEmpty());
    REQUIRE(QFile::remove(savedProjectPath(workingDir)));
    REQUIRE(QDir(workingDir).removeRecursively());

    FileManager fm;
    for (const QString& path : fm.searchForUnsavedProjects())
        REQUIRE(QDir(path).dirName() != QDir(workingDir).dirName());
}
