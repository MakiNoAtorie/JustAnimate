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

#include <QApplication>
#include <QDir>
#include <QListWidget>
#include <QPainter>
#include <QTabletEvent>
#include <QtMath>

#include "bitmapimage.h"
#include "brushtool.h"
#include "colormanager.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "object.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "undoredomanager.h"

/// A hidden editor whose Brush paints the sample strokes, so samples use the real dab engine.
struct PresetSampler
{
    std::unique_ptr<ScribbleArea> area{ new ScribbleArea(nullptr) };
    std::unique_ptr<Editor> editor{ new Editor }; // destroyed before the area
    LayerBitmap* layer = nullptr;
    BrushTool* brush = nullptr;

    explicit PresetSampler(const QColor& ink)
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

    ~PresetSampler()
    {
        // ~ToolManager stores tool settings: reload them so the user's settings are written back unchanged.
        for (int t = 0; t < TOOL_TYPE_COUNT; ++t)
            editor->tools()->getTool(static_cast<ToolType>(t))->loadSettings();
    }

    void tablet(QEvent::Type type, QPointF pos, qreal pressure)
    {
        const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
        QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                       pressure, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
        QApplication::sendEvent(area.get(), &e);
    }

    QImage paint(const BrushPresets& presets, const QString& name, QSize size)
    {
        BitmapImage* image = layer->getBitmapImageAtFrame(1);
        image->clear();
        presets.apply(name, *brush);
        brush->seedRandom(1);

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
};

BrushPresetDock::BrushPresetDock(QWidget* parent, const QString& folder) : BaseDockWidget(parent), mPresets(folder)
{
    setWindowTitle(tr("Brush Presets", "Window title of brush preset dock"));
    mList = new QListWidget(this);
    mList->setIconSize(sampleSize());
    setWidget(mList);
    connect(mList, &QListWidget::currentRowChanged, this, &BrushPresetDock::applyRow);
    connect(mList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { applyRow(mList->row(item)); });
    connect(&mWatcher, &QFileSystemWatcher::directoryChanged, this, &BrushPresetDock::updateUI);
    connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, &BrushPresetDock::updateUI);
}

BrushPresetDock::~BrushPresetDock() = default;

void BrushPresetDock::initUI()
{
    QDir().mkpath(mPresets.folder());
    mWatcher.addPath(mPresets.folder());
    updateUI();
}

void BrushPresetDock::updateUI()
{
    if (!mSampler)
        mSampler = std::make_unique<PresetSampler>(palette().color(QPalette::Text));

    const QString current = mList->currentItem() ? mList->currentItem()->data(Qt::UserRole).toString() : QString();
    const QSignalBlocker blocker(mList);
    mList->clear();
    const QStringList names = mPresets.names();
    for (const QString& name : names)
    {
        const QString description = mPresets.description(name);
        auto item = new QListWidgetItem(sample(name), description.isEmpty() ? name : name + "\n" + description, mList);
        item->setData(Qt::UserRole, name);
        if (name == current)
            mList->setCurrentItem(item);
    }

    // Saving a preset replaces its file, which drops the file watch on some platforms.
    if (!mWatcher.files().isEmpty())
        mWatcher.removePaths(mWatcher.files());
    for (const QString& name : names)
        mWatcher.addPath(QDir(mPresets.folder()).filePath(name + ".json"));
}

void BrushPresetDock::applyRow(int row)
{
    QListWidgetItem* item = mList->item(row);
    if (!item)
        return;
    ToolManager* tools = editor()->tools();
    if (!tools->currentStrokeTool())
        tools->setCurrentTool(BRUSH);
    if (mPresets.apply(item->data(Qt::UserRole).toString(), *tools->currentStrokeTool()))
        emit tools->toolsReset(); // the tool options show the new values
}

QIcon BrushPresetDock::sample(const QString& name)
{
    return QIcon(QPixmap::fromImage(mSampler->paint(mPresets, name, sampleSize())));
}
