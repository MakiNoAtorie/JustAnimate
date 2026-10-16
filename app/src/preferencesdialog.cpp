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
#include "preferencesdialog.h"

#include "ui_preferencesdialog.h"

#include <QAbstractButton>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "prefsearch.h"
#include "theme.h"

PreferencesDialog::PreferencesDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);
    const char* pageIcons[] = { "settings", "file", "timeline", "tool", "keyboard" };
    for (int i = 0; i < ui->contentsWidget->count() && i < int(std::size(pageIcons)); ++i)
        ui->contentsWidget->item(i)->setIcon(Theme::icon(QStringLiteral(":/icons/tabler/%1.svg").arg(pageIcons[i])));

    mSearchEdit = new QLineEdit(this);
    mSearchEdit->setClearButtonEnabled(true);
    mSearchEdit->setPlaceholderText(tr("Search preferences"));
    ui->mainLayout->insertWidget(0, mSearchEdit);
    connect(mSearchEdit, &QLineEdit::textChanged, this, &PreferencesDialog::search);

    ui->buttonBox->button(QDialogButtonBox::Close)->setToolTip(tr("Close the preferences."));
    addTooltips();

    for (int i = 0; i < 0 + 1 * ui->contentsWidget->count(); i++) {
        QListWidgetItem* item = ui->contentsWidget->item(i);
        // Fill entire width
        item->setSizeHint({std::numeric_limits<int>::max(),
                           ui->contentsWidget->visualItemRect(item).height()});
    }
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::init(PreferenceManager* m)
{
    Q_ASSERT(m != nullptr);
    mPrefManager = m;

    ui->general->setManager(mPrefManager);
    ui->general->updateValues();

    ui->filesPage->setManager(mPrefManager);
    ui->filesPage->updateValues();

    ui->timeline->setManager(mPrefManager);
    ui->timeline->updateValues();

    ui->tools->setManager(mPrefManager);
    ui->tools->updateValues();

    ui->shortcuts->setManager(mPrefManager);

    connect(ui->general, &GeneralPage::windowOpacityChange, this, &PreferencesDialog::windowOpacityChange);
    connect(ui->timeline, &TimelinePage::soundScrubChanged, this, &PreferencesDialog::soundScrubChanged);
    connect(ui->timeline, &TimelinePage::soundScrubMsecChanged, this, &PreferencesDialog::soundScrubMsecChanged);
    connect(ui->filesPage, &FilesPage::clearRecentList, this, &PreferencesDialog::clearRecentList);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::close);

    auto onCurrentItemChanged = static_cast<void (QListWidget::*)(QListWidgetItem*, QListWidgetItem*)>(&QListWidget::currentItemChanged);
    connect(ui->contentsWidget, onCurrentItemChanged, this, &PreferencesDialog::changePage);
}

void PreferencesDialog::closeEvent(QCloseEvent*)
{
    done(QDialog::Accepted);
}

void PreferencesDialog::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
    if (!current)
        current = previous;

    ui->pagesWidget->setCurrentIndex(ui->contentsWidget->row(current));
}

void PreferencesDialog::updateRecentListBtn(bool isEmpty)
{
    if (isEmpty)
    {
        emit updateRecentFileListBtn();
    }
}

void PreferencesDialog::addTooltips()
{
    // Object names separated by spaces share one explanation; a tooltip set in the .ui file wins.
    const struct { const char* names; QString text; } tips[] = {
        { "languageCombo", tr("Language of the menus and dialogs. Takes effect after a restart.") },
        { "windowOpacityLabel windowOpacityLevel", tr("How see-through the main window is.") },
        { "shadowsBox", tr("Draw shadows under the docks and panels.") },
        { "cursorStyleLabel cursorStyleBox", tr("Brush outline shows the size and shape of the mark around a small cross. Cross only shows just the cross.") },
        { "checkerBackgroundButton", tr("Show a checkerboard behind the drawing.") },
        { "whiteBackgroundButton", tr("Show a white background behind the drawing.") },
        { "greyBackgroundButton", tr("Show a grey background behind the drawing.") },
        { "dotsBackgroundButton", tr("Show a dotted background behind the drawing.") },
        { "weaveBackgroundButton", tr("Show a weave pattern behind the drawing.") },
        { "antialiasingBox", tr("Smooth the edges of lines on screen.") },
        { "curveSmoothingLabel curveSmoothingLevel", tr("How much vector curves are smoothed after you draw them.") },
        { "highResBox", tr("Use the sub-pixel position that a graphics tablet reports.") },
        { "tabletApiLabel tabletApiBox", tr("The input system used to read a graphics tablet. Try another one if pen pressure does not work.") },
        { "gridCheckBox", tr("Show a grid over the canvas.") },
        { "labGridW gridSizeInputW", tr("Distance between vertical grid lines.") },
        { "labGridH gridSizeInputH", tr("Distance between horizontal grid lines.") },
        { "actionSafeCheckBox labActionSafe actionSafeInput", tr("Show the area where all important action should stay, as a percentage of the frame.") },
        { "titleSafeCheckBox labTitleSafe titleSafeInput", tr("Show the area where all titles should stay, as a percentage of the frame.") },
        { "safeHelperTextCheckbox labSafeHelperText", tr("Write a label on each safe area line.") },
        { "invertScrollDirectionBox", tr("Zoom in when you scroll the wheel the other way.") },
        { "cacheLabel framePoolSizeSpin", tr("The most memory used to keep drawn frames ready. A larger value is faster but uses more RAM.") },
        { "undoStepsBox", tr("How many steps you can undo and redo.") },
        { "undoRedoGroupApplyButton", tr("Use the new number of undo steps.") },
        { "undoRedoGroupCancelButton", tr("Keep the current number of undo steps.") },
        { "presetListWidget", tr("Presets you can start a new project from.") },
        { "removePreset", tr("Remove the selected preset.") },
        { "setDefaultPreset", tr("Use the selected preset when the app starts with the default preset.") },
        { "askPresetRbtn", tr("Ask which preset to use each time the app starts.") },
        { "loadDefaultPresetRbtn", tr("Start a new project from the default preset.") },
        { "loadLastActiveRbtn", tr("Open the project you used last.") },
        { "showWelcomeCheckBox", tr("Show the welcome screen when the app starts.") },
        { "autosaveCheckBox autosaveNumberLabel autosaveNumberBox", tr("Save a backup after this many changes.") },
        { "autosaveByTimeCheckBox autosaveByTimeNumberBox autosaveByTimeLabel", tr("Save a backup after this many minutes.") },
        { "lengthSizeLabel timelineLength", tr("Number of frames the timeline shows.") },
        { "scrubBox", tr("Play only a short piece of sound while you scrub.") },
        { "flipRollNumDrawingsLanel flipRollNumDrawingsSlider flipRollNumDrawingsSpinBox", tr("How many drawings the flip roll shows at once.") },
        { "flipRollMsecsLabel flipRollMsecsSlider flipRollMsecsSpinBox", tr("How long each drawing shows during a flip roll.") },
        { "flipInBtwnMsecLabel flipInBtwnMsecSlider flipInBtwnMsecSpinBox", tr("How long each drawing shows during a flip inbetween.") },
        { "soundScrubSlider soundScrubSpinBox", tr("How long the sound plays at each frame while you scrub.") },
        { "labelVisibilityText layerVisibilityComboBox", tr("Which layers are visible when a project opens.") },
        { "labelVisibility visibilitySlider visibilitySpinbox", tr("How visible the other layers are when layer visibility is relative.") },
        { "useQuickSizingBox", tr("Change the brush size and feather with a key and a drag on the canvas.") },
        { "rotationIncrementLabel rotationIncrementSlider rotationIncrementDisplay", tr("Angle steps used when the Move tool rotates a selection with Shift held.") },
        { "themeCombo", tr("Light or dark colours for the app, or the colours of the system.") },
        { "accentCombo", tr("The colour that marks the active tool, selections, sliders, the playhead and focus.") },
        { "resetCurveButton", tr("Set the pressure curve back to a straight line.") },
        { "clearButton", tr("Remove the key from the selected shortcut.") },
        { "btnSaveShortcuts", tr("Save all shortcuts to a file.") },
        { "btnLoadShortcuts", tr("Load shortcuts from a file.") },
        { "restoreShortcutsButton", tr("Set all shortcuts back to their default keys.") },
    };
    for (const auto& tip : tips) {
        for (const QString& name : QString(tip.names).split(' ')) {
            for (QWidget* w : findChildren<QWidget*>(name)) {
                if (w->toolTip().isEmpty()) {
                    w->setToolTip(tip.text);
                }
            }
        }
    }
}

void PreferencesDialog::search(const QString& query)
{
    const bool hasQuery = !query.trimmed().isEmpty();
    int firstPage = -1;
    for (int page = 0; page < ui->pagesWidget->count(); ++page) {
        QWidget* pageWidget = ui->pagesWidget->widget(page);
        for (QWidget* w : pageWidget->findChildren<QWidget*>()) {
            QString label;
            if (auto* g = qobject_cast<QGroupBox*>(w)) {
                label = g->title();
            } else if (auto* l = qobject_cast<QLabel*>(w)) {
                label = l->text();
            } else if (auto* b = qobject_cast<QAbstractButton*>(w)) {
                label = b->text();
            } else {
                continue;
            }
            const bool hit = hasQuery && !label.isEmpty() && prefLabelMatches(label, query);
            // A palette, not a style sheet: a style sheet selector needs an objectName.
            if (hit != w->property("prefSearchHit").toBool()) {
                w->setProperty("prefSearchHit", hit);
                QPalette pal; // Empty: the widget inherits its parent's palette again.
                if (hit) {
                    pal = w->palette();
                    pal.setColor(QPalette::Window, pal.color(QPalette::Highlight));
                    pal.setColor(QPalette::WindowText, pal.color(QPalette::HighlightedText));
                    pal.setColor(QPalette::ButtonText, pal.color(QPalette::HighlightedText));
                }
                w->setPalette(pal);
                w->setAutoFillBackground(hit);
            }
            if (hit && firstPage < 0) {
                firstPage = page;
            }
        }
    }
    if (ui->shortcuts->filter(query) && hasQuery && firstPage < 0) {
        firstPage = ui->pagesWidget->indexOf(ui->shortcuts);
    }
    if (firstPage >= 0) {
        ui->contentsWidget->setCurrentRow(firstPage);
    }
}
