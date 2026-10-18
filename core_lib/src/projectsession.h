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
#ifndef PROJECTSESSION_H
#define PROJECTSESSION_H

#include <functional>

#include <QObject>
#include <QStringList>

#include "pencilerror.h"

class Editor;

/// Opens, saves, autosaves and recovers the project, and owns its dirty state and recent files.
/// Prompts are injected callbacks.
class ProjectSession : public QObject
{
    Q_OBJECT
public:
    enum class SaveChoice { Save, Discard, Cancel };
    enum class Task { Open, Save };
    enum class RecoverChoice { Open, Discard, Cancel };

    ProjectSession(Editor* editor, QObject* parent);

    /// The "save changes?" prompt. Unset means Cancel.
    std::function<SaveChoice()> askSaveChanges;
    /// The save-path prompt; an empty path cancels. Unset means cancel.
    std::function<QString()> askSavePath;
    /// The "restore this crashed project?" prompt, given its working folder. Unset means Cancel.
    std::function<RecoverChoice(const QString& folder)> askRecover;

    /// The undo stack is not at its clean index, or an unrecorded change was flagged.
    bool isDirty() const;

    /// Opens the project and adds it to the recent files and the last-used path.
    Status open(const QString& path);
    /// Opens the project with no progress, recent file or last-used path: the command line export.
    Status load(const QString& path);
    /// Saves to the current .pclx file; any other project asks a path first.
    Status save();
    Status saveAs(const QString& path);
    /// True when the project may be replaced: it is clean, discarded or saved.
    bool maybeSave();
    /// True when the app may close; same prompt as maybeSave.
    bool close();

    /// The count-based autosave: saves a named project, else emits autoSaveSkipped.
    bool autoSave();
    /// The timed autosave: writes the project to its working folder for crash recovery.
    Status autoSaveTick();
    /// Offers each crashed project in turn; true when one was restored.
    bool recover();

    QStringList recentFiles() const { return mRecentFiles; }
    void clearRecentFiles();
    /// The project opened or saved last, or empty.
    QString lastPath() const;

    /// Sound clips the last save wrote without audio.
    QStringList skippedSoundClips() const { return mSkippedSoundClips; }

signals:
    /// value reaches maximum when the task ends.
    void progress(ProjectSession::Task task, int value, int maximum);
    void saved(const QString& path);
    void saveFailed(const Status& status);
    void recentFilesChanged(const QStringList& files);
    void autoSaveSkipped();
    /// ok is false when a chosen project could not be restored.
    void recovered(bool ok);

private:
    static constexpr int MAX_RECENT_FILES = 10;

    void remember(const QString& path);
    void setRecentFiles(const QStringList& files);

    Editor* mEditor = nullptr;
    QStringList mSkippedSoundClips;
    QStringList mRecentFiles;
};

#endif // PROJECTSESSION_H
