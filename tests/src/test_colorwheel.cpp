#include "catch.hpp"

#include <QApplication>
#include <QEventLoop>
#include <QTimer>

#include "colorwheel.h"

namespace
{
void waitMs(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
}

TEST_CASE("ColorWheel rebuilds once after a burst of resizes", "[ColorWheel]")
{
    ColorWheel wheel(nullptr);
    wheel.resize(200, 200);
    wheel.show();
    QApplication::processEvents();
    const int before = wheel.rebuildCount();

    for (int i = 0; i < 40; ++i)
    {
        wheel.resize(200 + i * 3, 200 + i * 3);
        QApplication::processEvents();
    }

    CHECK(wheel.rebuildCount() == before);
    waitMs(300);
    CHECK(wheel.rebuildCount() == before + 1);
}

TEST_CASE("ColorWheel after a resize looks like a wheel made at that size", "[ColorWheel]")
{
    ColorWheel resized(nullptr);
    resized.resize(200, 200);
    resized.show();
    QApplication::processEvents();
    resized.resize(260, 240);
    QApplication::processEvents();
    waitMs(300);

    ColorWheel fresh(nullptr);
    fresh.resize(260, 240);
    fresh.show();
    QApplication::processEvents();

    CHECK(resized.grab().toImage() == fresh.grab().toImage());
}
