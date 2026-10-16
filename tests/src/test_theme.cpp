#include "catch.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDirIterator>
#include <QSet>
#include <QStyle>
#include <QSvgRenderer>

#include "editor.h"
#include "mainwindow2.h"
#include "pencildef.h"
#include "toolmanager.h"
#include "pencilsettings.h"
#include "testfixture.h"
#include "theme.h"

TEST_CASE("System after Dark restores the platform palette and style", "[Theme]")
{
    const QRgb window = QApplication::palette().color(QPalette::Window).rgb();
    const QString style = QApplication::style()->objectName();
    Theme::apply(Theme::Dark);
    Theme::apply(Theme::System);
    CHECK(QApplication::palette().color(QPalette::Window).rgb() == window);
    CHECK(QApplication::style()->objectName() == style);
}

TEST_CASE("Theme choice applies and persists", "[Theme]")
{
    const SettingsGuard guard({ SETTING_THEME });

    Theme::save(Theme::Dark);
    CHECK(Theme::saved() == Theme::Dark);
    Theme::apply(Theme::Dark);
    CHECK(Theme::isDark());

    Theme::save(Theme::Light);
    CHECK(Theme::saved() == Theme::Light);
    Theme::apply(Theme::Light);
    CHECK_FALSE(Theme::isDark());

    Theme::apply(Theme::System);
}

TEST_CASE("Accent colour persists and becomes the highlight", "[Theme]")
{
    const SettingsGuard guard({ SETTING_THEME, SETTING_ACCENT });
    pencilSettings().remove(SETTING_ACCENT);
    CHECK(Theme::savedAccent() == Theme::Blue);

    Theme::saveAccent(Theme::Pink);
    CHECK(Theme::savedAccent() == Theme::Pink);
    Theme::apply(Theme::Dark);
    CHECK(QApplication::palette().color(QPalette::Highlight) == Theme::palette(Theme::Dark, Theme::Pink).color(QPalette::Highlight));

    // Every choice gives its own highlight.
    QSet<QRgb> seen;
    for (int a = 0; a < Theme::ACCENT_COUNT; ++a)
        seen << Theme::palette(Theme::Light, static_cast<Theme::Accent>(a)).color(QPalette::Highlight).rgb();
    CHECK(seen.size() == Theme::ACCENT_COUNT);
    CHECK(Theme::ACCENT_COUNT == 8);

    Theme::saveAccent(Theme::Blue);
    Theme::apply(Theme::System);
}

namespace
{
// The colour of the most opaque pixel: the stroke, not its antialiased edge.
QColor strokeColor(const QIcon& icon, QIcon::State state = QIcon::Off)
{
    const QImage img = icon.pixmap(QSize(24, 24), 1.0, QIcon::Normal, state).toImage();
    QColor best(Qt::transparent);
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixelColor(x, y).alpha() > best.alpha())
                best = img.pixelColor(x, y);
    return best;
}

QStringList missingIconWarnings;
void collectIconWarnings(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    if (msg.contains("icon", Qt::CaseInsensitive))
        missingIconWarnings << msg;
}
}

TEST_CASE("Icons recolour from the theme text colour", "[Theme]")
{
    const SettingsGuard guard({ SETTING_THEME, SETTING_ACCENT });
    const QIcon pencil = Theme::icon(":/icons/tabler/pencil.svg");
    REQUIRE_FALSE(pencil.isNull());

    Theme::apply(Theme::Dark);
    CHECK(strokeColor(pencil).rgb() == QApplication::palette().color(QPalette::WindowText).rgb());
    CHECK(strokeColor(pencil, QIcon::On).rgb() == QApplication::palette().color(QPalette::Highlight).rgb());
    Theme::apply(Theme::Light);
    CHECK(strokeColor(pencil).rgb() == QApplication::palette().color(QPalette::WindowText).rgb());

    // Sharp at 200 %: drawn at device pixels, not scaled up.
    CHECK(pencil.pixmap(QSize(24, 24), 2.0).size() == QSize(48, 48));

    CHECK(Theme::icon(":/icons/tabler/no-such-icon.svg").isNull());
    Theme::apply(Theme::System);
}

TEST_CASE("Every icon resource exists and loads", "[Theme]")
{
    int count = 0;
    for (QDirIterator it(":/icons/tabler", { "*.svg" }); it.hasNext(); ++count)
    {
        const QString path = it.next();
        INFO(path.toStdString());
        CHECK_FALSE(Theme::icon(path).isNull());
    }
    CHECK(count > 0);
    for (QDirIterator it(":/icons/general", { "cursor-*.svg" }); it.hasNext();)
    {
        const QString path = it.next();
        INFO(path.toStdString());
        CHECK(QSvgRenderer(path).isValid());
    }

    // Every icon the UI asks for loads: set icons are not null and nothing warns.
    missingIconWarnings.clear();
    const QtMessageHandler old = qInstallMessageHandler(collectIconWarnings);
    {
        MainWindow2 window;
        window.show();
        QApplication::processEvents();
        Editor* editor = window.findChild<Editor*>();
        REQUIRE(editor);
        for (int tool = 0; tool < TOOL_TYPE_COUNT; ++tool)
            editor->tools()->setCurrentTool(static_cast<ToolType>(tool));
        QApplication::processEvents();

        QStringList broken;
        // A null icon with a cache key was set from a file that did not load.
        for (QAction* a : window.findChildren<QAction*>())
            if (a->icon().cacheKey() != 0 && a->icon().isNull())
                broken << a->objectName();
        for (QAbstractButton* b : window.findChildren<QAbstractButton*>())
            if (b->icon().cacheKey() != 0 && b->icon().isNull())
                broken << b->objectName();
        INFO(broken.join("\n").toStdString());
        CHECK(broken.isEmpty());
    }
    qInstallMessageHandler(old);
    INFO(missingIconWarnings.join("\n").toStdString());
    CHECK(missingIconWarnings.isEmpty());
}
