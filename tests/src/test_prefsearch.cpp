#include "catch.hpp"

#include "prefsearch.h"
#include "preferencesdialog.h"
#include "shortcutspage.h"
#include "toolspage.h"
#include "pencilsettings.h"
#include "editor.h"
#include "preferencemanager.h"
#include "testfixture.h"

#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QStandardItemModel>
#include <QTreeView>

TEST_CASE("PrefSearch finds options by name", "[PrefSearch]")
{
    REQUIRE(prefLabelMatches("Maximum Number of Undo/Redo Steps", "undo"));
    REQUIRE(prefLabelMatches("Enable &Action Safe area (%)", "action safe"));
    REQUIRE(prefLabelMatches("Invert Zoom Direction", "  DIRECTION zoom "));
    REQUIRE(prefLabelMatches("Short scrub", ""));
    REQUIRE_FALSE(prefLabelMatches("Short scrub", "autosave"));
    REQUIRE_FALSE(prefLabelMatches("Invert Zoom Direction", "zoom scroll"));
}

TEST_CASE("PreferencesDialog search highlights unnamed labels and filters shortcuts", "[PrefSearch]")
{
    // Same default-shortcut setup the app does at startup: a clean machine has none.
    checkExistingShortcuts();
    PreferencesDialog dialog(nullptr);
    QLabel* unnamed = nullptr;
    for (QLabel* l : dialog.findChildren<QLabel*>())
    {
        if (l->objectName().isEmpty() && l->text().startsWith("Pressure curve")) { unnamed = l; }
    }
    REQUIRE(unnamed);
    auto* tree = dialog.findChild<QTreeView*>("treeView");
    REQUIRE(tree);
    const QAbstractItemModel* model = tree->model();
    REQUIRE(model->rowCount() > 1);
    const QString first = model->index(0, 0).data().toString();
    int other = 1;
    while (other < model->rowCount() && prefLabelMatches(model->index(other, 0).data().toString(), first)) { ++other; }
    REQUIRE(other < model->rowCount());

    dialog.search("pressure curve");
    CHECK(unnamed->autoFillBackground());
    CHECK(unnamed->palette().color(QPalette::Window) == unnamed->palette().color(QPalette::Highlight));

    dialog.search(first);
    CHECK_FALSE(unnamed->autoFillBackground());
    CHECK_FALSE(tree->isRowHidden(0, QModelIndex()));
    CHECK(tree->isRowHidden(other, QModelIndex()));

    dialog.search("");
    CHECK_FALSE(tree->isRowHidden(other, QModelIndex()));
}

TEST_CASE("Shortcuts page names the Layout commands with Glossary terms", "[PrefSearch]")
{
    checkExistingShortcuts();
    ShortcutsPage page;
    CHECK(page.filter("Reset layout"));
    CHECK(page.filter("Lock Panels"));
    CHECK_FALSE(page.filter("Reset Windows"));
    CHECK_FALSE(page.filter("Lock Windows"));
}
TEST_CASE("Preferences - undo steps apply when editing finishes after one confirmation", "[PrefSearch]")
{
    const SettingsGuard guard({ SETTING_UNDO_REDO_MAX_STEPS });
    Editor editor;
    PreferenceManager prefs(&editor);
    prefs.loadPrefs();
    prefs.set(SETTING::UNDO_REDO_MAX_STEPS, 100);
    PreferencesDialog dialog(nullptr);
    dialog.init(&prefs);
    auto* box = dialog.findChild<QSpinBox*>("undoStepsBox");
    REQUIRE(box);
    CHECK_FALSE(dialog.findChild<QPushButton*>("undoRedoGroupApplyButton"));

    int asked = 0;
    QTimer answer;
    QObject::connect(&answer, &QTimer::timeout, [&asked] {
        if (auto* m = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
        {
            ++asked;
            m->button(QMessageBox::Yes)->click();
        }
    });
    answer.start(0);
    box->setValue(150);
    CHECK(prefs.getInt(SETTING::UNDO_REDO_MAX_STEPS) == 100);
    emit box->editingFinished();
    CHECK(asked == 1);
    CHECK(prefs.getInt(SETTING::UNDO_REDO_MAX_STEPS) == 150);
}

TEST_CASE("Reset all tools button asks first and resets only on yes", "[PrefSearch]")
{
    Editor editor;
    PreferenceManager prefs(&editor);
    prefs.loadPrefs();
    PreferencesDialog dialog(nullptr);
    dialog.init(&prefs);
    auto* page = dialog.findChild<ToolsPage*>();
    auto* button = dialog.findChild<QPushButton*>("resetToolsButton");
    REQUIRE(page);
    REQUIRE(button);
    int resets = 0;
    QObject::connect(&dialog, &PreferencesDialog::resetToolsRequested, [&resets] { ++resets; });
    bool answer = false;
    page->confirmReset = [&answer] { return answer; };
    button->click();
    CHECK(resets == 0);
    answer = true;
    button->click();
    CHECK(resets == 1);
}
