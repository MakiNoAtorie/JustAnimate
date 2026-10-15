#include "catch.hpp"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "object.h"
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
