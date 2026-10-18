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
#include <QComboBox>
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

QImage PresetSampler::paint(const BrushPresets& presets, const QJsonObject& preset, QSize size)
{
    presets.apply(preset, *brush);
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
                                     const QString& original, const QString& tool, QWidget* parent)
    : QDialog(parent), mPresets(presets), mOriginal(original), mTool(source.isEmpty() ? tool : presets.tool(source))
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

    // One control per engine setting the Brush stores, from its descriptor.
    for (const ToolPropertyDescriptor& d : ToolProperties::descriptors())
    {
        if (!brush->properties().has(d.id))
            continue;
        const QVariant value = brush->properties().value(d.id);
        const int id = d.id;
        const auto set = [this, id](const QVariant& v) {
            mSampler->brush->setValue(id, v);
            updatePreview();
        };
        QWidget* control = nullptr;
        if (const QStringList items = d.itemTexts(); !items.isEmpty())
        {
            auto box = new QComboBox(this);
            box->addItems(items);
            box->setCurrentIndex(value.toInt() - int(d.min));
            const int min = int(d.min);
            connect(box, &QComboBox::currentIndexChanged, this, [set, min](int index) { set(index + min); });
            control = box;
        }
        else if (d.type == PropertyInfo::INTEGER)
        {
            auto box = new QSpinBox(this);
            box->setRange(int(d.min), int(d.max));
            box->setValue(value.toInt());
            connect(box, &QSpinBox::valueChanged, this, [set](int v) { set(v); });
            control = box;
        }
        else if (d.type == PropertyInfo::REAL)
        {
            auto box = new QDoubleSpinBox(this);
            box->setDecimals(1);
            box->setRange(d.min, d.max);
            box->setValue(value.toDouble());
            connect(box, &QDoubleSpinBox::valueChanged, this, [set](double v) { set(v); });
            control = box;
        }
        else
        {
            auto box = new QCheckBox(this);
            box->setChecked(value.toBool());
            connect(box, &QCheckBox::toggled, this, [set](bool v) { set(v); });
            control = box;
        }
        control->setObjectName(d.key);
        form->addRow(d.text(), control);
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
    if (!mPresets.save(newName, *mSampler->brush, mDescription->text().trimmed(), mTool))
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
