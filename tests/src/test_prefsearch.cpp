#include "catch.hpp"

#include "prefsearch.h"
#include "preferencesdialog.h"

#include <QLabel>
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