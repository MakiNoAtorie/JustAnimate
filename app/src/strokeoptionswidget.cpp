#include "strokeoptionswidget.h"
#include "ui_strokeoptionswidget.h"

#include "editor.h"
#include "pencilsettings.h"
#include "basetool.h"
#include "stroketool.h"
#include "polylinetool.h"
#include "shapetool.h"
#include "util.h"

#include "toolmanager.h"
#include "layermanager.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QMessageBox>

StrokeOptionsWidget::StrokeOptionsWidget(Editor* editor, QWidget *parent) :
    BaseWidget(parent),
    ui(new Ui::StrokeOptionsWidget)
{
    ui->setupUi(this);

    mEditor = editor;

    initUI();

    setContentsMargins(0,0,0,0);
}

StrokeOptionsWidget::~StrokeOptionsWidget()
{
    delete ui;
}

void StrokeOptionsWidget::initUI()
{
    StrokeTool* strokeTool = mEditor->tools()->currentStrokeTool();
    if (strokeTool == nullptr) { return; }

    const StrokeToolProperties p = strokeTool->strokeToolProperties();

    auto widthInfo = p.getInfo(StrokeToolProperties::WIDTH_VALUE);
    ui->sizeSlider->init(tr("Width"), SpinSlider::EXPONENT, widthInfo.minReal(), widthInfo.maxReal());

    auto featherInfo = p.getInfo(StrokeToolProperties::FEATHER_VALUE);
    ui->featherSlider->init(tr("Feather"), SpinSlider::LOG, featherInfo.minReal(), featherInfo.maxReal());

    auto spacingInfo = p.getInfo(StrokeToolProperties::SPACING_VALUE);
    ui->spacingSlider->init(tr("Spacing %"), SpinSlider::LOG, spacingInfo.minReal(), spacingInfo.maxReal());
    ui->flowSlider->init(tr("Flow %"), SpinSlider::LINEAR, 1, 100);
    ui->opacitySlider->init(tr("Opacity %"), SpinSlider::LINEAR, 1, 100);

    auto aspectInfo = p.getInfo(StrokeToolProperties::TIP_ASPECT_VALUE);
    ui->tipAspectSlider->init(tr("Tip Aspect %"), SpinSlider::LINEAR, aspectInfo.minReal(), aspectInfo.maxReal());
    auto angleInfo = p.getInfo(StrokeToolProperties::TIP_ANGLE_VALUE);
    ui->tipAngleSlider->init(tr("Tip Angle"), SpinSlider::LINEAR, angleInfo.minReal(), angleInfo.maxReal());

    mDynamicsBox = new QWidget(this);
    auto* dynamics = new QFormLayout(mDynamicsBox);
    dynamics->setContentsMargins(0, 0, 0, 0);
    struct Row { int type; QString label; QString suffix; };
    const Row rows[] = {
        { StrokeToolProperties::SPEED_TO_SIZE_VALUE, tr("Speed to size"), "%" },
        { StrokeToolProperties::TILT_TO_SIZE_VALUE, tr("Tilt to size"), "%" },
        { StrokeToolProperties::TILT_TO_ANGLE_VALUE, tr("Tilt to tip angle"), "%" },
        { StrokeToolProperties::SIZE_JITTER_VALUE, tr("Size jitter"), "%" },
        { StrokeToolProperties::SCATTER_VALUE, tr("Scatter"), "%" },
        { StrokeToolProperties::ANGLE_JITTER_VALUE, tr("Angle jitter"), QString(QChar(0x00B0)) },
        { StrokeToolProperties::DAB_COUNT_VALUE, tr("Dabs per step"), "" },
        // Taper also gives mouse strokes thin ends, like simulated pressure.
        { StrokeToolProperties::TAPER_START_VALUE, tr("Start taper"), tr(" px") },
        { StrokeToolProperties::TAPER_END_VALUE, tr("End taper"), tr(" px") },
    };
    for (const auto& [type, label, suffix] : rows)
    {
        const PropertyInfo info = p.getInfo(type);
        auto* box = new QSpinBox(mDynamicsBox);
        box->setRange(info.minInt(), info.maxInt());
        box->setSuffix(suffix);
        dynamics->addRow(label, box);
        mDynamicsSpinBoxes.append({ type, box });
        connect(box, &QSpinBox::valueChanged, this, [this, type = type](int value) { mCurrentTool->setDynamic(type, value); });
    }
    ui->verticalLayout_2->insertWidget(ui->verticalLayout_2->indexOf(ui->tipImageLayout) + 1, mDynamicsBox);

    // Item index is the ShapeTool::Mode value.
    mShapeModeCombo = new QComboBox(this);
    mShapeModeCombo->addItems({ tr("Line"), tr("Rectangle"), tr("Ellipse") });
    ui->verticalLayout_2->insertWidget(0, mShapeModeCombo);
    connect(mShapeModeCombo, &QComboBox::activated, this, [this](int mode) {
        static_cast<ShapeTool*>(mCurrentTool)->setShapeMode(mode);
    });

    mCurrentTool = strokeTool;

    makeConnectionFromUIToModel();
}

void StrokeOptionsWidget::updateUI()
{
    StrokeTool* strokeTool = mEditor->tools()->currentStrokeTool();
    if (strokeTool == nullptr) { return; }

    Q_ASSERT(strokeTool);

    updateToolConnections(strokeTool);

    setVisibility(strokeTool);

    const StrokeToolProperties& p = strokeTool->strokeToolProperties();

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::WIDTH_VALUE))
    {
        PropertyInfo info = p.getInfo(StrokeToolProperties::WIDTH_VALUE);
        QSignalBlocker b(ui->sizeSlider);
        ui->sizeSlider->setRange(info.minReal(), info.maxReal());
        QSignalBlocker b2(ui->sizeSpinBox);
        ui->sizeSpinBox->setRange(info.minReal(), info.maxReal());

        setWidthValue(info.realValue());
    }
    if (strokeTool->isPropertyEnabled(StrokeToolProperties::FEATHER_VALUE))
    {
        auto info = p.getInfo(StrokeToolProperties::FEATHER_VALUE);
        QSignalBlocker b3(ui->featherSlider);
        ui->featherSlider->setRange(info.minReal(), info.maxReal());
        QSignalBlocker b4(ui->featherSpinBox);
        ui->featherSpinBox->setRange(info.minReal(), info.maxReal());

        setFeatherValue(info.realValue());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::SPACING_VALUE))
    {
        auto info = p.getInfo(StrokeToolProperties::SPACING_VALUE);
        QSignalBlocker b5(ui->spacingSlider);
        ui->spacingSlider->setRange(info.minReal(), info.maxReal());
        QSignalBlocker b6(ui->spacingSpinBox);
        ui->spacingSpinBox->setRange(info.minReal(), info.maxReal());

        setSpacingValue(info.realValue());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::TIP_ASPECT_VALUE)) {
        updateTip();
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::SCATTER_VALUE)) {
        updateDynamics();
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::FLOW_VALUE)) {
        setFlowValue(p.flow());
        setStrokeOpacityValue(p.opacity());
        setWashEnabled(p.washEnabled());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::FEATHER_ENABLED)) {
        setFeatherEnabled(p.featherEnabled());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::PRESSURE_ENABLED)) {
        setPressureEnabled(p.pressureEnabled());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::INVISIBILITY_ENABLED)) {
        setPenInvisibilityEnabled(p.invisibilityEnabled());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::ANTI_ALIASING_ENABLED)) {
        setAntiAliasingEnabled(p.AntiAliasingEnabled());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::SMOOTHING_MODE)) {
        setSmoothingMode(static_cast<int>(p.smoothingMode()));
        setSmoothingStrength(p.smoothingStrength());
    }

    if (strokeTool->isPropertyEnabled(StrokeToolProperties::FILLCONTOUR_ENABLED)) {
        setFillContourEnabled(p.fillContourEnabled());
    }

    if (strokeTool->type() == POLYLINE) {
        const PolylineToolProperties polyP = static_cast<const PolylineTool*>(strokeTool)->settings();
        setClosedPathEnabled(polyP.closedPathEnabled());
        setBezierPathEnabled(polyP.bezierPathEnabled());
    }

    if (strokeTool->type() == SHAPE) {
        setShapeMode(static_cast<const ShapeTool*>(strokeTool)->shapeMode());
    }
}

void StrokeOptionsWidget::updateToolConnections(BaseTool* tool)
{
    if (mCurrentTool) {
        disconnect(mCurrentTool, nullptr, this, nullptr);
    }

    StrokeTool* strokeTool = static_cast<StrokeTool*>(tool);
    mCurrentTool = strokeTool;

    makeConnectionFromModelToUI(strokeTool);
}

void StrokeOptionsWidget::makeConnectionFromModelToUI(StrokeTool* strokeTool)
{
    connect(strokeTool, &StrokeTool::widthChanged, this, &StrokeOptionsWidget::setWidthValue);
    connect(strokeTool, &StrokeTool::featherChanged, this, &StrokeOptionsWidget::setFeatherValue);
    connect(strokeTool, &StrokeTool::spacingChanged, this, &StrokeOptionsWidget::setSpacingValue);
    connect(strokeTool, &StrokeTool::tipChanged, this, &StrokeOptionsWidget::updateTip);
    connect(strokeTool, &StrokeTool::dynamicsChanged, this, &StrokeOptionsWidget::updateDynamics);
    connect(strokeTool, &StrokeTool::flowChanged, this, &StrokeOptionsWidget::setFlowValue);
    connect(strokeTool, &StrokeTool::strokeOpacityChanged, this, &StrokeOptionsWidget::setStrokeOpacityValue);
    connect(strokeTool, &StrokeTool::washEnabledChanged, this, &StrokeOptionsWidget::setWashEnabled);
    connect(strokeTool, &StrokeTool::featherEnabledChanged, this, &StrokeOptionsWidget::setFeatherEnabled);
    connect(strokeTool, &StrokeTool::pressureEnabledChanged, this, &StrokeOptionsWidget::setPressureEnabled);
    connect(strokeTool, &StrokeTool::smoothingModeChanged, this, &StrokeOptionsWidget::setSmoothingMode);
    connect(strokeTool, &StrokeTool::smoothingStrengthChanged, this, &StrokeOptionsWidget::setSmoothingStrength);
    connect(strokeTool, &StrokeTool::antiAliasingEnabledChanged, this, &StrokeOptionsWidget::setAntiAliasingEnabled);
    connect(strokeTool, &StrokeTool::fillContourEnabledChanged, this, &StrokeOptionsWidget::setFillContourEnabled);
    connect(strokeTool, &StrokeTool::invisibleStrokeEnabledChanged, this, &StrokeOptionsWidget::setPenInvisibilityEnabled);

    if (strokeTool->type() == POLYLINE) {
        PolylineTool* polyline = static_cast<PolylineTool*>(strokeTool);
        connect(polyline, &PolylineTool::bezierPathEnabledChanged, this, &StrokeOptionsWidget::setBezierPathEnabled);
        connect(polyline, &PolylineTool::closePathChanged, this, &StrokeOptionsWidget::setClosedPathEnabled);
    }
    if (strokeTool->type() == SHAPE) {
        connect(static_cast<ShapeTool*>(strokeTool), &ShapeTool::shapeModeChanged, this, &StrokeOptionsWidget::setShapeMode);
    }
}

void StrokeOptionsWidget::makeConnectionFromUIToModel()
{
    auto spinboxValueChanged = static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged);

    connect(ui->useBezierBox, &QCheckBox::clicked, [=](bool enabled) {
        PolylineTool* tool = static_cast<PolylineTool*>(mCurrentTool);
        tool->setUseBezier(enabled);
    });

    connect(ui->useClosedPathBox, &QCheckBox::clicked, [=](bool enabled) {
        PolylineTool* tool = static_cast<PolylineTool*>(mCurrentTool);
        tool->setClosePath(enabled);
    });
    connect(ui->usePressureBox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setPressureEnabled(enabled);
    });

    connect(ui->makeInvisibleBox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setStrokeInvisibleEnabled(enabled);
    });

    connect(ui->useFeatherBox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setFeatherEnabled(enabled);
    });

    connect(ui->useAABox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setAntiAliasingEnabled(enabled);
    });

    connect(ui->fillContourBox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setFillContourEnabled(enabled);
    });

    connect(ui->sizeSlider, &SpinSlider::valueChanged, [=](qreal value) {
        mCurrentTool->setWidth(value);
    });

    connect(ui->sizeSpinBox, spinboxValueChanged, [=](qreal value) {
        mCurrentTool->setWidth(value);
    });

    connect(ui->featherSlider, &SpinSlider::valueChanged, [=](qreal value) {
        mCurrentTool->setFeather(value);
    });

    connect(ui->featherSpinBox, spinboxValueChanged, [=](qreal value) {
        mCurrentTool->setFeather(value);
    });

    connect(ui->spacingSlider, &SpinSlider::valueChanged, [=](qreal value) {
        mCurrentTool->setSpacing(value);
    });

    connect(ui->spacingSpinBox, spinboxValueChanged, [=](qreal value) {
        mCurrentTool->setSpacing(value);
    });

    connect(ui->tipAspectSlider, &SpinSlider::valueChanged, [=](qreal value) {
        mCurrentTool->setTipAspect(value);
    });
    connect(ui->tipAngleSlider, &SpinSlider::valueChanged, [=](qreal value) {
        mCurrentTool->setTipAngle(value);
    });
    connect(ui->squareTipBox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setTipSquare(enabled);
    });
    connect(ui->followDirectionBox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setTipFollowsDirection(enabled);
    });
    connect(ui->loadTipImageButton, &QPushButton::clicked, [=] {
        const QString fileName = QFileDialog::getOpenFileName(this, tr("Open Brush Tip"), QString(),
                                                              tr("Brush tips (*.png *.gbr);;All files (*)"));
        if (fileName.isEmpty()) return;
        const Status st = mCurrentTool->setTipImage(fileName);
        if (!st.ok()) QMessageBox::warning(this, tr("Open Brush Tip"), st.description());
    });
    connect(ui->clearTipImageButton, &QPushButton::clicked, [=] {
        mCurrentTool->setTipImage(QString());
    });

    connect(ui->flowSlider, &SpinSlider::valueChanged, [=](qreal value) {
        mCurrentTool->setFlow(value);
    });

    connect(ui->opacitySlider, &SpinSlider::valueChanged, [=](qreal value) {
        mCurrentTool->setStrokeOpacity(value);
    });

    connect(ui->washBox, &QCheckBox::clicked, [=](bool enabled) {
        mCurrentTool->setWashEnabled(enabled);
    });

    clearFocusOnFinished(ui->spacingSpinBox);
    clearFocusOnFinished(ui->sizeSpinBox);
    clearFocusOnFinished(ui->featherSpinBox);

    connect(ui->smoothingModeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [=](int value) {
        mCurrentTool->setSmoothingMode(value);
    });

    connect(ui->smoothingStrengthSlider, &QSlider::valueChanged, [=](int value) {
        mCurrentTool->setSmoothingStrength(value);
    });
}

void StrokeOptionsWidget::setVisibility(BaseTool* tool)
{
    ui->sizeSlider->setVisible(tool->isPropertyEnabled(StrokeToolProperties::WIDTH_VALUE));
    ui->sizeSpinBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::WIDTH_VALUE));
    ui->featherSlider->setVisible(tool->isPropertyEnabled(StrokeToolProperties::FEATHER_VALUE));
    ui->featherSpinBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::FEATHER_VALUE));
    ui->spacingSlider->setVisible(tool->isPropertyEnabled(StrokeToolProperties::SPACING_VALUE));
    ui->spacingSpinBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::SPACING_VALUE));
    const bool tip = tool->isPropertyEnabled(StrokeToolProperties::TIP_ASPECT_VALUE);
    ui->tipAspectSlider->setVisible(tip);
    ui->tipAngleSlider->setVisible(tip);
    ui->squareTipBox->setVisible(tip);
    ui->followDirectionBox->setVisible(tip);
    ui->flowSlider->setVisible(tool->isPropertyEnabled(StrokeToolProperties::FLOW_VALUE));
    ui->opacitySlider->setVisible(tool->isPropertyEnabled(StrokeToolProperties::OPACITY_VALUE));
    ui->washBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::WASH_ENABLED));
    ui->loadTipImageButton->setVisible(tip);
    ui->clearTipImageButton->setVisible(tip);
    if (mDynamicsBox) mDynamicsBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::SCATTER_VALUE));
    ui->useFeatherBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::FEATHER_ENABLED));
    ui->usePressureBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::PRESSURE_ENABLED));
    ui->makeInvisibleBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::INVISIBILITY_ENABLED));
    ui->useAABox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::ANTI_ALIASING_ENABLED));
    ui->smoothingLabel->setVisible(tool->isPropertyEnabled(StrokeToolProperties::SMOOTHING_MODE));
    ui->smoothingModeCombo->setVisible(tool->isPropertyEnabled(StrokeToolProperties::SMOOTHING_MODE));
    ui->smoothingStrengthSlider->setVisible(tool->isPropertyEnabled(StrokeToolProperties::SMOOTHING_MODE));
    ui->fillContourBox->setVisible(tool->isPropertyEnabled(StrokeToolProperties::FILLCONTOUR_ENABLED));
    ui->useBezierBox->setVisible(tool->isPropertyEnabled(PolylineToolProperties::BEZIERPATH_ENABLED));
    ui->useClosedPathBox->setVisible(tool->isPropertyEnabled(PolylineToolProperties::CLOSEDPATH_ENABLED));
    if (mShapeModeCombo) mShapeModeCombo->setVisible(tool->isPropertyEnabled(ShapeToolProperties::SHAPEMODE_VALUE));
}

void StrokeOptionsWidget::setShapeMode(int mode)
{
    QSignalBlocker b(mShapeModeCombo);
    mShapeModeCombo->setCurrentIndex(mode);
}

void StrokeOptionsWidget::setWidthValue(qreal width)
{
    QSignalBlocker b(ui->sizeSlider);
    ui->sizeSlider->setValue(width);

    QSignalBlocker b2(ui->sizeSpinBox);
    ui->sizeSpinBox->setValue(width);
}

void StrokeOptionsWidget::setFeatherValue(qreal featherValue)
{
    QSignalBlocker b(ui->featherSlider);
    ui->featherSlider->setValue(featherValue);

    QSignalBlocker b2(ui->featherSpinBox);
    ui->featherSpinBox->setValue(featherValue);
}

void StrokeOptionsWidget::setSpacingValue(qreal spacing)
{
    QSignalBlocker b(ui->spacingSlider);
    ui->spacingSlider->setValue(spacing);

    QSignalBlocker b2(ui->spacingSpinBox);
    ui->spacingSpinBox->setValue(spacing);
}

void StrokeOptionsWidget::setFlowValue(qreal flow)
{
    QSignalBlocker b(ui->flowSlider);
    ui->flowSlider->setValue(flow);
}

void StrokeOptionsWidget::setStrokeOpacityValue(qreal opacity)
{
    QSignalBlocker b(ui->opacitySlider);
    ui->opacitySlider->setValue(opacity);
}

void StrokeOptionsWidget::setWashEnabled(bool enabled)
{
    QSignalBlocker b(ui->washBox);
    ui->washBox->setChecked(enabled);
}

void StrokeOptionsWidget::updateTip()
{
    const StrokeToolProperties& p = mCurrentTool->strokeToolProperties();
    QSignalBlocker b(ui->tipAspectSlider);
    ui->tipAspectSlider->setValue(p.tipAspect());
    QSignalBlocker b2(ui->tipAngleSlider);
    ui->tipAngleSlider->setValue(p.tipAngle());
    QSignalBlocker b3(ui->squareTipBox);
    ui->squareTipBox->setChecked(p.tipSquare());
    QSignalBlocker b4(ui->followDirectionBox);
    ui->followDirectionBox->setChecked(p.tipFollowsDirection());
}

void StrokeOptionsWidget::updateDynamics()
{
    const StrokeToolProperties& p = mCurrentTool->strokeToolProperties();
    for (const auto& [type, box] : mDynamicsSpinBoxes)
    {
        QSignalBlocker b(box);
        box->setValue(p.getInfo(type).intValue());
    }
}

void StrokeOptionsWidget::setFeatherEnabled(bool enabled)
{
    QSignalBlocker b(ui->useFeatherBox);
    ui->useFeatherBox->setChecked(enabled);
}

void StrokeOptionsWidget::setPenInvisibilityEnabled(bool enabled)
{
    QSignalBlocker b(ui->makeInvisibleBox);
    ui->makeInvisibleBox->setChecked(enabled);
}

void StrokeOptionsWidget::setPressureEnabled(bool enabled)
{
    QSignalBlocker b(ui->usePressureBox);
    ui->usePressureBox->setChecked(enabled);
}

void StrokeOptionsWidget::setAntiAliasingEnabled(bool enabled)
{
    QSignalBlocker b(ui->useAABox);
    ui->useAABox->setChecked(enabled);
}

void StrokeOptionsWidget::setSmoothingMode(int mode)
{
    QSignalBlocker b(ui->smoothingModeCombo);
    ui->smoothingModeCombo->setCurrentIndex(mode);
    ui->smoothingStrengthSlider->setEnabled(mode != static_cast<int>(SmoothingMode::None));
}

void StrokeOptionsWidget::setSmoothingStrength(int strength)
{
    QSignalBlocker b(ui->smoothingStrengthSlider);
    ui->smoothingStrengthSlider->setValue(strength);
}

void StrokeOptionsWidget::setFillContourEnabled(bool enabled)
{
    QSignalBlocker b(ui->fillContourBox);
    ui->fillContourBox->setChecked(enabled);
}

void StrokeOptionsWidget::setBezierPathEnabled(bool enabled)
{
    QSignalBlocker b(ui->useBezierBox);
    ui->useBezierBox->setChecked(enabled);
}

void StrokeOptionsWidget::setClosedPathEnabled(bool enabled)
{
    QSignalBlocker b(ui->useClosedPathBox);
    ui->useClosedPathBox->setChecked(enabled);
}
