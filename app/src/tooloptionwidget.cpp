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
#include "tooloptionwidget.h"
#include "ui_tooloptions.h"

#include <QPushButton>
#include <QSettings>
#include <QDebug>
#include <QBoxLayout>
#include <QGridLayout>
#include <algorithm>
#include <tuple>
#include <QMainWindow>

#include "cameraoptionswidget.h"
#include "bucketoptionswidget.h"
#include "strokeoptionswidget.h"
#include "transformoptionswidget.h"
#include "spinslider.h"
#include "editor.h"
#include "util.h"
#include "layer.h"
#include "layermanager.h"
#include "stroketool.h"
#include "toolmanager.h"
#include "basewidget.h"
#include "viewmanager.h"


ToolOptionWidget::ToolOptionWidget(QWidget* parent) : BaseDockWidget(parent)
{
    setWindowTitle(tr("Tool settings", "Window title of tool option panel like pen width, feather etc.."));

    QWidget* innerWidget = new QWidget;
    setWidget(innerWidget);
    ui = new Ui::ToolOptions;
    ui->setupUi(innerWidget);
}

ToolOptionWidget::~ToolOptionWidget()
{
    delete ui;
}

void ToolOptionWidget::initUI()
{
    mBucketOptionsWidget = new BucketOptionsWidget(editor(), this);
    mCameraOptionsWidget = new CameraOptionsWidget(editor(), this);

    mBucketOptionsWidget->setHidden(true);
    mCameraOptionsWidget->setHidden(true);
    
    mStrokeOptionsWidget = new StrokeOptionsWidget(editor(), this);
    mTransformOptionsWidget = new TransformOptionsWidget(editor(), this);
    ui->scrollAreaWidgetContents->layout()->addWidget(mBucketOptionsWidget);
    ui->scrollAreaWidgetContents->layout()->addWidget(mCameraOptionsWidget);
    ui->scrollAreaWidgetContents->layout()->addWidget(mStrokeOptionsWidget);
    ui->scrollAreaWidgetContents->layout()->addWidget(mTransformOptionsWidget);

    mResetViewButton = new QPushButton(tr("Reset View"), this);
    mResetViewButton->setObjectName("resetViewButton");
    mResetViewButton->setToolTip(tr("Set the view rotation back to 0 degrees"));
    mResetViewButton->setHidden(true);
    connect(mResetViewButton, &QPushButton::clicked, editor()->view(), &ViewManager::resetRotation);
    ui->scrollAreaWidgetContents->layout()->addWidget(mResetViewButton);
    ui->scrollAreaWidgetContents->layout()->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));

    for (QBoxLayout* layout : ui->scrollAreaWidgetContents->findChildren<QBoxLayout*>())
    {
        if (layout->direction() == QBoxLayout::TopToBottom && !layout->objectName().isEmpty()) { mColumnLayouts << layout; }
    }
    for (QGridLayout* grid : ui->scrollAreaWidgetContents->findChildren<QGridLayout*>())
    {
        if (grid->objectName().isEmpty()) { continue; }
        QList<GridCell> cells;
        for (int i = 0; i < grid->count(); ++i)
        {
            GridCell c{grid->itemAt(i), 0, 0, 0, 0, grid->itemAt(i)->alignment()};
            grid->getItemPosition(i, &c.row, &c.column, &c.rowSpan, &c.columnSpan);
            cells << c;
        }
        std::sort(cells.begin(), cells.end(), [](const GridCell& a, const GridCell& b)
        {
            return std::tie(a.row, a.column) < std::tie(b.row, b.column);
        });
        mGrids << qMakePair(grid, cells);
        mMargins.insert(grid, grid->contentsMargins());
    }
    for (QBoxLayout* layout : mColumnLayouts) { mMargins.insert(layout, layout->contentsMargins()); }
    connect(this, &QDockWidget::dockLocationChanged, this, &ToolOptionWidget::updateOrientation);
    connect(this, &QDockWidget::topLevelChanged, this, &ToolOptionWidget::updateOrientation);
    updateOrientation();

    makeConnectionToEditor(editor());
    updateUI();
}

void ToolOptionWidget::updateUI()
{
    BaseTool* currentTool = editor()->tools()->currentTool();
    Q_ASSERT(currentTool);

    updateUIForTool(currentTool);
}

void ToolOptionWidget::makeConnectionToEditor(Editor* editor)
{
    connect(editor->tools(), &ToolManager::toolChanged, this, &ToolOptionWidget::onToolChanged);
    connect(editor->tools(), &ToolManager::toolsReset, this, &ToolOptionWidget::onToolsReset);
    connect(editor->layers(), &LayerManager::currentLayerChanged, this, &ToolOptionWidget::onLayerChanged);
}

void ToolOptionWidget::setWidgetVisibility(BaseWidget* widget, bool isVisible)
{
    widget->setVisible(isVisible);

    if (isVisible) {
        widget->updateUI();
    }
}

void ToolOptionWidget::updateUIForTool(BaseTool* tool)
{
    setWidgetVisibility(mBucketOptionsWidget, tool->type() == BUCKET);
    setWidgetVisibility(mCameraOptionsWidget, tool->type() == CAMERA);
    setWidgetVisibility(mStrokeOptionsWidget, editor()->tools()->isStrokeTool(tool));
    setWidgetVisibility(mTransformOptionsWidget, editor()->tools()->isTransformTool(tool));
    mResetViewButton->setVisible(tool->type() == ROTATE);
}

void ToolOptionWidget::updateOrientation()
{
    // A row in the top or bottom dock area; a column at a side or floating
    auto* window = qobject_cast<QMainWindow*>(parentWidget());
    const Qt::DockWidgetArea area = window ? window->dockWidgetArea(this) : Qt::NoDockWidgetArea;
    const bool row = !isFloating() && (area == Qt::TopDockWidgetArea || area == Qt::BottomDockWidgetArea);
    for (QBoxLayout* layout : mColumnLayouts)
    {
        layout->setDirection(row ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
        // Tall items such as forms start at the top of the strip, so its first line shows
        for (int i = 0; i < layout->count(); ++i) { layout->itemAt(i)->setAlignment(row ? Qt::AlignTop : Qt::Alignment()); }
        layout->invalidate();
    }
    // Margins above and below would make the strip taller than one control
    for (auto it = mMargins.cbegin(); it != mMargins.cend(); ++it)
    {
        const QMargins m = it.value();
        it.key()->setContentsMargins(m.left(), row ? 0 : m.top(), m.right(), row ? 0 : m.bottom());
    }
    // A grid goes into one line of cells in the strip and back to its designed rows elsewhere
    for (const auto& [grid, cells] : mGrids)
    {
        while (grid->count() > 0) { grid->takeAt(0); }
        for (int i = 0; i < cells.size(); ++i)
        {
            const GridCell& c = cells[i];
            if (row) { grid->addItem(c.item, 0, i, 1, 1, Qt::AlignTop); }
            else { grid->addItem(c.item, c.row, c.column, c.rowSpan, c.columnSpan, c.alignment); }
        }
    }
}

void ToolOptionWidget::onLayerChanged(int layerIndex)
{
    LayerManager* layerManager = editor()->layers();
    Layer* layer = layerManager->getLayer(layerIndex);
    if (layer->type() != layerManager->currentLayer()->type()) {
        return;
    }

    updateUIForTool(editor()->tools()->currentTool());
}

void ToolOptionWidget::onToolChanged(ToolType toolType)
{
    BaseTool* tool = editor()->tools()->getTool(toolType);
    updateUIForTool(tool);
}

void ToolOptionWidget::onToolsReset()
{
    updateUIForTool(editor()->tools()->currentTool());
}
