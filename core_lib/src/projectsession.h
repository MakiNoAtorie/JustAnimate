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

/// Opens and saves the project and owns its dirty state. Prompts are injected callbacks.
class ProjectSession : public QObject
{
    Q_OBJECT
public:
    enum class SaveChoice { Save, Discard, Cancel };
    enum class Task { Open, Save };

    ProjectSession(Editor* editor, QObject* parent);

    /// The "save changes?" prompt. Unset means Cancel.
    std::function<SaveChoice()> askSaveChanges;
    /// The save-path prompt; an empty path cancels. Unset means cancel.
    std::function<QString()> askSavePath;

    /// The undo stack is not at its clean index, or an unrecorded change was flagged.
    bool isDirty() const;

    Status open(const QString& path);
    /// Saves to the current .pclx file; any other project asks a path first.
    Status save();
    Status saveAs(const QString& path);
    /// True when the project may be replaced: it is clean, discarded or saved.
    bool maybeSave();

    /// Sound clips the last save wrote without audio.
    QStringList skippedSoundClips() const { return mSkippedSoundClips; }

signals:
    /// value reaches maximum when the task ends.
    void progress(ProjectSession::Task task, int value, int maximum);
    void saved(const QString& path);
    void saveFailed(const Status& status);

private:
    Editor* mEditor = nullptr;
    QStringList mSkippedSoundClips;
};

#endif // PROJECTSESSION_H
