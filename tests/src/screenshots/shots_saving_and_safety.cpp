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
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QTimer>

#include "editor.h"
#include "errordialog.h"
#include "filemanager.h"
#include "layermanager.h"
#include "object.h"
#include "preferencesdialog.h"
#include "screenshots.h"
#include "statusbar.h"

namespace
{
QAction* action(ShotWindow& shot, const char* name)
{
    auto* found = shot.window.findChild<QAction*>(name);
    REQUIRE(found);
    return found;
}

// A popup menu is its own window: grab it into a label.
void saveMenu(ShotWindow& shot, const char* menuName, const QString& name)
{
    auto* menu = shot.window.findChild<QMenu*>(menuName);
    REQUIRE(menu);
    menu->popup(QPoint(20, 20));
    QApplication::processEvents();
    const QPixmap picture = menu->grab();
    menu->hide();
    QLabel label;
    label.setPixmap(picture);
    label.resize(picture.size() / picture.devicePixelRatio());
    saveShot(&label, name);
}

// One undoable stroke: Undo turns on in the Edit menu.
void drawStroke(ShotWindow& shot)
{
    Edit stroke = shot.editor->edit(shot.editor->layers()->currentLayer(), shot.editor->currentFrame(), "Draw");
    shot.editor->setModified(shot.editor->currentLayerIndex(), shot.editor->currentFrame());
    stroke.commit();
    QApplication::processEvents();
}

// The right end of the status bar holds the unsaved-changes indicator.
QPixmap statusBarPicture(ShotWindow& shot, bool modified)
{
    auto* bar = shot.window.findChild<StatusBar*>();
    REQUIRE(bar);
    bar->updateModifiedStatus(modified);
    const QPixmap all = bar->grab();
    const qreal ratio = all.devicePixelRatio();
    return all.copy(QRect(all.width() - qRound(330 * ratio), 0, qRound(330 * ratio), all.height()));
}
} // namespace

// Shots for docs/manual/09-saving-and-safety.md. Each SECTION saves images/saving-and-safety-<shot>.png.
TEST_CASE("Manual screenshots: Saving and safety", "[.screenshots]")
{
    SECTION("file-menu")
    {
        ShotWindow shot;
        saveMenu(shot, "menuFile", "saving-and-safety-file-menu");
    }

    SECTION("legacy-pcl")
    {
        ShotWindow shot;
        FileManager fm;
        const Status st = fm.save(shot.editor->object(), "Ball.pcl");
        REQUIRE_FALSE(st.ok());
        ErrorDialog dialog(st.title(), st.description(), st.details().str());
        dialog.show();
        QApplication::processEvents();
        saveShot(&dialog, "saving-and-safety-legacy-pcl");
    }

    SECTION("unsaved-prompt")
    {
        ShotWindow shot;
        shot.editor->object()->setModified(true);
        bool grabbed = false;
        // The prompt blocks in exec(): a timer grabs it, then rejects it, which keeps the project.
        QTimer::singleShot(0, [&] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            REQUIRE(box);
            saveShot(box, "saving-and-safety-unsaved-prompt");
            grabbed = true;
            box->reject();
        });
        action(shot, "actionNew")->trigger();
        CHECK(grabbed);
    }

    SECTION("unsaved-indicator")
    {
        ShotWindow shot;
        const QPixmap saved = statusBarPicture(shot, false);
        drawStroke(shot);
        const QPixmap unsaved = statusBarPicture(shot, true);
        QPixmap both(qMax(saved.width(), unsaved.width()), saved.height() + unsaved.height());
        both.setDevicePixelRatio(saved.devicePixelRatio());
        both.fill(Qt::transparent);
        QPainter painter(&both);
        painter.drawPixmap(0, 0, unsaved);
        painter.drawPixmap(0, unsaved.height() / unsaved.devicePixelRatio(), saved);
        painter.end();
        QLabel label;
        label.setPixmap(both);
        label.resize(both.size() / both.devicePixelRatio());
        saveShot(&label, "saving-and-safety-unsaved-indicator");
    }

    SECTION("edit-menu")
    {
        ShotWindow shot;
        drawStroke(shot);
        saveMenu(shot, "menuEdit", "saving-and-safety-edit-menu");
    }

    SECTION("autosave")
    {
        ShotWindow shot;
        shot.window.preferences();
        auto* prefs = shot.window.findChild<PreferencesDialog*>();
        REQUIRE(prefs);
        auto* pages = prefs->findChild<QListWidget*>("contentsWidget");
        REQUIRE(pages);
        pages->setCurrentRow(1);
        QApplication::processEvents();
        saveShot(prefs, "saving-and-safety-autosave");
        prefs->close();
    }

    SECTION("recovery")
    {
        ShotWindow shot;
        // The app shows this box from a private member: same texts, built here and never exec()ed.
        QMessageBox box(&shot.window);
        box.setWindowTitle("Restore project");
        box.setIconPixmap(QPixmap(":/icons/logo.png"));
        box.setText("<h4>Restore Project?</h4>JustAnimate didn't close correctly. Would you like to restore the project?");
        box.setInformativeText("<b>Ball</b>");
        box.setStandardButtons(QMessageBox::Open | QMessageBox::Discard | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel);
        box.show();
        QApplication::processEvents();
        saveShot(&box, "saving-and-safety-recovery");
    }

    SECTION("import-layers")
    {
        ShotWindow shot;
        action(shot, "actionImportLayers_from_pclx")->trigger();
        QDialog* dialog = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets())
        {
            if (w->isVisible() && w->inherits("ImportLayersDialog")) { dialog = qobject_cast<QDialog*>(w); }
        }
        REQUIRE(dialog);
        saveShot(dialog, "saving-and-safety-import-layers");
        dialog->close();
    }
}
