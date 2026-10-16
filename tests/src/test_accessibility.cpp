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

#include <algorithm>
#include <cmath>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QKeyEvent>
#include <QListWidget>
#include <QSet>
#include <QSlider>

#include "editor.h"
#include "layermanager.h"
#include "mainwindow2.h"
#include "preferencesdialog.h"
#include "theme.h"
#include "toolmanager.h"
#include "timelinecells.h"

namespace
{
// WCAG 2.x relative luminance and contrast ratio.
double luminance(const QColor& c)
{
    auto channel = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

bool isControl(const QWidget* w)
{
    // Qt's own helpers (toolbar overflow button) are not ours to label.
    if (w->objectName().startsWith("qt_")) return false;
    return qobject_cast<const QAbstractButton*>(w) || qobject_cast<const QAbstractSpinBox*>(w)
        || qobject_cast<const QComboBox*>(w) || qobject_cast<const QSlider*>(w);
}

QString describe(const QWidget* w)
{
    QString path;
    for (const QObject* o = w; o; o = o->parent())
        path.prepend(QString("/%1(%2)").arg(o->metaObject()->className(), o->objectName()));
    return path;
}

}

TEST_CASE("Theme text roles pass WCAG AA contrast", "[Accessibility]")
{
    const std::pair<QPalette::ColorRole, QPalette::ColorRole> pairs[] = {
        { QPalette::WindowText, QPalette::Window },
        { QPalette::Text, QPalette::Base },
        { QPalette::Text, QPalette::AlternateBase },
        { QPalette::ButtonText, QPalette::Button },
        { QPalette::ToolTipText, QPalette::ToolTipBase },
        { QPalette::HighlightedText, QPalette::Highlight },
        { QPalette::Link, QPalette::Base },
        { QPalette::PlaceholderText, QPalette::Base },
    };
    for (Theme::Mode mode : { Theme::Light, Theme::Dark })
    {
        const QPalette p = Theme::palette(mode);
        for (const auto& [fg, bg] : pairs)
        {
            INFO("mode " << mode << " roles " << fg << " on " << bg);
            CHECK(contrast(p.color(fg), p.color(bg)) >= 4.5);
        }
    }
}

TEST_CASE("Every visible main window control has a tooltip", "[Accessibility]")
{
    MainWindow2 window;
    window.show();
    QApplication::processEvents();

    Editor* editor = window.findChild<Editor*>();
    REQUIRE(editor);

    // Each tool shows its own options: check them all.
    QSet<QString> missing;
    for (int tool = 0; tool < TOOL_TYPE_COUNT; ++tool)
    {
        editor->tools()->setCurrentTool(static_cast<ToolType>(tool));
        QApplication::processEvents();
        for (QWidget* w : window.findChildren<QWidget*>())
        {
            if (isControl(w) && w->isVisibleTo(&window) && w->toolTip().isEmpty())
                missing << describe(w);
        }
    }
    INFO(QStringList(missing.begin(), missing.end()).join("\n").toStdString());
    CHECK(missing.isEmpty());
}

TEST_CASE("Every visible preference control has a tooltip", "[Accessibility]")
{
    MainWindow2 window;
    Editor* editor = window.findChild<Editor*>();
    REQUIRE(editor);
    PreferencesDialog dialog(&window);
    dialog.init(editor->preference());
    auto* pageList = dialog.findChild<QListWidget*>("contentsWidget");
    REQUIRE(pageList);
    REQUIRE(pageList->count() > 1);

    QSet<QString> missing;
    for (int row = 0; row < pageList->count(); ++row)
    {
        pageList->setCurrentRow(row);
        for (QWidget* w : dialog.findChildren<QWidget*>())
        {
            if (isControl(w) && w->isVisibleTo(&dialog) && w->toolTip().isEmpty())
                missing << describe(w);
        }
    }
    INFO(QStringList(missing.begin(), missing.end()).join("\n").toStdString());
    CHECK(missing.isEmpty());
}
TEST_CASE("Tab reaches every visible dock", "[Accessibility]")
{
    MainWindow2 window;
    window.show();
    QApplication::processEvents();

    // Saved settings can float a dock: a floating dock is its own window with its own chain.
    for (QDockWidget* dock : window.findChildren<QDockWidget*>()) dock->setFloating(false);
    QApplication::processEvents();

    // Tab follows the focus chain and stops on visible, enabled widgets that take Tab focus.
    QSet<QDockWidget*> reached;
    for (QWidget* w = window.nextInFocusChain(); w != &window; w = w->nextInFocusChain())
    {
        if ((w->focusPolicy() & Qt::TabFocus) != Qt::TabFocus || !w->isVisibleTo(&window) || !w->isEnabled()) continue;
        for (QWidget* p = w; p; p = p->parentWidget())
        {
            if (auto* dock = qobject_cast<QDockWidget*>(p)) { reached << dock; break; }
        }
    }
    for (QDockWidget* dock : window.findChildren<QDockWidget*>())
    {
        INFO(dock->objectName().toStdString());
        if (dock->isVisibleTo(&window)) CHECK(reached.contains(dock));
    }
}
TEST_CASE("Timeline arrow keys move the frame and the layer", "[Accessibility]")
{
    MainWindow2 window;
    window.show();
    QApplication::processEvents();
    Editor* editor = window.findChild<Editor*>();
    REQUIRE(editor);
    REQUIRE(editor->layers()->count() > 1);

    const auto cells = window.findChildren<TimeLineCells*>();
    REQUIRE(cells.size() == 2);
    for (TimeLineCells* c : cells)
        CHECK((c->focusPolicy() & Qt::TabFocus) == Qt::TabFocus);

    TimeLineCells* tracks = cells.last();
    editor->scrubTo(5);
    editor->layers()->setCurrentLayer(0);

    auto press = [&](int key) {
        QKeyEvent e(QEvent::KeyPress, key, Qt::NoModifier);
        QApplication::sendEvent(tracks, &e);
    };
    press(Qt::Key_Right);
    CHECK(editor->currentFrame() == 6);
    press(Qt::Key_Left);
    press(Qt::Key_Left);
    CHECK(editor->currentFrame() == 4);
    press(Qt::Key_Up);
    CHECK(editor->layers()->currentLayerIndex() == 1);
    press(Qt::Key_Down);
    CHECK(editor->layers()->currentLayerIndex() == 0);
}
