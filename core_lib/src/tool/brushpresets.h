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
#ifndef BRUSHPRESETS_H
#define BRUSHPRESETS_H

#include <QString>
#include <QStringList>

#include "pencilerror.h"

class StrokeTool;

/// Brush presets: one JSON file per preset, `<folder>/<name>.json`, of the form
/// `{ "settings": { "<property identifier>": value, ... }, "tipImage": "<file or empty>" }`.
/// Preset files are untrusted: unknown keys are ignored, and missing or bad
/// values give the property default.
class BrushPresets
{
public:
    /// An empty folder means the default folder in the app data location.
    explicit BrushPresets(const QString& folder = QString());

    /// Names of the readable presets, sorted. Corrupt files are skipped with a warning.
    QStringList names() const;

    /// Creates or overwrites the preset with the tool's stroke settings and tip image.
    /// Global preferences such as the pressure curve are not part of a preset.
    bool save(const QString& name, StrokeTool& tool) const;

    /// Sets every tool property and the tip image from the preset; a tip file
    /// that cannot load gives the generated tip. Returns false and changes
    /// nothing when the preset is missing or corrupt.
    bool apply(const QString& name, StrokeTool& tool) const;

    /// Fails if `to` already exists.
    bool rename(const QString& from, const QString& to) const;
    bool remove(const QString& name) const;

    /// Writes the preset and its tip image file to a `.jabrush` zip:
    /// `preset.json` (`name`, `settings`, `tipImage` = entry name or empty) and `tip.<suffix>`.
    Status exportPreset(const QString& name, const QString& fileName) const;

    /// Adds the preset in a `.jabrush` file; its tip goes to `<folder>/tips/`.
    /// The file is untrusted: it is fully checked before anything is written, and
    /// the import fails when a preset of that name exists.
    Status importPreset(const QString& fileName) const;

    /// A name is valid when it is a plain file name: no path separators, no
    /// leading dot or space, no characters Windows forbids.
    static bool isValidName(const QString& name);

    QString folder() const { return mFolder; }

private:
    QString filePath(const QString& name) const;

    QString mFolder;
};

#endif // BRUSHPRESETS_H
