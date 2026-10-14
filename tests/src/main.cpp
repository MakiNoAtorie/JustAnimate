/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <QApplication>
#include <QStandardPaths>
#include <cstring>

int crashAfterAutosave(bool thenSave); // test_emergencysave.cpp

#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif

int main(int argc, char* argv[])
{
#ifdef _MSC_VER
    // A failed assert must not open a dialog: it blocks unattended runs.
    for (int type : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
    {
        _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
    }
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    QApplication a(argc, argv);
    QStandardPaths::setTestModeEnabled(true); // keep crash-recovery files out of the user's folders

    if (argc == 2 && std::strcmp(argv[1], "--crash-after-autosave") == 0)
        return crashAfterAutosave(false);
    if (argc == 2 && std::strcmp(argv[1], "--crash-after-save") == 0)
        return crashAfterAutosave(true);

    int result = Catch::Session().run(argc, argv);

    return result;
}
