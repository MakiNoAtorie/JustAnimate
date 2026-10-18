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
#include "projectsession.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QSettings>

#include "autosaverbytime.h"
#include "editor.h"
#include "filemanager.h"
#include "fileformat.h"
#include "object.h"
#include "pencildef.h"
#include "pencilsettings.h"
#include "undoredomanager.h"

ProjectSession::ProjectSession(Editor* editor, QObject* parent) : QObject(parent), mEditor(editor)
{
    for (const QString& file : pencilSettings().value(RECENT_FILES).toStringList())
    {
        if (!file.isEmpty()) { mRecentFiles << file; }
    }
    connect(mEditor, &Editor::needSave, this, &ProjectSession::autoSave);
    auto* timer = new AutosaverByTime(mEditor->preference(), this);
    connect(timer, &AutosaverByTime::timeout, this, &ProjectSession::autoSaveTick);
}

ProjectSession::~ProjectSession()
{
    // The worker posts to this object before the future is ready.
    if (mBackgroundSave.valid()) { mBackgroundSave.wait(); }
}

bool ProjectSession::isDirty() const
{
    return mEditor->undoRedo()->hasUnsavedChanges();
}

Status ProjectSession::open(const QString& path)
{
    finishBackgroundSave();
    int value = 0;
    int maximum = 100;
    emit progress(Task::Open, value, maximum);
    const Status st = mEditor->openObject(path, [&](int v)
    {
        emit progress(Task::Open, value = v, maximum);
    }, [&](int m)
    {
        emit progress(Task::Open, value, maximum = m);
    });
    emit progress(Task::Open, maximum, maximum);

    if (st.ok() && !mEditor->object()->filePath().isEmpty())
    {
        remember(mEditor->object()->filePath());
    }
    return st;
}

Status ProjectSession::load(const QString& path)
{
    finishBackgroundSave();
    return mEditor->openObject(path, [](int) {}, [](int) {});
}

Status ProjectSession::save()
{
    // Legacy .pcl is open-only: its project goes to Save As .pclx.
    const QString path = mEditor->object()->filePath();
    if (QFileInfo(path).suffix().compare(PFF_BIG_LETTER_EXTENSION, Qt::CaseInsensitive) == 0)
    {
        return saveAs(path);
    }
    return saveAs(askSavePath ? askSavePath() : QString());
}

Status ProjectSession::saveAs(const QString& path)
{
    if (path.isEmpty()) { return Status::CANCELED; }
    finishBackgroundSave();

    int value = 0;
    int maximum = 100;
    emit progress(Task::Save, value, maximum);
    mEditor->prepareSave();

    FileManager fm;
    connect(&fm, &FileManager::progressChanged, this, [&](int v)
    {
        emit progress(Task::Save, value = v, maximum);
    });
    connect(&fm, &FileManager::progressRangeChanged, this, [&](int m)
    {
        emit progress(Task::Save, value, maximum = m + 3);
    });

    const Status st = fm.save(mEditor->object(), path);
    mEditor->undoRedo()->releaseExpectedDrawings(); // the save crops Drawings into new buffers
    mSkippedSoundClips = fm.skippedSoundClips();
    emit progress(Task::Save, maximum, maximum);

    if (!st.ok())
    {
        emit saveFailed(st);
        return st;
    }

    mEditor->undoRedo()->markSaved();
    mEditor->object()->setFilePath(path);
    mEditor->clearTemporary();
    remember(path);
    mEditor->resetAutoSaveCounter();

    emit saved(path);
    return st;
}

bool ProjectSession::maybeSave()
{
    finishBackgroundSave();
    if (!isDirty()) { return true; }

    switch (askSaveChanges ? askSaveChanges() : SaveChoice::Cancel)
    {
    case SaveChoice::Save: return save().ok();
    case SaveChoice::Discard: return true;
    case SaveChoice::Cancel: break;
    }
    return false;
}

bool ProjectSession::close()
{
    return maybeSave();
}

bool ProjectSession::autoSave()
{
    const QString path = mEditor->object()->filePath();
    if (path.isEmpty())
    {
        emit autoSaveSkipped();
        return false;
    }
    // Legacy .pcl goes through save(), which asks for a .pclx path.
    if (QFileInfo(path).suffix().compare(PFF_BIG_LETTER_EXTENSION, Qt::CaseInsensitive) != 0) { return save().ok(); }
    // The next count saves the edits made while this one runs.
    if (mBackgroundSave.valid()) { return false; }

    mEditor->prepareSave();
    mBackgroundPath = path;
    mBackgroundPoint = mEditor->undoRedo()->savePoint();
    // An unrecorded change made while the archive is written sets it again, and keeps the project dirty.
    mBackgroundWasModified = mEditor->object()->isModified();
    mEditor->object()->setModified(false);

    FileManager fm;
    mBackgroundSave = fm.saveInBackground(mEditor->object(), path, [this]
    {
        QMetaObject::invokeMethod(this, [this] { finishBackgroundSave(); }, Qt::QueuedConnection);
    });
    mSkippedSoundClips = fm.skippedSoundClips();
    mEditor->undoRedo()->releaseExpectedDrawings(); // the snapshot crops Drawings into new buffers
    // The working folder could not be written: no worker runs.
    if (mBackgroundSave.ready())
    {
        return finishBackgroundSave().ok();
    }
    return true;
}

Status ProjectSession::finishBackgroundSave()
{
    if (!mBackgroundSave.valid()) { return Status::OK; }
    const Status st = mBackgroundSave.finish(mEditor->object());
    if (!st.ok())
    {
        if (mBackgroundWasModified) { mEditor->object()->setModified(true); }
        emit saveFailed(st);
        return st;
    }
    if (mEditor->undoRedo()->savePoint() == mBackgroundPoint && !mEditor->object()->isModified())
    {
        mEditor->undoRedo()->markSaved();
    }
    mEditor->clearTemporary();
    remember(mBackgroundPath);
    emit saved(mBackgroundPath);
    return st;
}

Status ProjectSession::autoSaveTick()
{
    finishBackgroundSave();
    FileManager fm;
    const Status st = fm.writeToWorkingFolder(mEditor->object());
    mEditor->undoRedo()->releaseExpectedDrawings(); // the save crops Drawings into new buffers
    return st;
}

bool ProjectSession::recover()
{
    finishBackgroundSave();
    FileManager fm;
    for (const QString& folder : fm.searchForUnsavedProjects())
    {
        switch (askRecover ? askRecover(folder) : RecoverChoice::Cancel)
        {
        case RecoverChoice::Cancel: break;
        case RecoverChoice::Discard:
            QDir(folder).removeRecursively();
            break;
        case RecoverChoice::Open:
        {
            FileManager recovery;
            Object* o = recovery.recoverUnsavedProject(folder);
            if (!recovery.error().ok())
            {
                emit recovered(false);
                break;
            }
            mEditor->setObject(o);
            emit recovered(true);
            return true;
        }
        }
    }
    return false;
}

void ProjectSession::clearRecentFiles()
{
    setRecentFiles({});
}

QString ProjectSession::lastPath() const
{
    return pencilSettings().value(LAST_PCLX_PATH).toString();
}

void ProjectSession::remember(const QString& path)
{
    pencilSettings().setValue(LAST_PCLX_PATH, path);
    QStringList files = mRecentFiles;
    files.removeAll(path);
    files.prepend(path);
    setRecentFiles(files.mid(0, MAX_RECENT_FILES));
}

void ProjectSession::setRecentFiles(const QStringList& files)
{
    mRecentFiles = files;
    pencilSettings().setValue(RECENT_FILES, mRecentFiles);
    emit recentFilesChanged(mRecentFiles);
}
