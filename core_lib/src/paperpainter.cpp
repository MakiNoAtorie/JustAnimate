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
#include "paperpainter.h"

#include <algorithm>
#include <QCoreApplication>
#include <QPainter>
#include <QRectF>

#include "bitmapimage.h"
#include "layer.h"
#include "layercamera.h"
#include "object.h"
#include "objectdata.h"
#include "util.h"
#include "vectorimage.h"

namespace
{
// Sheet margin and Title block height, as parts of the camera frame height.
constexpr qreal kMargin = 0.06;
constexpr qreal kBlock = 0.16;

void setPixelFont(QPainter& painter, qreal size)
{
    QFont font = painter.font();
    font.setPixelSize(qMax(1, qRound(size)));
    painter.setFont(font);
}

// Text in a cell at pixelSize, wrapped and clipped to the cell.
void drawCellText(QPainter& painter, const QRectF& cell, const QString& text, qreal pixelSize, Qt::Alignment align)
{
    painter.save();
    setPixelFont(painter, pixelSize);
    const QRectF inner = cell.adjusted(pixelSize * 0.4, pixelSize * 0.2, -pixelSize * 0.4, -pixelSize * 0.2);
    painter.setClipRect(inner, Qt::IntersectClip);
    painter.drawText(inner, align | Qt::TextWordWrap, text);
    painter.restore();
}

// A boxed field: its label small at the top left, its value below it.
void paintField(QPainter& painter, const QRectF& cell, const QString& label, const QString& value, qreal labelSize, qreal valueSize)
{
    painter.drawRect(cell);
    drawCellText(painter, cell, label, labelSize, Qt::AlignLeft | Qt::AlignTop);
    drawCellText(painter, cell.adjusted(0, labelSize * 1.2, 0, 0), value, valueSize, Qt::AlignLeft | Qt::AlignTop);
}
} // namespace

QRectF paperSheetRect(const QRectF& cameraFrame)
{
    const qreal margin = cameraFrame.height() * kMargin;
    return cameraFrame.adjusted(-margin, -2 * margin - cameraFrame.height() * kBlock, margin, margin);
}

void paintPaperTemplate(QPainter& painter, const QRectF& cameraFrame, const PaperTemplate& paper)
{
    if (!paper.on || cameraFrame.isEmpty()) { return; }

    // Sizes scale with the frame, so the canvas and every page DPI draw the same sheet.
    const QRectF& f = cameraFrame;
    const qreal margin = f.height() * kMargin;
    const qreal blockHeight = f.height() * kBlock;
    const QRectF block(f.left(), f.top() - margin - blockHeight, f.width(), blockHeight);

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRect(paperSheetRect(f));
    painter.setPen(QPen(Qt::black, f.height() * 0.004));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(f);

    // Top row: TITLE takes two units, ID, NAME, STATE and DATE one each. NOTES spans the bottom row and holds three lines.
    const qreal unit = block.width() / 6;
    const qreal topHeight = block.height() * 0.35;
    const qreal labelSize = block.height() * 0.1;
    paintField(painter, QRectF(block.left(), block.top(), 2 * unit, topHeight),
               QCoreApplication::translate("PaperTemplate", "TITLE"), paper.title, labelSize, labelSize * 1.6);
    const QString labels[] = { QCoreApplication::translate("PaperTemplate", "ID"), QCoreApplication::translate("PaperTemplate", "NAME"),
                               QCoreApplication::translate("PaperTemplate", "STATE"), QCoreApplication::translate("PaperTemplate", "DATE") };
    const QString values[] = { paper.id, paper.name, paper.state, paper.date };
    for (int i = 0; i < 4; ++i)
    {
        paintField(painter, QRectF(block.left() + (2 + i) * unit, block.top(), unit, topHeight), labels[i], values[i], labelSize, labelSize * 1.6);
    }
    paintField(painter, QRectF(block.left(), block.top() + topHeight, block.width(), block.height() - topHeight),
               QCoreApplication::translate("PaperTemplate", "NOTES"), paper.notes, labelSize, labelSize * 1.2);
    painter.restore();
}

QRectF paperFrameOnPage(const QRectF& page, const QSizeF& cameraSize)
{
    if (cameraSize.isEmpty()) { return QRectF(); }
    // The sheet is the frame plus a margin on each side, and above it the Title block between two margins.
    const qreal w = cameraSize.width(), h = cameraSize.height();
    const QSizeF sheet(w + 2 * kMargin * h, h * (1 + 3 * kMargin + kBlock));
    const QRectF avail = page.adjusted(page.height() * 0.03, page.height() * 0.03, -page.height() * 0.03, -page.height() * 0.03);
    const qreal s = qMin(avail.width() / sheet.width(), avail.height() / sheet.height());
    const QPointF sheetTopLeft = avail.center() - QPointF(sheet.width() * s / 2, sheet.height() * s / 2);
    return QRectF(sheetTopLeft + QPointF(kMargin * h * s, (2 * kMargin + kBlock) * h * s), QSizeF(w * s, h * s));
}

PagePaintFn paperFramePages(const Object& object, const LayerCamera& camera, int firstFrame)
{
    return [&object, &camera, firstFrame](QPainter& painter, const QRectF& page, int index) {
        const int frame = firstFrame + index;
        const QRectF f = paperFrameOnPage(page, camera.getViewSize());
        PaperTemplate paper = object.data()->paperTemplate();
        paper.on = true; // the export is the sheet, whether the canvas shows it or not
        paintPaperTemplate(painter, f, paper);
        const QRect target = f.toRect();
        painter.drawImage(target, object.renderFrame(frame, camera.getViewAtFrame(frame), camera.getViewSize(), target.size(), true, true));
    };
}

namespace
{
// Storyboard page layout, as parts of the page height. Columns as parts of the table width: S, C, PICTURE, NOTES, AUDIO, TIME.
constexpr qreal kSbMargin = 0.04;
constexpr qreal kSbHeader = 0.04;
constexpr qreal kSbColumnHeader = 0.025;
constexpr qreal kSbFooter = 0.03;
constexpr int kSbRows = 5;
constexpr qreal kSbColumns[] = { 0.06, 0.06, 0.40, 0.24, 0.14, 0.10 };

QRectF storyboardColumn(const QRectF& page, int column, qreal top, qreal height)
{
    const qreal m = page.height() * kSbMargin;
    const qreal width = page.width() - 2 * m;
    qreal left = page.left() + m;
    for (int i = 0; i < column; ++i) { left += kSbColumns[i] * width; }
    return QRectF(left, top, kSbColumns[column] * width, height);
}
} // namespace

int storyboardPageCount(int panels)
{
    return (panels + kSbRows - 1) / kSbRows;
}

QRectF storyboardCell(const QRectF& page, int row, int column)
{
    const qreal h = page.height();
    const qreal tableTop = page.top() + h * (kSbMargin + kSbHeader + kSbColumnHeader);
    const qreal rowHeight = h * (1 - 2 * kSbMargin - kSbHeader - kSbColumnHeader - kSbFooter) / kSbRows;
    return storyboardColumn(page, column, tableTop + row * rowHeight, rowHeight);
}

QRectF storyboardPicture(const QRectF& cell, const QSizeF& cameraSize)
{
    if (cameraSize.isEmpty()) { return QRectF(); }
    const QRectF avail = cell.adjusted(cell.height() * 0.05, cell.height() * 0.05, -cell.height() * 0.05, -cell.height() * 0.05);
    const QSizeF size = cameraSize.scaled(avail.size(), Qt::KeepAspectRatio);
    return QRectF(avail.center() - QPointF(size.width() / 2, size.height() / 2), size);
}

PagePaintFn storyboardPages(const Object& object, const Layer& board, const LayerCamera& camera,
                            const QList<QPair<int, int>>& panels, int fps)
{
    return [&object, &board, &camera, panels, fps](QPainter& painter, const QRectF& page, int index) {
        const qreal h = page.height();
        const qreal text = h * 0.011;
        painter.save();
        painter.setPen(QPen(Qt::black, h * 0.0008));
        painter.setBrush(Qt::NoBrush);

        const QRectF first = storyboardCell(page, 0, 0);
        const QRectF header(first.left(), page.top() + h * kSbMargin, storyboardCell(page, 0, 5).right() - first.left(), h * kSbHeader);
        drawCellText(painter, header, QCoreApplication::translate("Storyboard", "PRODUCTION: %1").arg(object.data()->paperTemplate().title),
                     text * 1.4, Qt::AlignLeft | Qt::AlignVCenter);

        const QString labels[] = { QCoreApplication::translate("Storyboard", "S"), QCoreApplication::translate("Storyboard", "C"),
                                   QCoreApplication::translate("Storyboard", "PICTURE"), QCoreApplication::translate("Storyboard", "NOTES"),
                                   QCoreApplication::translate("Storyboard", "AUDIO"), QCoreApplication::translate("Storyboard", "TIME") };
        for (int column = 0; column < 6; ++column)
        {
            const QRectF cell = storyboardColumn(page, column, first.top() - h * kSbColumnHeader, h * kSbColumnHeader);
            painter.drawRect(cell);
            drawCellText(painter, cell, labels[column], text, Qt::AlignHCenter | Qt::AlignVCenter);
        }

        for (int row = 0; row < kSbRows; ++row)
        {
            for (int column = 0; column < 6; ++column) { painter.drawRect(storyboardCell(page, row, column)); }
            const int panel = index * kSbRows + row;
            if (panel >= panels.size()) { continue; }
            const int frame = panels[panel].first;
            KeyFrame* key = board.getKeyFrameAt(frame);
            const PanelFields f = key ? key->panelFields() : PanelFields();
            const QString values[] = { f.scene, f.cut, QString(), f.notes, f.audio,
                                       formatSecondsFrames(panels[panel].second - frame + 1, fps) };
            for (int column = 0; column < 6; ++column)
            {
                drawCellText(painter, storyboardCell(page, row, column), values[column], text, Qt::AlignLeft | Qt::AlignTop);
            }

            // The panel's Drawing alone, through the camera at the panel's first frame.
            const QRectF picture = storyboardPicture(storyboardCell(page, row, 2), camera.getViewSize());
            const QRect target = picture.toRect();
            if (target.isEmpty()) { continue; }
            QImage image(target.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);
            {
                QPainter p(&image);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::SmoothPixmapTransform);
                const QSize view = camera.getViewSize();
                p.setWorldTransform(camera.getViewAtFrame(frame) * QTransform::fromTranslate(view.width() / 2.0, view.height() / 2.0));
                p.setWindow(QRect(QPoint(), view));
                if (board.type() == Layer::BITMAP && key) { static_cast<BitmapImage*>(key)->paintImage(p); }
                if (board.type() == Layer::VECTOR && key) { static_cast<VectorImage*>(key)->paintImage(p, object, false, false, true); }
            }
            painter.drawImage(target, image);
            painter.drawRect(target);
        }

        const QRectF footer(first.left(), storyboardCell(page, kSbRows - 1, 0).bottom(), header.width(), h * kSbFooter);
        drawCellText(painter, footer,
                     QCoreApplication::translate("Storyboard", "PAGE %1 / %2").arg(index + 1).arg(storyboardPageCount(panels.size())),
                     text, Qt::AlignRight | Qt::AlignVCenter);
        painter.restore();
    };
}

namespace
{
// Timesheet page geometry, as parts of the page: margin, header, memo; 72 rows below a column header of 5 rows.
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
} // namespace

int timesheetSheetCount(int length)
{
    return qMax(1, (length + 2 * kSheetRows - 1) / (2 * kSheetRows));
}

QRectF timesheetCell(const QRectF& page, int column, int row)
{
    const QRectF c = sheetColumn(page, row / kSheetRows, column);
    const qreal h = sheetRowHeight(page);
    return QRectF(c.left(), c.top() + (kSheetHeadRows + row % kSheetRows) * h, c.width(), h);
}

PagePaintFn timesheetPages(const Object& object, const TimesheetInfo& info)
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
        const QString labels[] = { QCoreApplication::translate("Timesheet", "TITLE"), QCoreApplication::translate("Timesheet", "SCENE"),
                                   QCoreApplication::translate("Timesheet", "SHOT"), QCoreApplication::translate("Timesheet", "TIME"),
                                   QCoreApplication::translate("Timesheet", "ANIMATOR"), QCoreApplication::translate("Timesheet", "SHEET NO.") };
        const QString values[] = { object.data()->paperTemplate().title, info.scene, info.shot,
                                   formatSecondsFrames(info.length, info.fps), info.animator,
                                   QString("%1 / %2").arg(index + 1).arg(timesheetSheetCount(info.length)) };
        const qreal labelSize = header.height() * 0.2;
        const qreal fieldWidth = header.width() / 6;
        for (int i = 0; i < 6; ++i)
        {
            paintField(painter, QRectF(header.left() + i * fieldWidth, header.top(), fieldWidth, header.height()), labels[i], values[i],
                       labelSize, labelSize * 1.6);
        }
        const qreal memoHeight = page.height() * kSheetMemo;
        paintField(painter, QRectF(page.left() + m, page.bottom() - m - memoHeight, page.width() - 2 * m, memoHeight),
                   QCoreApplication::translate("Timesheet", "MEMO"), QString(), labelSize, labelSize);

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
                                 Layer::timesheetColumnName(static_cast<TimesheetColumn>(column + 1)));
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
                for (const Layer::Run& run : object.getLayer(layerOf[column])->runs(info.length))
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
