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

#include "actioncommands.h"
#include "appinfo.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QApplication>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>

#include "pencildef.h"
#include "editor.h"
#include "object.h"
#include "viewmanager.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "soundmanager.h"
#include "playbackmanager.h"
#include "colormanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "util.h"
#include "app_util.h"

#include "layercamera.h"
#include "layersound.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "vectorimage.h"
#include "soundclip.h"
#include "camera.h"
#include "pagewriter.h"
#include "paperpainter.h"

#include "importimageseqdialog.h"
#include "importpositiondialog.h"
#include "movieimporter.h"
#include "movieexporter.h"
#include "filedialog.h"
#include "exportmoviedialog.h"
#include "exportimagedialog.h"
#include "aboutdialog.h"
#include "doubleprogressdialog.h"
#include "checkupdatesdialog.h"
#include "errordialog.h"


ActionCommands::ActionCommands(QWidget* parent) : QObject(parent)
{
    mParent = parent;
}

ActionCommands::~ActionCommands() {}

Status ActionCommands::importAnimatedImage()
{
    ImportImageSeqDialog fileDialog(mParent, ImportExportDialog::Import, FileType::ANIMATED_IMAGE);
    fileDialog.exec();
    if (fileDialog.result() != QDialog::Accepted)
    {
        return Status::CANCELED;
    }
    int frameSpacing = fileDialog.getSpace();
    QString strImgFileLower = fileDialog.getFilePath();

    ImportPositionDialog positionDialog(mEditor, mParent);
    positionDialog.exec();
    if (positionDialog.result() != QDialog::Accepted)
    {
        return Status::CANCELED;
    }

    // Show a progress dialog, as this could take a while if the gif is huge
    QProgressDialog progressDialog(tr("Importing Animated Image..."), tr("Abort"), 0, 100, mParent);
    hideQuestionMark(progressDialog);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.show();

    Status st = mEditor->importAnimatedImage(strImgFileLower, frameSpacing, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
        QApplication::processEvents();
    }, [&progressDialog]() {
        return progressDialog.wasCanceled();
    });

    progressDialog.setValue(100);
    progressDialog.close();

    if (!st.ok())
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html());
        errorDialog.exec();
        return Status::SAFE;
    }

    return Status::OK;
}

Status ActionCommands::importMovieVideo()
{
    QString filePath = FileDialog::getOpenFileName(mParent, FileType::MOVIE);
    if (filePath.isEmpty())
    {
        return Status::FAIL;
    }

    // Show a progress dialog, as this can take a while if you have lots of images.
    QProgressDialog progressDialog(tr("Importing movie..."), tr("Abort"), 0, 100, mParent);
    hideQuestionMark(progressDialog);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumWidth(250);
    progressDialog.show();

    QMessageBox information(mParent);
    information.setIcon(QMessageBox::Warning);
    information.setText(tr("You are importing a lot of frames, beware this could take some time. Are you sure you want to proceed?"));
    information.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    information.setDefaultButton(QMessageBox::Yes);

    MovieImporter importer(this);
    importer.setCore(mEditor);

    connect(&progressDialog, &QProgressDialog::canceled, &importer, &MovieImporter::cancel);

    Status st = importer.run(filePath, mEditor->playback()->fps(), FileType::MOVIE, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
        QApplication::processEvents();
    }, [&progressDialog](QString progMessage) {
        progressDialog.setLabelText(progMessage);
    }, [&information]() {

        int ret = information.exec();
        return ret == QMessageBox::Yes;
    });

    if (!st.ok() && st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
        return Status::SAFE;
    }

    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();

    progressDialog.setValue(100);
    progressDialog.close();

    return Status::OK;
}

Status ActionCommands::importSound(FileType type)
{
    const UndoMacro macro(mEditor->undoRedo(), tr("Import Sound"));
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr)
    {
        Q_ASSERT(layer);
        return Status::FAIL;
    }

    if (layer->type() != Layer::SOUND)
    {
        QMessageBox msg;
        msg.setText(tr("No sound layer exists as a destination for your import. Create a new sound layer?"));
        msg.addButton(tr("Create sound layer"), QMessageBox::AcceptRole);
        msg.addButton(tr("Don't create layer"), QMessageBox::RejectRole);

        int buttonClicked = msg.exec();
        if (buttonClicked != QMessageBox::AcceptRole)
        {
            return Status::SAFE;
        }

        // Create new sound layer.
        addNewSoundLayer();
    }

    layer = mEditor->layers()->currentLayer();
    Q_ASSERT(layer->type() == Layer::SOUND);

    if (mEditor->refusesHidden(layer) || mEditor->refusesEdit(layer)) { return Status::SAFE; }
    QString strSoundFile = FileDialog::getOpenFileName(mParent, type);

    Status st = Status::FAIL;

    if (strSoundFile.isEmpty())
    {
        st = Status::CANCELED;
    }
    else
    {
        // Go to the first frame that has no clip; the import records the clip as one undo step.
        int frame = mEditor->currentFrame();
        while (layer->keyExists(frame) && !layer->getKeyFrameAt(frame)->fileName().isEmpty()) { ++frame; }
        mEditor->scrubTo(frame);

        // Convert even if it already is a WAV file to strip metadata that the
        // DirectShow media player backend on Windows can't handle
        st = convertSoundToWav(strSoundFile);
    }

    if (st.ok())
    {
        showSoundClipWarningIfNeeded();
    }
    return st;
}

Status ActionCommands::convertSoundToWav(const QString& filePath)
{
    QProgressDialog progressDialog(tr("Importing sound..."), tr("Abort"), 0, 100, mParent);
    hideQuestionMark(progressDialog);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.show();

    MovieImporter importer(this);
    importer.setCore(mEditor);

    Status st = importer.run(filePath, mEditor->playback()->fps(), FileType::SOUND, [&progressDialog](int prog) {
        progressDialog.setValue(prog);
        QApplication::processEvents();
    }, [](QString progressMessage) {
        Q_UNUSED(progressMessage)
        // Not needed
    }, []() {
        return true;
    });

    connect(&progressDialog, &QProgressDialog::canceled, &importer, &MovieImporter::cancel);

    if (!st.ok() && st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
    }
    return st;
}

Status ActionCommands::exportGif()
{
    // exporting gif
    return exportMovie(true);
}

Status ActionCommands::exportMovie(bool isGif)
{
    FileType fileType = (isGif) ? FileType::GIF : FileType::MOVIE;

    int clipCount = mEditor->sound()->soundClipCount();
    if (fileType == FileType::MOVIE && clipCount >= MovieExporter::MAX_SOUND_FRAMES)
    {
        ErrorDialog errorDialog(tr("Something went wrong"), tr("You currently have a total of %1 sound clips. Due to current limitations, you will be unable to export any animation exceeding %2 sound clips. We recommend splitting up larger projects into multiple smaller project to stay within this limit.").arg(clipCount).arg(MovieExporter::MAX_SOUND_FRAMES), QString(), mParent);
        errorDialog.exec();
        return Status::FAIL;
    }

    ExportMovieDialog* dialog = new ExportMovieDialog(mParent, ImportExportDialog::Export, fileType);
    OnScopeExit(dialog->deleteLater());

    dialog->init();

    std::vector< std::pair<QString, QSize> > camerasInfo;
    auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
    for (LayerCamera* i : cameraLayers)
    {
        camerasInfo.push_back(std::make_pair(i->name(), i->getViewSize()));
    }

    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer->type() == Layer::CAMERA)
    {
        QString strName = currLayer->name();
        auto it = std::find_if(camerasInfo.begin(), camerasInfo.end(),
            [strName](std::pair<QString, QSize> p)
        {
            return p.first == strName;
        });

        Q_ASSERT(it != camerasInfo.end());

        std::swap(camerasInfo[0], *it);
    }

    dialog->setCamerasInfo(camerasInfo);

    int lengthWithSounds = mEditor->layers()->animationLength(true);
    int length = mEditor->layers()->animationLength(false);

    dialog->setDefaultRange(1, length, lengthWithSounds);
    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }
    QString strMoviePath = dialog->getFilePath();
    if (!MovieExporter::mayOverwrite(strMoviePath, [this, &strMoviePath] {
            return QMessageBox::question(mParent, APP_NAME,
                       tr("%1 already exists. Replace it?").arg(QDir::toNativeSeparators(strMoviePath))) == QMessageBox::Yes;
        }))
    {
        return Status::SAFE;
    }

    ExportMovieDesc desc;
    desc.strFileName = strMoviePath;
    desc.startFrame = dialog->getStartFrame();
    desc.endFrame = dialog->getEndFrame();
    desc.fps = mEditor->playback()->fps();
    desc.exportSize = dialog->getExportSize();
    desc.strCameraName = dialog->getSelectedCameraName();
    desc.loop = dialog->getLoop();
    desc.alpha = dialog->getTransparency();
    desc.quality = dialog->getQuality();
    desc.gifColors = dialog->getGifColors();
    desc.gifDither = dialog->getGifDither();

    DoubleProgressDialog progressDlg(mParent);
    progressDlg.setWindowModality(Qt::WindowModal);
    progressDlg.setWindowTitle(tr("Exporting movie"));
    Qt::WindowFlags eFlags = Qt::Dialog | Qt::WindowTitleHint;
    progressDlg.setWindowFlags(eFlags);
    progressDlg.show();

    MovieExporter ex;

    connect(&progressDlg, &DoubleProgressDialog::canceled, [&ex]
    {
        ex.cancel();
    });

    // The start points and length for the current minor operation segment on the major progress bar
    float minorStart, minorLength;

    Status st = ex.run(mEditor->object(), desc,
        [&progressDlg, &minorStart, &minorLength](float f, float final)
        {
            progressDlg.major->setValue(f);

            minorStart = f;
            minorLength = qMax(0.f, final - minorStart);

            QApplication::processEvents();
        },
        [&progressDlg, &minorStart, &minorLength](float f) {
            progressDlg.minor->setValue(f);

            progressDlg.major->setValue(minorStart + f * minorLength);

            QApplication::processEvents();
        },
        [&progressDlg](QString s) {
            progressDlg.setStatus(s);
            QApplication::processEvents();
        }
    );

    if (st.ok())
    {
        if (!ex.skippedClips().isEmpty())
        {
            QMessageBox::warning(mParent, tr("Missing Sound Files"),
                                 tr("These sound clips were left out of the movie because their files are missing:\n%1").arg(ex.skippedClips().join('\n')));
        }
        if (QFile::exists(strMoviePath))
        {
            if (isGif) {
                auto btn = QMessageBox::question(mParent, APP_NAME,
                                                 tr("Finished. Open file location?"));

                if (btn == QMessageBox::Yes)
                {
                    QString path = dialog->getAbsolutePath();
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                }
                return Status::OK;
            }
            auto btn = QMessageBox::question(mParent, APP_NAME,
                                             tr("Finished. Open movie now?", "When movie export done."));
            if (btn == QMessageBox::Yes)
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(strMoviePath));
            }
        }
        else
        {
            ErrorDialog errorDialog(tr("Unknown export error"), tr("The export did not produce any errors, however we can't find the output file. Your export may not have completed successfully."), QString(), mParent);
            errorDialog.exec();
        }
    }
    else if(st != Status::CANCELED)
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), mParent);
        errorDialog.exec();
    }

    return st;
}

Status ActionCommands::exportImageSequence()
{
    auto dialog = new ExportImageDialog(mParent, FileType::IMAGE_SEQUENCE);
    OnScopeExit(dialog->deleteLater());

    dialog->init();

    std::vector< std::pair<QString, QSize> > camerasInfo;
    auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
    for (LayerCamera* i : cameraLayers)
    {
        camerasInfo.push_back(std::make_pair(i->name(), i->getViewSize()));
    }

    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer->type() == Layer::CAMERA)
    {
        QString strName = currLayer->name();
        auto it = std::find_if(camerasInfo.begin(), camerasInfo.end(),
            [strName](std::pair<QString, QSize> p)
        {
            return p.first == strName;
        });

        Q_ASSERT(it != camerasInfo.end());
        std::swap(camerasInfo[0], *it);
    }
    dialog->setCamerasInfo(camerasInfo);

    int lengthWithSounds = mEditor->layers()->animationLength(true);
    int length = mEditor->layers()->animationLength(false);

    dialog->setDefaultRange(1, length, lengthWithSounds);

    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }

    QString strFilePath = dialog->getFilePath();
    QSize exportSize = dialog->getExportSize();
    QString exportFormat = dialog->getExportFormat();
    bool exportKeyframesOnly = dialog->getExportKeyframesOnly();
    bool useTransparency = dialog->getTransparency();
    int startFrame = dialog->getStartFrame();
    int endFrame  = dialog->getEndFrame();

    if (dialog->getSpriteSheet() && !MovieExporter::mayOverwrite(strFilePath, [this, &strFilePath] {
            return QMessageBox::question(mParent, APP_NAME,
                       tr("%1 already exists. Replace it?").arg(QDir::toNativeSeparators(strFilePath))) == QMessageBox::Yes;
        }))
    {
        return Status::SAFE;
    }

    QString sCameraLayerName = dialog->getCameraLayerName();
    LayerCamera* cameraLayer = static_cast<LayerCamera*>(mEditor->layers()->findLayerByName(sCameraLayerName, Layer::CAMERA));

    // Show a progress dialog, as this can take a while if you have lots of frames.
    QProgressDialog progress(tr("Exporting image sequence..."), tr("Abort"), 0, 100, mParent);
    hideQuestionMark(progress);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    Status st = Status::OK;
    if (dialog->getSpriteSheet())
    {
        st = mEditor->object()->exportSpriteSheet(startFrame, endFrame, cameraLayer, exportSize, strFilePath,
                                                  dialog->getSpriteSheetColumns(), useTransparency, true, &progress, 100);
    }
    else
    {
        st = mEditor->object()->exportFrames(startFrame, endFrame,
                                    cameraLayer,
                                    exportSize,
                                    strFilePath,
                                    exportFormat,
                                    useTransparency,
                                    exportKeyframesOnly,
                                    mEditor->layers()->currentLayer()->name(),
                                    true,
                                    &progress,
                                    100);
    }
    if (st == Status::CANCELED) { return Status::SAFE; }

    if (!st.ok())
    {
        ErrorDialog errorDialog(tr("Something went wrong"), tr("Unable to export one or more images in the image sequence."), st.details().html(), mParent);
        errorDialog.exec();
        return st;
    }

    progress.close();

    return Status::OK;
}

Status ActionCommands::exportImage()
{
    // Options
    auto dialog = new ExportImageDialog(mParent, FileType::IMAGE);
    OnScopeExit(dialog->deleteLater())

    dialog->init();

    std::vector< std::pair<QString, QSize> > camerasInfo;
    auto cameraLayers = mEditor->object()->getLayersByType< LayerCamera >();
    for (LayerCamera* i : cameraLayers)
    {
        camerasInfo.push_back(std::make_pair(i->name(), i->getViewSize()));
    }

    auto currLayer = mEditor->layers()->currentLayer();
    if (currLayer->type() == Layer::CAMERA)
    {
        QString strName = currLayer->name();
        auto it = std::find_if(camerasInfo.begin(), camerasInfo.end(),
            [strName](std::pair<QString, QSize> p)
        {
            return p.first == strName;
        });

        Q_ASSERT(it != camerasInfo.end());
        std::swap(camerasInfo[0], *it);
    }
    dialog->setCamerasInfo(camerasInfo);

    dialog->exec();

    if (dialog->result() == QDialog::Rejected)
    {
        return Status::SAFE;
    }

    QString filePath = dialog->getFilePath();
    QSize exportSize = dialog->getExportSize();
    QString exportFormat = dialog->getExportFormat();
    bool useTransparency = dialog->getTransparency();

    QString extension = "";
    QString formatStr = exportFormat;
    if (formatStr == "PNG" || formatStr == "png")
    {
        exportFormat = "PNG";
        extension = ".png";
    }
    if (formatStr == "JPG" || formatStr == "jpg" || formatStr == "JPEG" || formatStr == "jpeg")
    {
        exportFormat = "JPG";
        extension = ".jpg";
        useTransparency = false; // JPG doesn't support transparency, so we have to include the background
    }
    if (formatStr == "TIFF" || formatStr == "tiff" || formatStr == "TIF" || formatStr == "tif")
    {
        exportFormat = "TIFF";
        extension = ".tiff";
    }
    if (formatStr == "BMP" || formatStr == "bmp")
    {
        exportFormat = "BMP";
        extension = ".bmp";
        useTransparency = false;
    }
    if (formatStr == "WEBP" || formatStr == "webp") {
        exportFormat = "WEBP";
        extension = ".webp";
    }
    if (!filePath.endsWith(extension, Qt::CaseInsensitive))
    {
        filePath += extension;
    }

    // Export
    QString sCameraLayerName = dialog->getCameraLayerName();
    LayerCamera* cameraLayer = static_cast<LayerCamera*>(mEditor->layers()->findLayerByName(sCameraLayerName, Layer::CAMERA));

    QTransform view = cameraLayer->getViewAtFrame(mEditor->currentFrame());

    Status st = mEditor->object()->exportIm(mEditor->currentFrame(),
                                           view,
                                           cameraLayer->getViewSize(),
                                           exportSize,
                                           filePath,
                                           exportFormat,
                                           true,
                                           useTransparency);

    if (!st.ok())
    {
        ErrorDialog errorDialog(tr("Something went wrong"), tr("Unable to export image."), st.details().html(), mParent);
        errorDialog.exec();
        return st;
    }
    return Status::OK;
}

namespace
{
// Asks where to save, writes the pages and shows any error. Shared by the three page exports.
Status savePages(QWidget* parent, const QString& title, const PageSpec& spec, int pageCount, const PagePaintFn& paint)
{
    const QString path = QFileDialog::getSaveFileName(parent, title, QString(), ActionCommands::tr("PDF (*.pdf);;PNG pages (*.png)"));
    if (path.isEmpty()) { return Status::SAFE; }
    QProgressDialog progress(ActionCommands::tr("Exporting page 1 of %1...").arg(pageCount), ActionCommands::tr("Abort"), 0, pageCount, parent);
    hideQuestionMark(progress);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    // A window-modal setValue processes events, so Abort is seen between pages.
    const Status st = writePages(path, spec, pageCount, paint, [&progress, pageCount](int done) {
        progress.setValue(done);
        progress.setLabelText(ActionCommands::tr("Exporting page %1 of %2...").arg(std::min(done + 1, pageCount)).arg(pageCount));
        return !progress.wasCanceled();
    });
    progress.close();
    if (st == Status::CANCELED) { return Status::SAFE; }
    if (!st.ok())
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html(), parent);
        errorDialog.exec();
    }
    return st;
}
} // namespace

Status ActionCommands::exportOnPaper()
{
    const std::vector<LayerCamera*> cameras = mEditor->object()->getLayersByType<LayerCamera>();
    if (cameras.empty())
    {
        QMessageBox::information(mParent, tr("Export on Paper"), tr("Add a camera layer first: the pages show its frame."));
        return Status::SAFE;
    }
    QDialog dialog(mParent);
    dialog.setWindowTitle(tr("Export on Paper"));
    auto form = new QFormLayout(&dialog);
    auto size = new QComboBox(&dialog);
    size->addItems({ tr("A4 landscape"), tr("B4 landscape") });
    auto camera = new QComboBox(&dialog);
    for (const LayerCamera* layer : cameras)
    {
        camera->addItem(layer->name());
        if (layer == mEditor->layers()->currentLayer()) { camera->setCurrentIndex(camera->count() - 1); }
    }
    const int length = std::max(1, mEditor->layers()->animationLength());
    auto from = new QSpinBox(&dialog);
    auto to = new QSpinBox(&dialog);
    from->setRange(1, 9999);
    to->setRange(1, 9999);
    from->setValue(1);
    to->setValue(length);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(tr("Paper:"), size);
    form->addRow(tr("Camera:"), camera);
    form->addRow(tr("From frame:"), from);
    form->addRow(tr("To frame:"), to);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) { return Status::SAFE; }

    PageSpec spec;
    spec.size = size->currentIndex() == 1 ? PageSpec::B4 : PageSpec::A4;
    const int first = std::min(from->value(), to->value());
    return savePages(mParent, tr("Export on Paper"), spec, std::abs(to->value() - from->value()) + 1,
                     paperFramePages(*mEditor->object(), *cameras[camera->currentIndex()], first));
}

Status ActionCommands::exportStoryboard()
{
    const Object* object = mEditor->object();
    const Layer* board = nullptr;
    for (int i = 0; i < object->getLayerCount() && board == nullptr; ++i)
    {
        if (object->getLayer(i)->isStoryboard()) { board = object->getLayer(i); }
    }
    const std::vector<LayerCamera*> cameras = object->getLayersByType<LayerCamera>();
    if (board == nullptr || cameras.empty())
    {
        QMessageBox::information(mParent, tr("Export Storyboard"),
                                 board == nullptr ? tr("Make a layer a Storyboard layer first (layer menu).")
                                                  : tr("Add a camera layer first: the pictures show its frame."));
        return Status::SAFE;
    }
    QList<QPair<int, int>> panels;
    for (const Layer::Run& run : board->runs(mEditor->layers()->animationLength())) { panels << qMakePair(run.first, run.last); }
    PageSpec spec;
    spec.orientation = QPageLayout::Portrait;
    return savePages(mParent, tr("Export Storyboard"), spec, storyboardPageCount(panels.size()),
                     storyboardPages(*object, *board, *cameras.front(), panels, mEditor->playback()->fps()));
}

Status ActionCommands::exportTimesheet()
{
    QDialog dialog(mParent);
    dialog.setWindowTitle(tr("Export Timesheet"));
    auto form = new QFormLayout(&dialog);
    auto size = new QComboBox(&dialog);
    size->addItems({ tr("A4 portrait"), tr("B4 portrait") });
    PaperTemplate paper = mEditor->object()->data()->paperTemplate();
    const TimesheetInfo prefill = timesheetPrefill(paper);
    auto scene = new QLineEdit(prefill.scene, &dialog);
    auto shot = new QLineEdit(prefill.shot, &dialog);
    auto animator = new QLineEdit(prefill.animator, &dialog);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(tr("Paper:"), size);
    form->addRow(tr("Scene:"), scene);
    form->addRow(tr("Shot:"), shot);
    form->addRow(tr("Animator:"), animator);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) { return Status::SAFE; }

    PageSpec spec;
    spec.size = size->currentIndex() == 1 ? PageSpec::B4 : PageSpec::A4;
    spec.orientation = QPageLayout::Portrait;
    TimesheetInfo info; // TITLE comes from the Title block
    info.scene = scene->text();
    info.shot = shot->text();
    info.animator = animator->text();
    rememberTimesheet(paper, info);
    if (paper != mEditor->object()->data()->paperTemplate())
    {
        mEditor->object()->data()->setPaperTemplate(paper);
        mEditor->object()->modification();
    }
    info.length = std::max(1, mEditor->layers()->animationLength());
    info.fps = mEditor->playback()->fps();
    return savePages(mParent, tr("Export Timesheet"), spec, timesheetSheetCount(info.length), timesheetPages(*mEditor->object(), info));
}

void ActionCommands::flipSelectionX()
{
    bool flipVertical = false;
    mEditor->flipSelection(flipVertical);
}

void ActionCommands::flipSelectionY()
{
    bool flipVertical = true;
    mEditor->flipSelection(flipVertical);
}

void ActionCommands::selectAll()
{
    mEditor->selectAll();
}

void ActionCommands::deselectAll()
{
    mEditor->deselectAll();
}

void ActionCommands::ZoomIn()
{
    mEditor->view()->scaleUp();
}

void ActionCommands::ZoomOut()
{
    mEditor->view()->scaleDown();
}

void ActionCommands::rotateClockwise()
{
    // Rotation direction is inverted if view is flipped either vertically or horizontally
    const float delta = mEditor->view()->isFlipHorizontal() == !mEditor->view()->isFlipVertical() ? -15.f : 15.f;
    mEditor->view()->rotateRelative(delta);
}

void ActionCommands::rotateCounterClockwise()
{
    // Rotation direction is inverted if view is flipped either vertically or horizontally
    const float delta = mEditor->view()->isFlipHorizontal() == !mEditor->view()->isFlipVertical() ? 15.f : -15.f;
    mEditor->view()->rotateRelative(delta);
}

void ActionCommands::PlayStop()
{
    PlaybackManager* playback = mEditor->playback();
    if (playback->isPlaying())
    {
        playback->stop();
    }
    else
    {
        playback->play();
    }
}

namespace
{
// Remembers whether the last key event was an auto-repeat. Qt sends ShortcutOverride before it triggers the action.
class KeyRepeatWatcher : public QObject
{
public:
    using QObject::QObject;
    bool autoRepeat = false;
    bool eventFilter(QObject* obj, QEvent* e) override
    {
        if (e->type() == QEvent::ShortcutOverride || e->type() == QEvent::KeyPress)
        {
            autoRepeat = static_cast<QKeyEvent*>(e)->isAutoRepeat();
        }
        else if (e->type() == QEvent::MouseButtonPress) { autoRepeat = false; }
        return QObject::eventFilter(obj, e);
    }
};
} // namespace

// A held key repeats faster than the playback fps; drop the extra repeats so flipping keeps the project rate.
static bool flipThrottled(Editor* editor)
{
    static KeyRepeatWatcher* watcher = [] {
        auto* w = new KeyRepeatWatcher(qApp);
        qApp->installEventFilter(w);
        return w;
    }();
    static QElapsedTimer sinceFlip;
    const bool skip = skipFrameStep(watcher->autoRepeat, sinceFlip.isValid() ? sinceFlip.elapsed() : -1, editor->playback()->fps());
    if (!skip) { sinceFlip.start(); }
    return skip;
}

void ActionCommands::GotoNextFrame()
{
    if (flipThrottled(mEditor)) { return; }
    mEditor->scrubForward();
}

void ActionCommands::GotoPrevFrame()
{
    if (flipThrottled(mEditor)) { return; }
    mEditor->scrubBackward();
}

void ActionCommands::GotoNextKeyFrame()
{
    mEditor->scrubNextKeyFrame();
}

void ActionCommands::GotoPrevKeyFrame()
{
    mEditor->scrubPreviousKeyFrame();
}

Status ActionCommands::addNewKey()
{
    // Sound keyframes should not be empty, so we try to import a sound instead
    if (mEditor->layers()->currentLayer()->type() == Layer::SOUND)
    {
        return importSound(FileType::SOUND);
    }

    KeyFrame* key = mEditor->addNewKey();
    Camera* cam = dynamic_cast<Camera*>(key);
    if (cam)
    {
        mEditor->view()->forceUpdateViewTransform();
    }

    return Status::OK;
}

static bool anyFrameSelected(const Object* object)
{
    for (int i = 0; i < object->getLayerCount(); ++i)
    {
        if (object->getLayer(i)->hasAnySelectedFrames()) { return true; }
    }
    return false;
}

void ActionCommands::exposeSelectedFrames(int value, bool hold)
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    const bool hasSelectedFrames = anyFrameSelected(mEditor->object());

    // With no selection, expose the drawing at the current frame; it does not move.
    KeyFrame* key = currentLayer->getLastKeyFrameAtPosition(mEditor->currentFrame());
    if (!hasSelectedFrames) {
        if (key == nullptr) { return; }
        currentLayer->setFrameSelected(key->pos(), true);
    }

    mEditor->layers()->setExposureForSelectedFrames(value, hold);
    emit mEditor->updateTimeLine();

    if (!hasSelectedFrames) {
        currentLayer->setFrameSelected(key->pos(), false);
    }
}

void ActionCommands::cleanExposureOfSelectedFrames()
{
    LayerManager* layers = mEditor->layers();
    const bool hasSelectedFrames = anyFrameSelected(mEditor->object());
    if (!hasSelectedFrames) {
        const int index = layers->currentLayerIndex();
        layers->selectFrames(index, index, 1, layers->currentLayer()->getMaxKeyFramePosition());
    }

    layers->cleanExposureOfSelectedFrames();
    emit mEditor->updateTimeLine();

    if (!hasSelectedFrames) { layers->deselectAllFrames(); }
}

void ActionCommands::addExposureToSelectedFrames()
{
    exposeSelectedFrames(1);
}

void ActionCommands::subtractExposureFromSelectedFrames()
{
    exposeSelectedFrames(-1);
}

Status ActionCommands::insertKeyFrameAtCurrentPosition()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    int currentPosition = mEditor->currentFrame();
    if (mEditor->refusesEdit(currentLayer)) { return Status::SAFE; }

    const UndoMacro macro(mEditor->undoRedo(), tr("New drawing"));
    const SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MOVE);
    currentLayer->insertExposureAt(currentPosition);
    mEditor->undoRedo()->record(saveStateId, tr("Add exposure"));
    return addNewKey();
}

void ActionCommands::removeSelectedFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();

    if (!currentLayer->hasAnySelectedFrames()) { return; }

    mEditor->removeSelectedKeyFrames(tr("Remove drawings"));
}

void ActionCommands::reverseSelectedFrames()
{
    Layer* currentLayer = mEditor->layers()->currentLayer();
    if (mEditor->refusesEdit(currentLayer)) { return; }

    const SAVESTATE_ID saveStateId = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MOVE);
    const bool reversed = currentLayer->reverseOrderOfSelection();
    mEditor->undoRedo()->record(saveStateId, tr("Reverse frames"));
    if (!reversed) {
        return;
    }

    if (currentLayer->type() == Layer::CAMERA) {
        mEditor->view()->forceUpdateViewTransform();
    }
    emit mEditor->framesModified();
};

void ActionCommands::removeKey()
{
    mEditor->removeKey();
}

void ActionCommands::duplicateLayer()
{
    Layer* fromLayer = mEditor->layers()->currentLayer();
    int currFrame = mEditor->currentFrame();
    mEditor->layers()->duplicateLayer(fromLayer, tr("%1 (copy)", "Default duplicate layer name").arg(fromLayer->name()));
    mEditor->scrubTo(currFrame);
}

void ActionCommands::duplicateKey()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr) return;
    if (mEditor->refusesHidden(layer) || mEditor->refusesEdit(layer)) { return; }

    KeyFrame* key = layer->getKeyFrameAt(mEditor->currentFrame());
    if (key == nullptr) return;

    // Duplicating a selected keyframe is not handled properly.
    // The desired behavior is to clear selection anyway so we just do that.
    deselectAll();

    KeyFrame* dupKey = key->clone();
    dupKey->setLinkId(0); // a duplicate is independent; Alt-drag links

    int nextEmptyFrame = mEditor->currentFrame() + 1;
    while (layer->keyExistsWhichCovers(nextEmptyFrame))
    {
        nextEmptyFrame += 1;
    }

    layer->addKeyFrame(nextEmptyFrame, dupKey);
    mEditor->scrubTo(nextEmptyFrame);
    emit mEditor->frameModified(nextEmptyFrame);

    if (layer->type() == Layer::SOUND)
    {
        mEditor->sound()->processSound(dynamic_cast<SoundClip*>(dupKey));
        showSoundClipWarningIfNeeded();
    }

    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->layers()->currentLayerChanged(mEditor->layers()->currentLayerIndex()); // trigger timeline repaint.
}

void ActionCommands::moveFrameForward()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer)
    {
        if (layer->moveKeyFrame(mEditor->currentFrame(), 1))
        {
            mEditor->scrubForward();
        }
    }
    mEditor->layers()->notifyAnimationLengthChanged();
    emit mEditor->framesModified();
}

void ActionCommands::moveFrameBackward()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer)
    {
        if (layer->moveKeyFrame(mEditor->currentFrame(), -1))
        {
            mEditor->scrubBackward();
        }
    }
    emit mEditor->framesModified();
}

Status ActionCommands::addNewBitmapLayer()
{
    mEditor->layers()->createBitmapLayer(mEditor->layers()->nameSuggestLayer(tr("Bitmap Layer")));
    return Status::OK;
}

Status ActionCommands::addNewVectorLayer()
{
    mEditor->layers()->createVectorLayer(mEditor->layers()->nameSuggestLayer(tr("Vector Layer")));
    return Status::OK;
}

Status ActionCommands::addNewCameraLayer()
{
    bool ok;
    QString text = QInputDialog::getText(nullptr, tr("Layer Properties", "A popup when creating a new layer"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         mEditor->layers()->nameSuggestLayer(tr("Camera Layer")), &ok);
    if (ok && !text.isEmpty())
    {
        mEditor->layers()->createCameraLayer(text);
    }
    return Status::OK;
}

Status ActionCommands::addNewSoundLayer()
{
    Layer* layer = mEditor->layers()->createSoundLayer(mEditor->layers()->nameSuggestLayer(tr("Sound Layer")));
    mEditor->layers()->setCurrentLayer(layer);
    return Status::OK;
}

Status ActionCommands::deleteCurrentLayer()
{
    LayerManager* layerMgr = mEditor->layers();
    // The menu action and timeline button are disabled when this is false.
    if (!layerMgr->canDeleteLayer(mEditor->currentLayerIndex())) {
        return Status::CANCELED;
    }
    return layerMgr->deleteLayer(mEditor->currentLayerIndex());
}

void ActionCommands::setLayerVisibilityIndex(int index)
{
    mEditor->setLayerVisibility(static_cast<LayerVisibility>(index));
}

// Recolours the Drawing at frame as one KEYFRAME_MODIFY record. The state captures the Drawing at the playhead, so visit the key first.
static void recolourDrawing(Editor* editor, LayerBitmap* layer, int frame, QRgb color, const QString& description)
{
    if (editor->currentFrame() != frame) { editor->scrubTo(frame); }
    const SAVESTATE_ID saveStateId = editor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY, layer);
    layer->getBitmapImageAtFrame(frame)->fillNonAlphaPixels(color);
    editor->undoRedo()->record(saveStateId, description);
}

void ActionCommands::changeKeyframeLineColor()
{
    Layer* layer = mEditor->layers()->currentLayer();
    const int frame = mEditor->currentFrame();
    if (layer->type() != Layer::BITMAP || !layer->keyExists(frame) || mEditor->refusesEdit(layer)) { return; }
    recolourDrawing(mEditor, static_cast<LayerBitmap*>(layer), frame, mEditor->color()->frontColor().rgb(), tr("Change Line Color"));
    mEditor->updateFrame();
}

void ActionCommands::changeallKeyframeLineColor()
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer->type() != Layer::BITMAP || mEditor->refusesEdit(layer)) { return; }
    const QRgb color = mEditor->color()->frontColor().rgb();
    const QString description = tr("Change Line Color");
    const int playhead = mEditor->currentFrame();
    {
        const UndoMacro macro(mEditor->undoRedo(), description);
        QList<int> positions;
        layer->foreachKeyFrame([&](KeyFrame* key) { positions.append(key->pos()); });
        for (int pos : positions) { recolourDrawing(mEditor, static_cast<LayerBitmap*>(layer), pos, color, description); }
    }
    mEditor->scrubTo(playhead);
    mEditor->updateFrame();
}

void ActionCommands::resetAllTools()
{
    mEditor->tools()->resetAllTools();
}

void ActionCommands::help()
{
    QString url = UPSTREAM_DOCS_URL;
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::quickGuide()
{
    QString sDocPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString sCopyDest = QDir(sDocPath).filePath("pencil2d_quick_guide.pdf");

    QFile quickGuideFile(":/app/pencil2d_quick_guide.pdf");
    quickGuideFile.copy(sCopyDest);

    QDesktopServices::openUrl(QUrl::fromLocalFile(sCopyDest));
}

void ActionCommands::website()
{
    QString url = APP_REPO_URL;
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::forum()
{
    QString url = "https://discuss.pencil2d.org/";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::discord()
{
    QString url = "https://discord.gg/8FxdV2g";
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::reportbug()
{
    QString url = APP_ISSUES_URL;
    QDesktopServices::openUrl(QUrl(url));
}

void ActionCommands::checkForUpdates()
{
    CheckUpdatesDialog dialog;
    dialog.startChecking();
    dialog.exec();
}

// This action is a temporary measure until we have an automated recover mechanism in place
void ActionCommands::openTemporaryDirectory()
{
    int ret = QMessageBox::warning(mParent, tr("Warning"), tr("The temporary directory is meant to be used only by JustAnimate. Do not modify it unless you know what you are doing."), QMessageBox::Cancel, QMessageBox::Ok);
    if (ret == QMessageBox::Ok)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::temp().filePath(APP_TEMP_DIR_NAME)));
    }
}

void ActionCommands::about()
{
    AboutDialog* aboutBox = new AboutDialog(mParent);
    aboutBox->setAttribute(Qt::WA_DeleteOnClose);
    aboutBox->init();
    aboutBox->exec();
}

void ActionCommands::showSoundClipWarningIfNeeded()
{
    int clipCount = mEditor->sound()->soundClipCount();
    if (clipCount >= MovieExporter::MAX_SOUND_FRAMES && !mSuppressSoundWarning) {
        QMessageBox::warning(mParent, tr("Warning"), tr("You currently have a total of %1 sound clips. Due to current limitations, you will be unable to export any animation exceeding %2 sound clips. We recommend splitting up larger projects into multiple smaller project to stay within this limit.").arg(clipCount).arg(MovieExporter::MAX_SOUND_FRAMES));
        mSuppressSoundWarning = true;
    } else {
        mSuppressSoundWarning = false;
    }
}
