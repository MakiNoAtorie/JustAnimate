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
#include "storyboardpanel.h"

#include <QAbstractButton>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QTableWidget>

#include "editor.h"
#include "keyframe.h"
#include "layer.h"
#include "layermanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "util.h"

StoryboardPanel::StoryboardPanel(QWidget* parent) : BaseDockWidget(parent)
{
}

void StoryboardPanel::initUI()
{
    setWindowTitle(tr("Storyboard", "Subpanel title"));

    mTable = new QTableWidget(0, ColumnCount, this);
    mTable->setHorizontalHeaderLabels({ tr("Scene"), tr("Cut"), tr("Notes"), tr("Audio"), tr("Time") });
    mTable->horizontalHeader()->setSectionResizeMode(Notes, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(Audio, QHeaderView::Stretch);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mTable->setCornerButtonEnabled(false);
    if (auto* corner = mTable->findChild<QAbstractButton*>()) { corner->setToolTip(tr("Storyboard panels")); }
    mTable->setToolTip(tr("One row per storyboard panel; Time is its Exposure in seconds+frames"));
    setWidget(mTable);

    connect(mTable, &QTableWidget::itemChanged, this, &StoryboardPanel::onItemChanged);
    connect(mTable, &QTableWidget::currentCellChanged, this, [this](int row) {
        if (row >= 0 && row < mPanelFrames.size()) { editor()->scrubTo(mPanelFrames[row]); }
    });

    Editor* e = editor();
    const auto refresh = [this] { updateUI(); };
    connect(e, &Editor::frameModified, this, refresh);
    connect(e, &Editor::framesModified, this, refresh);
    connect(e, &Editor::updateTimeLine, this, refresh);
    connect(e, &Editor::objectLoaded, this, refresh);
    connect(e->layers(), &LayerManager::currentLayerChanged, this, refresh);
    connect(e->layers(), &LayerManager::layerCountChanged, this, refresh);
    connect(e->layers(), &LayerManager::animationLengthChanged, this, refresh);
    connect(e->playback(), &PlaybackManager::fpsChanged, this, refresh);
    updateUI();
}

Layer* StoryboardPanel::storyboardLayer() const
{
    Layer* current = editor()->layers()->currentLayer();
    if (current && current->isStoryboard()) { return current; }
    const Object* object = editor()->object();
    for (int i = 0; object && i < object->getLayerCount(); ++i)
    {
        if (object->getLayer(i)->isStoryboard()) { return object->getLayer(i); }
    }
    return nullptr;
}

void StoryboardPanel::updateUI()
{
    if (mTable == nullptr) { return; }
    const QSignalBlocker blocker(mTable);
    const Layer* layer = storyboardLayer();
    mLayerId = layer ? layer->id() : 0;
    mPanelFrames.clear();
    mPanelTimes.clear();
    const QList<Layer::Run> runs = layer ? layer->runs(editor()->layers()->animationLength()) : QList<Layer::Run>();
    const int fps = editor()->playback()->fps();

    // Items are updated in place: this can run inside itemChanged of one of them.
    mTable->setRowCount(runs.size());
    for (int row = 0; row < runs.size(); ++row)
    {
        const Layer::Run& run = runs[row];
        mPanelFrames << run.first;
        mPanelTimes << run.last - run.first + 1;
        const PanelFields f = layer->getKeyFrameAt(run.first)->panelFields();
        const QString texts[ColumnCount] = { f.scene, f.cut, f.notes, f.audio, formatSecondsFrames(mPanelTimes.last(), fps) };
        for (int column = 0; column < ColumnCount; ++column)
        {
            QTableWidgetItem* item = mTable->item(row, column);
            if (item == nullptr)
            {
                item = new QTableWidgetItem;
                mTable->setItem(row, column, item);
            }
            item->setText(texts[column]);
        }
        QTableWidgetItem* time = mTable->item(row, Time);
        time->setFlags(time->flags() | Qt::ItemIsEditable);
        time->setToolTip(row == runs.size() - 1 ? tr("The last storyboard panel holds to the end of the project; its Time sets the project length") : QString());
    }
}

void StoryboardPanel::onItemChanged(QTableWidgetItem* item)
{
    const int row = item->row();
    Layer* layer = editor()->layers()->findLayerById(mLayerId);
    if (layer == nullptr || row < 0 || row >= mPanelFrames.size()) { return; }
    const int frame = mPanelFrames[row];

    if (item->column() == Time)
    {
        const int time = parseSecondsFrames(item->text(), editor()->playback()->fps());
        if (time > 0 && time != mPanelTimes[row] && row == mPanelFrames.size() - 1)
        {
            // No next Drawing to move: the last panel holds to the project length.
            editor()->layers()->setSceneLength(frame + time - 1);
        }
        else if (time > 0 && time != mPanelTimes[row])
        {
            // changeExposure edits the current layer.
            editor()->layers()->setCurrentLayer(layer);
            editor()->layers()->changeExposure(frame, time - mPanelTimes[row]);
        }
    }
    else
    {
        PanelFields f = layer->getKeyFrameAt(frame)->panelFields();
        QString* fields[] = { &f.scene, &f.cut, &f.notes, &f.audio };
        *fields[item->column()] = item->text();
        editor()->layers()->setPanelFields(layer, frame, f);
    }
    updateUI(); // shows the stored value when the edit was refused
}
