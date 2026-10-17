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
#include "duplicatepressfilter.h"

#include <QMouseEvent>

#include <cstdlib>

bool DuplicatePressFilter::eventFilter(QObject* obj, QEvent* e)
{
    const QEvent::Type type = e->type();
    if (!e->spontaneous() || !obj->isWindowType()
        || (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease && type != QEvent::MouseButtonDblClick))
    {
        return false;
    }
    const auto* me = static_cast<QMouseEvent*>(e);
    const Stamp stamp{ me->device(), me->timestamp() };
    if (type == QEvent::MouseButtonDblClick) { return stamp == mDropped; }

    // Qt can send one press to a popup window and then to the window under it: same stamp, not a duplicate.
    // The two sources of a pen tap come a few ms apart; the window keeps a lost release from blocking later clicks.
    const quint64 window = 200;
    const bool press = type == QEvent::MouseButtonPress;
    const Stamp& last = press ? mPress : mRelease;
    const bool held = mHeld.testFlag(me->button());
    if (held == press && !(stamp == last) && quint64(std::llabs(qint64(stamp.time - last.time))) < window)
    {
        mDropped = stamp;
        return true;
    }
    mHeld.setFlag(me->button(), press);
    (press ? mPress : mRelease) = stamp;
    return false;
}
