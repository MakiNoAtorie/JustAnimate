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
#ifndef BRUSHPRESETDOCK_H
#define BRUSHPRESETDOCK_H

#include <memory>

#include <QFileSystemWatcher>
#include <QIcon>

#include "basedockwidget.h"
#include "brushpresets.h"

class QListWidget;
struct PresetSampler;

/// Lists the brush presets with a sample stroke painted by the real dab engine.
/// Selecting a row applies the preset to the current stroke tool; New, Edit and
/// Duplicate open the preset editor.
class BrushPresetDock : public BaseDockWidget
{
    Q_OBJECT

public:
    /// An empty folder means the default preset folder.
    explicit BrushPresetDock(QWidget* parent, const QString& folder = QString());
    ~BrushPresetDock() override;

    void initUI() override;
    void updateUI() override;

    static QSize sampleSize() { return QSize(160, 40); }

private:
    void applyRow(int row);
    QString currentName() const;
    void openEditor(const QString& source, const QString& name, const QString& original);
    QIcon sample(const QString& name);

    BrushPresets mPresets;
    QListWidget* mList = nullptr;
    QFileSystemWatcher mWatcher;
    std::unique_ptr<PresetSampler> mSampler;
};

#endif // BRUSHPRESETDOCK_H
