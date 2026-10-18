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
#include "xsheet.h"

#include <QAbstractScrollArea>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QToolBar>
#include <QVBoxLayout>

#include "cellmenus.h"
#include "editor.h"
#include "frameeditgesture.h"
#include "keyframe.h"
#include "layer.h"
#include "layermanager.h"
#include "object.h"
#include "preferencemanager.h"
#include "timelinecells.h"

namespace
{
constexpr int kHeader = 22;      // layer names
constexpr int kFrameColumn = 40; // frame numbers
constexpr int kRow = 18;
constexpr int kColumn = 72;
constexpr int kHandle = 3;      // the bottom pixels of an Exposure's last cell
constexpr int kExtraRows = 24;  // empty rows after the scene, to click into

bool isTimesheetLayout(Editor* editor) { return editor->preference()->get<SETTING::XSHEET_LAYOUT>() == 1; }
}


class XSheetCells : public QAbstractScrollArea
{
public:
    XSheetCells(Editor* editor, QWidget* parent) : QAbstractScrollArea(parent), mEditor(editor), mGesture(editor)
    {
        viewport()->setMouseTracking(true);
        verticalScrollBar()->setSingleStep(kRow);
        horizontalScrollBar()->setSingleStep(kColumn / 4);
    }

    struct Column
    {
        int layer; // -1: a Timesheet column no layer fills
        TimesheetColumn sheet;
    };

    /// The columns left to right, without folded group members. Western: the timeline's rows top to bottom.
    /// Timesheet: every fixed column Key A to Camera, each with its layers bottom-up or empty.
    QList<Column> columns() const
    {
        QList<Column> result;
        const Object* object = mEditor->object();
        if (!isTimesheet())
        {
            for (int i = object->getLayerCount() - 1; i >= 0; --i)
            {
                if (!object->isLayerFolded(i)) { result << Column{ i, TimesheetColumn::Default }; }
            }
            return result;
        }
        for (int c = static_cast<int>(TimesheetColumn::KeyA); c <= static_cast<int>(TimesheetColumn::Camera); ++c)
        {
            const TimesheetColumn sheet = static_cast<TimesheetColumn>(c);
            const int before = result.size();
            for (int i = 0; i < object->getLayerCount(); ++i)
            {
                if (!object->isLayerFolded(i) && effectiveTimesheetColumn(*object, i) == sheet) { result << Column{ i, sheet }; }
            }
            if (result.size() == before) { result << Column{ -1, sheet }; }
        }
        return result;
    }

    static int indexOf(const QList<Column>& cols, int layerIndex)
    {
        for (int i = 0; i < cols.size(); ++i)
        {
            if (cols[i].layer == layerIndex) { return i; }
        }
        return -1;
    }

    bool isTimesheet() const { return isTimesheetLayout(mEditor); }

    QRect cellRect(int layerIndex, int frame) const
    {
        return QRect(kFrameColumn + indexOf(columns(), layerIndex) * kColumn - horizontalScrollBar()->value(),
                     kHeader + (frame - 1) * kRow - verticalScrollBar()->value(), kColumn, kRow);
    }

    void updateScrollBars()
    {
        const int width = kFrameColumn + columns().size() * kColumn;
        const int height = kHeader + rowCount() * kRow;
        horizontalScrollBar()->setRange(0, qMax(0, width - viewport()->width()));
        horizontalScrollBar()->setPageStep(viewport()->width());
        verticalScrollBar()->setRange(0, qMax(0, height - viewport()->height()));
        verticalScrollBar()->setPageStep(viewport()->height());
        viewport()->update();
    }

    void showFrame(int frame)
    {
        updateScrollBars();
        const int top = kHeader + (frame - 1) * kRow - verticalScrollBar()->value();
        if (top < kHeader) { verticalScrollBar()->setValue(verticalScrollBar()->value() - (kHeader - top)); }
        else if (top + kRow > viewport()->height()) { verticalScrollBar()->setValue(verticalScrollBar()->value() + top + kRow - viewport()->height()); }
    }

protected:
    void resizeEvent(QResizeEvent*) override { updateScrollBars(); }
    void scrollContentsBy(int, int) override { viewport()->update(); }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::RightButton)
        {
            showMenu(event->pos(), event->globalPosition().toPoint());
            return;
        }
        const int layerIndex = layerAt(event->pos());
        if (event->button() != Qt::LeftButton || layerIndex < 0) { return; }
        mEditor->layers()->setCurrentLayer(layerIndex);
        const int exposureEnd = event->modifiers() == Qt::NoModifier ? exposureEndAt(event->pos()) : 0;
        const Layer* layer = mEditor->object()->getLayer(layerIndex);
        const int frame = frameAt(event->pos().y());
        const bool link = event->modifiers() == Qt::AltModifier && layer->isPaintable() && layer->getLastKeyFrameAtPosition(frame);
        if (exposureEnd > 0) { mGesture.press(layerIndex, exposureEnd, FrameHit::ExposureEdge, event->modifiers()); }
        else if (link) { mGesture.press(layerIndex, frame, FrameHit::Link, event->modifiers()); }
        else
        {
            mScrubbing = true;
            mEditor->scrubTo(frame);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int frame = frameAt(event->pos().y());
        if (mGesture.hit() != FrameHit::Empty)
        {
            mGesture.drag(layerAt(event->pos()), frame);
            viewport()->update();
        }
        else if (mScrubbing) { mEditor->scrubTo(frame); }
        else if (exposureEndAt(event->pos()) > 0) { viewport()->setCursor(Qt::SizeVerCursor); }
        else { viewport()->unsetCursor(); }
    }

    void mouseReleaseEvent(QMouseEvent*) override
    {
        mGesture.release();
        mScrubbing = false;
        viewport()->update();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(viewport());
        const QPalette& pal = palette();
        const QRect area = viewport()->rect();
        const QList<Column> cols = columns();
        const Object* object = mEditor->object();
        const int currentLayer = mEditor->layers()->currentLayerIndex();
        const int sceneLength = mEditor->layers()->animationLength();
        const int firstFrame = verticalScrollBar()->value() / kRow + 1;
        const int lastFrame = firstFrame + area.height() / kRow + 1;
        painter.fillRect(area, pal.color(QPalette::Base));

        const int column = indexOf(cols, currentLayer);
        if (column >= 0)
        {
            painter.fillRect(cellRect(currentLayer, 1).x(), 0, kColumn, area.height(), pal.color(QPalette::AlternateBase));
        }
        QColor highlight = pal.color(QPalette::Highlight);
        highlight.setAlpha(70);
        painter.fillRect(0, cellRect(currentLayer, mEditor->currentFrame()).y(), area.width(), kRow, highlight);

        painter.setPen(pal.color(QPalette::Midlight));
        for (int frame = firstFrame; frame <= lastFrame; ++frame)
        {
            const int y = kHeader + frame * kRow - verticalScrollBar()->value() - 1;
            painter.drawLine(0, y, area.width(), y);
        }
        for (int i = 0; i <= cols.size(); ++i)
        {
            const int x = kFrameColumn + i * kColumn - horizontalScrollBar()->value() - 1;
            painter.drawLine(x, 0, x, area.height());
        }

        for (const Column& col : cols)
        {
            if (col.layer < 0) { continue; }
            const int layerIndex = col.layer;
            for (const Layer::Run& run : object->getLayer(layerIndex)->runs(sceneLength))
            {
                if (run.last < firstFrame || run.first > lastFrame) { continue; }
                const QRect cell = cellRect(layerIndex, run.first);
                painter.setPen(pal.color(QPalette::Text));
                painter.drawText(cell.adjusted(4, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QString::number(run.number));
                TimeLineCells::paintMark(painter, run.mark, QRect(cell.right() - kRow, cell.top(), kRow, kRow), pal.color(QPalette::Text));
                if (run.linked) { TimeLineCells::paintLink(painter, cell.adjusted(0, 0, -1, -1), pal.color(QPalette::Highlight)); }
                if (run.last == run.first) { continue; }
                // The Exposure line runs from the first cell to the bottom of the last.
                painter.setPen(QPen(pal.color(QPalette::Text), 2));
                const int x = cell.left() + kColumn / 3;
                painter.drawLine(x, cell.bottom(), x, cellRect(layerIndex, run.last).bottom() - 2);
            }
        }

        if (mGesture.hit() == FrameHit::ExposureEdge && column >= 0)
        {
            const KeyFrame* key = object->getLayer(currentLayer)->getLastKeyFrameAtPosition(mGesture.startFrame());
            const QRect end = cellRect(currentLayer, qMax(key ? key->pos() : 1, mGesture.lastFrame()));
            painter.fillRect(end.left(), end.bottom() - 1, kColumn, 3, pal.color(QPalette::Highlight));
        }

        // Frame numbers stay at the left, layer names at the top.
        painter.fillRect(0, 0, kFrameColumn, area.height(), pal.color(QPalette::Button));
        painter.setPen(pal.color(QPalette::ButtonText));
        for (int frame = firstFrame; frame <= lastFrame; ++frame)
        {
            const QRect row(0, cellRect(currentLayer, frame).y(), kFrameColumn - 4, kRow);
            painter.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QString::number(frame));
        }
        painter.fillRect(0, 0, area.width(), kHeader, pal.color(QPalette::Button));
        for (int i = 0; i < cols.size(); ++i)
        {
            const int layerIndex = cols[i].layer;
            const QRect name(kFrameColumn + i * kColumn - horizontalScrollBar()->value() + 4, 0, kColumn - 8, kHeader);
            QFont font = painter.font();
            font.setBold(layerIndex >= 0 && layerIndex == currentLayer);
            painter.setFont(font);
            QString text = layerIndex >= 0 ? object->getLayer(layerIndex)->name() : QString();
            if (isTimesheet()) { text = (Layer::timesheetColumnName(cols[i].sheet) + " " + text).trimmed(); }
            painter.drawText(name, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(font).elidedText(text, Qt::ElideRight, name.width()));
        }
    }

private:
    int rowCount() const { return qMax(mEditor->layers()->animationLength(), mEditor->currentFrame()) + kExtraRows; }

    int frameAt(int y) const { return qMax(1, (y - kHeader + verticalScrollBar()->value()) / kRow + 1); }

    /// The layer whose column holds x, or -1 outside the columns.
    int columnLayerAt(int x) const
    {
        const QList<Column> cols = columns();
        const int column = (x - kFrameColumn + horizontalScrollBar()->value()) / kColumn;
        if (x < kFrameColumn || column >= cols.size()) { return -1; }
        return cols[column].layer;
    }

    /// The layer whose column holds pos, or -1 outside the cells.
    int layerAt(const QPoint& pos) const { return pos.y() < kHeader ? -1 : columnLayerAt(pos.x()); }

    /// The timeline's layer menu on a column header, its cell menu on a cell.
    void showMenu(const QPoint& pos, const QPoint& globalPos)
    {
        const int layerIndex = columnLayerAt(pos.x());
        if (layerIndex < 0) { return; }
        Layer* layer = mEditor->object()->getLayer(layerIndex);
        if (pos.y() < kHeader)
        {
            mEditor->layers()->setCurrentLayer(layerIndex);
            CellMenus::showLayerMenu(mEditor, layer, globalPos);
        }
        else { CellMenus::showCellMenu(mEditor, layer, frameAt(pos.y()), globalPos); }
        viewport()->update();
    }

    /// The frame whose bottom edge is under pos and ends a Drawing's Exposure, or 0.
    int exposureEndAt(const QPoint& pos) const
    {
        const int layerIndex = layerAt(pos);
        if (layerIndex < 0) { return 0; }
        const Layer* layer = mEditor->object()->getLayer(layerIndex);
        const int frame = frameAt(pos.y());
        if (cellRect(layerIndex, frame).bottom() - pos.y() >= kHandle || !layer->keyExists(frame + 1) ||
            layer->getLastKeyFrameAtPosition(frame) == nullptr) { return 0; }
        return frame;
    }

    Editor* mEditor;
    FrameEditGesture mGesture; // Exposure edge and link drags
    bool mScrubbing = false;
};

XSheet::XSheet(QWidget* parent) : BaseDockWidget(parent)
{
}

void XSheet::initUI()
{
    setWindowTitle(tr("X-sheet", "Subpanel title"));

    QWidget* content = new QWidget(this);
    mToolBar = new QToolBar(content);
    mToolBar->setIconSize(QSize(22, 22));
    QAction* timesheet = mToolBar->addAction(tr("Timesheet"));
    timesheet->setObjectName("actionXSheetTimesheet");
    timesheet->setToolTip(tr("Timesheet layout: Key A-F, Dialogue, In-between A-F, Camera"));
    timesheet->setCheckable(true);
    timesheet->setChecked(isTimesheetLayout(editor()));
    connect(timesheet, &QAction::toggled, this, [this](bool on) { editor()->preference()->set<SETTING::XSHEET_LAYOUT>(on ? 1 : 0); });
    mToolBar->addSeparator();
    mCells = new XSheetCells(editor(), content);
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mToolBar);
    layout->addWidget(mCells);
    setWidget(content);

    Editor* e = editor();
    const auto refresh = [this] { updateUI(); };
    connect(e, &Editor::scrubbed, this, [this](int frame) { mCells->showFrame(frame); });
    connect(e, &Editor::frameModified, this, refresh);
    connect(e, &Editor::framesModified, this, refresh);
    connect(e, &Editor::updateTimeLine, this, refresh);
    connect(e, &Editor::objectLoaded, this, refresh);
    connect(e->layers(), &LayerManager::currentLayerChanged, this, refresh);
    connect(e->layers(), &LayerManager::layerCountChanged, this, refresh);
    connect(e->layers(), &LayerManager::animationLengthChanged, this, refresh);
    connect(e->preference(), &PreferenceManager::optionChanged, this, [this, timesheet](SETTING s) {
        if (s != SETTING::XSHEET_LAYOUT) { return; }
        timesheet->setChecked(isTimesheetLayout(editor()));
        updateUI();
    });
}

void XSheet::updateUI()
{
    mCells->updateScrollBars();
}

void XSheet::setEditActions(const QList<QAction*>& actions)
{
    mToolBar->addActions(actions);
}

QRect XSheet::cellRect(int layerIndex, int frame) const
{
    return mCells->cellRect(layerIndex, frame);
}
