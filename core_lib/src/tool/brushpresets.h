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

#include "pencildef.h"
#include "pencilerror.h"

class StrokeTool;

/// Brush presets: one JSON file per preset, `<folder>/<name>.json`, of the form
/// `{ "settings": { "<property identifier>": value, ... }, "tipImage": "<file or empty>",
/// "pressureCurve": "<curve or empty>", "description": "<text>", "tool": "brush|eraser" }`.
/// Preset files are untrusted: unknown keys are ignored, and missing or bad
/// values give the property default.
class BrushPresets
{
public:
    /// An empty folder means the default folder in the app data location.
    explicit BrushPresets(const QString& folder = QString());

    /// Names of the readable presets for `tool` (a `toolKey`; empty = every preset),
    /// sorted. Corrupt files are skipped with a warning.
    QStringList names(const QString& tool = QString()) const;

    /// The preset group of a tool: "eraser" for the Eraser, "brush" for every other tool.
    static QString toolKey(ToolType type);
    /// The optional "tool" of the preset; "brush" when absent, as before presets had one.
    QString tool(const QString& name) const;

    /// Creates or overwrites the preset with the tool's stroke settings, tip image
    /// and pressure curve override (`StrokeTool::pressureCurve`, empty = global curve).
    /// `presetTool` is the preset's "tool"; empty = `toolKey(tool.type())`.
    bool save(const QString& name, StrokeTool& tool, const QString& description = QString(),
              const QString& presetTool = QString()) const;

    /// Sets every tool property and the tip image from the preset; a tip file
    /// that cannot load gives the generated tip. Returns false and changes
    /// nothing when the preset is missing or corrupt.
    bool apply(const QString& name, StrokeTool& tool) const;

    /// Copies each default in the pack (resources under `:/brushpresets`) into the
    /// folder once: an existing file stays, so user edits stay, and a deleted default
    /// stays deleted. A default added in a later version is still copied.
    void addDefaults() const;

    /// The optional "description" text of the preset; empty when absent or not a string.
    QString description(const QString& name) const;

    /// Writes the "Classic" preset once, like addDefaults: on any stroke tool it draws
    /// like that tool's defaults before v0.3 (per-dab flow, pressure on).
    bool addClassic() const;

    /// Fails if `to` already exists.
    bool rename(const QString& from, const QString& to) const;
    /// Also deletes the preset's tip file in `<folder>/tips/` when no other preset uses it.
    bool remove(const QString& name) const;
    /// Whether a preset file of this name exists.
    bool exists(const QString& name) const;
    /// The preset's file, `<folder>/<name>.json`.
    QString filePath(const QString& name) const;

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
    /// Writes a built-in preset unless it was seeded before; an existing file stays.
    bool seed(const QString& name, const QByteArray& json) const;

    QString mFolder;
};

#endif // BRUSHPRESETS_H
