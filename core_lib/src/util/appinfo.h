/*

JustAnimate - Traditional Animation Software
Based on Pencil2D (https://www.pencil2d.org)
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
#ifndef APPINFO_H
#define APPINFO_H

// Central place for product branding, so the name and project URLs
// are not scattered across the code base.

#define APP_NAME            "JustAnimate"
#define APP_ORGANIZATION    "JustAnimate"
#define APP_DOMAIN          "github.com/makinoatorie/justanimate"
#define APP_DESKTOP_ID      "io.github.makinoatorie.JustAnimate"

// Folder name used under the system temp / app data directories
#define APP_TEMP_DIR_NAME   "JustAnimate"

#define APP_REPO_URL        "https://github.com/makinoatorie/justanimate"
#define APP_ISSUES_URL      APP_REPO_URL "/issues"
#define APP_NEW_ISSUE_URL   APP_REPO_URL "/issues/new"
#define APP_RELEASES_URL    APP_REPO_URL "/releases"
#define APP_RELEASES_FEED   APP_REPO_URL "/releases.atom"
#define APP_ONLINE_MANUAL_URL APP_REPO_URL "/blob/master/docs/manual/00-index.md"

#endif // APPINFO_H
