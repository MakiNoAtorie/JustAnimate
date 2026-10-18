#include "strokeoptionswidget.h"
#include "ui_strokeoptionswidget.h"

#include "editor.h"
#include "pencilsettings.h"
#include "basetool.h"
#include "stroketool.h"
#include "toolproperties.h"
#include "util.h"

#include "toolmanager.h"
#include "layermanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSlider>
#include <QSpinBox>

#include <algorithm>

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
    using S = StrokeToolProperties;
    const auto initSlider = [this](SpinSlider* slider, int id, SpinSlider::GROWTH_TYPE growth)
    {
        const ToolPropertyDescriptor* d = ToolProperties::descriptor(id);
        slider->init(d->text(), growth, d->min, d->max);
        bind(id, slider);
    };
    initSlider(ui->sizeSlider, S::WIDTH_VALUE, SpinSlider::EXPONENT);
    bind(S::WIDTH_VALUE, ui->sizeSpinBox);
    initSlider(ui->featherSlider, S::FEATHER_VALUE, SpinSlider::LOG);
    bind(S::FEATHER_VALUE, ui->featherSpinBox);
    initSlider(ui->spacingSlider, S::SPACING_VALUE, SpinSlider::LOG);
    bind(S::SPACING_VALUE, ui->spacingSpinBox);
    initSlider(ui->flowSlider, S::FLOW_VALUE, SpinSlider::LINEAR);
    initSlider(ui->opacitySlider, S::OPACITY_VALUE, SpinSlider::LINEAR);
    initSlider(ui->tipAspectSlider, S::TIP_ASPECT_VALUE, SpinSlider::LINEAR);
    initSlider(ui->tipAngleSlider, S::TIP_ANGLE_VALUE, SpinSlider::LINEAR);
    bind(S::WASH_ENABLED, ui->washBox);
    ui->smoothingLabel->setText(ToolProperties::descriptor(S::SMOOTHING_MODE)->text());
    bind(S::SMOOTHING_MODE, ui->smoothingModeCombo);
    bind(S::SMOOTHING_STRENGTH, ui->smoothingStrengthSlider);
    bind(S::FEATHER_ENABLED, ui->useFeatherBox);
    bind(S::TIP_SQUARE_ENABLED, ui->squareTipBox);
    bind(S::TIP_FOLLOW_DIRECTION_ENABLED, ui->followDirectionBox);
    bind(S::FILLCONTOUR_ENABLED, ui->fillContourBox);
    bind(PolylineToolProperties::CLOSEDPATH_ENABLED, ui->useClosedPathBox);
    bind(PolylineToolProperties::BEZIERPATH_ENABLED, ui->useBezierBox);
    bind(S::PRESSURE_ENABLED, ui->usePressureBox);
    bind(S::ANTI_ALIASING_ENABLED, ui->useAABox);
    bind(S::INVISIBILITY_ENABLED, ui->makeInvisibleBox);

    auto* shapeMode = new QComboBox(this);
    ui->verticalLayout_2->insertWidget(0, shapeMode);
    bind(ShapeToolProperties::SHAPEMODE_VALUE, shapeMode);

    mDynamicsBox = new QWidget(this);
    // A grid, not a form: Tool settings lays a named grid out in one row in its strip
    auto* dynamics = new QGridLayout(mDynamicsBox);
    dynamics->setObjectName("dynamicsLayout");
    dynamics->setContentsMargins(0, 0, 0, 0);
    const auto suffix = [](int id) -> QString
    {
        switch (id)
        {
        case S::ANGLE_JITTER_VALUE: return QString(QChar(0x00B0));
        // Taper also gives mouse strokes thin ends, like simulated pressure.
        case S::TAPER_START_VALUE:
        case S::TAPER_END_VALUE: return tr(" px");
        case S::DAB_COUNT_VALUE:
        case S::GRAIN_BRIGHTNESS_VALUE: return QString();
        default: return "%";
        }
    };
    // Every other property with a row goes in the grid, in descriptor order.
    for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
    {
        const bool bound = std::any_of(mRows.cbegin(), mRows.cend(), [&d](const auto& row) { return row.first == d.id; });
        if (bound || d.uses.isEmpty()) { continue; }
        QWidget* field = nullptr;
        if (d.row == ToolRowKind::ComboBox)
        {
            field = new QComboBox(mDynamicsBox);
        }
        else
        {
            auto* box = new QSpinBox(mDynamicsBox);
            box->setSuffix(suffix(d.id));
            field = box;
        }
        field->setToolTip(d.text());
        auto* label = new QLabel(d.text(), mDynamicsBox);
        label->setBuddy(field);
        const int row = dynamics->rowCount();
        dynamics->addWidget(label, row, 0);
        dynamics->addWidget(field, row, 1);
        bind(d.id, field);
    }
    ui->verticalLayout_2->insertWidget(ui->verticalLayout_2->indexOf(ui->tipImageLayout) + 1, mDynamicsBox);

    mCurrentTool = mEditor->tools()->currentStrokeTool();

    makeConnectionFromUIToModel();
}

void StrokeOptionsWidget::bind(int id, QWidget* control)
{
    const ToolPropertyDescriptor* d = ToolProperties::descriptor(id);
    Q_ASSERT(d);
    control->setProperty("toolProperty", id);
    mRows.append({ id, control });
    const auto set = [this, id](const QVariant& value) { if (mCurrentTool) { mCurrentTool->setValue(id, value); } };
    if (auto* slider = qobject_cast<SpinSlider*>(control))
    {
        connect(slider, &SpinSlider::valueChanged, this, [set](qreal v) { set(v); });
    }
    else if (auto* spin = qobject_cast<QDoubleSpinBox*>(control))
    {
        spin->setRange(d->min, d->max);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [set](double v) { set(v); });
        clearFocusOnFinished(spin);
    }
    else if (auto* box = qobject_cast<QSpinBox*>(control))
    {
        box->setRange(int(d->min), int(d->max));
        connect(box, &QSpinBox::valueChanged, this, [set](int v) { set(v); });
    }
    else if (auto* strength = qobject_cast<QSlider*>(control))
    {
        strength->setRange(int(d->min), int(d->max));
        connect(strength, &QSlider::valueChanged, this, [set](int v) { set(v); });
    }
    else if (auto* check = qobject_cast<QCheckBox*>(control))
    {
        check->setText(d->text());
        connect(check, &QCheckBox::clicked, this, [set](bool v) { set(v); });
    }
    else if (auto* combo = qobject_cast<QComboBox*>(control))
    {
        // Item index is the value less the minimum.
        if (combo->count() == 0) { combo->addItems(d->itemTexts()); }
        combo->setToolTip(d->text());
        const int min = int(d->min);
        connect(combo, &QComboBox::activated, this, [set, min](int index) { set(index + min); });
    }
}

void StrokeOptionsWidget::updateUI()
{
    StrokeTool* strokeTool = mEditor->tools()->currentStrokeTool();
    if (strokeTool == nullptr) { return; }

    updateToolConnections(strokeTool);

    setVisibility(strokeTool);

    for (const auto& row : mRows)
        updateRow(row.first);
}

void StrokeOptionsWidget::updateToolConnections(BaseTool* tool)
{
    if (mCurrentTool) {
        disconnect(mCurrentTool, nullptr, this, nullptr);
    }

    mCurrentTool = static_cast<StrokeTool*>(tool);
    connect(mCurrentTool, &StrokeTool::propertyChanged, this, &StrokeOptionsWidget::updateRow);
}

void StrokeOptionsWidget::updateRow(int id)
{
    const QVariant value = mCurrentTool->properties().value(id);
    if (!value.isValid()) { return; }
    for (const auto& [rowId, control] : mRows)
    {
        if (rowId != id) { continue; }
        QSignalBlocker b(control);
        if (auto* slider = qobject_cast<SpinSlider*>(control)) { slider->setValue(value.toReal()); }
        else if (auto* spin = qobject_cast<QDoubleSpinBox*>(control)) { spin->setValue(value.toDouble()); }
        else if (auto* box = qobject_cast<QSpinBox*>(control)) { box->setValue(value.toInt()); }
        else if (auto* strength = qobject_cast<QSlider*>(control)) { strength->setValue(value.toInt()); }
        else if (auto* check = qobject_cast<QCheckBox*>(control)) { check->setChecked(value.toBool()); }
        else if (auto* combo = qobject_cast<QComboBox*>(control))
        {
            combo->setCurrentIndex(value.toInt() - int(ToolProperties::descriptor(id)->min));
        }
    }
    if (id == StrokeToolProperties::SMOOTHING_MODE)
    {
        ui->smoothingStrengthSlider->setEnabled(value.toInt() != static_cast<int>(SmoothingMode::None));
    }
}

void StrokeOptionsWidget::makeConnectionFromUIToModel()
{
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
}

void StrokeOptionsWidget::setVisibility(BaseTool* tool)
{
    for (const auto& [id, control] : mRows)
        control->setVisible(tool->isPropertyEnabled(id));
    const bool tip = tool->isPropertyEnabled(StrokeToolProperties::TIP_ASPECT_VALUE);
    ui->loadTipImageButton->setVisible(tip);
    ui->clearTipImageButton->setVisible(tip);
    ui->smoothingLabel->setVisible(tool->isPropertyEnabled(StrokeToolProperties::SMOOTHING_MODE));
    bool dynamics = false;
    for (QLabel* label : mDynamicsBox->findChildren<QLabel*>())
    {
        label->setVisible(!label->buddy()->isHidden());
        dynamics = dynamics || !label->isHidden();
    }
    mDynamicsBox->setVisible(dynamics);
}
