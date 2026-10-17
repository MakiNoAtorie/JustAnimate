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

#include <algorithm>
#include <QAbstractScrollArea>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QToolBar>
#include <QVBoxLayout>

#include "editor.h"
#include "keyframe.h"
#include "layer.h"
#include "layermanager.h"
#include "objectdata.h"
#include "object.h"
#include "preferencemanager.h"
#include "util.h"
#include "timelinecells.h"

namespace
{
constexpr int kHeader = 22;      // layer names
constexpr int kFrameColumn = 40; // frame numbers
constexpr int kRow = 18;
constexpr int kColumn = 72;
constexpr int kHandle = 3;      // the bottom pixels of an Exposure's last cell
constexpr int kExtraRows = 24;  // empty rows after the scene, to click into
}

QString XSheet::columnName(TimesheetColumn column)
{
    const int c = static_cast<int>(column);
    if (column >= TimesheetColumn::KeyA && column <= TimesheetColumn::KeyF)
    {
        return tr("Key %1").arg(QChar('A' + c - static_cast<int>(TimesheetColumn::KeyA)));
    }
    if (column >= TimesheetColumn::InbetweenA && column <= TimesheetColumn::InbetweenF)
    {
        return tr("In-between %1").arg(QChar('A' + c - static_cast<int>(TimesheetColumn::InbetweenA)));
    }
    if (column == TimesheetColumn::Dialogue) { return tr("Dialogue"); }
    if (column == TimesheetColumn::Camera) { return tr("Camera"); }
    return tr("From layer order");
}

namespace
{
// Timesheet export geometry, as parts of the page: margin, header, memo; 72 rows below a column header of 5 rows.
constexpr qreal kSheetMargin = 0.04;
constexpr qreal kSheetHeader = 0.06;
constexpr qreal kSheetMemo = 0.08;
constexpr qreal kSheetGap = 0.012;
constexpr int kSheetRows = 72;
constexpr int kSheetHeadRows = 5;
constexpr int kSheetColumns = 14; // Key A-F, Dialogue, In-between A-F, Camera
constexpr qreal kFrameUnits = 2;  // the frame mark column is two cell columns wide

QRectF sheetBody(const QRectF& page)
{
    const qreal m = page.width() * kSheetMargin;
    const qreal top = page.top() + m + page.height() * (kSheetHeader + kSheetGap);
    const qreal bottom = page.bottom() - m - page.height() * (kSheetMemo + kSheetGap);
    return QRectF(page.left() + m, top, page.width() - 2 * m, bottom - top);
}

// The full height of a column (-1 the frame marks) in half 0 or 1, column header included.
QRectF sheetColumn(const QRectF& page, int half, int column)
{
    const QRectF body = sheetBody(page);
    const qreal gap = page.width() * kSheetGap;
    const qreal halfWidth = (body.width() - gap) / 2;
    const qreal unit = halfWidth / (kSheetColumns + kFrameUnits);
    const qreal left = body.left() + half * (halfWidth + gap) + (column < 0 ? 0 : (kFrameUnits + column) * unit);
    return QRectF(left, body.top(), column < 0 ? kFrameUnits * unit : unit, body.height());
}

qreal sheetRowHeight(const QRectF& page) { return sheetBody(page).height() / (kSheetRows + kSheetHeadRows); }

void setPixelFont(QPainter& painter, qreal size)
{
    QFont font = painter.font();
    font.setPixelSize(qMax(1, qRound(size)));
    painter.setFont(font);
}

void paintBox(QPainter& painter, const QRectF& cell, const QString& label, const QString& value, qreal labelSize)
{
    painter.drawRect(cell);
    painter.save();
    painter.setClipRect(cell, Qt::IntersectClip);
    const QRectF inner = cell.adjusted(labelSize * 0.4, labelSize * 0.2, -labelSize * 0.4, 0);
    setPixelFont(painter, labelSize);
    painter.drawText(inner, Qt::AlignLeft | Qt::AlignTop, label);
    setPixelFont(painter, labelSize * 1.6);
    painter.drawText(inner.adjusted(0, labelSize * 1.3, 0, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, value);
    painter.restore();
}
} // namespace

int XSheet::timesheetSheetCount(int length)
{
    return qMax(1, (length + 2 * kSheetRows - 1) / (2 * kSheetRows));
}

QRectF XSheet::timesheetCell(const QRectF& page, int column, int row)
{
    const QRectF c = sheetColumn(page, row / kSheetRows, column);
    const qreal h = sheetRowHeight(page);
    return QRectF(c.left(), c.top() + (kSheetHeadRows + row % kSheetRows) * h, c.width(), h);
}

PagePaintFn XSheet::timesheetPages(const Object& object, const TimesheetInfo& info)
{
    return [&object, info](QPainter& painter, const QRectF& page, int index) {
        const qreal rowH = sheetRowHeight(page);
        const qreal thin = qMax(1.0, page.width() / 1500);
        const int firstFrame = index * 2 * kSheetRows + 1;
        painter.save();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, thin));

        // Header: TITLE from the Title block, then the dialog's fields, the scene TIME and SHEET NO.
        const qreal m = page.width() * kSheetMargin;
        const QRectF header(page.left() + m, page.top() + m, page.width() - 2 * m, page.height() * kSheetHeader);
        const QString labels[] = { tr("TITLE"), tr("SCENE"), tr("SHOT"), tr("TIME"), tr("ANIMATOR"), tr("SHEET NO.") };
        const QString values[] = { object.data()->paperTemplate().title, info.scene, info.shot,
                                   formatSecondsFrames(info.length, info.fps), info.animator,
                                   QString("%1 / %2").arg(index + 1).arg(timesheetSheetCount(info.length)) };
        const qreal labelSize = header.height() * 0.2;
        const qreal fieldWidth = header.width() / 6;
        for (int i = 0; i < 6; ++i)
        {
            paintBox(painter, QRectF(header.left() + i * fieldWidth, header.top(), fieldWidth, header.height()), labels[i], values[i], labelSize);
        }
        const qreal memoHeight = page.height() * kSheetMemo;
        paintBox(painter, QRectF(page.left() + m, page.bottom() - m - memoHeight, page.width() - 2 * m, memoHeight), tr("MEMO"), QString(), labelSize);

        // The topmost layer in each column.
        int layerOf[kSheetColumns];
        std::fill(std::begin(layerOf), std::end(layerOf), -1);
        for (int i = object.getLayerCount() - 1; i >= 0; --i)
        {
            const int c = static_cast<int>(effectiveTimesheetColumn(object, i)) - 1;
            if (c >= 0 && c < kSheetColumns && layerOf[c] < 0) { layerOf[c] = i; }
        }

        for (int half = 0; half < 2; ++half)
        {
            const int halfFirst = firstFrame + half * kSheetRows;
            const int halfLast = halfFirst + kSheetRows - 1;
            const int rowBase = half * kSheetRows;
            painter.setPen(QPen(Qt::black, thin));
            for (int column = -1; column < kSheetColumns; ++column)
            {
                const QRectF col = sheetColumn(page, half, column);
                const QRectF head(col.left(), col.top(), col.width(), kSheetHeadRows * rowH);
                painter.drawRect(col);
                painter.drawLine(head.bottomLeft(), head.bottomRight());
                if (column < 0) { continue; }
                // Column names run bottom to top so "In-between A" fits a narrow column.
                painter.save();
                painter.translate(head.bottomLeft());
                painter.rotate(-90);
                setPixelFont(painter, qMin(rowH * 0.8, col.width() * 0.6));
                painter.drawText(QRectF(rowH * 0.2, 0, head.height() - rowH * 0.4, head.width()), Qt::AlignLeft | Qt::AlignVCenter,
                                 columnName(static_cast<TimesheetColumn>(column + 1)));
                painter.restore();
            }

            setPixelFont(painter, rowH * 0.7);
            for (int r = 0; r < kSheetRows; ++r)
            {
                const int frame = halfFirst + r;
                const QRectF marks = timesheetCell(page, -1, rowBase + r);
                // A thicker line ends each second.
                const bool second = info.fps > 0 && frame % info.fps == 0;
                painter.setPen(QPen(Qt::black, second ? thin * 2.5 : thin * 0.5));
                painter.drawLine(marks.bottomLeft(), timesheetCell(page, kSheetColumns - 1, rowBase + r).bottomRight());
                painter.setPen(QPen(Qt::black, thin));
                painter.drawText(marks.adjusted(0, 0, -rowH * 0.3, 0), Qt::AlignRight | Qt::AlignVCenter, formatSecondsFrames(frame, info.fps));
            }

            // Each Drawing: its number in its first cell and an Exposure line to its last.
            painter.setPen(QPen(Qt::black, thin * 1.5));
            for (int column = 0; column < kSheetColumns; ++column)
            {
                if (layerOf[column] < 0) { continue; }
                for (const Run& run : runs(object.getLayer(layerOf[column]), info.length))
                {
                    if (run.last < halfFirst || run.first > halfLast) { continue; }
                    const bool starts = run.first >= halfFirst;
                    const QRectF first = timesheetCell(page, column, rowBase + qMax(run.first, halfFirst) - halfFirst);
                    const QRectF end = timesheetCell(page, column, rowBase + qMin(run.last, halfLast) - halfFirst);
                    if (starts) { painter.drawText(first, Qt::AlignCenter, QString::number(run.number)); }
                    const qreal top = starts ? first.bottom() : first.top();
                    if (end.bottom() - thin > top) { painter.drawLine(QPointF(first.center().x(), top), QPointF(first.center().x(), end.bottom() - thin)); }
                }
            }
        }
        painter.restore();
    };
}

QList<XSheet::Run> XSheet::runs(const Layer* layer, int sceneLength)
{
    QList<Run> result;
    if (layer->keyFrameCount() == 0) { return result; }
    for (int pos = layer->firstKeyFramePosition();;)
    {
        const KeyFrame* key = layer->getKeyFrameAt(pos);
        const int next = layer->getNextKeyFramePosition(pos);
        const bool last = next <= pos;
        const int end = layer->type() == Layer::SOUND ? pos + key->length() - 1 : last ? qMax(pos, sceneLength) : next - 1;
        result.append({ pos, end, static_cast<int>(result.size()) + 1, key->mark(), layer->isLinked(key) });
        if (last) { return result; }
        pos = next;
    }
}

class XSheetCells : public QAbstractScrollArea
{
public:
    XSheetCells(Editor* editor, QWidget* parent) : QAbstractScrollArea(parent), mEditor(editor)
    {
        viewport()->setMouseTracking(true);
        verticalScrollBar()->setSingleStep(kRow);
        horizontalScrollBar()->setSingleStep(kColumn / 4);
    }

    /// Layer indices left to right: the timeline's rows top to bottom, without folded group members.
    /// The Timesheet layout orders them by column, Keyframe A first.
    QList<int> columns() const
    {
        QList<int> result;
        const Object* object = mEditor->object();
        for (int i = object->getLayerCount() - 1; i >= 0; --i)
        {
            if (!object->isLayerFolded(i)) { result << i; }
        }
        if (isTimesheet())
        {
            std::stable_sort(result.begin(), result.end(), [object](int a, int b) {
                return effectiveTimesheetColumn(*object, a) < effectiveTimesheetColumn(*object, b); });
        }
        return result;
    }

    bool isTimesheet() const { return mEditor->preference()->getInt(SETTING::XSHEET_LAYOUT) == 1; }

    QRect cellRect(int layerIndex, int frame) const
    {
        return QRect(kFrameColumn + columns().indexOf(layerIndex) * kColumn - horizontalScrollBar()->value(),
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
        const int layerIndex = layerAt(event->pos());
        if (event->button() != Qt::LeftButton || layerIndex < 0) { return; }
        mEditor->layers()->setCurrentLayer(layerIndex);
        mExposureEnd = event->modifiers() == Qt::NoModifier ? exposureEndAt(event->pos()) : 0;
        mDragTo = mExposureEnd;
        const Layer* layer = mEditor->object()->getLayer(layerIndex);
        const int frame = frameAt(event->pos().y());
        const bool link = event->modifiers() == Qt::AltModifier && layer->isPaintable() && layer->getLastKeyFrameAtPosition(frame);
        mLinkFrom = link ? frame : 0;
        mScrubbing = mExposureEnd == 0 && mLinkFrom == 0;
        if (mScrubbing) { mEditor->scrubTo(frameAt(event->pos().y())); }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int frame = frameAt(event->pos().y());
        if (mExposureEnd > 0)
        {
            mDragTo = frame;
            viewport()->update();
        }
        else if (mScrubbing) { mEditor->scrubTo(frame); }
        else if (exposureEndAt(event->pos()) > 0) { viewport()->setCursor(Qt::SizeVerCursor); }
        else { viewport()->unsetCursor(); }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (mExposureEnd > 0) { mEditor->layers()->changeExposure(mExposureEnd, mDragTo - mExposureEnd); }
        if (mLinkFrom > 0) { mEditor->layers()->linkDrawing(mLinkFrom, frameAt(event->pos().y())); }
        mExposureEnd = 0;
        mLinkFrom = 0;
        mScrubbing = false;
        viewport()->update();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(viewport());
        const QPalette& pal = palette();
        const QRect area = viewport()->rect();
        const QList<int> cols = columns();
        const Object* object = mEditor->object();
        const int currentLayer = mEditor->layers()->currentLayerIndex();
        const int sceneLength = mEditor->layers()->animationLength();
        const int firstFrame = verticalScrollBar()->value() / kRow + 1;
        const int lastFrame = firstFrame + area.height() / kRow + 1;
        painter.fillRect(area, pal.color(QPalette::Base));

        const int column = cols.indexOf(currentLayer);
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

        for (int layerIndex : cols)
        {
            for (const XSheet::Run& run : XSheet::runs(object->getLayer(layerIndex), sceneLength))
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

        if (mExposureEnd > 0 && column >= 0)
        {
            const KeyFrame* key = object->getLayer(currentLayer)->getLastKeyFrameAtPosition(mExposureEnd);
            const QRect end = cellRect(currentLayer, qMax(key ? key->pos() : 1, mDragTo));
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
        for (int layerIndex : cols)
        {
            const QRect name = cellRect(layerIndex, 1).adjusted(4, -kHeader, -4, -kRow);
            QFont font = painter.font();
            font.setBold(layerIndex == currentLayer);
            painter.setFont(font);
            QString text = object->getLayer(layerIndex)->name();
            if (isTimesheet()) { text = XSheet::columnName(effectiveTimesheetColumn(*object, layerIndex)) + " " + text; }
            painter.drawText(name, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(font).elidedText(text, Qt::ElideRight, name.width()));
        }
    }

private:
    int rowCount() const { return qMax(mEditor->layers()->animationLength(), mEditor->currentFrame()) + kExtraRows; }

    int frameAt(int y) const { return qMax(1, (y - kHeader + verticalScrollBar()->value()) / kRow + 1); }

    /// The layer whose column holds pos, or -1 outside the cells.
    int layerAt(const QPoint& pos) const
    {
        const QList<int> cols = columns();
        const int column = (pos.x() - kFrameColumn + horizontalScrollBar()->value()) / kColumn;
        if (pos.x() < kFrameColumn || pos.y() < kHeader || column >= cols.size()) { return -1; }
        return cols[column];
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
    int mExposureEnd = 0; // the frame whose bottom edge is being dragged, or 0
    int mDragTo = 0;
    int mLinkFrom = 0; // the frame of the Drawing being Alt-dragged to link it, or 0
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
    timesheet->setChecked(editor()->preference()->getInt(SETTING::XSHEET_LAYOUT) == 1);
    connect(timesheet, &QAction::toggled, this, [this](bool on) { editor()->preference()->set(SETTING::XSHEET_LAYOUT, on ? 1 : 0); });
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
        timesheet->setChecked(editor()->preference()->getInt(SETTING::XSHEET_LAYOUT) == 1);
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

QList<int> XSheet::columns() const
{
    return mCells->columns();
}
