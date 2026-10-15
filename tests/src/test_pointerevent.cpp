#include "catch.hpp"

#include <QPointingDevice>

#include "pointerevent.h"

TEST_CASE("PointerEvent: tablet event exposes tilt and timestamp", "[PointerEvent]")
{
    QTabletEvent tablet(QEvent::TabletMove, QPointingDevice::primaryPointingDevice(),
                        QPointF(1, 2), QPointF(1, 2), 0.5, 20.0f, -30.0f, 0.0f, 0.0, 0.0f,
                        Qt::NoModifier, Qt::NoButton, Qt::NoButton);
    tablet.setTimestamp(1234);

    PointerEvent event(&tablet, QPointF());

    CHECK(event.tiltX() == 20.0);
    CHECK(event.tiltY() == -30.0);
    CHECK(event.timestamp() == 1234);
}

TEST_CASE("PointerEvent: mouse event has zero tilt and a timestamp", "[PointerEvent]")
{
    QMouseEvent mouse(QEvent::MouseMove, QPointF(1, 2), QPointF(1, 2),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    mouse.setTimestamp(5678);

    PointerEvent event(&mouse, QPointF());

    CHECK(event.tiltX() == 0.0);
    CHECK(event.tiltY() == 0.0);
    CHECK(event.timestamp() == 5678);
}
