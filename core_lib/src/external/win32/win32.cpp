/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2009 Mj Mendoza IV
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "platformhandler.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSettings>

#include <ShObjIdl.h>

#include <QtGui/private/qguiapplication_p.h>

#include "pencildef.h"

namespace PlatformHandler
{
    void configurePlatformSpecificSettings() {}
    bool isDarkMode() { return false; }

    bool setWinTabEnabled(bool enabled)
    {
        auto app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::Private::QWindowsApplication>() : nullptr;
        if (!app) { return false; }
        // The return value of setWinTabEnabled() does not tell if a WinTab driver was found.
        app->setWinTabEnabled(enabled);
        return app->isWinTabEnabled();
    }
    void initialise()
    {
#if _WIN32_WINNT >= _WIN32_WINNT_WIN7
        QString version(APP_VERSION);
        if (version.startsWith("99.0.0")) {
            SetCurrentProcessExplicitAppUserModelID(L"JustAnimate.JustAnimate.Nightly");
        } else if (version == "0.0.0.0") {
            SetCurrentProcessExplicitAppUserModelID(L"JustAnimate.JustAnimate.Development");
        } else {
            SetCurrentProcessExplicitAppUserModelID(L"JustAnimate.JustAnimate.Release");
        }
#endif // _WIN32_WINNT >= _WIN32_WINNT_WIN7

    };
}
