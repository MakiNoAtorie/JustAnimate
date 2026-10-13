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
#include "emergencysave.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include "fileformat.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
using PathChar = wchar_t;
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
using PathChar = char;
#endif

namespace
{
constexpr int kMaxPath = 4096;

/// Paths of one completed autosave, fixed before any signal can need them.
struct Slot
{
    PathChar from[kMaxPath];
    PathChar tmp[kMaxPath];
    PathChar to[kMaxPath];
};

// Two slots so the handler always reads a whole one: a new autosave is
// written into the inactive slot, then published by flipping gActive.
Slot gSlots[2];
std::atomic<int> gActive{ -1 };
std::atomic_flag gEntered = ATOMIC_FLAG_INIT;
static_assert(std::atomic<int>::is_always_lock_free, "the signal handler needs a lock-free index");

bool toNative(const QString& path, PathChar* out)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    if (native.size() >= kMaxPath) { return false; }
    out[native.toWCharArray(out)] = L'\0';
#else
    const QByteArray native = QFile::encodeName(path);
    if (native.size() >= kMaxPath) { return false; }
    std::memcpy(out, native.constData(), static_cast<size_t>(native.size()) + 1);
#endif
    return true;
}

// Static, not on the stack: after a stack overflow the handler has little
// stack. gEntered lets only one thread use it.
char gBuffer[16384];

/// Copy slot.from to slot.tmp, then rename it to slot.to. Only system
/// calls and static storage, so it is safe to run inside a signal handler.
void publish(const Slot& slot)
{
    char* const buffer = gBuffer;
    const unsigned bufferSize = sizeof(gBuffer);
#ifdef Q_OS_WIN
    HANDLE in = CreateFileW(slot.from, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (in == INVALID_HANDLE_VALUE) { return; }
    HANDLE out = CreateFileW(slot.tmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (out == INVALID_HANDLE_VALUE) { CloseHandle(in); return; }
    bool ok = true;
    DWORD got = 0;
    while (ok && ReadFile(in, buffer, bufferSize, &got, nullptr) && got > 0)
    {
        DWORD written = 0;
        ok = WriteFile(out, buffer, got, &written, nullptr) && written == got;
    }
    ok = ok && FlushFileBuffers(out) != 0;
    CloseHandle(out);
    CloseHandle(in);
    if (ok) { MoveFileExW(slot.tmp, slot.to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH); }
#else
    const int in = open(slot.from, O_RDONLY | O_CLOEXEC);
    if (in < 0) { return; }
    const int out = open(slot.tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out < 0) { close(in); return; }
    bool ok = true;
    for (;;)
    {
        const ssize_t got = read(in, buffer, bufferSize);
        if (got == 0) { break; }
        if (got < 0) { ok = (errno == EINTR); if (ok) { continue; } break; }
        for (ssize_t done = 0; ok && done < got;)
        {
            const ssize_t n = write(out, buffer + done, static_cast<size_t>(got - done));
            if (n > 0) { done += n; }
            else if (n < 0 && errno == EINTR) { continue; }
            else { ok = false; }
        }
        if (!ok) { break; }
    }
    ok = ok && fsync(out) == 0;
    close(out);
    close(in);
    if (ok) { rename(slot.tmp, slot.to); }
#endif
}

/// A second fault (another thread, or a fault while publishing) skips the
/// copy instead of re-entering.
void publishOnce()
{
    if (gEntered.test_and_set()) { return; }
    const int active = gActive.load();
    if (active >= 0) { publish(gSlots[active]); }
}

extern "C" void onFatalSignal(int sig)
{
    publishOnce();
#ifdef Q_OS_WIN
    // SIGABRT only, and the CRT has reset it already: abort() continues.
    Q_UNUSED(sig)
#else
    signal(sig, SIG_DFL);
    raise(sig);
#endif
}

#ifdef Q_OS_WIN
// Faults are SEH exceptions here, on any thread. The CRT maps them to
// signals only on some threads, so use the process-wide filter.
LONG WINAPI onUnhandledException(EXCEPTION_POINTERS*)
{
    publishOnce();
    return EXCEPTION_CONTINUE_SEARCH; // default crash handling goes on
}
#endif
} // namespace

namespace EmergencySave
{

void install()
{
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(onUnhandledException);
    std::signal(SIGABRT, onFatalSignal);
#else
    // Own stack, so a stack overflow can still run the handler.
    // ponytail: main thread only; give worker threads one if they recurse.
    static char altStack[65536];
    stack_t stack = {};
    stack.ss_sp = altStack;
    stack.ss_size = sizeof(altStack);
    sigaltstack(&stack, nullptr);

    struct sigaction action = {};
    action.sa_handler = onFatalSignal;
    action.sa_flags = SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    for (int sig : { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS })
        sigaction(sig, &action, nullptr);
#endif
}

void setCompletedAutosave(const QString& archive, const QString& name)
{
    const QString dir = publishDir();
    QDir().mkpath(dir);
    const QString to = QDir(dir).filePath(name + PFF_EXTENSION);

    const int next = (gActive.load() == 0) ? 1 : 0;
    Slot& slot = gSlots[next];
    if (toNative(QFileInfo(archive).absoluteFilePath(), slot.from) &&
        toNative(to + ".tmp", slot.tmp) &&
        toNative(to, slot.to))
    {
        gActive.store(next);
    }
}

QString publishDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("recovery");
}

} // namespace EmergencySave
