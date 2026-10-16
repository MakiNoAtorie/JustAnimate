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
#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H

#include <QDialog>

class QComboBox;
class QCheckBox;
class QListWidget;

class WelcomeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WelcomeDialog(int defaultPreset, const QStringList& recentFiles, QWidget* parent = nullptr);

    // Set when the user chose a recent project. Empty after "New" or "Open...".
    QString openPath() const { return mOpenPath; }
    bool browseRequested() const { return mBrowse; }
    int presetIndex() const;
    // -1 unless the user chose a template.
    int templateIndex() const { return mTemplate; }
    bool shouldAlwaysUse() const;

private:
    void fillPresets(int defaultPreset);

    QComboBox* mPresets = nullptr;
    QCheckBox* mAlwaysUse = nullptr;
    QListWidget* mRecent = nullptr;
    QString mOpenPath;
    bool mBrowse = false;
    int mTemplate = -1;
};

#endif // WELCOMEDIALOG_H
