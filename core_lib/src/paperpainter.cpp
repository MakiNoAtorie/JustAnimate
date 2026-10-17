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

#include "objectdata.h"

namespace
{
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
    const qreal margin = f.height() * 0.06;
    const qreal blockHeight = f.height() * 0.16;
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
