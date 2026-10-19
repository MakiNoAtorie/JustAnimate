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

#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSpinBox>

#include "actioncommands.h"
#include "appinfo.h"
#include "editor.h"
#include "errordialog.h"
#include "exportimagedialog.h"
#include "exportmoviedialog.h"
#include "objectdata.h"
#include "papertemplatedialog.h"
#include "pencilerror.h"
#include "screenshots.h"

namespace
{
// The dialog opens on the last save path; a fixed name keeps the user's folders out of the shot.
void showPath(ImportExportDialog& dialog, const QString& name)
{
    dialog.findChild<QLineEdit*>("fileEdit")->setText("\"" + name + "\"");
    emit dialog.filePathsChanged(QStringList(name));
}

void prepare(ExportMovieDialog& dialog)
{
    dialog.init();
    dialog.setCamerasInfo({ { "Camera", QSize(1280, 720) } });
    dialog.setDefaultRange(1, 18, 24);
}

void prepare(ExportImageDialog& dialog)
{
    dialog.init();
    dialog.setCamerasInfo({ { "Camera", QSize(1280, 720) } });
    dialog.setDefaultRange(1, 18, 24);
}
} // namespace

// Shots for docs/manual/10-exporting.md. Each SECTION saves images/exporting-<shot>.png.
TEST_CASE("Manual screenshots: Exporting", "[.screenshots]")
{
    ShotWindow shot;

    SECTION("menu")
    {
        saveMenu(shot.window.findChild<QMenu*>("menuExport"), "exporting-menu");
    }

    SECTION("movie-mp4")
    {
        ExportMovieDialog dialog(&shot.window, ImportExportDialog::Export, FileType::MOVIE);
        prepare(dialog);
        showPath(dialog, "ball.mp4");
        dialog.show();
        saveShot(&dialog, "exporting-movie-mp4");
    }

    SECTION("movie-webm")
    {
        ExportMovieDialog dialog(&shot.window, ImportExportDialog::Export, FileType::MOVIE);
        prepare(dialog);
        showPath(dialog, "ball.webm");
        dialog.findChild<QCheckBox*>("transparencyCheckBox")->setChecked(true);
        dialog.show();
        saveShot(&dialog, "exporting-movie-webm");
    }

    SECTION("movie-odd")
    {
        ExportMovieDialog dialog(&shot.window, ImportExportDialog::Export, FileType::MOVIE);
        prepare(dialog);
        showPath(dialog, "ball.mp4");
        dialog.findChild<QSpinBox*>("widthSpinBox")->setValue(1281);
        dialog.show();
        saveShot(&dialog, "exporting-movie-odd");
    }

    SECTION("gif")
    {
        ExportMovieDialog dialog(&shot.window, ImportExportDialog::Export, FileType::GIF);
        prepare(dialog);
        showPath(dialog, "ball.gif");
        for (auto* spin : dialog.findChildren<QSpinBox*>())
        {
            if (spin->minimum() == 3 && spin->maximum() == 256) { spin->setValue(64); }
        }
        dialog.show();
        saveShot(&dialog, "exporting-gif");
    }

    SECTION("range")
    {
        ExportMovieDialog dialog(&shot.window, ImportExportDialog::Export, FileType::MOVIE);
        prepare(dialog);
        showPath(dialog, "ball.mp4");
        dialog.findChild<QSpinBox*>("startSpinBox")->setValue(5);
        dialog.findChild<QCheckBox*>("frameCheckBox")->click();
        dialog.show();
        saveShot(dialog.findChild<QGroupBox*>("frameRangeGroupBox"), "exporting-range");
    }

    SECTION("image")
    {
        ExportImageDialog dialog(&shot.window, FileType::IMAGE);
        prepare(dialog);
        showPath(dialog, "ball.png");
        dialog.findChild<QCheckBox*>("cbTransparency")->setChecked(true);
        dialog.show();
        saveShot(&dialog, "exporting-image");
    }

    SECTION("sequence")
    {
        ExportImageDialog dialog(&shot.window, FileType::IMAGE_SEQUENCE);
        prepare(dialog);
        showPath(dialog, "ball.png");
        dialog.findChild<QCheckBox*>("cbTransparency")->setChecked(true);
        dialog.show();
        saveShot(&dialog, "exporting-sequence");
    }

    SECTION("sprite-sheet")
    {
        ExportImageDialog dialog(&shot.window, FileType::IMAGE_SEQUENCE);
        prepare(dialog);
        showPath(dialog, "ball.png");
        for (auto* box : dialog.findChildren<QCheckBox*>())
        {
            if (box->text() == "Sprite sheet (one PNG)") { box->setChecked(true); }
        }
        for (auto* spin : dialog.findChildren<QSpinBox*>())
        {
            if (spin->prefix() == "Columns: ") { spin->setValue(6); }
        }
        dialog.show();
        saveShot(&dialog, "exporting-sprite-sheet");
    }

    SECTION("overwrite")
    {
        // The box of QMessageBox::question with the app text, shown here and never exec()ed.
        QMessageBox box(QMessageBox::Question, APP_NAME, ActionCommands::overwriteQuestion("C:/Animation/ball.mp4"),
                        QMessageBox::Yes | QMessageBox::No);
        box.show();
        QApplication::processEvents();
        saveShot(&box, "exporting-overwrite");
    }

    SECTION("progress")
    {
        QProgressDialog progress(&shot.window);
        ActionCommands::setUpMovieProgress(progress);
        progress.show();
        progress.setValue(400);
        saveShot(&progress, "exporting-progress");
    }

    SECTION("ffmpeg-missing")
    {
        const Status st(Status::ERROR_FFMPEG_NOT_FOUND);
        ErrorDialog dialog(st.title(), st.description(), st.details().html(), &shot.window);
        dialog.show();
        saveShot(&dialog, "exporting-ffmpeg-missing");
    }

    SECTION("paper-template")
    {
        PaperTemplate paper;
        paper.on = true;
        paper.title = "Bouncing ball";
        paper.id = "C012";
        paper.name = "Ball";
        paper.state = "Rough";
        paper.date = "2026-10-19";
        PaperTemplateDialog dialog(paper, &shot.window);
        dialog.show();
        saveShot(&dialog, "exporting-paper-template");
    }
}
