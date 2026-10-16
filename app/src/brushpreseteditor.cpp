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
#include "brushpreseteditor.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabletEvent>
#include <QVBoxLayout>
#include <QtMath>

#include "bitmapimage.h"
#include "brushtool.h"
#include "colormanager.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "preferencemanager.h"
#include "pressurecurvewidget.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "undoredomanager.h"

PresetSampler::PresetSampler(const QColor& ink) : area(new ScribbleArea(nullptr)), editor(new Editor)
{
    Object* object = new Object;
    object->init();
    layer = object->addNewBitmapLayer();
    area->setEditor(editor.get());
    editor->setScribbleArea(area.get());
    editor->setObject(object);
    editor->init();
    area->init();
    area->resize(400, 100);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(BRUSH);
    editor->color()->setFrontColor(ink);
    brush = static_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
}

PresetSampler::~PresetSampler()
{
    // ~ToolManager stores tool settings: reload them so the user's settings are written back unchanged.
    for (int t = 0; t < TOOL_TYPE_COUNT; ++t)
        editor->tools()->getTool(static_cast<ToolType>(t))->loadSettings();
}

QImage PresetSampler::paint(const BrushPresets& presets, const QString& name, QSize size)
{
    presets.apply(name, *brush);
    return paint(size);
}

QImage PresetSampler::paint(QSize size)
{
    BitmapImage* image = layer->getBitmapImageAtFrame(1);
    image->clear();
    brush->seedRandom(1);

    const auto tablet = [this](QEvent::Type type, QPointF pos, qreal pressure) {
        const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
        QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                       pressure, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
        QApplication::sendEvent(area.get(), &e);
    };
    // An S curve whose pressure rises and falls.
    const int steps = 40;
    const auto point = [](qreal t) { return QPointF(50 + 300 * t, 50 - 25 * qSin(t * 2 * M_PI)); };
    tablet(QEvent::TabletPress, point(0), 0.1);
    for (int i = 1; i <= steps; ++i)
    {
        const qreal t = qreal(i) / steps;
        tablet(QEvent::TabletMove, point(t), 0.1 + 0.9 * qSin(t * M_PI));
    }
    tablet(QEvent::TabletRelease, point(1), 0.1);
    editor->undoRedo()->clearStack();

    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    const QRect bounds = image->bounds();
    if (bounds.isEmpty())
        return out;
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSize fitted = bounds.size().scaled(size, Qt::KeepAspectRatio);
    p.drawImage(QRect(QPoint((size.width() - fitted.width()) / 2, (size.height() - fitted.height()) / 2), fitted),
                *image->image());
    return out;
}

BrushPresetEditor::BrushPresetEditor(const BrushPresets& presets, const QString& source, const QString& name,
                                     const QString& original, QWidget* parent)
    : QDialog(parent), mPresets(presets), mOriginal(original)
{
    setWindowTitle(tr("Brush Preset"));
    mSampler = std::make_unique<PresetSampler>(palette().color(QPalette::Text));
    BrushTool* brush = mSampler->brush;
    if (source.isEmpty() || !mPresets.apply(source, *brush))
    {
        brush->toolProperties().restoreProperties();
        brush->setTipImage(QString());
        brush->setPressureCurve(QString());
    }

    mName = new QLineEdit(name, this);
    mName->setObjectName("name");
    mDescription = new QLineEdit(mPresets.description(source), this);
    mDescription->setObjectName("description");

    mTip = new QLineEdit(brush->tipImagePath(), this);
    mTip->setObjectName("tipImage");
    mTip->setPlaceholderText(tr("Generated tip"));
    auto browse = new QPushButton(tr("Browse..."), this);
    auto tipRow = new QHBoxLayout;
    tipRow->addWidget(mTip);
    tipRow->addWidget(browse);
    connect(mTip, &QLineEdit::editingFinished, this, [this] {
        const QString file = mTip->text().trimmed();
        if (file == mSampler->brush->tipImagePath())
            return;
        const Status st = mSampler->brush->setTipImage(file);
        mMessage->setText(st.ok() ? QString() : tr("Cannot load the tip image %1.").arg(file));
        mTip->setText(mSampler->brush->tipImagePath());
        updatePreview();
    });
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Brush Tip"), mTip->text(), tr("Brush tips (*.png *.gbr)"));
        if (file.isEmpty())
            return;
        mTip->setText(file);
        emit mTip->editingFinished();
    });

    auto form = new QFormLayout;
    form->addRow(tr("Name"), mName);
    form->addRow(tr("Description"), mDescription);
    form->addRow(tr("Tip image"), tipRow);

    // One control per engine setting, from the tool's own property ranges.
    ToolProperties& props = brush->toolProperties();
    for (const auto& range : props.typeRanges())
    {
        for (int key = range.first; key < range.second; ++key)
        {
            const PropertyInfo info = props.getInfo(key);
            QWidget* control = nullptr;
            switch (info.type())
            {
            case PropertyInfo::INTEGER:
            {
                auto box = new QSpinBox(this);
                box->setRange(info.minInt(), info.maxInt());
                box->setValue(info.intValue());
                connect(box, &QSpinBox::valueChanged, this, [this, key](int v) {
                    mSampler->brush->toolProperties().setBaseValue(key, PropertyInfo(v));
                    updatePreview();
                });
                control = box;
                break;
            }
            case PropertyInfo::REAL:
            {
                auto box = new QDoubleSpinBox(this);
                box->setDecimals(1);
                box->setRange(info.minReal(), info.maxReal());
                box->setValue(info.realValue());
                connect(box, &QDoubleSpinBox::valueChanged, this, [this, key](double v) {
                    mSampler->brush->toolProperties().setBaseValue(key, PropertyInfo(qreal(v)));
                    updatePreview();
                });
                control = box;
                break;
            }
            case PropertyInfo::BOOL:
            {
                auto box = new QCheckBox(this);
                box->setChecked(info.boolValue());
                connect(box, &QCheckBox::toggled, this, [this, key](bool v) {
                    mSampler->brush->toolProperties().setBaseValue(key, PropertyInfo(v));
                    updatePreview();
                });
                control = box;
                break;
            }
            case PropertyInfo::INVALID:
                continue;
            }
            control->setObjectName(props.identifier(key));
            form->addRow(settingLabel(key, props.identifier(key)), control);
        }
    }

    mGlobalCurve = new QCheckBox(tr("Use the global pressure curve"), this);
    mGlobalCurve->setObjectName("globalCurve");
    mGlobalCurve->setChecked(brush->pressureCurve().isEmpty());
    mCurve = new PressureCurveWidget(this);
    mCurve->setCurve(PressureCurve::fromString(brush->pressureCurve().isEmpty()
                                                   ? mSampler->editor->preference()->getString(SETTING::PRESSURE_CURVE)
                                                   : brush->pressureCurve()));
    mCurve->setEnabled(!mGlobalCurve->isChecked());
    connect(mGlobalCurve, &QCheckBox::toggled, this, [this](bool global) {
        mCurve->setEnabled(!global);
        applyCurve();
    });
    connect(mCurve, &PressureCurveWidget::curveChanged, this, &BrushPresetEditor::applyCurve);
    form->addRow(tr("Pressure curve"), mGlobalCurve);
    form->addRow(QString(), mCurve);

    auto settings = new QWidget(this);
    settings->setLayout(form);
    auto scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(settings);

    mPreviewLabel = new QLabel(this);
    mPreviewLabel->setFixedSize(previewSize());
    mMessage = new QLabel(this);
    mMessage->setWordWrap(true);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &BrushPresetEditor::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &BrushPresetEditor::reject);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(mPreviewLabel, 0, Qt::AlignHCenter);
    layout->addWidget(scroll);
    layout->addWidget(mMessage);
    layout->addWidget(buttons);
    updatePreview();
}

BrushPresetEditor::~BrushPresetEditor() = default;

QString BrushPresetEditor::settingLabel(int key, const QString& identifier)
{
    // The stroke options labels, so their translations apply here too.
    switch (key)
    {
    case StrokeToolProperties::WIDTH_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Width");
    case StrokeToolProperties::FEATHER_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Feather");
    case StrokeToolProperties::FEATHER_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Use Feather");
    case StrokeToolProperties::SMOOTHING_MODE: return QCoreApplication::translate("StrokeOptionsWidget", "Smoothing");
    case StrokeToolProperties::SMOOTHING_STRENGTH: return QCoreApplication::translate("StrokeOptionsWidget", "Smoothing strength");
    case StrokeToolProperties::PRESSURE_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Pressure", "Tool options");
    case StrokeToolProperties::INVISIBILITY_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Invisible", "Tool options");
    case StrokeToolProperties::ANTI_ALIASING_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Anti-Aliasing", "Brush AA");
    case StrokeToolProperties::FILLCONTOUR_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Fill Contour");
    case StrokeToolProperties::SPACING_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Spacing %");
    case StrokeToolProperties::TIP_SQUARE_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Square Tip", "Tool options");
    case StrokeToolProperties::TIP_ASPECT_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Tip Aspect %");
    case StrokeToolProperties::TIP_ANGLE_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Tip Angle");
    case StrokeToolProperties::TIP_FOLLOW_DIRECTION_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Follow Direction", "Tool options");
    case StrokeToolProperties::FLOW_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Flow %");
    case StrokeToolProperties::OPACITY_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Opacity %");
    case StrokeToolProperties::WASH_ENABLED: return QCoreApplication::translate("StrokeOptionsWidget", "Wash");
    case StrokeToolProperties::SPEED_TO_SIZE_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Speed to size");
    case StrokeToolProperties::TILT_TO_SIZE_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Tilt to size");
    case StrokeToolProperties::TILT_TO_ANGLE_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Tilt to tip angle");
    case StrokeToolProperties::SIZE_JITTER_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Size jitter");
    case StrokeToolProperties::SCATTER_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Scatter");
    case StrokeToolProperties::ANGLE_JITTER_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Angle jitter");
    case StrokeToolProperties::DAB_COUNT_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Dabs per step");
    case StrokeToolProperties::TAPER_START_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Start taper");
    case StrokeToolProperties::TAPER_END_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "End taper");
    case StrokeToolProperties::GRAIN_MODE_VALUE: return tr("Grain (0 off, 1 paper, 2 moving)");
    case StrokeToolProperties::GRAIN_SCALE_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Grain scale");
    case StrokeToolProperties::GRAIN_CONTRAST_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Grain contrast");
    case StrokeToolProperties::GRAIN_BRIGHTNESS_VALUE: return QCoreApplication::translate("StrokeOptionsWidget", "Grain brightness");
    case StrokeToolProperties::FLOW_PER_DAB_ENABLED: return tr("Flow per dab (Classic)");
    default: return identifier;
    }
}

QString BrushPresetEditor::name() const
{
    return mName->text().trimmed();
}

void BrushPresetEditor::applyCurve()
{
    mSampler->brush->setPressureCurve(mGlobalCurve->isChecked() ? QString() : mCurve->curve().toString());
    updatePreview();
}

void BrushPresetEditor::updatePreview()
{
    mPreview = mSampler->paint(previewSize());
    mPreviewLabel->setPixmap(QPixmap::fromImage(mPreview));
}

bool BrushPresetEditor::save()
{
    const QString newName = name();
    if (!BrushPresets::isValidName(newName))
    {
        mMessage->setText(tr("Enter a name without path characters."));
        return false;
    }
    if (newName != mOriginal && mPresets.exists(newName))
    {
        mMessage->setText(tr("A brush preset named %1 exists.").arg(newName));
        return false;
    }
    applyCurve();
    if (!mPresets.save(newName, *mSampler->brush, mDescription->text().trimmed()))
    {
        mMessage->setText(tr("Cannot write the brush preset %1.").arg(newName));
        return false;
    }
    if (!mOriginal.isEmpty() && newName != mOriginal)
        mPresets.remove(mOriginal);
    mOriginal = newName;
    return true;
}

void BrushPresetEditor::accept()
{
    if (save())
        QDialog::accept();
}
