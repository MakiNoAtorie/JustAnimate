#include "catch.hpp"

#include <algorithm>
#include <deque>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QMouseEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextEdit>

#include "basedockwidget.h"
#include "defaultlayout.h"
#include "editor.h"
#include "toolmanager.h"
#include "mainwindow2.h"
#include "pencildef.h"
#include "spinslider.h"
#include "testfixture.h"

namespace
{
struct TestDock
{
    TestDock(QMainWindow* win, const char* name) : dock(name, win)
    {
        dock.setObjectName(name);
        body.setMinimumSize(120, 90);
        dock.setWidget(&body);
    }
    QDockWidget dock;
    QWidget body;
};

// Declare after the window so the docks go first.
struct Docks
{
    QDockWidget* add(QMainWindow* win, const char* name) { return &owned.emplace_back(win, name).dock; }
    std::deque<TestDock> owned;
};
}

TEST_CASE("Default dock layout", "[DockLayout]")
{
    for (const QSize screen : {QSize(1366, 768), QSize(3840, 2160)})
    {
        QMainWindow win;
        QTextEdit central;
        win.setCentralWidget(&central);
        Docks owned;
        DefaultDockLayout docks;
        docks.left = {owned.add(&win, "a"), owned.add(&win, "b"), owned.add(&win, "c")};
        docks.right = {owned.add(&win, "d"), owned.add(&win, "e"), owned.add(&win, "f")};
        docks.bottom = owned.add(&win, "g");
        const QList<QDockWidget*> all = docks.left + docks.right + QList<QDockWidget*>{docks.bottom};

        // Scramble first so the test proves the function restores the layout
        for (QDockWidget* d : all) { win.addDockWidget(Qt::TopDockWidgetArea, d); }
        win.resize(screen);
        applyDefaultDockLayout(&win, docks, screen);
        win.show();
        QApplication::processEvents();

        const QRect inside = win.rect();
        for (int i = 0; i < all.size(); ++i)
        {
            REQUIRE(all[i]->isVisible());
            REQUIRE(!all[i]->isFloating());
            REQUIRE(inside.contains(all[i]->geometry()));
            for (int j = i + 1; j < all.size(); ++j)
            {
                INFO(all[i]->objectName().toStdString() << " vs " << all[j]->objectName().toStdString());
                REQUIRE(!all[i]->geometry().intersects(all[j]->geometry()));
            }
        }
        REQUIRE(win.centralWidget()->width() > screen.width() / 2);
        REQUIRE(win.dockWidgetArea(docks.bottom) == Qt::BottomDockWidgetArea);
        REQUIRE(win.dockWidgetArea(docks.left[0]) == Qt::LeftDockWidgetArea);
        REQUIRE(win.dockWidgetArea(docks.right[0]) == Qt::RightDockWidgetArea);
    }
}

TEST_CASE("Workspaces", "[DockLayout]")
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("ws.ini"), QSettings::IniFormat);
    QMainWindow win;
    QTextEdit central;
    win.setCentralWidget(&central);
    Docks owned;
    DefaultDockLayout docks;
    docks.left = {owned.add(&win, "ToolBox")};
    docks.right = {owned.add(&win, "ColorPalette"), owned.add(&win, "Onion Skin")};
    docks.bottom = owned.add(&win, "TimeLine");
    const QSize screen(1366, 768);
    win.resize(screen);
    win.show();

    switchWorkspace(&win, docks, screen, settings, "Animate");
    REQUIRE(docks.bottom->isVisible());

    switchWorkspace(&win, docks, screen, settings, "Ink");
    QApplication::processEvents();
    REQUIRE(!docks.bottom->isVisible());
    REQUIRE(!docks.right[0]->isVisible());
    REQUIRE(docks.left[0]->isVisible());

    // An edit to a workspace comes back when it is selected again
    docks.right[1]->show();
    switchWorkspace(&win, docks, screen, settings, "Draw");
    REQUIRE(!docks.right[1]->isVisible());
    switchWorkspace(&win, docks, screen, settings, "Ink");
    QApplication::processEvents();
    REQUIRE(docks.right[1]->isVisible());
    REQUIRE(currentWorkspace(settings) == "Ink");
}

TEST_CASE("Reset restores only the current workspace", "[DockLayout]")
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("ws.ini"), QSettings::IniFormat);
    QMainWindow win;
    QTextEdit central;
    win.setCentralWidget(&central);
    Docks owned;
    DefaultDockLayout docks;
    docks.left = {owned.add(&win, "ToolBox")};
    docks.right = {owned.add(&win, "ColorPalette"), owned.add(&win, "Onion Skin")};
    docks.bottom = owned.add(&win, "TimeLine");
    const QSize screen(1366, 768);
    win.resize(screen);
    win.show();

    // Ink keeps an edit: Onion Skin shown
    switchWorkspace(&win, docks, screen, settings, "Ink");
    docks.right[1]->show();
    switchWorkspace(&win, docks, screen, settings, "Draw");
    docks.bottom->show();
    saveCurrentWorkspace(&win, settings);

    resetCurrentWorkspace(&win, docks, screen, settings);
    QApplication::processEvents();
    REQUIRE(currentWorkspace(settings) == "Draw");
    REQUIRE(!docks.bottom->isVisible());

    switchWorkspace(&win, docks, screen, settings, "Ink");
    QApplication::processEvents();
    REQUIRE(docks.right[1]->isVisible());
}

TEST_CASE("Old saved Layouts are dropped once", "[DockLayout]")
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("ws.ini"), QSettings::IniFormat);
    settings.setValue(SETTING_WINDOW_STATE, QByteArray("old"));
    settings.setValue(SETTING_WORKSPACE_STATE + QString("Ink"), QByteArray("old"));
    settings.setValue(SETTING_WORKSPACE_CURRENT, "Ink");

    REQUIRE(dropOutdatedLayouts(settings));
    REQUIRE(!settings.contains(SETTING_WINDOW_STATE));
    REQUIRE(!settings.contains(SETTING_WORKSPACE_STATE + QString("Ink")));
    REQUIRE(currentWorkspace(settings) == "Ink");

    // A Layout saved by this version survives the next start
    settings.setValue(SETTING_WINDOW_STATE, QByteArray("new"));
    REQUIRE(!dropOutdatedLayouts(settings));
    REQUIRE(settings.value(SETTING_WINDOW_STATE).toByteArray() == "new");
}

TEST_CASE("MainWindow2 docks every Panel over an old saved state", "[DockLayout]")
{
    QSettings& settings = pencilSettings();
    const SettingsGuard guard({SETTING_WINDOW_STATE, SETTING_LAYOUT_VERSION, SETTING_WORKSPACE_CURRENT, "Workspace/State/Animate"});
    {
        MainWindow2 old;
        old.show();
        for (QDockWidget* dock : old.findChildren<QDockWidget*>()) { dock->setFloating(true); }
        settings.setValue(SETTING_WINDOW_STATE, old.saveState());
    }
    settings.remove(SETTING_LAYOUT_VERSION);

    MainWindow2 window;
    window.show();
    QApplication::processEvents();
    const QList<BaseDockWidget*> panels = window.findChildren<BaseDockWidget*>();
    REQUIRE(!panels.isEmpty());
    for (BaseDockWidget* panel : panels)
    {
        INFO(panel->objectName().toStdString());
        CHECK(!panel->isFloating());
    }

    // A floating Panel stays above JustAnimate only; a double-click on its title docks it back
    for (BaseDockWidget* panel : panels)
    {
        INFO(panel->objectName().toStdString());
        panel->setFloating(true);
        QApplication::processEvents();
        CHECK(!panel->windowFlags().testFlag(Qt::WindowStaysOnTopHint));

        QWidget* title = panel->titleBarWidget();
        const QPointF at(4, title->height() / 2);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, at, title->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(title, &dbl);
        CHECK(!panel->isFloating());
    }
}

TEST_CASE("MainWindow2 default Layout docks Tool settings on the left", "[DockLayout]")
{
    QSettings& settings = pencilSettings();
    const SettingsGuard guard({SETTING_WINDOW_STATE, SETTING_WINDOW_GEOMETRY, SETTING_LAYOUT_VERSION, SETTING_WORKSPACE_CURRENT,
                               "Workspace/State/Animate"});
    settings.remove(SETTING_WINDOW_STATE);
    settings.remove("Workspace/State/Animate");
    settings.remove(SETTING_WORKSPACE_CURRENT);

    MainWindow2 window;
    window.resize(1366, 768);
    window.show();
    QApplication::processEvents();

    const auto dock = [&window](const char* name) { return window.findChild<QDockWidget*>(name); };
    const auto checkArrangement = [&]
    {
        for (const char* name : {"BrushPresets", "ToolBox", "ToolOption"})
        {
            INFO(name);
            CHECK(window.dockWidgetArea(dock(name)) == Qt::LeftDockWidgetArea);
        }
        CHECK(window.tabifiedDockWidgets(dock("BrushPresets")).contains(dock("ToolBox")));
        for (const char* name : {"ColorWheel", "ColorPalette", "Color Inspector", "Storyboard"})
        {
            INFO(name);
            CHECK(window.dockWidgetArea(dock(name)) == Qt::RightDockWidgetArea);
        }
        CHECK(window.tabifiedDockWidgets(dock("ColorPalette")).contains(dock("Color Inspector")));
        CHECK(window.tabifiedDockWidgets(dock("ColorPalette")).contains(dock("Storyboard")));
        CHECK(window.dockWidgetArea(dock("TimeLine")) == Qt::BottomDockWidgetArea);
        CHECK(window.tabifiedDockWidgets(dock("TimeLine")).contains(dock("Onion Skin")));
        CHECK(!dock("ToolOption")->isFloating());
    };
    checkArrangement();

    // Size, then opacity, then the rest: in a row at the top, in a column at a side
    QDockWidget* toolSettings = dock("ToolOption");
    window.addDockWidget(Qt::TopDockWidgetArea, toolSettings);
    QApplication::processEvents();
    const auto at = [toolSettings](const char* name)
    {
        QWidget* w = toolSettings->findChild<QWidget*>(name);
        return w->mapTo(toolSettings, QPoint(0, 0));
    };
    const auto othersAfterOpacity = [&](bool row)
    {
        const QPoint opacity = at("opacitySlider");
        for (SpinSlider* slider : toolSettings->findChildren<SpinSlider*>())
        {
            const QString name = slider->objectName();
            if (!slider->isVisible() || name == "sizeSlider" || name == "opacitySlider") { continue; }
            const QPoint p = slider->mapTo(toolSettings, QPoint(0, 0));
            INFO(name.toStdString());
            CHECK((row ? p.x() > opacity.x() : p.y() > opacity.y()));
        }
    };
    CHECK(at("opacitySlider").x() > at("sizeSlider").x());
    CHECK(at("opacitySlider").y() == at("sizeSlider").y());
    othersAfterOpacity(true);

    // In the strip every tool's options stay one control high, whatever the style
    QComboBox combo;
    QDoubleSpinBox spin;
    QPushButton button;
    button.setIconSize(QSize(24, 24));
    button.setIcon(QIcon(":/icons/tabler/arrows-move.svg"));
    const int rowHeight = std::max({toolSettings->findChild<QWidget*>("sizeSlider")->sizeHint().height(), combo.sizeHint().height(),
                                    spin.sizeHint().height(), button.sizeHint().height()});
    QWidget* contents = toolSettings->findChild<QWidget*>("scrollAreaWidgetContents");
    for (int type = 0; type < TOOL_TYPE_COUNT; ++type)
    {
        window.mEditor->tools()->setCurrentTool(static_cast<ToolType>(type));
        QApplication::processEvents();
        for (QWidget* options : contents->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
        {
            if (options->isHidden()) { continue; }
            INFO("tool " << type << " " << options->metaObject()->className() << " " << options->sizeHint().height() << " > " << rowHeight);
            CHECK(options->sizeHint().height() <= rowHeight);
        }
    }
    window.mEditor->tools()->setCurrentTool(PENCIL);
    QApplication::processEvents();

    window.addDockWidget(Qt::LeftDockWidgetArea, toolSettings);
    QApplication::processEvents();
    CHECK(at("opacitySlider").y() > at("sizeSlider").y());
    othersAfterOpacity(false);

    toolSettings->setFloating(true);
    QApplication::processEvents();
    CHECK(at("opacitySlider").y() > at("sizeSlider").y());

    // Window > Layout lists the Layouts and Reset layout, which returns Tool settings to the left
    QMenu* layoutMenu = nullptr;
    for (QMenu* menu : window.menuBar()->findChildren<QMenu*>())
    {
        if (menu->title() == "Layout") { layoutMenu = menu; }
    }
    REQUIRE(layoutMenu);
    QStringList entries;
    QAction* reset = nullptr;
    for (QAction* a : layoutMenu->actions())
    {
        if (a->isSeparator()) { continue; }
        entries << a->text();
        if (a->text() == "Reset layout") { reset = a; }
    }
    CHECK(entries == QStringList{"Draw", "Animate", "Ink", "Reset layout"});
    CHECK(window.findChild<QAction*>("actionLockWindows")->text().toStdString() == "Lock Panels");
    REQUIRE(reset);
    reset->trigger();
    QApplication::processEvents();
    checkArrangement();
    CHECK(at("opacitySlider").y() > at("sizeSlider").y());
}

TEST_CASE("MainWindow2 has one Command bar that the Window menu hides and shows", "[DockLayout]")
{
    MainWindow2 window;
    window.show();
    QApplication::processEvents();

    const QList<QToolBar*> bars = window.findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
    REQUIRE(bars.size() == 1);
    QToolBar* bar = bars.first();
    CHECK(bar->objectName() == "CommandBar");

    // Main, View and Overlay commands in that order, a separator between groups
    QStringList names;
    for (QAction* a : bar->actions())
        names << (a->isSeparator() ? QString("|") : a->objectName());
    CHECK(names.mid(0, 4) == QStringList({"actionNew", "actionOpen", "actionSave", "|"}));
    CHECK(names.indexOf("actionUndo") < names.indexOf("actionCut"));
    CHECK(names.indexOf("actionCut") < names.indexOf("actionZoom_In"));
    CHECK(names.indexOf("actionZoom_In") < names.indexOf("actionGrid"));
    CHECK(names.count("|") == 6);

    QAction* toggle = bar->toggleViewAction();
    CHECK(window.findChild<QMenu*>("menuWindows")->actions().contains(toggle));
    CHECK(toggle->text() == "Command bar");
    toggle->trigger();
    QApplication::processEvents();
    CHECK(!bar->isVisible());
    const QByteArray hidden = window.saveState();
    toggle->trigger();
    QApplication::processEvents();
    CHECK(bar->isVisible());
    CHECK(window.restoreState(hidden));
    CHECK(!bar->isVisible());
}
TEST_CASE("MainWindow2 Tool settings keep the chosen tool while Alt holds the eyedropper", "[DockLayout]")
{
    MainWindow2 window;
    window.show();
    QApplication::processEvents();
    ToolManager* tools = window.mEditor->tools();
    tools->setCurrentTool(PENCIL);
    QWidget* size = window.findChild<QDockWidget*>("ToolOption")->findChild<QWidget*>("sizeSlider");
    REQUIRE(size->isVisible());

    REQUIRE(tools->setTemporaryTool(EYEDROPPER, {}, Qt::AltModifier));
    CHECK(size->isVisible());
    tools->clearTemporaryTool();
    CHECK(size->isVisible());
}
