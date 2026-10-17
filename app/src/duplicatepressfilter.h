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
#ifndef DUPLICATEPRESSFILTER_H
#define DUPLICATEPRESSFILTER_H

#include <QObject>

class QInputDevice;

// App-wide filter. A pen tap can reach a window as two mouse presses from two sources:
// the press Qt synthesizes from the tablet event and the press the tablet driver sends.
// The second press closes a popup that the first press opened, so drop it.
class DuplicatePressFilter : public QObject
{
public:
    using QObject::QObject;
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    struct Stamp
    {
        const QInputDevice* device = nullptr;
        quint64 time = 0;
        bool operator==(const Stamp& o) const { return device == o.device && time == o.time; }
    };
    Qt::MouseButtons mHeld;
    Stamp mPress;
    Stamp mRelease;
    Stamp mDropped;
};

#endif // DUPLICATEPRESSFILTER_H
