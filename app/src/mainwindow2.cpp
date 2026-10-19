/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2008-2009 Mj Mendoza IV
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "mainwindow2.h"
#include "appinfo.h"
#include "cellmenus.h"
#include "defaultlayout.h"
#include <QScreen>
#include "ui_mainwindow2.h"

// Qt headers
#include <QActionGroup>
#include <QCursor>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QList>
#include <QMenu>
#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTabletEvent>
#include <QStandardPaths>
#include <QDateTime>
#include <QLabel>
#include <QClipboard>
#include <QToolBar>
#include <QToolButton>
#include <QDebug>
#include <QLineEdit>
#include <QSpinBox>

// core_lib headers
#include "pencildef.h"
#include "platformhandler.h"
#include "theme.h"
#include "pencilsettings.h"
#include "object.h"
#include "editor.h"

#include "filemanager.h"
#include "colormanager.h"
#include "layermanager.h"
#include "cameraeasingtype.h"
#include "toolmanager.h"
#include "playbackmanager.h"
#include "selectionmanager.h"
#include "soundmanager.h"
#include "viewmanager.h"
#include "overlaymanager.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
#include "projectsession.h"

#include "actioncommands.h"
#include "brushpresets.h"
#include "fileformat.h"     //contains constants used by Pencil File Format
#include "util.h"
#include "undoredocommand.h"

// app headers
#include "colorbox.h"
#include "colorinspector.h"
#include "colorpalettewidget.h"
#include "tooloptionwidget.h"
#include "preferencesdialog.h"
#include "timeline.h"
#include "xsheet.h"
#include "storyboardpanel.h"
#include "toolbox.h"
#include "onionskinwidget.h"
#include "brushpresetdock.h"
#include "pegbaralignmentdialog.h"
#include "addtransparencytopaperdialog.h"
#include "repositionframesdialog.h"

#include "errordialog.h"
#include "filedialog.h"
#include "importimageseqdialog.h"
#include "importlayersdialog.h"
#include "importpositiondialog.h"
#include "layeropacitydialog.h"
#include "recentfilemenu.h"
#include "shortcutfilter.h"
#include "commandsearch.h"
#include "app_util.h"
#include "welcomedialog.h"
#include "welcomehelper.h"
#include "pegbaralignmentdialog.h"
#include "papertemplatedialog.h"


#ifdef GIT_TIMESTAMP
#define BUILD_DATE S__GIT_TIMESTAMP
#else
#define BUILD_DATE __DATE__
#endif

namespace {
    QString getWindowTitle() {
        QString version(APP_VERSION);
        if (version.startsWith("99.0.0")) {
            return QString("[*]" APP_NAME " Nightly Build %1").arg(BUILD_DATE);
        } else if (version == "0.0.0.0") {
            return QString("[*]" APP_NAME " Development Build %1").arg(BUILD_DATE);
        } else {
            return QString("[*]" APP_NAME " %1").arg(APP_VERSION);
        }
    }
}



void MainWindow2::setIcons()
{
    const std::pair<QAction*, const char*> icons[] = {
        { ui->menuZoom->menuAction(), "zoom" },
        { ui->menuPerspectiveLinesAngle->menuAction(), "angle" },
        { ui->actionNew, "file-plus" },
        { ui->actionOpen, "folder-open" },
        { ui->actionSave, "device-floppy" },
        { ui->actionSave_as, "device-floppy" },
        { ui->actionUndo, "arrow-back-up" },
        { ui->actionRedo, "arrow-forward-up" },
        { ui->actionCut, "cut" },
        { ui->actionCopy, "copy" },
        { ui->actionPaste, "clipboard" },
        { ui->actionClearFrame, "clear-all" },
        { ui->actionZoom_In, "zoom-in" },
        { ui->actionZoom_Out, "zoom-out" },
        { ui->actionReset_View, "focus-centered" },
        { ui->actionHorizontal_Flip, "flip-vertical" },
        { ui->actionVertical_Flip, "flip-horizontal" },
        { ui->actionGrid, "grid-4x4" },
        { ui->actionOnionSkin, "stack-2" },
        { ui->actionOnionPrev, "stack-back" },
        { ui->actionOnionNext, "stack-front" },
        { ui->actionLoop, "repeat" },
        { ui->actionAdd_Frame, "square-plus" },
        { ui->actionDuplicate_Frame, "copy-plus" },
        { ui->actionRemove_Frame, "square-minus" },
        { ui->actionNext_KeyFrame, "player-track-next" },
        { ui->actionPrev_KeyFrame, "player-track-prev" },
        { ui->actionMove, "arrows-move" },
        { ui->actionSelect, "marquee-2" },
        { ui->actionBrush, "brush" },
        { ui->actionPolyline, "polygon" },
        { ui->actionShape, "shape" },
        { ui->actionLassoFill, "lasso" },
        { ui->actionSmudge, "hand-finger" },
        { ui->actionPen, "ballpen" },
        { ui->actionHand, "hand-stop" },
        { ui->actionRotateView, "rotate-clockwise-2" },
        { ui->actionZoomTool, "zoom" },
        { ui->actionPencil, "pencil" },
        { ui->actionBucket, "bucket-droplet" },
        { ui->actionEyedropper, "color-picker" },
        { ui->actionEraser, "eraser" },
        { ui->actionNew_Bitmap_Layer, "photo" },
        { ui->actionNew_Vector_Layer, "vector" },
        { ui->actionNew_Sound_Layer, "volume" },
        { ui->actionNew_Storyboard_Layer, "clipboard" },
        { ui->actionNew_Camera_Layer, "video" },
        { ui->actionDelete_Current_Layer, "minus" },
        { ui->actionShowInvisibleLines, "line-dotted" },
        { ui->actionShowOutlinesOnly, "border-outer" },
        { ui->actionCenter, "crosshair" },
        { ui->actionThirds, "grid-3x3" },
        { ui->actionGoldenRatio, "spiral" },
        { ui->actionSafeAreas, "frame" },
        { ui->actionOnePointPerspective, "perspective" },
        { ui->actionTwoPointPerspective, "box" },
        { ui->actionThreePointPerspective, "cube" },
    };
    for (const auto& [action, name] : icons)
        action->setIcon(Theme::icon(QStringLiteral(":/icons/tabler/%1.svg").arg(name)));
    mStartIcon = Theme::icon(":/icons/tabler/player-play.svg");
    mStopIcon = Theme::icon(":/icons/tabler/player-stop.svg");
    ui->actionPlay->setIcon(mStartIcon);
}

MainWindow2::MainWindow2(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow2)
{
    ui->setupUi(this);
    setIcons();

    // Initialize order
    // 1. editor 2. object 3. scribble area 4. other widgets
    mEditor = new Editor(this);
    mEditor->setScribbleArea(ui->scribbleArea);
    mEditor->init();
    PlatformHandler::applyTabletApi(mEditor->preference());
    setUpSession();

    newObject();

    ui->scribbleArea->setEditor(mEditor);
    ui->scribbleArea->init();

    ui->statusBar->setEditor(mEditor);
    ui->statusBar->updateZoomStatus();
    ui->statusBar->setVisible(mEditor->preference()->get<SETTING::SHOW_STATUS_BAR>());

    mCommands = new ActionCommands(this);
    mCommands->setCore(mEditor);

    createDockWidgets();
    createToolbars();
    createMenus();
    setupKeyboardShortcuts();
    // A shortcut mid-stroke can land the stroke on another frame, layer or tool.
    (new ShortcutFilter(ui->scribbleArea, this))->installOnActions(this);

    readSettings();

    selectionChanged();
    currentLayerChanged();

    mEditor->tools()->setDefaultTool();
    ui->background->init(mEditor->preference());

    setWindowTitle(getWindowTitle());
}

MainWindow2::~MainWindow2()
{
    delete ui;
}

void MainWindow2::createDockWidgets()
{
    mTimeLine = new TimeLine(this);
    mTimeLine->setObjectName("TimeLine");

    mXSheet = new XSheet(this);
    mXSheet->setObjectName("XSheet");

    mColorBox = new ColorBox(this);
    mColorBox->setToolTip(tr("color palette:<br>use <b>(C)</b><br>toggle at cursor"));
    mColorBox->setObjectName("ColorWheel");

    mColorInspector = new ColorInspector(this);
    mColorInspector->setToolTip(tr("Color inspector"));
    mColorInspector->setObjectName("Color Inspector");

    mColorPalette = new ColorPaletteWidget(this);
    mColorPalette->setCore(mEditor);
    mColorPalette->setObjectName("ColorPalette");

    mOnionSkinWidget = new OnionSkinWidget(this);
    mOnionSkinWidget->setObjectName("Onion Skin");

    mBrushPresets = new BrushPresetDock(this);
    mBrushPresets->setObjectName("BrushPresets");

    mToolOptions = new ToolOptionWidget(this);
    mToolOptions->setObjectName("ToolOption");

    mToolBox = new ToolBoxDockWidget(this);
    mToolBox->setObjectName("ToolBox");

    mStoryboard = new StoryboardPanel(this);
    mStoryboard->setObjectName("Storyboard");

    mDockWidgets
        << mTimeLine
        << mXSheet
        << mColorBox
        << mColorInspector
        << mColorPalette
        << mOnionSkinWidget
        << mBrushPresets
        << mToolOptions
        << mToolBox
        << mStoryboard;


    for (BaseDockWidget* pWidget : mDockWidgets)
    {
        pWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
        pWidget->setFeatures(QDockWidget::DockWidgetFeature::DockWidgetClosable |
                             QDockWidget::DockWidgetFeature::DockWidgetMovable |
                             QDockWidget::DockWidgetFeature::DockWidgetFloatable);
        pWidget->setFocusPolicy(Qt::NoFocus);

        pWidget->setEditor(mEditor);
        pWidget->initUI();
        qDebug() << "Init Dock widget: " << pWidget->objectName();
    }

    applyDefaultDockLayout();
    setDockNestingEnabled(true);

    /*
    mPreview = new PreviewWidget( this );
    mPreview->setImage( mScribbleArea->mBufferImg );
    mPreview->setFeatures( QDockWidget::DockWidgetFloatable );
    mPreview->setFocusPolicy( Qt::NoFocus );
    addDockWidget( Qt::RightDockWidgetArea, mPreview );
    */

    makeConnections(mEditor, ui->scribbleArea);
    makeConnections(mEditor);
    makeConnections(mEditor, mTimeLine);
    makeConnections(mEditor, mColorBox);
    makeConnections(mEditor, mColorInspector);
    makeConnections(mEditor, mColorPalette);
    makeConnections(mEditor, mToolOptions);
    makeConnections(mEditor, ui->statusBar);

    for (BaseDockWidget* w : mDockWidgets)
    {
        if (w->isFloating()) {
            w->show();
            w->raise();
        }
        w->updateUI();
    }
}

void MainWindow2::createMenus()
{
    //--- File Menu ---
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow2::newDocument);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow2::openDocument);
    connect(ui->actionSave_as, &QAction::triggered, this, &MainWindow2::saveAsNewDocument);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow2::saveDocument);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow2::close);

    //--- Export Menu ---
    connect(ui->actionExport_Image, &QAction::triggered, mCommands, &ActionCommands::exportImage);
    connect(ui->actionExport_ImageSeq, &QAction::triggered, mCommands, &ActionCommands::exportImageSequence);
    connect(ui->actionExport_Movie, &QAction::triggered, mCommands, &ActionCommands::exportMovie);
    connect(ui->actionExport_Animated_GIF, &QAction::triggered, mCommands, &ActionCommands::exportGif);

    connect(ui->actionExport_Palette, &QAction::triggered, this, &MainWindow2::exportPalette);
    QAction* exportOnPaper = new QAction(tr("On Paper..."), this);
    connect(exportOnPaper, &QAction::triggered, mCommands, &ActionCommands::exportOnPaper);
    ui->menuExport->insertAction(ui->actionExport_Palette, exportOnPaper);
    QAction* exportStoryboard = new QAction(tr("Storyboard..."), this);
    connect(exportStoryboard, &QAction::triggered, mCommands, &ActionCommands::exportStoryboard);
    ui->menuExport->insertAction(ui->actionExport_Palette, exportStoryboard);
    QAction* exportTimesheet = new QAction(tr("Timesheet..."), this);
    connect(exportTimesheet, &QAction::triggered, mCommands, &ActionCommands::exportTimesheet);
    ui->menuExport->insertAction(ui->actionExport_Palette, exportTimesheet);

    //--- Import Menu ---
    //connect( ui->actionExport_Svg_Image, &QAction::triggered, editor, &Editor::saveSvg );
    connect(ui->actionImport_Image, &QAction::triggered, this, &MainWindow2::importImage);
    connect(ui->actionImport_ImageSeq, &QAction::triggered, this, &MainWindow2::importImageSequence);
    connect(ui->actionImport_ImageSeqNum, &QAction::triggered, this, &MainWindow2::importPredefinedImageSet);
    connect(ui->actionImportLayers_from_pclx, &QAction::triggered, this, &MainWindow2::importLayers);
    connect(ui->actionImport_MovieVideo, &QAction::triggered, this, &MainWindow2::importMovieVideo);
    connect(ui->actionImport_AnimatedImage, &QAction::triggered, this, &MainWindow2::importAnimatedImage);

    connect(ui->actionImport_Sound, &QAction::triggered, [=] { mCommands->importSound(FileType::SOUND); });
    connect(ui->actionImport_MovieAudio, &QAction::triggered, [=] { mCommands->importSound(FileType::MOVIE); });

    connect(ui->actionImport_Append_Palette, &QAction::triggered, this, &MainWindow2::importPalette);
    connect(ui->actionImport_Replace_Palette, &QAction::triggered, this, &MainWindow2::openPalette);

    // Brush preset files; the preset dock lists the result.
    ui->menuImport->addAction(tr("Brush Preset..."), this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Import Brush Preset"), QString(), tr("Brush preset (*.jabrush)"));
        const Status st = file.isEmpty() ? Status(Status::CANCELED) : mBrushPresets->importPreset(file);
        if (!st.ok() && st != Status::CANCELED)
            QMessageBox::warning(this, tr("Import Brush Preset"), st.description());
    });
    ui->menuExport->addAction(tr("Brush Preset..."), this, [this] {
        const QStringList names = BrushPresets().names();
        bool ok = false;
        const QString name = names.isEmpty() ? QString() : QInputDialog::getItem(this, tr("Export Brush Preset"), tr("Preset:"), names, 0, false, &ok);
        if (!ok)
            return;
        const QString file = QFileDialog::getSaveFileName(this, tr("Export Brush Preset"), name + ".jabrush", tr("Brush preset (*.jabrush)"));
        const Status st = file.isEmpty() ? Status(Status::CANCELED) : BrushPresets().exportPreset(name, file);
        if (!st.ok() && st != Status::CANCELED)
            QMessageBox::warning(this, tr("Export Brush Preset"), st.description());
    });

    //--- Edit Menu ---
    replaceUndoRedoActions();
    connect(ui->actionCut, &QAction::triggered, mEditor, &Editor::copyAndCut);
    connect(ui->actionCopy, &QAction::triggered, mEditor, &Editor::copy);
    connect(ui->actionPaste_Previous, &QAction::triggered, mEditor, &Editor::pasteFromPreviousFrame);
    connect(ui->actionPaste, &QAction::triggered, mEditor, &Editor::paste);
    connect(ui->actionClearFrame, &QAction::triggered, mEditor, &Editor::clearCurrentFrame);
    connect(mEditor->select(), &SelectionManager::selectionChanged, this, &MainWindow2::selectionChanged);
    connect(ui->actionFlip_X, &QAction::triggered, mCommands, &ActionCommands::flipSelectionX);
    connect(ui->actionFlip_Y, &QAction::triggered, mCommands, &ActionCommands::flipSelectionY);
    connect(ui->actionPegbarAlignment, &QAction::triggered, this, &MainWindow2::openPegAlignDialog);
    connect(ui->actionAdd_Transparency_to_paper, &QAction::triggered, this, &MainWindow2::openAddTranspToPaperDialog);
    connect(ui->actionSelect_All, &QAction::triggered, mCommands, &ActionCommands::selectAll);
    connect(ui->actionDeselect_All, &QAction::triggered, mCommands, &ActionCommands::deselectAll);
    connect(ui->actionReposition_Selected_Frames, &QAction::triggered, this, &MainWindow2::openRepositionDialog);
    connect(ui->actionPreference, &QAction::triggered, [=] { preferences(); });
    connect(ui->actionCommandSearch, &QAction::triggered, this, &MainWindow2::openCommandSearch);

    //--- Layer Menu ---
    connect(ui->actionNew_Bitmap_Layer, &QAction::triggered, mCommands, &ActionCommands::addNewBitmapLayer);
    connect(ui->actionNew_Vector_Layer, &QAction::triggered, mCommands, &ActionCommands::addNewVectorLayer);
    connect(ui->actionNew_Sound_Layer, &QAction::triggered, mCommands, &ActionCommands::addNewSoundLayer);
    connect(ui->actionNew_Storyboard_Layer, &QAction::triggered, mCommands, &ActionCommands::addNewStoryboardLayer);
    connect(ui->actionNew_Camera_Layer, &QAction::triggered, mCommands, qOverload<>(&ActionCommands::addNewCameraLayer));
    connect(ui->actionDelete_Current_Layer, &QAction::triggered, mCommands, &ActionCommands::deleteCurrentLayer);
    connect(ui->actionRename_Layer, &QAction::triggered, mTimeLine, &TimeLine::renameCurrentLayer);
    connect(ui->actionChangeLineColorCurrent_keyframe, &QAction::triggered, mCommands, &ActionCommands::changeKeyframeLineColor);
    connect(ui->actionChangeLineColorAll_keyframes_on_layer, &QAction::triggered, mCommands, &ActionCommands::changeallKeyframeLineColor);
    connect(ui->actionChangeLayerOpacity, &QAction::triggered, this, &MainWindow2::openLayerOpacityDialog);

    QList<QAction*> visibilityActions = ui->menuLayer_Visibility->actions();
    auto visibilityGroup = new QActionGroup(this);
    visibilityGroup->setExclusive(true);
    for (int i = 0; i < visibilityActions.size(); i++) {
        QAction* action = visibilityActions[i];
        visibilityGroup->addAction(action);
        connect(action, &QAction::triggered, [=] { mCommands->setLayerVisibilityIndex(i); });
    }
    visibilityActions[mEditor->preference()->get<SETTING::LAYER_VISIBILITY>()]->setChecked(true);
    connect(mEditor->preference(), &PreferenceManager::optionChanged, [=](SETTING e) {
        if (e == SETTING::LAYER_VISIBILITY) {
            visibilityActions[mEditor->preference()->get<SETTING::LAYER_VISIBILITY>()]->setChecked(true);
        }
    });

    // --- View Menu ---
    connect(ui->actionZoom_In, &QAction::triggered, mCommands, &ActionCommands::ZoomIn);
    connect(ui->actionZoom_Out, &QAction::triggered, mCommands, &ActionCommands::ZoomOut);
    connect(ui->actionRotate_Clockwise, &QAction::triggered, mCommands, &ActionCommands::rotateClockwise);
    connect(ui->actionRotate_Anticlockwise, &QAction::triggered, mCommands, &ActionCommands::rotateCounterClockwise);
    connect(ui->actionReset_Rotation, &QAction::triggered, mEditor->view(), &ViewManager::resetRotation);
    connect(ui->actionReset_View, &QAction::triggered, mEditor->view(), &ViewManager::resetView);
    connect(ui->actionCenter_View, &QAction::triggered, mEditor->view(), &ViewManager::centerView);
    connect(ui->actionZoom400, &QAction::triggered, mEditor->view(), &ViewManager::scale400);
    connect(ui->actionZoom300, &QAction::triggered, mEditor->view(), &ViewManager::scale300);
    connect(ui->actionZoom200, &QAction::triggered, mEditor->view(), &ViewManager::scale200);
    connect(ui->actionZoom100, &QAction::triggered, mEditor->view(), &ViewManager::scale100);
    connect(ui->actionZoom50, &QAction::triggered, mEditor->view(), &ViewManager::scale50);
    connect(ui->actionZoom33, &QAction::triggered, mEditor->view(), &ViewManager::scale33);
    connect(ui->actionZoom25, &QAction::triggered, mEditor->view(), &ViewManager::scale25);
    connect(ui->actionHorizontal_Flip, &QAction::triggered, mEditor->view(), &ViewManager::flipHorizontal);
    connect(ui->actionVertical_Flip, &QAction::triggered, mEditor->view(), &ViewManager::flipVertical);
    connect(mEditor->view(), &ViewManager::viewFlipped, this, &MainWindow2::viewFlipped);

    PreferenceManager* prefs = mEditor->preference();
    connect(ui->actionStatusBar, &QAction::triggered, ui->statusBar, &QStatusBar::setVisible);
    bindPreferenceSetting<SETTING::SHOW_STATUS_BAR>(ui->actionStatusBar, prefs);
    bindPreferenceSetting<SETTING::OVERLAY_PERSPECTIVE3>(ui->actionThreePointPerspective, prefs);
    bindPreferenceSetting<SETTING::OVERLAY_PERSPECTIVE2>(ui->actionTwoPointPerspective, prefs);
    bindPreferenceSetting<SETTING::OVERLAY_PERSPECTIVE1>(ui->actionOnePointPerspective, prefs);
    bindPreferenceSetting<SETTING::OVERLAY_SAFE>(ui->actionSafeAreas, prefs);
    bindPreferenceSetting<SETTING::OVERLAY_GOLDEN>(ui->actionGoldenRatio, prefs);
    bindPreferenceSetting<SETTING::OVERLAY_THIRDS>(ui->actionThirds, prefs);
    bindPreferenceSetting<SETTING::OVERLAY_CENTER>(ui->actionCenter, prefs);
    bindPreferenceSetting<SETTING::GRID>(ui->actionGrid, prefs);

    QAction* rulerAction = new QAction(tr("Ruler"), this);
    rulerAction->setCheckable(true);
    rulerAction->setToolTip(tr("Strokes that start near the ruler snap to it. Drag its ends with the Move tool."));
    ui->menuOverlays->insertAction(ui->menuPerspectiveLinesAngle->menuAction(), rulerAction);
    connect(rulerAction, &QAction::toggled, this, [this](bool on) {
        mEditor->overlays()->setRulerEnabled(on);
        mEditor->updateFrame();
    });

    QAction* perspectiveSnapAction = new QAction(tr("Snap to Perspective"), this);
    perspectiveSnapAction->setCheckable(true);
    perspectiveSnapAction->setToolTip(tr("Strokes follow the line from their start point toward a vanishing point of the visible perspective overlay."));
    ui->menuOverlays->insertAction(ui->menuPerspectiveLinesAngle->menuAction(), perspectiveSnapAction);
    connect(perspectiveSnapAction, &QAction::toggled, this, [this](bool on) {
        mEditor->overlays()->setPerspectiveSnapEnabled(on);
    });
    bindPreferenceSetting<SETTING::OUTLINES>(ui->actionShowOutlinesOnly, prefs);
    bindPreferenceSetting<SETTING::INVISIBLE_LINES>(ui->actionShowInvisibleLines, prefs);
    bindPreferenceSetting<SETTING::PREV_ONION>(ui->actionOnionPrev, prefs);
    bindPreferenceSetting<SETTING::NEXT_ONION>(ui->actionOnionNext, prefs);
    const auto syncOnionSkin = [this, prefs] {
        QSignalBlocker block(ui->actionOnionSkin);
        ui->actionOnionSkin->setChecked(prefs->get<SETTING::PREV_ONION>() || prefs->get<SETTING::NEXT_ONION>());
    };
    syncOnionSkin();
    connect(prefs, &PreferenceManager::optionChanged, this, [syncOnionSkin](SETTING s) {
        if (s == SETTING::PREV_ONION || s == SETTING::NEXT_ONION) { syncOnionSkin(); }
    });
    connect(ui->actionOnionSkin, &QAction::triggered, mEditor, &Editor::toggleOnionSkin);

    mShiftAndTraceAction = new QAction(tr("Shift and Trace"), this);
    mShiftAndTraceAction->setCheckable(true);
    mShiftAndTraceAction->setStatusTip(tr("Move and rotate the onion skins in the view to trace an inbetween. Drawings stay unchanged."));
    ui->menuOnion_Skin->addAction(mShiftAndTraceAction);
    connect(mShiftAndTraceAction, &QAction::toggled, ui->scribbleArea, &ScribbleArea::setShiftAndTrace);

    bool enableSafeArea = (prefs->get<SETTING::ACTION_SAFE_ON>() || prefs->get<SETTING::TITLE_SAFE_ON>());
    ui->actionSafeAreas->setEnabled(enableSafeArea);

    auto perspectiveLinesAngleGroup = new QActionGroup(this);
    perspectiveLinesAngleGroup->setExclusive(true);
    perspectiveLinesAngleGroup->addAction(ui->action2Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action3Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action5Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action7_5Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action10Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action15Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action20Degrees);
    perspectiveLinesAngleGroup->addAction(ui->action30Degrees);
    connect(perspectiveLinesAngleGroup, &QActionGroup::triggered, this, [&](QAction* action) {
        if (action == ui->action2Degrees)        mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(2);
        else if (action == ui->action3Degrees)   mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(3);
        else if (action == ui->action5Degrees)   mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(5);
        else if (action == ui->action7_5Degrees) mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(7);
        else if (action == ui->action10Degrees)  mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(10);
        else if (action == ui->action15Degrees)  mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(15);
        else if (action == ui->action20Degrees)  mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(20);
        else if (action == ui->action30Degrees)  mEditor->preference()->set<SETTING::OVERLAY_ANGLE>(30);
        else Q_UNREACHABLE();
        emit mEditor->view()->viewChanged();
    });

    switch (prefs->get<SETTING::OVERLAY_ANGLE>()) {
    case 2: ui->action2Degrees->setChecked(true); break;
    case 3: ui->action3Degrees->setChecked(true); break;
    case 5: ui->action5Degrees->setChecked(true); break;
    case 7: ui->action7_5Degrees->setChecked(true); break;
    case 10: ui->action10Degrees->setChecked(true); break;
    case 15: ui->action15Degrees->setChecked(true); break;
    case 20: ui->action20Degrees->setChecked(true); break;
    case 30: ui->action30Degrees->setChecked(true); break;
    }

    QAction* paperAction = ui->menuView->addAction(tr("Paper Template..."));
    paperAction->setStatusTip(tr("Show the genga sheet with its title block around the camera frame"));
    connect(paperAction, &QAction::triggered, this, [this] {
        PaperTemplateDialog dialog(mEditor->object()->data()->paperTemplate(), this);
        if (dialog.exec() == QDialog::Accepted) { mEditor->setPaperTemplate(dialog.paperTemplate()); }
    });

    //--- Animation Menu ---
    PlaybackManager* pPlaybackManager = mEditor->playback();
    connect(ui->actionPlay, &QAction::triggered, mCommands, &ActionCommands::PlayStop);
    connect(ui->actionPlay_Around, &QAction::triggered, pPlaybackManager, &PlaybackManager::playAround);

    connect(ui->actionLoop, &QAction::triggered, pPlaybackManager, &PlaybackManager::setLooping);
    connect(ui->actionLoopControl, &QAction::triggered, pPlaybackManager, &PlaybackManager::enableRangedPlayback);
    connect(pPlaybackManager, &PlaybackManager::loopStateChanged, ui->actionLoop, &QAction::setChecked);
    connect(pPlaybackManager, &PlaybackManager::loopStateChanged, mTimeLine, &TimeLine::setLoop);
    connect(pPlaybackManager, &PlaybackManager::rangedPlaybackStateChanged, ui->actionLoopControl, &QAction::setChecked);
    connect(pPlaybackManager, &PlaybackManager::rangedPlaybackStateChanged, mTimeLine, &TimeLine::setRangeState);
    connect(pPlaybackManager, &PlaybackManager::playStateChanged, mTimeLine, &TimeLine::setPlaying);
    connect(pPlaybackManager, &PlaybackManager::playStateChanged, this, &MainWindow2::changePlayState);
    connect(pPlaybackManager, &PlaybackManager::playStateChanged, ui->scribbleArea, &ScribbleArea::onPlayStateChanged);
    connect(ui->actionFlip_inbetween, &QAction::triggered, pPlaybackManager, &PlaybackManager::playFlipInBetween);
    connect(ui->actionFlip_rolling, &QAction::triggered, pPlaybackManager, &PlaybackManager::playFlipRoll);
    const std::pair<QAction*, Mark> markActions[] = { { ui->actionMark_Key, Mark::Key },
        { ui->actionMark_Breakdown, Mark::Breakdown }, { ui->actionMark_Inbetween, Mark::Inbetween },
        { ui->actionClear_Mark, Mark::None } };
    for (const auto& [action, mark] : markActions)
    {
        connect(action, &QAction::triggered, this, [this, mark = mark] { mEditor->layers()->setKeyMark(mEditor->currentFrame(), mark); });
    }
    connect(ui->actionCreate_Inbetweens, &QAction::triggered, this, &MainWindow2::createInbetweens);
    mXSheet->setEditActions({ ui->actionAdd_Frame, ui->actionExposure_Add, ui->actionExposure_Subtract,
                              ui->actionRipple_Insert_Frame, ui->actionRipple_Delete_Frame });

    connect(ui->actionAdd_Frame, &QAction::triggered, mCommands, &ActionCommands::insertKeyFrameAtCurrentPosition);
    connect(ui->actionRemove_Frame, &QAction::triggered, mCommands, &ActionCommands::removeKey);
    connect(ui->actionRipple_Insert_Frame, &QAction::triggered, this, [this] { mEditor->layers()->rippleInsertFrame(); });
    connect(ui->actionRipple_Delete_Frame, &QAction::triggered, this, [this] { mEditor->layers()->rippleRemoveFrame(); });
    connect(ui->actionExposure_Add, &QAction::triggered, this, [this] { mEditor->layers()->changeExposure(mEditor->currentFrame(), 1); });
    connect(ui->actionExposure_Subtract, &QAction::triggered, this, [this] { mEditor->layers()->changeExposure(mEditor->currentFrame(), -1); });
    connect(ui->actionAdd_Frame_Exposure, &QAction::triggered, mCommands, &ActionCommands::addExposureToSelectedFrames);
    connect(ui->actionSubtract_Frame_Exposure, &QAction::triggered, mCommands, &ActionCommands::subtractExposureFromSelectedFrames);
    connect(ui->actionHold_1s, &QAction::triggered, mCommands, [this] { mCommands->exposeSelectedFrames(1, true); });
    connect(ui->actionHold_2s, &QAction::triggered, mCommands, [this] { mCommands->exposeSelectedFrames(2, true); });
    connect(ui->actionHold_3s, &QAction::triggered, mCommands, [this] { mCommands->exposeSelectedFrames(3, true); });
    connect(ui->actionClean_Exposure, &QAction::triggered, mCommands, &ActionCommands::cleanExposureOfSelectedFrames);
    connect(ui->actionNext_Frame, &QAction::triggered, mCommands, &ActionCommands::GotoNextFrame);
    connect(ui->actionPrevious_Frame, &QAction::triggered, mCommands, &ActionCommands::GotoPrevFrame);
    connect(ui->actionNext_KeyFrame, &QAction::triggered, mCommands, &ActionCommands::GotoNextKeyFrame);
    connect(ui->actionPrev_KeyFrame, &QAction::triggered, mCommands, &ActionCommands::GotoPrevKeyFrame);
    connect(ui->actionDuplicate_Frame, &QAction::triggered, mCommands, &ActionCommands::duplicateKey);
    connect(ui->actionMove_Frame_Forward, &QAction::triggered, mCommands, &ActionCommands::moveFrameForward);
    connect(ui->actionMove_Frame_Backward, &QAction::triggered, mCommands, &ActionCommands::moveFrameBackward);

    connect(ui->actionReverse_Frames_Order, &QAction::triggered, mCommands, &ActionCommands::reverseSelectedFrames);
    connect(ui->actionRemove_Frames, &QAction::triggered, mCommands, &ActionCommands::removeSelectedFrames);


    auto toolsActionGroup = new QActionGroup(this);
    toolsActionGroup->setExclusive(true);
    toolsActionGroup->addAction(ui->actionMove);
    toolsActionGroup->addAction(ui->actionSelect);
    toolsActionGroup->addAction(ui->actionBrush);
    toolsActionGroup->addAction(ui->actionPolyline);
    toolsActionGroup->addAction(ui->actionShape);
    toolsActionGroup->addAction(ui->actionLassoFill);
    toolsActionGroup->addAction(ui->actionSmudge);
    toolsActionGroup->addAction(ui->actionPen);
    toolsActionGroup->addAction(ui->actionHand);
    toolsActionGroup->addAction(ui->actionRotateView);
    toolsActionGroup->addAction(ui->actionZoomTool);
    toolsActionGroup->addAction(ui->actionPencil);
    toolsActionGroup->addAction(ui->actionBucket);
    toolsActionGroup->addAction(ui->actionEyedropper);
    toolsActionGroup->addAction(ui->actionEraser);

    connect(toolsActionGroup, &QActionGroup::triggered, this, [&](QAction* action) {
        if (action == ui->actionMove) mToolBox->setActiveTool(MOVE);
        else if (action == ui->actionSelect) mToolBox->setActiveTool(SELECT);
        else if (action == ui->actionBrush) mToolBox->setActiveTool(BRUSH);
        else if (action == ui->actionPolyline) mToolBox->setActiveTool(POLYLINE);
        else if (action == ui->actionShape) mToolBox->setActiveTool(SHAPE);
        else if (action == ui->actionLassoFill) mToolBox->setActiveTool(LASSOFILL);
        else if (action == ui->actionSmudge) mToolBox->setActiveTool(SMUDGE);
        else if (action == ui->actionPen) mToolBox->setActiveTool(PEN);
        else if (action == ui->actionHand) mToolBox->setActiveTool(HAND);
        else if (action == ui->actionRotateView) mToolBox->setActiveTool(ROTATE);
        else if (action == ui->actionZoomTool) mToolBox->setActiveTool(ZOOM);
        else if (action == ui->actionPencil) mToolBox->setActiveTool(PENCIL);
        else if (action == ui->actionBucket) mToolBox->setActiveTool(BUCKET);
        else if (action == ui->actionEyedropper) mToolBox->setActiveTool(EYEDROPPER);
        else if (action == ui->actionEraser) mToolBox->setActiveTool(ERASER);
        else Q_UNREACHABLE();
    });

    // Both open under the cursor, so a pen hovering the canvas can pick a slice right away.
    auto cursorInCanvas = [this] {
        ScribbleArea* area = ui->scribbleArea;
        const QPoint p = area->mapFromGlobal(QCursor::pos());
        return QPointF(area->rect().contains(p) ? p : area->rect().center());
    };
    mRadialToolAction = new QAction(tr("Radial Tool Menu"), this);
    mRadialToolAction->setToolTip(tr("Opens a ring of tools under the cursor. Tap or release the pen on a tool to pick it."));
    ui->menuTools->addAction(mRadialToolAction);
    connect(mRadialToolAction, &QAction::triggered, this, [this, cursorInCanvas] {
        ui->scribbleArea->openToolMenu(cursorInCanvas());
    });
    mPresetPickerAction = new QAction(tr("Quick Preset Picker"), this);
    mPresetPickerAction->setToolTip(tr("Opens a ring of brush presets under the cursor. Tap or release the pen on a preset to apply it to the current tool."));
    ui->menuTools->addAction(mPresetPickerAction);
    connect(mPresetPickerAction, &QAction::triggered, this, [this, cursorInCanvas] {
        ui->scribbleArea->openPresetPicker(cursorInCanvas());
    });

    //--- Window Menu ---
    QMenu* winMenu = ui->menuWindows;
    const std::vector<QAction*> actions
    {
        mToolBox->toggleViewAction(),
        mToolOptions->toggleViewAction(),
        mColorBox->toggleViewAction(),
        mColorPalette->toggleViewAction(),
        mTimeLine->toggleViewAction(),
        mXSheet->toggleViewAction(),
        mColorInspector->toggleViewAction(),
        mOnionSkinWidget->toggleViewAction(),
        mBrushPresets->toggleViewAction(),
        mStoryboard->toggleViewAction()
    };

    for (QAction* action : actions)
    {
        action->setMenuRole(QAction::NoRole);
        winMenu->insertAction(mCommandBar->toggleViewAction(), action);
    }
    winMenu->insertSeparator(mCommandBar->toggleViewAction());

    QMenu* workspaceMenu = new QMenu(tr("Layout"), winMenu);
    auto* workspaceGroup = new QActionGroup(this);
    QSettings& workspaceSettings = pencilSettings();
    for (const QString& name : workspaceNames())
    {
        QAction* a = workspaceMenu->addAction(QCoreApplication::translate("Workspace", name.toUtf8().constData()));
        a->setData(name);
        a->setCheckable(true);
        a->setChecked(name == currentWorkspace(workspaceSettings));
        workspaceGroup->addAction(a);
    }
    connect(workspaceGroup, &QActionGroup::triggered, this, [this](QAction* a) { switchToWorkspace(a->data().toString()); });
    workspaceMenu->addSeparator();
    workspaceMenu->addAction(ui->actionResetWindows);
    winMenu->insertMenu(mCommandBar->toggleViewAction(), workspaceMenu);
    connect(ui->actionResetWindows, &QAction::triggered, this, &MainWindow2::resetAndDockAllSubWidgets);
    connect(ui->actionLockWindows, &QAction::toggled, this, &MainWindow2::lockWidgets);
    bindPreferenceSetting<SETTING::LAYOUT_LOCK>(ui->actionLockWindows, prefs);

    //--- Help Menu ---
    connect(ui->actionHelp, &QAction::triggered, mCommands, &ActionCommands::help);
    connect(ui->actionWebsite, &QAction::triggered, mCommands, &ActionCommands::website);
    connect(ui->actionCheck_for_Updates, &QAction::triggered, mCommands, &ActionCommands::checkForUpdates);
    connect(ui->actionReport_Bug, &QAction::triggered, mCommands, &ActionCommands::reportbug);
    connect(ui->actionExport_Log, &QAction::triggered, mCommands, &ActionCommands::exportLog);
    connect(ui->actionOpen_Temporary_Directory, &QAction::triggered, mCommands, &ActionCommands::openTemporaryDirectory);
    connect(ui->actionAbout, &QAction::triggered, mCommands, &ActionCommands::about);

    //--- Menus ---
    mRecentFileMenu = new RecentFileMenu(tr("Open Recent"), this);
    mRecentFileMenu->setRecentFiles(mSession->recentFiles());
    ui->menuFile->insertMenu(ui->actionSave, mRecentFileMenu);

    connect(mRecentFileMenu, &RecentFileMenu::clearRequested, mSession, &ProjectSession::clearRecentFiles);
    connect(mSession, &ProjectSession::recentFilesChanged, mRecentFileMenu, &RecentFileMenu::setRecentFiles);

    connect(mRecentFileMenu, &RecentFileMenu::loadRecentFile, this, &MainWindow2::openFile);
}

void MainWindow2::replaceUndoRedoActions()
{
    ui->menuEdit->removeAction(ui->actionUndo);
    ui->menuEdit->removeAction(ui->actionRedo);
    ui->actionUndo = mEditor->undoRedo()->createUndoAction(this, ui->actionUndo->icon());
    ui->actionRedo = mEditor->undoRedo()->createRedoAction(this, ui->actionRedo->icon());
    ui->menuEdit->insertAction(ui->actionCut, ui->actionUndo);
    ui->menuEdit->insertAction(ui->actionCut, ui->actionRedo);
}

void MainWindow2::setOpacity(int opacity)
{
    mEditor->preference()->set<SETTING::WINDOW_OPACITY>(100 - opacity);
    setWindowOpacity(opacity / 100.0);
}

void MainWindow2::updateSaveState()
{
    const bool hasUnsavedChanges = mSession->isDirty();
    setWindowModified(hasUnsavedChanges);
    ui->statusBar->updateModifiedStatus(hasUnsavedChanges);
}

void MainWindow2::openPegAlignDialog()
{
    if (mPegAlign != nullptr)
    {
        mPegAlign->activateWindow();
        mPegAlign->raise();
        return;
    }

    mPegAlign = new PegBarAlignmentDialog(mEditor, this);
    mPegAlign->setAttribute(Qt::WA_DeleteOnClose);

    Qt::WindowFlags flags = mPegAlign->windowFlags();
    flags |= Qt::WindowStaysOnTopHint;
    flags &= (~Qt::WindowContextHelpButtonHint);
    mPegAlign->setWindowFlags(flags);
    mPegAlign->show();
    connect(mPegAlign, &PegBarAlignmentDialog::finished, [=]
    {
        mPegAlign = nullptr;
    });
}

void MainWindow2::openLayerOpacityDialog()
{
    if (mLayerOpacityDialog != nullptr)
    {
        mLayerOpacityDialog->activateWindow();
        mLayerOpacityDialog->raise();
        return;
    }
    mLayerOpacityDialog = new LayerOpacityDialog(this);
    mLayerOpacityDialog->setAttribute(Qt::WA_DeleteOnClose);
    mLayerOpacityDialog->setCore(mEditor);
    mLayerOpacityDialog->initUI();
    mLayerOpacityDialog->setWindowFlags(mLayerOpacityDialog->windowFlags() | Qt::WindowStaysOnTopHint);
    mLayerOpacityDialog->show();

    connect(mLayerOpacityDialog, &LayerOpacityDialog::finished, [=]
    {
        mLayerOpacityDialog = nullptr;
    });
}

void MainWindow2::openAddTranspToPaperDialog()
{
    if (mAddTranspToPaper)
    {
        mAddTranspToPaper->activateWindow();
        mAddTranspToPaper->raise();
        return;
    }

    mAddTranspToPaper = new AddTransparencyToPaperDialog(this);
    mAddTranspToPaper->setCore(mEditor);
    mAddTranspToPaper->initUI();
    mAddTranspToPaper->setWindowFlag(Qt::WindowStaysOnTopHint);
    mAddTranspToPaper->show();

    connect(mAddTranspToPaper, &AddTransparencyToPaperDialog::finished, [=](int result)
    {
        if (result == QDialog::Accepted)
        {
            mSuppressAutoSaveDialog = true;
            mAddTranspToPaper->traceScannedDrawings();
            mSuppressAutoSaveDialog = false;
        }
        mAddTranspToPaper->deleteLater();
        mAddTranspToPaper = nullptr;
    });
}

void MainWindow2::openRepositionDialog()
{
    if (mEditor->layers()->currentLayer()->getSelectedFramesByPos().count() < 2)
    {
        QMessageBox::information(this, nullptr,
                                 tr("Please select at least 2 frames!"),
                                 QMessageBox::Ok);
        return;
    }
    if (mReposDialog != nullptr)
    {
        return;
    }

    mReposDialog = new RepositionFramesDialog(this);
    mReposDialog->setAttribute(Qt::WA_DeleteOnClose);
    mReposDialog->setWindowFlags(mReposDialog->windowFlags() | Qt::WindowStaysOnTopHint);
    hideQuestionMark(*mReposDialog);
    mReposDialog->setCore(mEditor);
    mReposDialog->initUI();
    mEditor->tools()->setCurrentTool(ToolType::MOVE);
    connect(mReposDialog, &RepositionFramesDialog::finished, this, &MainWindow2::closeRepositionDialog);
    mReposDialog->show();
}

void MainWindow2::closeRepositionDialog()
{
    selectionChanged();
    mReposDialog = nullptr;
}

void MainWindow2::currentLayerChanged()
{
    bool isBitmap = (mEditor->layers()->currentLayer()->type() == Layer::BITMAP);
    ui->menuChange_line_color->setEnabled(isBitmap);

    bool isVector = (mEditor->layers()->currentLayer()->type() == Layer::VECTOR);
    ui->actionShowInvisibleLines->setEnabled(isVector);
    ui->actionShowOutlinesOnly->setEnabled(isVector);
}

void MainWindow2::selectionChanged()
{
    bool somethingSelected = mEditor->select()->somethingSelected();
    ui->menuSelection->setEnabled(somethingSelected);
}

void MainWindow2::viewFlipped()
{
    ui->actionHorizontal_Flip->setChecked(mEditor->view()->isFlipHorizontal());
    ui->actionVertical_Flip->setChecked(mEditor->view()->isFlipVertical());
}

void MainWindow2::closeEvent(QCloseEvent* event)
{
    if (m2ndCloseEvent)
    {
        // https://bugreports.qt.io/browse/QTBUG-43344
        event->accept();
        return;
    }

    if (!mSession->close())
    {
        event->ignore();
        return;
    }
    writeSettings();
    event->accept();
    m2ndCloseEvent = true;
}

void MainWindow2::tabletEvent(QTabletEvent* event)
{
    event->ignore();
}

void MainWindow2::newDocument()
{
    if (mSession->maybeSave() && !tryLoadPreset())
    {
        newObject();
    }
}

void MainWindow2::openDocument()
{
    if (!mSession->maybeSave())
    {
        return;
    }
    QString fileName = FileDialog::getOpenFileName(this, FileType::ANIMATION);
    if (!fileName.isEmpty())
    {
        openObject(fileName);
    }
}

bool MainWindow2::saveAsNewDocument()
{
    return mSession->saveAs(FileDialog::getSaveFileName(this, FileType::ANIMATION)).ok();
}

void MainWindow2::openStartupFile(const QString& filename)
{
    if (mSession->recover())
    {
        return;
    }

    if (!filename.isEmpty() && openObject(filename))
    {
        return;
    }

    PreferenceManager* prefs = mEditor->preference();
    const int action = prefs->get<SETTING::STARTUP_ACTION>();
    if (action == STARTUP_LAST_PROJECT && loadMostRecent())
    {
        return;
    }
    if (filename.isEmpty() && action == STARTUP_WELCOME)
    {
        showWelcomeDialog();
        return;
    }
    newObjectFromPresets(prefs->get<SETTING::DEFAULT_PRESET>());
}

void MainWindow2::openFile(const QString& filename)
{
    if (mSession->maybeSave())
    {
        openObject(filename);
    }
}

bool MainWindow2::openObject(const QString& strFilePath)
{
    Status s = mSession->open(strFilePath);
    if (!s.ok())
    {
        ErrorDialog errorDialog(s.title(), s.description(), s.details().str());
        errorDialog.exec();
        emptyDocumentWhenErrorOccurred();
        return false;
    }

    closeDialogs();

    setWindowTitle(mEditor->object()->filePath().prepend("[*]"));
    setWindowModified(false);
    ui->statusBar->updateModifiedStatus(false);

    updateSaveState();

    if (!QFileInfo(strFilePath).isWritable())
    {
        QMessageBox::warning(this, tr("Warning"),
                             tr("This program does not currently have permission to write to the file you have selected. "
                                "Please make sure you have write permission for this file before attempting to save it. "
                                "Alternatively, you can use the Save As... menu option to save to a writable location."),
                             QMessageBox::Ok);
    }

    return true;
}

void MainWindow2::setUpSession()
{
    mSession = new ProjectSession(mEditor, this);
    mSession->askSaveChanges = [this]
    {
        const int ret = QMessageBox::warning(this, tr("Warning"),
                                             tr("This animation has been modified.\n Do you want to save your changes?"),
                                             QMessageBox::Discard | QMessageBox::Save | QMessageBox::Cancel);
        if (ret == QMessageBox::Save) { return ProjectSession::SaveChoice::Save; }
        if (ret == QMessageBox::Discard) { return ProjectSession::SaveChoice::Discard; }
        return ProjectSession::SaveChoice::Cancel;
    };
    mSession->askSavePath = [this] { return FileDialog::getSaveFileName(this, FileType::ANIMATION); };
    mSession->askRecover = [this](const QString& folder) { return askRecoverProject(folder); };
    connect(mSession, &ProjectSession::progress, this, [this](ProjectSession::Task task, int value, int maximum)
    {
        showProgress(task == ProjectSession::Task::Save, value, maximum);
    });
    connect(mSession, &ProjectSession::saved, this, &MainWindow2::onSaved);
    connect(mSession, &ProjectSession::saveFailed, this, &MainWindow2::showSaveError);
    connect(mSession, &ProjectSession::autoSaveSkipped, this, &MainWindow2::remindToSave);
    connect(mSession, &ProjectSession::recovered, this, &MainWindow2::showRecoveryResult);
}

void MainWindow2::showProgress(bool saving, int value, int maximum)
{
    if (value >= maximum)
    {
        mProgress.reset();
        return;
    }
    if (!mProgress)
    {
        mProgress.reset(new QProgressDialog(saving ? tr("Saving document...") : tr("Opening document..."), tr("Abort"), 0, 100, this));
        hideQuestionMark(*mProgress);
        mProgress->setWindowModality(Qt::WindowModal);
        mProgress->show();
    }
    mProgress->setRange(0, maximum);
    mProgress->setValue(value);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow2::showSaveError(const Status& st)
{
    QDir errorLogFolder(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    errorLogFolder.mkpath("./logs");
    errorLogFolder.cd("logs");

    QDateTime dt = QDateTime::currentDateTime();
    dt.setTimeSpec(Qt::UTC);
    QFile eLog(errorLogFolder.absoluteFilePath(QString("error-%1.txt").arg(dt.toString(Qt::ISODate))));
    if (eLog.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&eLog);
        out << st.details().str();
    }
    eLog.close();

    ErrorDialog errorDialog(st.title(),
                            st.description().append(tr("<br><br>An error has occurred and your file may not have saved successfully."
                                                       "\nIf you believe that this error is an issue with JustAnimate, please create a new issue at:"
                                                       "<br><a href='" APP_ISSUES_URL "'>" APP_ISSUES_URL "</a><br>"
                                                       "Please be sure to include the following details in your issue:")), st.details().html());
    errorDialog.exec();
}

void MainWindow2::onSaved(QString path)
{
    Welcome::saveThumbnail(mEditor->object(), path, mEditor->currentFrame());
    mTimeLine->updateContent();

    setWindowTitle(path.prepend("[*]"));
    updateSaveState();

    if (!mSession->skippedSoundClips().isEmpty())
    {
        QMessageBox::warning(this, tr("Missing Sound Files"),
                             tr("These sound clips were saved without audio because their files are missing:\n%1").arg(mSession->skippedSoundClips().join('\n')));
    }
}

bool MainWindow2::saveDocument()
{
    return mSession->save().ok();
}

void MainWindow2::remindToSave()
{
    if (!mSuppressAutoSaveDialog && mEditor->preference()->get<SETTING::AUTO_SAVE_REMINDER>())
    {
        const QKeySequence key = ui->actionSave->shortcut();
        mEditor->notice(key.isEmpty() ? tr("Unsaved project: save it to keep autosaving")
                                      : tr("Unsaved project: press %1").arg(key.toString(QKeySequence::NativeText)));
    }
}

void MainWindow2::emptyDocumentWhenErrorOccurred()
{
    newObject();

    setWindowTitle(getWindowTitle());
    updateSaveState();
}

void MainWindow2::importImage()
{
    QString strFilePath = FileDialog::getOpenFileName(this, FileType::IMAGE);

    if (strFilePath.isEmpty()) { return; }
    if (!QFile::exists(strFilePath)) { return; }

    ImportPositionDialog* positionDialog = new ImportPositionDialog(mEditor, this);
    OnScopeExit(delete positionDialog)

    positionDialog->exec();

    if (positionDialog->result() != QDialog::Accepted)
    {
        return;
    }

    ImportImageConfig importImageConfig = positionDialog->importConfig();
    Status st = mEditor->importImage(strFilePath, importImageConfig);
    if (!st.ok())
    {
        ErrorDialog errorDialog(st.title(), st.description(), st.details().html());
        errorDialog.exec();
        return;
    }

    ui->scribbleArea->updateFrame();
    mTimeLine->updateContent();
}

void MainWindow2::importImageSequence()
{
    mSuppressAutoSaveDialog = true;

    ImportImageSeqDialog* imageSeqDialog = new ImportImageSeqDialog(this);
    OnScopeExit(delete imageSeqDialog)
    imageSeqDialog->setCore(mEditor);

    connect(imageSeqDialog, &ImportImageSeqDialog::notifyAnimationLengthChanged, mEditor->layers(), &LayerManager::notifyAnimationLengthChanged);

    imageSeqDialog->exec();
    if (imageSeqDialog->result() == QDialog::Rejected)
    {
        return;
    }

    ImportPositionDialog* positionDialog = new ImportPositionDialog(mEditor, this);
    OnScopeExit(delete positionDialog)

    positionDialog->exec();
    if (positionDialog->result() != QDialog::Accepted)
    {
        return;
    }

    imageSeqDialog->importArbitrarySequence(positionDialog->importConfig());

    mSuppressAutoSaveDialog = false;
}

void MainWindow2::importPredefinedImageSet()
{
    ImportImageSeqDialog* imageSeqDialog = new ImportImageSeqDialog(this, ImportExportDialog::Import, FileType::IMAGE, ImportCriteria::PredefinedSet);
    OnScopeExit(delete imageSeqDialog)
    imageSeqDialog->setCore(mEditor);

    connect(imageSeqDialog, &ImportImageSeqDialog::notifyAnimationLengthChanged, mEditor->layers(), &LayerManager::notifyAnimationLengthChanged);

    mSuppressAutoSaveDialog = true;
    imageSeqDialog->exec();
    if (imageSeqDialog->result() == QDialog::Rejected)
    {
        return;
    }

    ImportPositionDialog* positionDialog = new  ImportPositionDialog(mEditor, this);
    OnScopeExit(delete positionDialog)

    positionDialog->exec();
    if (positionDialog->result() != QDialog::Accepted)
    {
        return;
    }

    imageSeqDialog->importPredefinedSet(positionDialog->importConfig());
    mSuppressAutoSaveDialog = false;
}

void MainWindow2::importLayers()
{
    ImportLayersDialog* importLayers = new ImportLayersDialog(this);
    importLayers->setCore(mEditor);
    importLayers->setAttribute(Qt::WA_DeleteOnClose);
    importLayers->open();
}

void MainWindow2::importAnimatedImage()
{
    // Flag this so we don't prompt the user about auto-save in the middle of the import.
    mSuppressAutoSaveDialog = true;

    mCommands->importAnimatedImage();

    mSuppressAutoSaveDialog = false;
}

void MainWindow2::lockWidgets(bool shouldLock)
{
    QDockWidget::DockWidgetFeatures feat = shouldLock
        ? QDockWidget::NoDockWidgetFeatures
        : (QDockWidget::DockWidgetFeature::DockWidgetClosable |
           QDockWidget::DockWidgetFeature::DockWidgetMovable |
           QDockWidget::DockWidgetFeature::DockWidgetFloatable);

    for (BaseDockWidget* d : mDockWidgets)
    {
        d->setFeatures(feat);
        d->lock(shouldLock);
    }
}

void MainWindow2::openCommandSearch()
{
    CommandSearch* search = new CommandSearch(collectCommands(menuBar()), this);
    search->setAttribute(Qt::WA_DeleteOnClose);
    search->open();
}

void MainWindow2::preferences()
{
    if (mPrefDialog)
    {
        mPrefDialog->activateWindow();
        mPrefDialog->raise();
        return;
    }
    mPrefDialog = new PreferencesDialog(this);
    mPrefDialog->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    mPrefDialog->setAttribute(Qt::WA_DeleteOnClose);
    mPrefDialog->init(mEditor->preference());

    connect(mPrefDialog, &PreferencesDialog::windowOpacityChange, this, &MainWindow2::setOpacity);
    connect(mPrefDialog, &PreferencesDialog::resetToolsRequested, mCommands, &ActionCommands::resetAllTools);
    connect(mPrefDialog, &PreferencesDialog::soundScrubChanged, mEditor->playback(), &PlaybackManager::setSoundScrubActive);
    connect(mPrefDialog, &PreferencesDialog::soundScrubMsecChanged, mEditor->playback(), &PlaybackManager::setSoundScrubMsec);
    connect(mPrefDialog, &PreferencesDialog::finished, [&]
    {
        clearKeyboardShortcuts();
        setupKeyboardShortcuts();
        mPrefDialog = nullptr;
    });

    mPrefDialog->show();
}

void MainWindow2::resetAndDockAllSubWidgets()
{
    QSettings& settings = pencilSettings();
    settings.remove(SETTING_WINDOW_GEOMETRY);
    settings.remove(SETTING_WINDOW_STATE);
    resetCurrentWorkspace(this, dockLayout(), availableScreenSize(), settings);
}

DefaultDockLayout MainWindow2::dockLayout() const
{
    return {nullptr, {mBrushPresets, mToolOptions}, {mColorBox, mColorPalette}, mTimeLine,
            {{mBrushPresets, mToolBox}, {mColorPalette, mColorInspector}, {mTimeLine, mXSheet}, {mTimeLine, mOnionSkinWidget},
             {mColorPalette, mStoryboard}}};
}

QSize MainWindow2::availableScreenSize() const
{
    return screen() ? screen()->availableSize() : QSize(1366, 768);
}

void MainWindow2::applyDefaultDockLayout()
{
    ::applyDefaultDockLayout(this, dockLayout(), availableScreenSize());
}

void MainWindow2::switchToWorkspace(const QString& name)
{
    QSettings& settings = pencilSettings();
    switchWorkspace(this, dockLayout(), availableScreenSize(), settings, name);
}

void MainWindow2::newObject()
{
    auto object = new Object();
    object->init();

    // default layers
    object->addNewCameraLayer();
    object->addNewBitmapLayer();
    // Layers are counted bottom up
    // 0 - Camera Layer
    // 1 - Bitmap Layer
    object->data()->setCurrentLayer(1);

    mEditor->setObject(object);

    closeDialogs();

    setWindowTitle(getWindowTitle());
}

bool MainWindow2::newObjectFromPresets(int presetIndex)
{
    QString presetFilePath = Welcome::presetPath(presetIndex);

    if (presetFilePath.isEmpty())
    {
        return false;
    }

    FileManager fm(this);
    Object* object = fm.load(presetFilePath);

    if (!fm.error().ok() || object == nullptr)
    {
        return false;
    }

    mEditor->setObject(object);
    object->setFilePath(QString());

    setWindowTitle(getWindowTitle());
    updateSaveState();

    return true;
}

bool MainWindow2::loadMostRecent()
{
    const QString myPath = mSession->lastPath();
    if (myPath.isEmpty() || !QFile::exists(myPath))
    {
        return false;
    }
    return openObject(myPath);
}

bool MainWindow2::tryLoadPreset()
{
    if (mEditor->preference()->get<SETTING::STARTUP_ACTION>() != STARTUP_WELCOME)
    {
        return newObjectFromPresets(mEditor->preference()->get<SETTING::DEFAULT_PRESET>());
    }
    showWelcomeDialog();
    return true;
}

void MainWindow2::showWelcomeDialog()
{
    auto* presetDialog = new WelcomeDialog(mEditor->preference()->get<SETTING::DEFAULT_PRESET>(),
                                           mSession->recentFiles(),
                                           mEditor->preference()->get<SETTING::LAST_TEMPLATE>(), this);
    presetDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(presetDialog, &WelcomeDialog::finished, [=](int result)
    {
        if (result != QDialog::Accepted)
        {
            return;
        }

        if (presetDialog->shouldHide())
        {
            mEditor->preference()->set<SETTING::STARTUP_ACTION>(Welcome::hiddenStartupAction(!mSession->recentFiles().isEmpty()));
            if (presetDialog->templateIndex() < 0)
            {
                mEditor->preference()->set<SETTING::DEFAULT_PRESET>(presetDialog->presetIndex());
            }
        }

        if (presetDialog->browseRequested())
        {
            openDocument();
            return;
        }
        if (!presetDialog->openPath().isEmpty())
        {
            openObject(presetDialog->openPath());
            return;
        }

        if (presetDialog->templateIndex() >= 0)
        {
            if (Object* object = Welcome::newObjectFromTemplate(presetDialog->templateIndex()))
            {
                mEditor->preference()->set<SETTING::LAST_TEMPLATE>(Welcome::templates().at(presetDialog->templateIndex()).name);
                mEditor->setObject(object);
                mEditor->setFps(mEditor->playback()->fps());
                closeDialogs();
                setWindowTitle(getWindowTitle());
                return;
            }
        }

        int presetIndex = presetDialog->presetIndex();
        if (!newObjectFromPresets(presetIndex))
        {
            newObject();
        }
    });
    presetDialog->open();
}

void MainWindow2::closeDialogs()
{
    for (auto dialog : findChildren<QDialog*>(QString(), Qt::FindDirectChildrenOnly)) {
        dialog->close();
    }
}

void MainWindow2::readSettings()
{
    QSettings& settings = pencilSettings();

    QVariant winGeometry = settings.value(SETTING_WINDOW_GEOMETRY);
    restoreGeometry(winGeometry.toByteArray());

    dropOutdatedLayouts(settings);
    restoreState(settings.value(SETTING_WINDOW_STATE).toByteArray());

    int opacity = mEditor->preference()->get<SETTING::WINDOW_OPACITY>();
    setOpacity(100 - opacity);

    bool isWindowsLocked = mEditor->preference()->get<SETTING::LAYOUT_LOCK>();
    lockWidgets(isWindowsLocked);
}

void MainWindow2::writeSettings()
{
    QSettings& settings = pencilSettings();
    settings.setValue(SETTING_WINDOW_GEOMETRY, saveGeometry());
    settings.setValue(SETTING_WINDOW_STATE, saveState());
    saveCurrentWorkspace(this, settings);
}

void MainWindow2::setupKeyboardShortcuts()
{
    checkExistingShortcuts();

    auto cmdKeySeq = [](QString strCommandName) -> QKeySequence
    {
        strCommandName = QString("shortcuts/") + strCommandName;
        QKeySequence keySequence(pencilSettings().value(strCommandName).toString());
        return keySequence;
    };

    // File menu
    ui->actionNew->setShortcut(cmdKeySeq(CMD_NEW_FILE));
    ui->actionOpen->setShortcut(cmdKeySeq(CMD_OPEN_FILE));
    ui->actionSave->setShortcut(cmdKeySeq(CMD_SAVE_FILE));
    ui->actionSave_as->setShortcut(cmdKeySeq(CMD_SAVE_AS));

    ui->actionImport_Image->setShortcut(cmdKeySeq(CMD_IMPORT_IMAGE));
    ui->actionImport_ImageSeq->setShortcut(cmdKeySeq(CMD_IMPORT_IMAGE_SEQ));
    ui->actionImport_ImageSeqNum->setShortcut(cmdKeySeq(CMD_IMPORT_IMAGE_PREDEFINED_SET));
    ui->actionImport_MovieVideo->setShortcut(cmdKeySeq(CMD_IMPORT_MOVIE_VIDEO));
    ui->actionImport_AnimatedImage->setShortcut(cmdKeySeq(CMD_IMPORT_ANIMATED_IMAGE));
    ui->actionImportLayers_from_pclx->setShortcut(cmdKeySeq(CMD_IMPORT_LAYERS));
    ui->actionImport_Sound->setShortcut(cmdKeySeq(CMD_IMPORT_SOUND));
    ui->actionImport_MovieAudio->setShortcut(cmdKeySeq(CMD_IMPORT_MOVIE_AUDIO));
    ui->actionImport_Append_Palette->setShortcut(cmdKeySeq(CMD_IMPORT_PALETTE));
    ui->actionImport_Replace_Palette->setShortcut(cmdKeySeq(CMD_IMPORT_PALETTE_REPLACE));

    ui->actionExport_Image->setShortcut(cmdKeySeq(CMD_EXPORT_IMAGE));
    ui->actionExport_ImageSeq->setShortcut(cmdKeySeq(CMD_EXPORT_IMAGE_SEQ));
    ui->actionExport_Movie->setShortcut(cmdKeySeq(CMD_EXPORT_MOVIE));
    ui->actionExport_Animated_GIF->setShortcut(cmdKeySeq(CMD_EXPORT_GIF));
    ui->actionExport_Palette->setShortcut(cmdKeySeq(CMD_EXPORT_PALETTE));

    // edit menu
    ui->actionUndo->setShortcut(cmdKeySeq(CMD_UNDO));
    ui->actionRedo->setShortcut(cmdKeySeq(CMD_REDO));
    ui->actionCut->setShortcut(cmdKeySeq(CMD_CUT));
    ui->actionCopy->setShortcut(cmdKeySeq(CMD_COPY));
    ui->actionPaste_Previous->setShortcut(cmdKeySeq(CMD_PASTE_FROM_PREVIOUS));
    ui->actionPaste->setShortcut(cmdKeySeq(CMD_PASTE));
    ui->actionClearFrame->setShortcut(cmdKeySeq(CMD_CLEAR_FRAME));
    ui->actionFlip_X->setShortcut(cmdKeySeq(CMD_SELECTION_FLIP_HORIZONTAL));
    ui->actionFlip_Y->setShortcut(cmdKeySeq(CMD_SELECTION_FLIP_VERTICAL));
    ui->actionSelect_All->setShortcut(cmdKeySeq(CMD_SELECT_ALL));
    ui->actionDeselect_All->setShortcut(cmdKeySeq(CMD_DESELECT_ALL));
    ui->actionPegbarAlignment->setShortcut(cmdKeySeq(CMD_PEGBAR_ALIGNMENT));
    ui->actionPreference->setShortcut(cmdKeySeq(CMD_PREFERENCE));
    ui->actionCommandSearch->setShortcut(cmdKeySeq(CMD_COMMAND_SEARCH));

    // View menu
    ui->actionResetWindows->setShortcut(cmdKeySeq(CMD_RESET_WINDOWS));
    ui->actionLockWindows->setShortcut(cmdKeySeq(CMD_LOCK_WINDOWS));
    ui->actionReset_View->setShortcut(cmdKeySeq(CMD_RESET_ZOOM_ROTATE));
    ui->actionCenter_View->setShortcut(cmdKeySeq(CMD_CENTER_VIEW));
    ui->actionZoom_In->setShortcut(cmdKeySeq(CMD_ZOOM_IN));
    ui->actionZoom_Out->setShortcut(cmdKeySeq(CMD_ZOOM_OUT));
    ui->actionZoom400->setShortcut(cmdKeySeq(CMD_ZOOM_400));
    ui->actionZoom300->setShortcut(cmdKeySeq(CMD_ZOOM_300));
    ui->actionZoom200->setShortcut(cmdKeySeq(CMD_ZOOM_200));
    ui->actionZoom100->setShortcut(cmdKeySeq(CMD_ZOOM_100));
    ui->actionZoom50->setShortcut(cmdKeySeq(CMD_ZOOM_50));
    ui->actionZoom33->setShortcut(cmdKeySeq(CMD_ZOOM_33));
    ui->actionZoom25->setShortcut(cmdKeySeq(CMD_ZOOM_25));
    ui->actionRotate_Clockwise->setShortcut(cmdKeySeq(CMD_ROTATE_CLOCK));
    ui->actionRotate_Anticlockwise->setShortcut(cmdKeySeq(CMD_ROTATE_ANTI_CLOCK));
    ui->actionReset_Rotation->setShortcut(cmdKeySeq(CMD_RESET_ROTATION));
    ui->actionHorizontal_Flip->setShortcut(cmdKeySeq(CMD_FLIP_HORIZONTAL));
    ui->actionVertical_Flip->setShortcut(cmdKeySeq(CMD_FLIP_VERTICAL));
    ui->actionGrid->setShortcut(cmdKeySeq(CMD_GRID));
    ui->actionCenter->setShortcut(cmdKeySeq(CMD_OVERLAY_CENTER));
    ui->actionThirds->setShortcut(cmdKeySeq(CMD_OVERLAY_THIRDS));
    ui->actionGoldenRatio->setShortcut(cmdKeySeq(CMD_OVERLAY_GOLDEN_RATIO));
    ui->actionSafeAreas->setShortcut(cmdKeySeq(CMD_OVERLAY_SAFE_AREAS));
    ui->actionOnePointPerspective->setShortcut(cmdKeySeq(CMD_OVERLAY_ONE_POINT_PERSPECTIVE));
    ui->actionTwoPointPerspective->setShortcut(cmdKeySeq(CMD_OVERLAY_TWO_POINT_PERSPECTIVE));
    ui->actionThreePointPerspective->setShortcut(cmdKeySeq(CMD_OVERLAY_THREE_POINT_PERSPECTIVE));
    ui->actionOnionSkin->setShortcut(cmdKeySeq(CMD_ONIONSKIN));
    ui->actionOnionPrev->setShortcut(cmdKeySeq(CMD_ONIONSKIN_PREV));
    ui->actionOnionNext->setShortcut(cmdKeySeq(CMD_ONIONSKIN_NEXT));
    ui->actionStatusBar->setShortcut(cmdKeySeq(CMD_TOGGLE_STATUS_BAR));

    // Animation menu
    ui->actionPlay->setShortcut(cmdKeySeq(CMD_PLAY));
    ui->actionPlay_Around->setShortcut(cmdKeySeq(CMD_PLAY_AROUND));
    ui->actionLoop->setShortcut(cmdKeySeq(CMD_LOOP));
    ui->actionLoopControl->setShortcut(cmdKeySeq(CMD_LOOP_CONTROL));
    ui->actionPrevious_Frame->setShortcut(cmdKeySeq(CMD_GOTO_PREV_FRAME));
    ui->actionNext_Frame->setShortcut(cmdKeySeq(CMD_GOTO_NEXT_FRAME));
    ui->actionPrev_KeyFrame->setShortcut(cmdKeySeq(CMD_GOTO_PREV_KEY_FRAME));
    ui->actionNext_KeyFrame->setShortcut(cmdKeySeq(CMD_GOTO_NEXT_KEY_FRAME));
    ui->actionAdd_Frame->setShortcut(cmdKeySeq(CMD_ADD_FRAME));
    ui->actionDuplicate_Frame->setShortcut(cmdKeySeq(CMD_DUPLICATE_FRAME));
    ui->actionRemove_Frame->setShortcut(cmdKeySeq(CMD_REMOVE_FRAME));
    ui->actionRipple_Insert_Frame->setShortcut(cmdKeySeq(CMD_RIPPLE_INSERT_FRAME));
    ui->actionRipple_Delete_Frame->setShortcut(cmdKeySeq(CMD_RIPPLE_DELETE_FRAME));
    ui->actionExposure_Add->setShortcut(cmdKeySeq(CMD_EXPOSURE_ADD));
    ui->actionExposure_Subtract->setShortcut(cmdKeySeq(CMD_EXPOSURE_SUBTRACT));
    ui->actionAdd_Frame_Exposure->setShortcut(cmdKeySeq(CMD_SELECTION_ADD_FRAME_EXPOSURE));
    ui->actionSubtract_Frame_Exposure->setShortcut(cmdKeySeq(CMD_SELECTION_SUBTRACT_FRAME_EXPOSURE));
    ui->actionHold_1s->setShortcut(cmdKeySeq(CMD_SELECTION_HOLD_1S));
    ui->actionHold_2s->setShortcut(cmdKeySeq(CMD_SELECTION_HOLD_2S));
    ui->actionHold_3s->setShortcut(cmdKeySeq(CMD_SELECTION_HOLD_3S));
    ui->actionClean_Exposure->setShortcut(cmdKeySeq(CMD_SELECTION_CLEAN_EXPOSURE));
    ui->actionReverse_Frames_Order->setShortcut(cmdKeySeq(CMD_REVERSE_SELECTED_FRAMES));
    ui->actionRemove_Frames->setShortcut(cmdKeySeq(CMD_REMOVE_SELECTED_FRAMES));
    ui->actionMove_Frame_Backward->setShortcut(cmdKeySeq(CMD_MOVE_FRAME_BACKWARD));
    ui->actionMove_Frame_Forward->setShortcut(cmdKeySeq(CMD_MOVE_FRAME_FORWARD));
    ui->actionFlip_inbetween->setShortcut(cmdKeySeq(CMD_FLIP_INBETWEEN));
    ui->actionFlip_rolling->setShortcut(cmdKeySeq(CMD_FLIP_ROLLING));
    ui->actionMark_Key->setShortcut(cmdKeySeq(CMD_MARK_KEY));
    ui->actionMark_Breakdown->setShortcut(cmdKeySeq(CMD_MARK_BREAKDOWN));
    ui->actionMark_Inbetween->setShortcut(cmdKeySeq(CMD_MARK_INBETWEEN));
    ui->actionClear_Mark->setShortcut(cmdKeySeq(CMD_MARK_CLEAR));
    ui->actionCreate_Inbetweens->setShortcut(cmdKeySeq(CMD_CREATE_INBETWEENS));
    ui->actionReposition_Selected_Frames->setShortcut(cmdKeySeq(CMD_SELECTION_REPOSITION_FRAMES));

    ui->actionMove->setShortcut(cmdKeySeq(CMD_TOOL_MOVE));
    ui->actionSelect->setShortcut(cmdKeySeq(CMD_TOOL_SELECT));
    ui->actionBrush->setShortcut(cmdKeySeq(CMD_TOOL_BRUSH));
    ui->actionPolyline->setShortcut(cmdKeySeq(CMD_TOOL_POLYLINE));
    ui->actionShape->setShortcut(cmdKeySeq(CMD_TOOL_SHAPE));
    ui->actionLassoFill->setShortcut(cmdKeySeq(CMD_TOOL_LASSOFILL));
    ui->actionSmudge->setShortcut(cmdKeySeq(CMD_TOOL_SMUDGE));
    ui->actionPen->setShortcut(cmdKeySeq(CMD_TOOL_PEN));
    ui->actionHand->setShortcut(cmdKeySeq(CMD_TOOL_HAND));
    ui->actionRotateView->setShortcut(cmdKeySeq(CMD_TOOL_ROTATE));
    ui->actionZoomTool->setShortcut(cmdKeySeq(CMD_TOOL_ZOOM));
    ui->actionPencil->setShortcut(cmdKeySeq(CMD_TOOL_PENCIL));
    ui->actionBucket->setShortcut(cmdKeySeq(CMD_TOOL_BUCKET));
    ui->actionEyedropper->setShortcut(cmdKeySeq(CMD_TOOL_EYEDROPPER));
    ui->actionEraser->setShortcut(cmdKeySeq(CMD_TOOL_ERASER));
    mRadialToolAction->setShortcut(cmdKeySeq(CMD_RADIAL_TOOL_MENU));
    mShiftAndTraceAction->setShortcut(cmdKeySeq(CMD_SHIFT_AND_TRACE));
    mPresetPickerAction->setShortcut(cmdKeySeq(CMD_QUICK_PRESET_PICKER));


    // Layer menu
    ui->actionNew_Bitmap_Layer->setShortcut(cmdKeySeq(CMD_NEW_BITMAP_LAYER));
    ui->actionNew_Vector_Layer->setShortcut(cmdKeySeq(CMD_NEW_VECTOR_LAYER));
    ui->actionNew_Camera_Layer->setShortcut(cmdKeySeq(CMD_NEW_CAMERA_LAYER));
    ui->actionNew_Sound_Layer->setShortcut(cmdKeySeq(CMD_NEW_SOUND_LAYER));
    ui->actionDelete_Current_Layer->setShortcut(cmdKeySeq(CMD_DELETE_CUR_LAYER));
    ui->actionRename_Layer->setShortcut(cmdKeySeq(CMD_RENAME_LAYER));
    ui->actionChangeLineColorCurrent_keyframe->setShortcut(cmdKeySeq(CMD_CHANGE_LINE_COLOR_KEYFRAME));
    ui->actionChangeLineColorAll_keyframes_on_layer->setShortcut(cmdKeySeq(CMD_CHANGE_LINE_COLOR_LAYER));
    ui->actionChangeLayerOpacity->setShortcut(cmdKeySeq(CMD_CHANGE_LAYER_OPACITY));

    ui->actionVisibilityCurrentLayerOnly->setShortcut(cmdKeySeq(CMD_CURRENT_LAYER_VISIBILITY));
    ui->actionVisibilityRelative->setShortcut(cmdKeySeq(CMD_RELATIVE_LAYER_VISIBILITY));
    ui->actionVisibilityAll->setShortcut(cmdKeySeq(CMD_ALL_LAYER_VISIBILITY));

    mToolBox->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_TOOLBOX));
    mToolOptions->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_TOOL_OPTIONS));
    mColorBox->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_COLOR_WHEEL));
    mColorPalette->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_COLOR_LIBRARY));
    mTimeLine->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_TIMELINE));
    mXSheet->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_XSHEET));
    mBrushPresets->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_BRUSH_PRESETS));
    mStoryboard->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_STORYBOARD));
    mColorInspector->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_COLOR_INSPECTOR));
    mOnionSkinWidget->toggleViewAction()->setShortcut(cmdKeySeq(CMD_TOGGLE_ONION_SKIN));

    ui->actionHelp->setShortcut(cmdKeySeq(CMD_HELP));
    ui->actionExit->setShortcut(cmdKeySeq(CMD_EXIT));
}

void MainWindow2::clearKeyboardShortcuts()
{
    QList<QAction*> actionList = findChildren<QAction*>();
    for (QAction* action : actionList)
    {
        action->setShortcut(QKeySequence(0));
    }
}

void MainWindow2::exportPalette()
{
    QString filePath = FileDialog::getSaveFileName(this, FileType::PALETTE);
    if (!filePath.isEmpty())
    {
        mEditor->object()->exportPalette(filePath);
    }
}

void MainWindow2::importPalette()
{
    QString filePath = FileDialog::getOpenFileName(this, FileType::PALETTE);
    if (!filePath.isEmpty())
    {
        mEditor->object()->importPalette(filePath);
        mColorPalette->refreshColorList();
        mColorPalette->adjustSwatches();
        mEditor->color()->setColorNumber(0);
    }
}

void MainWindow2::openPalette()
{
    for (int i = 0; i < mEditor->object()->getColorCount(); i++)
    {
        if (!mEditor->object()->isColorInUse(i))
        {
            continue;
        }

        QMessageBox msgBox;
        msgBox.setText(tr("Opening a palette will replace the old palette.\n"
                          "Color(s) in strokes will be altered by this action!"));
        msgBox.addButton(tr("Open Palette"), QMessageBox::AcceptRole);
        msgBox.addButton(QMessageBox::Cancel);

        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
        break;
    }

    QString filePath = FileDialog::getOpenFileName(this, FileType::PALETTE);
    if (filePath.isEmpty())
    {
        return;
    }

    mEditor->object()->openPalette(filePath);
    mColorPalette->refreshColorList();
    mEditor->color()->setColorNumber(0);
}

void MainWindow2::makeConnections(Editor* editor)
{
    connect(editor->undoRedo(), &UndoRedoManager::didUpdateUndoStack, this, &MainWindow2::updateSaveState);
    connect(editor, &Editor::needDisplayInfo, this, &MainWindow2::displayMessageBox);
    connect(editor, &Editor::needDisplayInfoNoTitle, this, &MainWindow2::displayMessageBoxNoTitle);
    connect(editor->layers(), &LayerManager::currentLayerChanged, this, &MainWindow2::currentLayerChanged);
    connect(editor->select(), &SelectionManager::selectionChanged, this, &MainWindow2::selectionChanged);
    connect(editor, &Editor::canCopyChanged, this, [=](bool canCopy) {
        ui->actionCopy->setEnabled(canCopy);
        ui->actionCut->setEnabled(canCopy);
    });
    connect(editor, &Editor::canPasteChanged, ui->actionPaste, &QAction::setEnabled);
}

void MainWindow2::makeConnections(Editor* editor, ColorBox* colorBox)
{
    connect(colorBox, &ColorBox::colorChanged, editor->color(), &ColorManager::setFrontColor);
    connect(editor->color(), &ColorManager::colorChanged, colorBox, &ColorBox::setColor);
}

void MainWindow2::makeConnections(Editor* editor, ColorInspector* colorInspector)
{
    connect(colorInspector, &ColorInspector::colorChanged, editor->color(), &ColorManager::setFrontColor);
    connect(editor->color(), &ColorManager::colorChanged, colorInspector, &ColorInspector::setColor);
}

void MainWindow2::makeConnections(Editor* editor, ScribbleArea* scribbleArea)
{
    connect(editor->tools(), &ToolManager::toolChanged, scribbleArea, &ScribbleArea::updateToolCursor);
    connect(editor->tools(), &ToolManager::toolChanged, mToolBox, &ToolBoxDockWidget::setActiveTool);

    connect(scribbleArea, &ScribbleArea::requestFocus, this, &MainWindow2::onFocusRequested);

    connect(editor->layers(), &LayerManager::currentLayerChanged, scribbleArea, &ScribbleArea::onLayerChanged);
    connect(editor->layers(), &LayerManager::layerDeleted, scribbleArea, &ScribbleArea::onLayerChanged);
    connect(editor, &Editor::scrubbed, scribbleArea, &ScribbleArea::onScrubbed);
    connect(editor, &Editor::frameModified, scribbleArea, &ScribbleArea::onFrameModified);
    connect(editor, &Editor::framesModified, scribbleArea, &ScribbleArea::onFramesModified);
    connect(editor, &Editor::objectLoaded, scribbleArea, &ScribbleArea::onObjectLoaded);
    connect(editor->view(), &ViewManager::viewChanged, scribbleArea, &ScribbleArea::onViewChanged);
}

void MainWindow2::makeConnections(Editor* pEditor, TimeLine* pTimeline)
{
    PlaybackManager* pPlaybackManager = pEditor->playback();
    connect(pTimeline, &TimeLine::duplicateLayerClick, mCommands, &ActionCommands::duplicateLayer);
    connect(pTimeline, &TimeLine::duplicateKeyClick, mCommands, &ActionCommands::duplicateKey);

    connect(pTimeline, &TimeLine::soundClick, pPlaybackManager, &PlaybackManager::enableSound);
    connect(pTimeline, &TimeLine::fpsChanged, pPlaybackManager, &PlaybackManager::setFps);
    connect(pTimeline, &TimeLine::fpsChanged, pEditor, &Editor::setFps);

    connect(pTimeline, &TimeLine::insertKeyClick, mCommands, &ActionCommands::insertKeyFrameAtCurrentPosition);
    connect(pTimeline, &TimeLine::removeKeyClick, mCommands, &ActionCommands::removeKey);

    connect(pTimeline, &TimeLine::newBitmapLayer, mCommands, &ActionCommands::addNewBitmapLayer);
    connect(pTimeline, &TimeLine::newVectorLayer, mCommands, &ActionCommands::addNewVectorLayer);
    connect(pTimeline, &TimeLine::newSoundLayer, mCommands, &ActionCommands::addNewSoundLayer);
    connect(pTimeline, &TimeLine::newStoryboardLayer, mCommands, &ActionCommands::addNewStoryboardLayer);
    connect(pTimeline, &TimeLine::newCameraLayer, mCommands, qOverload<>(&ActionCommands::addNewCameraLayer));
    connect(mTimeLine, &TimeLine::playButtonTriggered, mCommands, &ActionCommands::PlayStop);
    connect(pTimeline, &TimeLine::deleteCurrentLayerClick, mCommands, &ActionCommands::deleteCurrentLayer);

    // Clipboard state handling
    connect(QApplication::clipboard(), &QClipboard::dataChanged, mEditor, &Editor::clipboardChanged);
    connect(pTimeline, &TimeLine::selectionChanged, this, &MainWindow2::updateCopyCutPasteEnabled);
    connect(this, &MainWindow2::windowActivated, this, &MainWindow2::updateCopyCutPasteEnabled);
    connect(mEditor->select(), &SelectionManager::selectionChanged, this, &MainWindow2::updateCopyCutPasteEnabled);

    connect(pEditor->layers(), &LayerManager::currentLayerChanged, pTimeline, &TimeLine::updateUI);
    connect(pEditor->layers(), &LayerManager::animationLengthChanged, pTimeline, &TimeLine::extendLength);
    connect(pEditor->sound(), &SoundManager::soundClipDurationChanged, pTimeline, &TimeLine::updateUI);

    // Menu UI changes
    connect(ui->menuEdit, &QMenu::aboutToShow, this, &MainWindow2::updateCopyCutPasteEnabled);

    connect(pEditor, &Editor::objectLoaded, pTimeline, &TimeLine::onObjectLoaded);
    connect(pEditor, &Editor::updateTimeLine, pTimeline, &TimeLine::updateUI);
    connect(pEditor, &Editor::updateTimeLineCached, pTimeline, &TimeLine::updateUICached);

    connect(pEditor->layers(), &LayerManager::currentLayerChanged, this, &MainWindow2::updateLayerMenu);
    connect(pEditor->layers(), &LayerManager::layerCountChanged, this, &MainWindow2::updateLayerMenu);
}

void MainWindow2::makeConnections(Editor*, OnionSkinWidget*)
{
}

void MainWindow2::makeConnections(Editor* editor, ToolOptionWidget* toolOptions)
{
    toolOptions->makeConnectionToEditor(editor);
}

void MainWindow2::makeConnections(Editor* pEditor, ColorPaletteWidget* pColorPalette)
{
    connect(pEditor, &Editor::objectLoaded, pColorPalette, &ColorPaletteWidget::updateUI);

    ColorManager* pColorManager = pEditor->color();
    ScribbleArea* pScribbleArea = pEditor->getScribbleArea();

    connect(pColorPalette, &ColorPaletteWidget::colorNumberChanged, pColorManager, &ColorManager::setColorNumber);
    connect(pColorManager, &ColorManager::colorNumberChanged, pScribbleArea, &ScribbleArea::paletteColorChanged);
    connect(pColorManager, &ColorManager::colorNumberChanged, pColorPalette, &ColorPaletteWidget::selectColorNumber);
}

void MainWindow2::makeConnections(Editor* editor, StatusBar *statusBar)
{
    connect(editor->tools(), &ToolManager::toolChanged, statusBar, &StatusBar::updateToolStatus);
    connect(editor->tools()->getTool(POLYLINE), &BaseTool::isActiveChanged, statusBar, &StatusBar::updateToolStatus);

    connect(editor->view(), &ViewManager::viewChanged, statusBar, &StatusBar::updateZoomStatus);
    connect(statusBar, &StatusBar::zoomChanged, editor->view(), &ViewManager::scale);
    connect(editor, &Editor::statusMessage, statusBar, [statusBar](const QString& text) { statusBar->showMessage(text, 3000); });
}

void MainWindow2::updateCopyCutPasteEnabled()
{
    bool canCopy = mEditor->canCopy();
    bool canPaste = mEditor->canPaste();

    ui->actionCopy->setEnabled(canCopy);
    ui->actionCut->setEnabled(canCopy);
    ui->actionPaste->setEnabled(canPaste);
}

void MainWindow2::updateLayerMenu()
{
    ui->actionDelete_Current_Layer->setEnabled(mEditor->layers()->canDeleteLayer(mEditor->currentLayerIndex()));
}

void MainWindow2::createInbetweens()
{
    LayerManager* layers = mEditor->layers();
    // Other layer types refuse before the easing is asked.
    if (layers->currentLayer()->type() != Layer::VECTOR) { layers->createInbetweens(CameraEasingType::LINEAR); return; }
    CameraEasingType easing;
    if (CellMenus::chooseInbetweenEasing(mEditor, QCursor::pos(), easing)) { layers->createInbetweens(easing); }
}

void MainWindow2::changePlayState(bool isPlaying)
{
    if (isPlaying)
    {
        ui->actionPlay->setText(tr("Stop"));
        ui->actionPlay->setIcon(mStopIcon);
    }
    else
    {
        ui->actionPlay->setText(tr("Play"));
        ui->actionPlay->setIcon(mStartIcon);
    }
    update();
}

void MainWindow2::importMovieVideo()
{
    // Flag this so we don't prompt the user about auto-save in the middle of the import.
    mSuppressAutoSaveDialog = true;

    mCommands->importMovieVideo();

    mSuppressAutoSaveDialog = false;
}

bool MainWindow2::event(QEvent* event)
{
    if(event->type() == QEvent::WindowActivate) {
        emit windowActivated();
    }
    return QMainWindow::event(event);
}

void MainWindow2::onFocusRequested(QWidget *widget)
{
    ScribbleArea* scribbleArea = qobject_cast<ScribbleArea*>(widget);

    QWidget* currentFocus = QApplication::focusWidget();

    bool hasEditingFocus = false;

    if (currentFocus) {
        hasEditingFocus = qobject_cast<QLineEdit*>(currentFocus) ||
                    qobject_cast<QAbstractSpinBox*>(currentFocus);
    }

    if (scribbleArea && !scribbleArea->hasFocus() && !hasEditingFocus) {
        scribbleArea->setFocus();
    }
}

void MainWindow2::displayMessageBox(const QString& title, const QString& body)
{
    QMessageBox::information(this, tr(qPrintable(title)), tr(qPrintable(body)), QMessageBox::Ok);
}

void MainWindow2::displayMessageBoxNoTitle(const QString& body)
{
    QMessageBox::information(this, nullptr, tr(qPrintable(body)), QMessageBox::Ok);
}

ProjectSession::RecoverChoice MainWindow2::askRecoverProject(const QString& recoverPath)
{
    QString caption = tr("Restore Project?");
    QString text = tr("JustAnimate didn't close correctly. Would you like to restore the project?");

    QMessageBox msgBox(this);
    hideQuestionMark(msgBox); // Must be before setDefaultButton
    msgBox.setWindowTitle(tr("Restore project"));
    msgBox.setWindowModality(Qt::ApplicationModal);
    msgBox.setIconPixmap(QPixmap(":/icons/logo.png"));
    msgBox.setText(QString("<h4>%1</h4>%2").arg(caption, text));
    msgBox.setInformativeText(QString("<b>%1</b>").arg(retrieveProjectNameFromTempPath(recoverPath)));
    msgBox.setStandardButtons(QMessageBox::Open | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    switch (msgBox.exec())
    {
    case QMessageBox::Open: return ProjectSession::RecoverChoice::Open;
    case QMessageBox::Discard: return ProjectSession::RecoverChoice::Discard;
    default: return ProjectSession::RecoverChoice::Cancel;
    }
}

void MainWindow2::showRecoveryResult(bool ok)
{
    if (!ok)
    {
        const QString title = tr("Recovery Failed.");
        const QString text = tr("Sorry! JustAnimate is unable to restore your project");
        QMessageBox::information(this, title, QString("<h4>%1</h4>%2").arg(title, text));
        return;
    }

    updateSaveState();

    const QString title = tr("Recovery Succeeded!");
    const QString text = tr("Please save your work immediately to prevent loss of data");
    QMessageBox::information(this, title, QString("<h4>%1</h4>%2").arg(title, text));
}

void MainWindow2::createToolbars()
{
    mCommandBar = addToolBar(tr("Command bar"));
    mCommandBar->setObjectName("CommandBar");
    mCommandBar->setIconSize(QSize(22,22));
    const std::initializer_list<QAction*> groups[] = {
        { ui->actionNew, ui->actionOpen, ui->actionSave },
        { ui->actionUndo, ui->actionRedo },
        { ui->actionCut, ui->actionCopy, ui->actionPaste, ui->actionClearFrame },
        { ui->actionZoom_In, ui->actionZoom_Out, ui->actionReset_View },
        { ui->actionHorizontal_Flip, ui->actionVertical_Flip },
        { ui->actionShowInvisibleLines, ui->actionShowOutlinesOnly },
        { ui->actionGrid, ui->actionCenter, ui->actionThirds, ui->actionGoldenRatio, ui->actionSafeAreas,
          ui->actionOnePointPerspective, ui->actionTwoPointPerspective, ui->actionThreePointPerspective },
    };
    for (const auto& group : groups)
    {
        if (!mCommandBar->actions().isEmpty()) { mCommandBar->addSeparator(); }
        mCommandBar->addActions(group);
    }

    QToolButton* perspectiveLinesAngleButton = new QToolButton(this);
    perspectiveLinesAngleButton->setDefaultAction(ui->menuPerspectiveLinesAngle->menuAction());
    perspectiveLinesAngleButton->setPopupMode(QToolButton::InstantPopup);
    mCommandBar->addWidget(perspectiveLinesAngleButton);

    CommandSearchField* searchField = new CommandSearchField(this);
    connect(searchField, &CommandSearchField::activated, this, &MainWindow2::openCommandSearch);
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mCommandBar->addWidget(spacer);
    mCommandBar->addWidget(searchField);

    ui->menuWindows->insertAction(ui->actionStatusBar, mCommandBar->toggleViewAction());
}
