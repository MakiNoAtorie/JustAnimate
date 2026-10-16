#include "catch.hpp"

#include <QFile>
#include <QIcon>
#include <QImage>
#include <QString>

namespace {
const QString dataDir = QStringLiteral(JUSTANIMATE_SOURCE_DIR "/app/data/");
}

TEST_CASE("Window icon and About logo load from resources", "[AppIcon]")
{
    const QIcon icon(":/icons/icon.png");
    CHECK_FALSE(icon.isNull());
    CHECK(icon.availableSizes().contains(QSize(256, 256)));
    CHECK(QImage(":/icons/logo.png").size() == QSize(72, 72));
}

TEST_CASE("Every packaged icon file exists", "[AppIcon]")
{
    QStringList files = { "icons/justanimate.svg", "justanimate.ico", "justanimate.icns" };
    for (int s : { 16, 24, 32, 48, 64, 128, 256 })
        files << QString("hicolor/%1x%1/apps/io.github.makinoatorie.JustAnimate.png").arg(s);
    for (const QString& f : files)
        CHECK(QFile::exists(dataDir + f));

    // ICO directory lists every Windows size
    QFile ico(dataDir + "justanimate.ico");
    REQUIRE(ico.open(QIODevice::ReadOnly));
    const QByteArray head = ico.read(6);
    CHECK(head.at(4) == 6);
}
