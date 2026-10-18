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

#include <QFileInfo>
#include <QSettings>

#include "editor.h"
#include "filemanager.h"
#include "fileformat.h"
#include "object.h"
#include "pencildef.h"
#include "pencilsettings.h"
#include "undoredomanager.h"

ProjectSession::ProjectSession(Editor* editor, QObject* parent) : QObject(parent), mEditor(editor)
{
}

bool ProjectSession::isDirty() const
{
    return mEditor->undoRedo()->hasUnsavedChanges();
}

Status ProjectSession::open(const QString& path)
{
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

    if (st.ok())
    {
        pencilSettings().setValue(LAST_PCLX_PATH, mEditor->object()->filePath());
    }
    return st;
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
    pencilSettings().setValue(LAST_PCLX_PATH, path);
    mEditor->resetAutoSaveCounter();

    emit saved(path);
    return st;
}

bool ProjectSession::maybeSave()
{
    if (!isDirty()) { return true; }

    switch (askSaveChanges ? askSaveChanges() : SaveChoice::Cancel)
    {
    case SaveChoice::Save: return save().ok();
    case SaveChoice::Discard: return true;
    case SaveChoice::Cancel: break;
    }
    return false;
}
