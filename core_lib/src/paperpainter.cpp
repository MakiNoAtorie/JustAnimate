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

void paintCell(QPainter& painter, const QRectF& cell, const QString& label, const QString& value, qreal labelSize)
{
    painter.drawRect(cell);
    painter.save();
    painter.setClipRect(cell, Qt::IntersectClip); // long values stay inside their cell
    const QRectF inner = cell.adjusted(labelSize * 0.4, labelSize * 0.2, -labelSize * 0.4, -labelSize * 0.2);
    QFont font = painter.font();
    font.setPixelSize(qMax(1, qRound(labelSize)));
    painter.setFont(font);
    painter.drawText(inner, Qt::AlignLeft | Qt::AlignTop, label);
    font.setPixelSize(qMax(1, qRound(labelSize * 1.6)));
    painter.setFont(font);
    painter.drawText(inner.adjusted(0, labelSize * 1.3, 0, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, value);
    painter.restore();
}
} // namespace

void paintPaperTemplate(QPainter& painter, const QRectF& cameraFrame, const PaperTemplate& paper)
{
    if (!paper.on || cameraFrame.isEmpty()) { return; }

    // Sizes scale with the frame, so the canvas and every page DPI draw the same sheet.
    const QRectF& f = cameraFrame;
    const qreal margin = f.height() * kMargin;
    const qreal blockHeight = f.height() * kBlock;
    const QRectF block(f.left(), f.top() - margin - blockHeight, f.width(), blockHeight);
    const QRectF sheet = f.adjusted(-margin, -2 * margin - blockHeight, margin, margin);

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRect(sheet);
    painter.setPen(QPen(Qt::black, f.height() * 0.004));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(f);

    // Top row: TITLE takes two units, ID, NAME, STATE and DATE one each. NOTES spans the bottom row.
    const qreal unit = block.width() / 6;
    const qreal topHeight = block.height() * 0.55;
    const qreal labelSize = topHeight * 0.2;
    auto tr = [](const char* text) { return QCoreApplication::translate("PaperTemplate", text); };
    paintCell(painter, QRectF(block.left(), block.top(), 2 * unit, topHeight), tr("TITLE"), paper.title, labelSize);
    const QString labels[] = { tr("ID"), tr("NAME"), tr("STATE"), tr("DATE") };
    const QString values[] = { paper.id, paper.name, paper.state, paper.date };
    for (int i = 0; i < 4; ++i)
    {
        paintCell(painter, QRectF(block.left() + (2 + i) * unit, block.top(), unit, topHeight), labels[i], values[i], labelSize);
    }
    paintCell(painter, QRectF(block.left(), block.top() + topHeight, block.width(), block.height() - topHeight),
              tr("NOTES"), paper.notes, labelSize);
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

void drawCellText(QPainter& painter, const QRectF& cell, const QString& text, qreal pixelSize, Qt::Alignment align)
{
    painter.save();
    QFont font = painter.font();
    font.setPixelSize(qMax(1, qRound(pixelSize)));
    painter.setFont(font);
    const QRectF inner = cell.adjusted(pixelSize * 0.4, pixelSize * 0.3, -pixelSize * 0.4, -pixelSize * 0.3);
    painter.setClipRect(inner, Qt::IntersectClip); // long values stay inside their cell
    painter.drawText(inner, align | Qt::TextWordWrap, text);
    painter.restore();
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
        auto tr = [](const char* text) { return QCoreApplication::translate("Storyboard", text); };
        const qreal h = page.height();
        const qreal text = h * 0.011;
        painter.save();
        painter.setPen(QPen(Qt::black, h * 0.0008));
        painter.setBrush(Qt::NoBrush);

        const QRectF first = storyboardCell(page, 0, 0);
        const QRectF header(first.left(), page.top() + h * kSbMargin, storyboardCell(page, 0, 5).right() - first.left(), h * kSbHeader);
        drawCellText(painter, header, tr("PRODUCTION: %1").arg(object.data()->paperTemplate().title), text * 1.4, Qt::AlignLeft | Qt::AlignVCenter);

        const QString labels[] = { tr("S"), tr("C"), tr("PICTURE"), tr("NOTES"), tr("AUDIO"), tr("TIME") };
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
        drawCellText(painter, footer, tr("PAGE %1 / %2").arg(index + 1).arg(storyboardPageCount(panels.size())), text,
                     Qt::AlignRight | Qt::AlignVCenter);
        painter.restore();
    };
}
