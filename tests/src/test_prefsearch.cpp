#include "catch.hpp"

#include "app_util.h"
#include "actioncommands.h"
#include "generalpage.h"
#include "stroketool.h"
#include "toolmanager.h"
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
    prefs.set<SETTING::UNDO_REDO_MAX_STEPS>(100);
    PreferencesDialog dialog(nullptr);
    dialog.init(&prefs);
    auto* box = dialog.findChild<QSpinBox*>("undoStepsBox");
    REQUIRE(box);
    CHECK_FALSE(dialog.findChild<QPushButton*>("undoRedoGroupApplyButton"));

    auto* page = dialog.findChild<GeneralPage*>();
    REQUIRE(page);
    int asked = 0;
    bool answer = false;
    page->confirmUndoStepsChange = [&] { ++asked; return answer; };
    box->setValue(150);
    CHECK(prefs.get<SETTING::UNDO_REDO_MAX_STEPS>() == 100);
    emit box->editingFinished();
    CHECK(asked == 1);
    CHECK(prefs.get<SETTING::UNDO_REDO_MAX_STEPS>() == 100);
    CHECK(box->value() == 100);

    answer = true;
    box->setValue(150);
    emit box->editingFinished();
    CHECK(asked == 2);
    CHECK(prefs.get<SETTING::UNDO_REDO_MAX_STEPS>() == 150);
}

TEST_CASE("Reset all tools button asks first and resets only on yes", "[PrefSearch]")
{
    TestSession s;
    REQUIRE(s.start());
    auto* pencil = static_cast<StrokeTool*>(s.editor->tools()->getTool(PENCIL));
    const ToolSettingsGuard toolGuard(pencil);
    pencil->resetSettings();
    const qreal defaultWidth = pencil->get(ToolProperty::Width);
    pencil->set(ToolProperty::Width, defaultWidth + 7);

    ActionCommands commands(nullptr);
    commands.setCore(s.editor.get());
    PreferencesDialog dialog(nullptr);
    dialog.init(s.editor->preference());
    // Same wiring as MainWindow2.
    QObject::connect(&dialog, &PreferencesDialog::resetToolsRequested, &commands, &ActionCommands::resetAllTools);
    auto* page = dialog.findChild<ToolsPage*>();
    auto* button = dialog.findChild<QPushButton*>("resetToolsButton");
    REQUIRE(page);
    REQUIRE(button);
    bool answer = false;
    page->confirmReset = [&answer] { return answer; };
    button->click();
    CHECK(pencil->get(ToolProperty::Width) == defaultWidth + 7);
    answer = true;
    button->click();
    CHECK(pencil->get(ToolProperty::Width) == defaultWidth);
}
