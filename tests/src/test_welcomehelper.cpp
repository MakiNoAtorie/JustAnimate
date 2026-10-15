#include "catch.hpp"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <memory>

#include "layercamera.h"
#include "object.h"
#include "objectdata.h"
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
