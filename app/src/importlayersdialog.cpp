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
#include "importlayersdialog.h"
#include "ui_importlayersdialog.h"

#include <QProgressDialog>

#include "app_util.h"
#include "filemanager.h"
#include "filedialog.h"
#include "errordialog.h"
#include "fileformat.h"
#include "layermanager.h"
#include "soundmanager.h"
#include "undoredomanager.h"
#include "layer.h"
#include "layersound.h"
#include "layervector.h"
#include "soundclip.h"


ImportLayersDialog::ImportLayersDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImportLayersDialog)
{
    ui->setupUi(this);
    connect(ui->btnSelectFile, &QPushButton::clicked, this, &ImportLayersDialog::getFileName);
    connect(ui->btnImportLayers, &QPushButton::clicked, this, &ImportLayersDialog::importLayers);
    connect(ui->lwLayers, &QListWidget::itemSelectionChanged, this, &ImportLayersDialog::listWidgetChanged);
    connect(ui->btnClose, &QPushButton::clicked, this, &ImportLayersDialog::cancel);
    ui->lwLayers->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->btnImportLayers->setEnabled(false);

    hideQuestionMark(*this);
}

ImportLayersDialog::~ImportLayersDialog()
{
    delete ui;
}

void ImportLayersDialog::setCore(Editor *editor)
{
    mEditor = editor;
}

Status ImportLayersDialog::loadFile(const QString& fileName)
{
    mFileName = fileName;
    return getLayers();
}

void ImportLayersDialog::getFileName()
{
    mFileName.clear();
    ui->lwLayers->clear();
    const QString fileName = FileDialog::getOpenFileName(this, FileType::ANIMATION, tr("Choose file"));
    if (fileName.isEmpty()) { return; }

    Status st = loadFile(fileName);
    if (!st.ok())
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().str(), this);
        errorDialog.exec();
    }
}

void ImportLayersDialog::listWidgetChanged()
{
    mItemsSelected.clear();
    for (int i = 0; i < ui->lwLayers->count(); i++)
        if (ui->lwLayers->item(i)->isSelected())
            mItemsSelected.append(i);

    if (!mItemsSelected.isEmpty())
        ui->btnImportLayers->setEnabled(true);
    else
        ui->btnImportLayers->setEnabled(false);
}

void ImportLayersDialog::importLayers()
{
    Q_ASSERT(ui->lwLayers->count() == mImportObject->getLayerCount());
    QList<int> layerIds;
    for (const QListWidgetItem* item : ui->lwLayers->selectedItems())
    {
        layerIds.append(item->data(Qt::UserRole).toInt());
    }
    importLayersById(layerIds);
}

void ImportLayersDialog::importLayersById(const QList<int>& layerIds)
{
    Object* object = mEditor->object();
    int currentFrame = mEditor->currentFrame();

    QMap<int, int> importedColors;
    Edit edit = mEditor->edit(tr("Import Layers"));

    for (int layerId : layerIds)
    {
        mImportLayer = mImportObject->takeLayer(layerId);
        if (mImportLayer == nullptr) { continue; }
        mImportLayer->setName(mEditor->layers()->nameSuggestLayer(mImportLayer->name()));
        loadKeyFrames(mImportLayer); // all keyframes of this layer must be in memory

        object->addLayer(mImportLayer);
        mEditor->undoRedo()->recordLayerAdd(mImportLayer->id(), tr("Import Layers"));

        if (mImportLayer->type() == Layer::VECTOR)
        {
            object->importVectorColors(static_cast<LayerVector*>(mImportLayer), *mImportObject, importedColors);
        }

        if (mImportLayer->type() == Layer::SOUND)
        {
            LayerSound* layerSound = static_cast<LayerSound*>(mImportLayer);
            layerSound->foreachKeyFrame([this](KeyFrame* key)
            {
                SoundClip* clip = dynamic_cast<SoundClip*>(key);
                Status st = mEditor->sound()->loadSound(clip, clip->fileName());
                Q_ASSERT(st.ok());
            });
        }
    }
    mEditor->object()->modification();

    mImportObject.reset();
    getLayers();
    mEditor->scrubTo(currentFrame);
    edit.commit();
}

void ImportLayersDialog::cancel()
{
    close();
}

Status ImportLayersDialog::getLayers()
{
    QProgressDialog progress(tr("Opening document..."), tr("Abort"), 0, 100, this);

    // Don't show progress bar if running without a GUI (aka. when rendering from command line)
    if (isVisible())
    {
        hideQuestionMark(progress);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
    }

    FileManager fm;
    connect(&fm, &FileManager::progressChanged, [&progress](int p)
    {
        progress.setValue(p);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    });
    connect(&fm, &FileManager::progressRangeChanged, [&progress](int max)
    {
        progress.setRange(0, max + 3);
    });
    mImportObject.reset(fm.load(mFileName));

    ui->lwLayers->clear();
    if (mImportObject == nullptr)
    {
        mFileName.clear();
        return fm.error();
    }

    for (int i = 0; i < mImportObject->getLayerCount(); i++)
    {
        const QString layerName = mImportObject->getLayer(i)->name();
        const int layerId = mImportObject->getLayer(i)->id();

        // Store the layer name as well as layer ID cuz two layers could have the same name
        QListWidgetItem* item = new QListWidgetItem(layerName);
        item->setData(Qt::UserRole, layerId);
        ui->lwLayers->addItem(item);
    }
    return Status::OK;
}

void ImportLayersDialog::loadKeyFrames(Layer* importedLayer)
{
    // Pencil2D only keeps a small portion of keyframes in the memory initially
    // Here we need to force load all the keyframes of this layer into memory
    // Otherwise the keyframe data will lose after mImportObject is deleted
    importedLayer->foreachKeyFrame([](KeyFrame* k)
    {
        k->loadFile();
    });
}
