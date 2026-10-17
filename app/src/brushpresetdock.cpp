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
#include "brushpresetdock.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "brushpreseteditor.h"
#include "editor.h"
#include "toolmanager.h"

BrushPresetDock::BrushPresetDock(QWidget* parent, const QString& folder) : BaseDockWidget(parent), mPresets(folder)
{
    setWindowTitle(tr("Brush Presets", "Window title of brush preset dock"));
    mList = new QListWidget(this);
    mList->setIconSize(sampleSize());
    auto newButton = new QPushButton(tr("New"), this);
    auto editButton = new QPushButton(tr("Edit"), this);
    auto duplicateButton = new QPushButton(tr("Duplicate"), this);
    newButton->setToolTip(tr("Create a new brush preset"));
    editButton->setToolTip(tr("Edit the selected brush preset"));
    duplicateButton->setToolTip(tr("Duplicate the selected brush preset"));
    auto buttons = new QHBoxLayout;
    buttons->addWidget(newButton);
    buttons->addWidget(editButton);
    buttons->addWidget(duplicateButton);
    auto content = new QWidget(this);
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mList);
    layout->addLayout(buttons);
    setWidget(content);
    connect(newButton, &QPushButton::clicked, this, [this] { openEditor(QString(), QString(), QString()); });
    connect(editButton, &QPushButton::clicked, this, [this] {
        const QString name = currentName();
        if (!name.isEmpty())
            openEditor(name, name, name);
    });
    connect(duplicateButton, &QPushButton::clicked, this, [this] {
        const QString name = currentName();
        if (!name.isEmpty())
            openEditor(name, tr("%1 copy").arg(name), QString());
    });
    connect(mList, &QListWidget::currentRowChanged, this, &BrushPresetDock::applyRow);
    connect(mList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { applyRow(mList->row(item)); });
}

BrushPresetDock::~BrushPresetDock() = default;

void BrushPresetDock::initUI()
{
    connect(editor()->tools(), &ToolManager::toolChanged, this, &BrushPresetDock::updateUI);
    updateUI();
}

QString BrushPresetDock::currentTool() const
{
    const BaseTool* tool = editor()->tools()->currentTool();
    return BrushPresets::toolKey(tool ? tool->type() : BRUSH);
}

void BrushPresetDock::updateUI()
{
    if (!mSampler)
        mSampler = std::make_unique<PresetSampler>(palette().color(QPalette::Text));

    const QString current = currentName();
    const QSignalBlocker blocker(mList);
    mList->clear();
    for (const BrushPresets::Entry& entry : mPresets.entries(currentTool()))
    {
        const QString text = entry.description.isEmpty() ? entry.name : entry.name + "\n" + entry.description;
        auto item = new QListWidgetItem(sample(entry.json), text, mList);
        item->setData(Qt::UserRole, entry.name);
        if (entry.name == current)
            mList->setCurrentItem(item);
    }
}

void BrushPresetDock::applyRow(int row)
{
    QListWidgetItem* item = mList->item(row);
    if (!item)
        return;
    const QString name = item->data(Qt::UserRole).toString(); // a tool change rebuilds the list
    ToolManager* tools = editor()->tools();
    if (!tools->currentStrokeTool())
        tools->setCurrentTool(BRUSH);
    if (mPresets.apply(name, *tools->currentStrokeTool()))
        emit tools->toolsReset(); // the tool options show the new values
}

QIcon BrushPresetDock::sample(const QJsonObject& preset)
{
    return QIcon(QPixmap::fromImage(mSampler->paint(mPresets, preset, sampleSize())));
}

QString BrushPresetDock::currentName() const
{
    return mList->currentItem() ? mList->currentItem()->data(Qt::UserRole).toString() : QString();
}

void BrushPresetDock::openEditor(const QString& source, const QString& name, const QString& original)
{
    auto dialog = new BrushPresetEditor(mPresets, source, name, original, currentTool(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        // Select the saved preset, which applies it to the current tool.
        updateUI();
        for (int row = 0; row < mList->count(); ++row)
        {
            if (mList->item(row)->data(Qt::UserRole).toString() != dialog->name())
                continue;
            const QSignalBlocker blocker(mList);
            mList->setCurrentRow(row);
            applyRow(row);
        }
    });
    dialog->open();
}
