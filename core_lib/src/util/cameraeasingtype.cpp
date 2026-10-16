/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include <QCoreApplication>
#include <QEasingCurve>

#include "cameraeasingtype.h"

// Array structure must match CameraEasingType
static const char* interpolationNames[] {
    QT_TRANSLATE_NOOP("CameraEasingType", "Linear"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Moderate Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Moderate Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Moderate Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Moderate Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Quick Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Quick Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Quick Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Quick Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fast Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fast Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fast Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fast Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Faster Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Faster Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Faster Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Faster Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Slow Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Slow Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Slow Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Slow Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fastest Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fastest Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fastest Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Fastest Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Circle-based Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Circle-based Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Circle-based Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Circle-based Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Elastic Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Elastic Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Elastic Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Elastic Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Overshoot Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Overshoot Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Overshoot Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Overshoot Ease-out - Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Bounce Ease-in"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Bounce Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Bounce Ease-in - Ease-out"),
    QT_TRANSLATE_NOOP("CameraEasingType", "Bounce Ease-out - Ease-in"),
};

QString getInterpolationText(CameraEasingType type)
{
    return QCoreApplication::translate("CameraEasingType", interpolationNames[static_cast<int>(type)]);
}

qreal easedProgress(CameraEasingType type, qreal percent)
{
    QEasingCurve easing;

    switch (type)
    {
    case CameraEasingType::LINEAR : easing.setType(QEasingCurve::Linear); break;
    case CameraEasingType::INQUAD : easing.setType(QEasingCurve::InQuad); break;
    case CameraEasingType::OUTQUAD : easing.setType(QEasingCurve::OutQuad); break;
    case CameraEasingType::INOUTQUAD : easing.setType(QEasingCurve::InOutQuad); break;
    case CameraEasingType::OUTINQUAD : easing.setType(QEasingCurve::OutInQuad); break;
    case CameraEasingType::INCUBIC : easing.setType(QEasingCurve::InCubic); break;
    case CameraEasingType::OUTCUBIC : easing.setType(QEasingCurve::OutCubic); break;
    case CameraEasingType::INOUTCUBIC : easing.setType(QEasingCurve::InOutCubic); break;
    case CameraEasingType::OUTINCUBIC : easing.setType(QEasingCurve::OutInCubic); break;
    case CameraEasingType::INQUART : easing.setType(QEasingCurve::InQuart); break;
    case CameraEasingType::OUTQUART : easing.setType(QEasingCurve::OutQuart); break;
    case CameraEasingType::INOUTQUART : easing.setType(QEasingCurve::InOutQuart); break;
    case CameraEasingType::OUTINQUART : easing.setType(QEasingCurve::OutInQuart); break;
    case CameraEasingType::INQUINT : easing.setType(QEasingCurve::InQuint); break;
    case CameraEasingType::OUTQUINT : easing.setType(QEasingCurve::OutQuint); break;
    case CameraEasingType::INOUTQUINT : easing.setType(QEasingCurve::InOutQuint); break;
    case CameraEasingType::OUTINQUINT : easing.setType(QEasingCurve::OutInQuint); break;
    case CameraEasingType::INSINE : easing.setType(QEasingCurve::InSine); break;
    case CameraEasingType::OUTSINE : easing.setType(QEasingCurve::OutSine); break;
    case CameraEasingType::INOUTSINE : easing.setType(QEasingCurve::InOutSine); break;
    case CameraEasingType::OUTINSINE: easing.setType(QEasingCurve::OutInSine); break;
    case CameraEasingType::INEXPO : easing.setType(QEasingCurve::InExpo); break;
    case CameraEasingType::OUTEXPO : easing.setType(QEasingCurve::OutExpo); break;
    case CameraEasingType::INOUTEXPO : easing.setType(QEasingCurve::InOutExpo); break;
    case CameraEasingType::OUTINEXPO: easing.setType(QEasingCurve::OutInExpo); break;
    case CameraEasingType::INCIRC : easing.setType(QEasingCurve::InCirc); break;
    case CameraEasingType::OUTCIRC : easing.setType(QEasingCurve::OutCirc); break;
    case CameraEasingType::INOUTCIRC : easing.setType(QEasingCurve::InOutCirc); break;
    case CameraEasingType::OUTINCIRC: easing.setType(QEasingCurve::OutInCirc); break;
    case CameraEasingType::INELASTIC: easing.setType(QEasingCurve::InElastic); break;
    case CameraEasingType::OUTELASTIC: easing.setType(QEasingCurve::OutElastic); break;
    case CameraEasingType::INOUTELASTIC: easing.setType(QEasingCurve::InOutElastic); break;
    case CameraEasingType::OUTINELASTIC: easing.setType(QEasingCurve::OutInElastic); break;
    case CameraEasingType::INBACK: easing.setType(QEasingCurve::InBack); break;
    case CameraEasingType::OUTBACK: easing.setType(QEasingCurve::OutBack); break;
    case CameraEasingType::INOUTBACK: easing.setType(QEasingCurve::InOutBack); break;
    case CameraEasingType::OUTINBACK: easing.setType(QEasingCurve::OutInBack); break;
    case CameraEasingType::INBOUNCE: easing.setType(QEasingCurve::InBounce); break;
    case CameraEasingType::OUTBOUNCE: easing.setType(QEasingCurve::OutBounce); break;
    case CameraEasingType::INOUTBOUNCE: easing.setType(QEasingCurve::InOutBounce); break;
    case CameraEasingType::OUTINBOUNCE: easing.setType(QEasingCurve::OutInBounce); break;
    default: Q_UNREACHABLE(); break;
    }
    return easing.valueForProgress(percent);
}
