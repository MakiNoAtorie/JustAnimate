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
#ifndef EMERGENCYSAVE_H
#define EMERGENCYSAVE_H

class QString;

/** On a fatal signal, copy the last completed autosave out of the temp
 *  folder into a durable folder, so a crash followed by a temp cleanup
 *  (reboot, OS housekeeping) still leaves it for startup recovery.
 *  The handler only copies a file named beforehand: no allocation, no
 *  locks, static buffers, and a second fault skips the copy.
 *  Limit on Windows: the Win32 file calls can use the process heap
 *  internally, so a crash from a corrupt heap can lose the copy (the
 *  autosave in the temp folder stays). */
namespace EmergencySave
{
    /// POSIX: SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS on an alternate stack.
    /// Windows: the unhandled-exception filter, and SIGABRT.
    void install();

    /// Remember a completed autosave archive. Called outside signal context;
    /// `name` is its working folder's name, used for the published copy.
    void setCompletedAutosave(const QString& archive, const QString& name);

    /// Forget it: a manual save made it stale.
    void clearCompletedAutosave();

    /// Where crash-time copies are published, as `<name>.pclx`.
    QString publishDir();
}

#endif // EMERGENCYSAVE_H
