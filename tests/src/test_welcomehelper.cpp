#include "catch.hpp"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include "layercamera.h"
#include "object.h"
#include "objectdata.h"
#include "preferencemanager.h"
#include "testfixture.h"
#include "welcomedialog.h"
#include "welcomehelper.h"

TEST_CASE("Welcome screen helper", "[Welcome]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString project = dir.filePath("a.pclx");
    const QString gone = dir.filePath("gone.pclx");
    QFile f(project);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.close();

    SECTION("thumbnail path is stable and distinct per project")
    {
        CHECK(Welcome::thumbnailPath(project) == Welcome::thumbnailPath(project));
        CHECK(Welcome::thumbnailPath(project) != Welcome::thumbnailPath(gone));
        CHECK(Welcome::thumbnailPath(project).endsWith(".png"));
    }

    SECTION("missing files are flagged, not dropped")
    {
        QFile::remove(Welcome::thumbnailPath(project));
        const auto entries = Welcome::recentEntries({ project, gone });
        REQUIRE(entries.size() == 2);
        CHECK_FALSE(entries[0].missing);
        CHECK(entries[0].thumbnail.isEmpty());
        CHECK(entries[1].missing);
        CHECK(entries[1].name == "gone");
    }

    SECTION("saved thumbnail is picked up")
    {
        Object obj;
        obj.init();
        obj.addNewCameraLayer();
        REQUIRE(Welcome::saveThumbnail(&obj, project, 1));
        const auto entries = Welcome::recentEntries({ project });
        REQUIRE(entries.size() == 1);
        CHECK_FALSE(entries[0].thumbnail.isEmpty());
        CHECK_FALSE(QImage(entries[0].thumbnail).isNull());
        QFile::remove(entries[0].thumbnail);
    }
}

TEST_CASE("Welcome templates", "[Welcome]")
{
    REQUIRE_FALSE(Welcome::templates().isEmpty());
    for (int i = 0; i < Welcome::templates().size(); ++i)
    {
        const Welcome::Template& t = Welcome::templates().at(i);
        std::unique_ptr<Object> obj(Welcome::newObjectFromTemplate(i));
        REQUIRE(obj);
        CHECK(obj->data()->getFrameRate() == t.fps);
        auto cameras = obj->getLayersByType<LayerCamera>();
        REQUIRE(cameras.size() == 1);
        CHECK(cameras.front()->getViewSize() == t.size);
        CHECK(obj->getLayerCount() == 2);
    }
    CHECK(Welcome::newObjectFromTemplate(-1) == nullptr);
    CHECK(Welcome::newObjectFromTemplate(Welcome::templates().size()) == nullptr);
}

TEST_CASE("Welcome anime presets", "[Welcome]")
{
    const QList<QPair<QString, QSize>> expected = { { "Anime 1080p", QSize(1920, 1080) }, { "Anime 720p", QSize(1280, 720) } };
    for (const auto& e : expected)
    {
        int index = -1;
        for (int i = 0; i < Welcome::templates().size(); ++i)
        {
            if (Welcome::templates().at(i).name == e.first) { index = i; }
        }
        REQUIRE(index >= 0);
        std::unique_ptr<Object> obj(Welcome::newObjectFromTemplate(index));
        REQUIRE(obj);
        CHECK(obj->data()->getFrameRate() == 24);
        auto cameras = obj->getLayersByType<LayerCamera>();
        REQUIRE(cameras.size() == 1);
        CHECK(cameras.front()->getViewSize() == e.second);
        CHECK(cameras.front()->getViewRect() == QRect(QPoint(-e.second.width() / 2, -e.second.height() / 2), e.second));
    }
}

TEST_CASE("Welcome dialog preselects the last chosen template by name", "[Welcome]")
{
    SettingsGuard guard({ SETTING_LAST_TEMPLATE });
    TestSession s;
    REQUIRE(s.start());
    PreferenceManager* prefs = s.editor->preference();
    prefs->set(SETTING::LAST_TEMPLATE, QString("Square 1080"));
    CHECK(pencilSettings().value(SETTING_LAST_TEMPLATE).toString() == "Square 1080");

    const auto currentTemplate = [&](const QString& last) {
        WelcomeDialog dialog(0, {}, last);
        return dialog.findChild<QComboBox*>("welcomeNew")->currentIndex();
    };
    CHECK(currentTemplate(prefs->getString(SETTING::LAST_TEMPLATE)) == Welcome::templateIndex("Square 1080"));
    CHECK(Welcome::templateIndex("Square 1080") > 0);
    CHECK(currentTemplate("Removed template") == 0);
    CHECK(currentTemplate(QString()) == 0);
}
TEST_CASE("Welcome New project row lists templates, a separator, then presets", "[Welcome]")
{
    WelcomeDialog dialog(0, {}, QString());
    auto* row = dialog.findChild<QComboBox*>("welcomeNew");
    REQUIRE(row);
    const int n = Welcome::templates().size();
    REQUIRE(row->count() >= n + 2);
    CHECK(row->itemText(0).startsWith(Welcome::templates().at(0).name));
    CHECK(row->itemData(n, Qt::AccessibleDescriptionRole).toString() == "separator");
    CHECK(row->itemText(n + 1) == "Default");
}

TEST_CASE("Welcome hides Recent projects while the list is empty", "[Welcome]")
{
    WelcomeDialog empty(0, {}, QString());
    CHECK(empty.findChild<QListWidget*>("welcomeRecent")->isHidden());

    WelcomeDialog some(0, { "missing.pclx" }, QString());
    CHECK_FALSE(some.findChild<QListWidget*>("welcomeRecent")->isHidden());
}

TEST_CASE("Welcome checkbox hides the screen and picks the startup choice", "[Welcome]")
{
    WelcomeDialog dialog(0, {}, QString());
    auto* hide = dialog.findChild<QCheckBox*>("welcomeHide");
    REQUIRE(hide);
    CHECK(hide->text() == "Don't show this screen again");
    CHECK_FALSE(dialog.shouldHide());
    hide->setChecked(true);
    CHECK(dialog.shouldHide());

    CHECK(Welcome::hiddenStartupAction(true) == STARTUP_LAST_PROJECT);
    CHECK(Welcome::hiddenStartupAction(false) == STARTUP_DEFAULT_PRESET);
}
