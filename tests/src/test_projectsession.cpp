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

#include <QAction>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QTemporaryDir>

#include "editor.h"
#include "fileformat.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "pencildef.h"
#include "pencilsettings.h"
#include "projectsession.h"
#include "testfixture.h"
#include "undoredomanager.h"

using SaveChoice = ProjectSession::SaveChoice;

namespace
{
// A recorded edit as a stroke tool makes it: snapshot, mark the frame, record.
void drawStroke(Editor* editor)
{
    UndoRedoManager* undoRedo = editor->undoRedo();
    const SAVESTATE_ID state = undoRedo->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    editor->setModified(editor->currentLayerIndex(), editor->currentFrame());
    undoRedo->record(state, "Draw");
}
}

TEST_CASE("ProjectSession: save then reopen keeps the project")
{
    TestSession session;
    session.object->addNewBitmapLayer()->setName("Ink");
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ProjectSession project(editor, nullptr);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("project.pclx");

    drawStroke(editor);
    REQUIRE(project.isDirty());

    int saved = 0;
    QObject::connect(&project, &ProjectSession::saved, [&] { saved++; });
    REQUIRE(project.saveAs(path).ok());
    CHECK(saved == 1);
    CHECK_FALSE(project.isDirty());
    CHECK(editor->object()->filePath() == path);

    REQUIRE(project.open(path).ok());
    CHECK_FALSE(project.isDirty());
    CHECK(editor->object()->filePath() == path);
    CHECK(editor->object()->findLayerByName("Ink", Layer::BITMAP) != nullptr);
}

TEST_CASE("ProjectSession: a failed save returns an error and keeps the project dirty")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ProjectSession project(editor, nullptr);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // A path whose parent is a regular file can never be written, even as root.
    QFile blocker(dir.filePath("not-a-folder"));
    REQUIRE(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    drawStroke(editor);
    int failed = 0;
    QObject::connect(&project, &ProjectSession::saveFailed, [&] { failed++; });

    const Status st = project.saveAs(dir.filePath("not-a-folder/project.pclx"));
    CHECK_FALSE(st.ok());
    CHECK(failed == 1);
    CHECK(project.isDirty());
    CHECK(editor->object()->filePath().isEmpty());
}

TEST_CASE("ProjectSession: dirty follows undo to the clean index")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ProjectSession project(editor, nullptr);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    QAction* redoAction = editor->undoRedo()->createRedoAction(&parent, QIcon());

    REQUIRE_FALSE(project.isDirty());
    drawStroke(editor);
    REQUIRE(project.isDirty());

    undoAction->trigger();
    CHECK_FALSE(project.isDirty());
    redoAction->trigger();
    CHECK(project.isDirty());
    undoAction->trigger();
    CHECK_FALSE(project.isDirty());

    SECTION("An unrecorded change stays dirty through undo")
    {
        emit editor->framesModified();
        drawStroke(editor);
        undoAction->trigger();
        CHECK(project.isDirty());
    }
}

TEST_CASE("ProjectSession: undoing an attribute edit or a new layer to the clean index leaves the project clean")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ProjectSession project(editor, nullptr);

    QObject parent;
    QAction* undoAction = editor->undoRedo()->createUndoAction(&parent, QIcon());
    REQUIRE_FALSE(project.isDirty());

    SECTION("Attribute")
    {
        const int length = editor->object()->sceneLength();
        REQUIRE(editor->layers()->setSceneLength(length + 10));
        REQUIRE(project.isDirty());
        undoAction->trigger();
        CHECK(editor->object()->sceneLength() == length);
    }
    SECTION("New layer")
    {
        const int count = editor->object()->getLayerCount();
        REQUIRE(editor->layers()->createBitmapLayer("Ink") != nullptr);
        REQUIRE(project.isDirty());
        undoAction->trigger();
        CHECK(editor->object()->getLayerCount() == count);
    }
    CHECK_FALSE(project.isDirty());
}

TEST_CASE("ProjectSession: the save-changes prompt appears exactly when dirty")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ProjectSession project(editor, nullptr);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    int asked = 0;
    SaveChoice choice = SaveChoice::Cancel;
    project.askSaveChanges = [&] { asked++; return choice; };
    project.askSavePath = [&] { return dir.filePath("project.pclx"); };

    REQUIRE(project.maybeSave());
    CHECK(asked == 0);

    drawStroke(editor);

    SECTION("Cancel keeps the project")
    {
        CHECK_FALSE(project.maybeSave());
        CHECK(asked == 1);
        CHECK(project.isDirty());
    }
    SECTION("Discard lets it go unsaved")
    {
        choice = SaveChoice::Discard;
        CHECK(project.maybeSave());
        CHECK(asked == 1);
        CHECK(project.isDirty());
    }
    SECTION("Save asks a path and saves")
    {
        choice = SaveChoice::Save;
        CHECK(project.maybeSave());
        CHECK(asked == 1);
        CHECK_FALSE(project.isDirty());
        CHECK(QFile::exists(dir.filePath("project.pclx")));
    }
    SECTION("Save with no path chosen keeps the project")
    {
        choice = SaveChoice::Save;
        project.askSavePath = [] { return QString(); };
        CHECK_FALSE(project.maybeSave());
        CHECK(project.isDirty());
    }
}

TEST_CASE("ProjectSession: the autosave tick writes the autosave file and recovery offers it")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ProjectSession project(editor, nullptr);

    const QDir workingDir(editor->object()->workingDir());
    REQUIRE_FALSE(workingDir.exists(PFF_AUTOSAVE_FILE_NAME));
    REQUIRE(project.autoSaveTick().ok());
    CHECK(workingDir.exists(PFF_AUTOSAVE_FILE_NAME));

    QStringList offered;
    project.askRecover = [&](const QString& folder)
    {
        offered << QDir(folder).canonicalPath();
        return ProjectSession::RecoverChoice::Cancel;
    };
    CHECK_FALSE(project.recover());
    CHECK(offered.contains(workingDir.canonicalPath()));
    CHECK(workingDir.exists(PFF_AUTOSAVE_FILE_NAME));
}

TEST_CASE("ProjectSession: close with unsaved changes asks to save")
{
    TestSession session;
    session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    Editor* editor = session.editor.get();
    ProjectSession project(editor, nullptr);

    int asked = 0;
    project.askSaveChanges = [&] { asked++; return SaveChoice::Cancel; };
    CHECK(project.close());
    CHECK(asked == 0);

    drawStroke(editor);
    CHECK_FALSE(project.close());
    CHECK(asked == 1);
}

TEST_CASE("ProjectSession: a saved project heads the recent files once")
{
    const SettingsGuard guard({ RECENT_FILES, LAST_PCLX_PATH });
    pencilSettings().remove(RECENT_FILES);
    TestSession session;
    session.object->addNewBitmapLayer();
    REQUIRE(session.start());
    ProjectSession project(session.editor.get(), nullptr);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString a = dir.filePath("a.pclx");
    const QString b = dir.filePath("b.pclx");

    QStringList shown;
    QObject::connect(&project, &ProjectSession::recentFilesChanged, [&](const QStringList& files) { shown = files; });
    REQUIRE(project.saveAs(a).ok());
    REQUIRE(project.saveAs(b).ok());
    REQUIRE(project.open(a).ok());
    CHECK(project.recentFiles() == QStringList({ a, b }));
    CHECK(shown == project.recentFiles());
    CHECK(project.lastPath() == a);
    CHECK(ProjectSession(session.editor.get(), nullptr).recentFiles() == QStringList({ a, b }));

    project.clearRecentFiles();
    CHECK(project.recentFiles().isEmpty());
    CHECK(shown.isEmpty());
}
