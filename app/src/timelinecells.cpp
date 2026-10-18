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

#include "timelinecells.h"
#include "theme.h"

#include <QApplication>
#include <QResizeEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QLineEdit>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QDebug>

#include "attributes.h"
#include "camerapropertiesdialog.h"
#include "editor.h"
#include "keyframe.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "viewmanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "soundclip.h"
#include "timeline.h"

#include "cellmenus.h"
#include "pencilsettings.h"

namespace
{
// Layer row: visibility dot, onion toggle, type icon, then the name.
constexpr int kOnionToggleLeft = 17;
constexpr int kOnionToggleRight = 35;
// The layer type icon sits between kOnionToggleRight and kLockAlphaLeft.
constexpr int kLockAlphaLeft = 57;
constexpr int kLockAllLeft = 75;
constexpr int kLayerNameLeft = 94;
}

TimeLineCells::TimeLineCells(TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE type) : QWidget(parent), mGesture(editor)
{
    mTimeLine = parent;
    mEditor = editor;
    mPrefs = editor->preference();
    mType = type;

    mFrameLength = mPrefs->get<SETTING::TIMELINE_SIZE>();
    mFontSize = mPrefs->get<SETTING::LABEL_FONT_SIZE>();
    mFrameSize = mPrefs->get<SETTING::FRAME_SIZE>();
    mbShortScrub = mPrefs->get<SETTING::SHORT_SCRUB>();
    mDrawFrameNumber = mPrefs->get<SETTING::DRAW_LABEL>();

    setMinimumSize(500, 4 * mLayerHeight);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    // Tab focus only: a click must leave keyboard focus on the canvas.
    setFocusPolicy(Qt::TabFocus);

    connect(mPrefs, &PreferenceManager::optionChanged, this, &TimeLineCells::loadSetting);
    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        mFlashTimer = new QTimer(this);
        mFlashTimer->setInterval(150);
        connect(mFlashTimer, &QTimer::timeout, this, [this] {
            if (--mFlashTicks <= 0) { mFlashTimer->stop(); }
            update();
        });
        connect(editor, &Editor::layerRefused, this, &TimeLineCells::flashLayer);
    }
}

void TimeLineCells::flashLayer(int layerId)
{
    mFlashLayerId = layerId;
    mFlashTicks = 6;
    mFlashTimer->start();
    update();
}

TimeLineCells::~TimeLineCells()
{
    delete mCache;
}

void TimeLineCells::loadSetting(SETTING setting)
{
    switch (setting)
    {
    case SETTING::TIMELINE_SIZE:
        mFrameLength = mPrefs->get<SETTING::TIMELINE_SIZE>();
        mTimeLine->updateLength();
        break;
    case SETTING::LABEL_FONT_SIZE:
        mFontSize = mPrefs->get<SETTING::LABEL_FONT_SIZE>();
        break;
    case SETTING::FRAME_SIZE:
        mFrameSize = mPrefs->get<SETTING::FRAME_SIZE>();
        mTimeLine->updateLength();
        break;
    case SETTING::SHORT_SCRUB:
        mbShortScrub = mPrefs->get<SETTING::SHORT_SCRUB>();
        break;
    case SETTING::DRAW_LABEL:
        mDrawFrameNumber = mPrefs->get<SETTING::DRAW_LABEL>();
        break;
    default:
        break;
    }

    updateContent();
}

int TimeLineCells::getFrameNumber(int x) const
{
    return mFrameOffset + 1 + (x - mOffsetX) / mFrameSize;
}

int TimeLineCells::getFrameX(int frameNumber) const
{
    return mOffsetX + (frameNumber - mFrameOffset) * mFrameSize;
}

int TimeLineCells::exposureEdgeAt(const QPoint& pos) const
{
    const int layerNumber = getLayerNumber(pos.y());
    if (mType != TIMELINE_CELL_TYPE::Tracks || layerNumber < 0 || layerNumber >= mEditor->object()->getLayerCount()) { return 0; }
    const Layer* layer = mEditor->object()->getLayer(layerNumber);
    const int edge = mFrameOffset + qRound((pos.x() - mOffsetX) / double(mFrameSize));
    // The handle is the last 3 px of an Exposure, which ends where the next Drawing starts.
    if (edge < 1 || getFrameNumber(pos.x()) != edge || getFrameX(edge) - pos.x() > 3 || !layer->keyExists(edge + 1) ||
        layer->getLastKeyFrameAtPosition(edge) == nullptr) { return 0; }
    return edge;
}

void TimeLineCells::setFrameSize(int size)
{
    mFrameSize = size;
    mPrefs->set<SETTING::FRAME_SIZE>(mFrameSize);
    updateContent();
}

int TimeLineCells::getLayerNumber(int y) const
{
    if (y < mOffsetY) { return -1; }

    // Rows show layers in descending order and skip folded group members
    const Object* object = mEditor->object();
    int row = mLayerOffset + (y - mOffsetY) / mLayerHeight;
    for (int i = object->getLayerCount() - 1; i >= 0; --i)
    {
        if (object->isLayerFolded(i)) { continue; }
        if (row-- == 0) { return i; }
    }
    // The row just below the last layer is -1, rows further down are layer 0
    return row == 0 ? -1 : 0;
}

int TimeLineCells::getInbetweenLayerNumber(int y) const {
    int layerNumber = getLayerNumber(y);
    // Round the layer number towards the drag start
    if(layerNumber != mFromLayer) {
        if(mMouseMoveY > 0 && y < getLayerY(layerNumber) + mLayerHeight / 2) {
            layerNumber++;
        }
        else if(mMouseMoveY < 0 && y > getLayerY(layerNumber) + mLayerHeight / 2) {
            layerNumber--;
        }
    }
    return layerNumber;
}

int TimeLineCells::getLayerY(int layerNumber) const
{
    const Object* object = mEditor->object();
    int row = std::min(0, object->getLayerCount() - 1 - layerNumber);
    for (int i = object->getLayerCount() - 1; i > layerNumber; --i)
    {
        if (!object->isLayerFolded(i)) { ++row; }
    }
    return mOffsetY + (row - mLayerOffset) * mLayerHeight;
}

void TimeLineCells::updateFrame(int frameNumber)
{
    int x = getFrameX(frameNumber);
    update(x - mFrameSize, 0, mFrameSize + 1, height());
}

void TimeLineCells::updateContent()
{
    mRedrawContent = true;
    update();
}

bool TimeLineCells::didDetachLayer() const {
    return abs(mMouseMoveY) > mLayerDetachThreshold;
}

void TimeLineCells::showFrameMenu(QPoint pos)
{
    const int frameNumber = getFrameNumber(pos.x());
    Layer* curLayer = mEditor->layers()->currentLayer();
    Q_ASSERT(curLayer);

    // The camera menu highlights its key while it is open.
    mHighlightFrameEnabled = curLayer->type() == Layer::CAMERA && curLayer->keyExists(frameNumber);
    mHighlightedFrame = frameNumber;
    update();
    CellMenus::showCellMenu(mEditor, curLayer, frameNumber, mapToGlobal(pos));
    mHighlightFrameEnabled = false;
    mHighlightedFrame = -1;
    updateContent();
}

void TimeLineCells::drawContent()
{
    if (mCache == nullptr)
    {
        mCache = new QPixmap(size());
        if (mCache->isNull())
        {
            // fail to create cache
            return;
        }
    }

    QPainter painter(mCache);

    // grey background of the view
    const QPalette palette = QApplication::palette();
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Base));
    painter.drawRect(QRect(0, 0, width(), height()));

    const int widgetWidth = width();

    // Draw non-current layers
    const Object* object = mEditor->object();
    Q_ASSERT(object != nullptr);
    for (int i = 0; i < object->getLayerCount(); i++)
    {
        if (i == mEditor->layers()->currentLayerIndex() || object->isLayerFolded(i))
        {
            continue;
        }
        const Layer* layeri = object->getLayer(i);

        if (layeri != nullptr)
        {
            const int layerY = getLayerY(i);
            switch (mType)
            {
            case TIMELINE_CELL_TYPE::Tracks:
                paintTrack(painter, layeri, mOffsetX,
                           layerY, widgetWidth - mOffsetX,
                           mLayerHeight, false, mFrameSize);
                break;

            case TIMELINE_CELL_TYPE::Layers:
                paintLabel(painter, layeri, 0,
                           layerY, widgetWidth - 1,
                           mLayerHeight, false, mEditor->layerVisibility());
                break;
            }
        }
    }

    // Draw current layer
    const Layer* currentLayer = mEditor->layers()->currentLayer();
    if (didDetachLayer())
    {
        int layerYMouseMove = getLayerY(mEditor->layers()->currentLayerIndex()) + mMouseMoveY;
        if (mType == TIMELINE_CELL_TYPE::Tracks)
        {
            paintTrack(painter, currentLayer,
                       mOffsetX, layerYMouseMove,
                       widgetWidth - mOffsetX, mLayerHeight,
                       true, mFrameSize);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers)
        {
            paintLabel(painter, currentLayer,
                       0, layerYMouseMove,
                       widgetWidth - 1, mLayerHeight, true, mEditor->layerVisibility());

            paintLayerGutter(painter);
        }
    }
    else
    {
        if (mType == TIMELINE_CELL_TYPE::Tracks)
        {
            paintTrack(painter,
                       currentLayer,
                       mOffsetX,
                       getLayerY(mEditor->layers()->currentLayerIndex()),
                       widgetWidth - mOffsetX,
                       mLayerHeight,
                       true,
                       mFrameSize);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers)
        {
            paintLabel(painter,
                       currentLayer,
                       0,
                       getLayerY(mEditor->layers()->currentLayerIndex()),
                       widgetWidth - 1,
                       mLayerHeight,
                       true,
                       mEditor->layerVisibility());
        }
    }

    // --- draw track bar background
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::Base));
    painter.drawRect(QRect(0, 0, width() - 1, mOffsetY - 1));

    // --- draw bottom line splitter for track bar
    painter.setPen(palette.color(QPalette::Mid));
    painter.drawLine(0, mOffsetY - 2, width() - 1, mOffsetY - 2);

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        // --- draw circle
        painter.setPen(palette.color(QPalette::Text));
        if (mEditor->layerVisibility() == LayerVisibility::CURRENTONLY)
        {
            painter.setBrush(palette.color(QPalette::Base));
        }
        else if (mEditor->layerVisibility() == LayerVisibility::RELATED)
        {
            QColor color = palette.color(QPalette::Text);
            color.setAlpha(128);
            painter.setBrush(color);
        }
        else if (mEditor->layerVisibility() == LayerVisibility::ALL)
        {
            painter.setBrush(palette.brush(QPalette::Text));
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawEllipse(6, 4, 9, 9);
        painter.setRenderHint(QPainter::Antialiasing, false);
    }
    else if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        paintTicks(painter, palette);

        for (int i = 0; i < object->getLayerCount(); i++) {
            paintSelectedFrames(painter, object->getLayer(i), i);
        }
    }
    mRedrawContent = false;
}

void TimeLineCells::paintTicks(QPainter& painter, const QPalette& palette) const
{
    painter.setPen(palette.color(QPalette::Text));
    painter.setBrush(palette.brush(QPalette::Text));
    int fps = mEditor->playback()->fps();
    for (int i = mFrameOffset; i < mFrameOffset + (width() - mOffsetX) / mFrameSize; i++)
    {
        // line x pos + some offset
        const int lineX = getFrameX(i) + 1;
        if (i + 1 >= mTimeLine->getRangeLower() && i < mTimeLine->getRangeUpper())
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette.color(QPalette::Highlight));

            painter.drawRect(lineX, 1, mFrameSize + 1, 2);

            painter.setPen(palette.color(QPalette::Text));
            painter.setBrush(palette.brush(QPalette::Text));
        }

        // Draw large tick at fps mark
        if (i % fps == 0 || i % fps == fps / 2)
        {
            painter.drawLine(lineX, 1, lineX, 5);
        }
        else // draw small tick
        {
            painter.drawLine(lineX, 1, lineX, 3);
        }
        if (i == 0 || i % fps == fps - 1)
        {
            int incr = (i < 9) ? 4 : 0; // poor man’s text centering
            painter.drawText(QPoint(lineX + incr, 15), QString::number(i + 1));
        }
    }

    // Markers: colour bar under the ruler, and the name right of it when it fits before the next marker.
    const auto& markers = mEditor->object()->data()->markers();
    QFont nameFont = painter.font();
    nameFont.setPixelSize(9);
    const QFontMetrics nameMetrics(nameFont);
    painter.save();
    painter.setFont(nameFont);
    for (auto it = markers.constBegin(); it != markers.constEnd(); ++it)
    {
        const int barX = getFrameX(it.key() - 1);
        painter.fillRect(barX, 16, mFrameSize, 4, it.value().color);

        const auto next = std::next(it);
        const int textX = barX + mFrameSize + 2;
        const int limit = (next == markers.constEnd()) ? width() : getFrameX(next.key() - 1) - 2;
        const int textWidth = nameMetrics.horizontalAdvance(it.value().name);
        if (it.value().name.isEmpty() || textX + textWidth > limit) { continue; }
        const QRect textRect(textX, mOffsetY - 2 - nameMetrics.height(), textWidth, nameMetrics.height());
        painter.fillRect(textRect, palette.color(QPalette::Base));
        painter.setPen(palette.color(QPalette::Text));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, it.value().name);
    }
    painter.restore();
}

void TimeLineCells::paintTrack(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, int frameSize) const
{
    const QPalette palette = QApplication::palette();
    QColor col;
    // Color each track according to the layer type
    if (layer->type() == Layer::BITMAP) col = QColor(51, 155, 252);
    if (layer->type() == Layer::VECTOR) col = QColor(70, 205, 123);
    if (layer->type() == Layer::SOUND) col = QColor(255, 141, 112);
    if (layer->type() == Layer::CAMERA) col = QColor(253, 202, 92);
    // Dim invisible layers
    if (!layer->visible()) col.setAlpha(64);

    painter.save();
    painter.setBrush(col);
    painter.setPen(QPen(QBrush(palette.color(QPalette::Mid)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRect(x, y - 1, width, height);

    if (!layer->visible())
    {
        painter.restore();
        return;
    }

    // Changes the appearance if selected
    if (selected)
    {
        paintSelection(painter, x, y, width, height);
    }
    else
    {
        painter.save();
        QLinearGradient linearGrad(QPointF(0, y), QPointF(0, y + height));
        linearGrad.setColorAt(0, QColor(255,255,255,150));
        linearGrad.setColorAt(1, QColor(0,0,0,0));
        painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        painter.setBrush(linearGrad);
        painter.drawRect(x, y - 1, width, height);
        painter.restore();
    }

    paintFrames(painter, col, layer, y, height, selected, frameSize);

    painter.restore();
}

void TimeLineCells::paintFrames(QPainter& painter, QColor trackCol, const Layer* layer, int y, int height, bool selected, int frameSize) const
{
    painter.setPen(QPen(QBrush(QColor(40, 40, 40)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    int recTop = y + 1;
    int standardWidth = frameSize - 2;

    int recHeight = height - 4;

    const QList<int> selectedFrames = layer->getSelectedFramesByPos();
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        int framePos = key->pos();
        int recWidth = standardWidth;
        int recLeft = getFrameX(framePos) - recWidth;

        const int nextPos = layer->getNextKeyFramePosition(framePos);
        if (key->tween() && nextPos > framePos)
        {
            // Tween marker: a line from this key to the next key.
            painter.save();
            painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
            const int midY = recTop + recHeight / 2;
            painter.drawLine(getFrameX(framePos), midY, getFrameX(nextPos) - standardWidth, midY);
            painter.restore();
        }

        // Selected frames are painted separately
        if (selectedFrames.contains(framePos)) {
            return;
        }

        if (key->length() > 1)
        {
            // This is especially for sound clips.
            // Sound clips are the only type of KeyFrame with variable frame length.
            recWidth = frameSize * key->length();
        }

        // Paint the frame border
        painter.setPen(QPen(QBrush(QColor(40, 40, 40)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        // Paint the frame contents
        if (selected)
        {
            painter.setBrush(QColor(trackCol.red(), trackCol.green(), trackCol.blue(), 150));
        }

        painter.drawRect(recLeft, recTop, recWidth, recHeight);
        paintMark(painter, key->mark(), QRect(recLeft, recTop, standardWidth, recHeight), palette().color(QPalette::Shadow));
        if (layer->isLinked(key)) { paintLink(painter, QRect(recLeft, recTop, standardWidth, recHeight), palette().color(QPalette::Highlight)); }

        if (layer->type() == Layer::SOUND)
        {
            paintWaveform(painter, static_cast<const SoundClip*>(key), recTop, recHeight, frameSize);
        }
    });

    if (layer->type() != Layer::SOUND) { return; }

    // Lip-sync markers: a colour bar at the left of the cell and the phoneme over the waveform.
    painter.save();
    QFont nameFont = painter.font();
    nameFont.setPixelSize(9);
    painter.setFont(nameFont);
    const QMap<int, FrameMarker> markers = mEditor->object()->data()->markers(layer->id());
    for (auto it = markers.constBegin(); it != markers.constEnd(); ++it)
    {
        const int left = getFrameX(it.key()) - frameSize + 1;
        painter.fillRect(left, recTop, 2, recHeight, it.value().color);
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRect(left + 3, recTop, frameSize * 4, recHeight), Qt::AlignLeft | Qt::AlignTop, it.value().name);
    }
    painter.restore();
}

void TimeLineCells::paintWaveform(QPainter& painter, const SoundClip* clip, int recTop, int recHeight, int frameSize) const
{
    const QVector<SoundClip::Peak> peaks = clip->waveformPeaks(mEditor->fps());
    if (peaks.isEmpty()) { return; }

    painter.save();
    painter.setPen(QColor(40, 40, 40, 160));
    painter.setBrush(QColor(40, 40, 40, 110));
    const qreal mid = recTop + recHeight / 2.0;
    const qreal half = recHeight / 2.0 - 1;
    const int firstVisible = qMax(0, getFrameNumber(0) - clip->pos());
    const int lastVisible = qMin<int>(peaks.size() - 1, getFrameNumber(width()) - clip->pos());
    for (int i = firstVisible; i <= lastVisible; i++)
    {
        // Peak i spans the cell of frame pos() + i.
        const int left = getFrameX(clip->pos() + i) - frameSize + 1;
        const qreal top = mid - peaks[i].max * half;
        const qreal bottom = mid - peaks[i].min * half;
        painter.drawRect(QRectF(left, top, frameSize - 2, qMax<qreal>(1, bottom - top)));
    }
    painter.restore();
}

void TimeLineCells::paintCurrentFrameBorder(QPainter &painter, int recLeft, int recTop, int recWidth, int recHeight) const
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(Qt::white);
    painter.drawRect(recLeft, recTop, recWidth, recHeight);
    painter.restore();
}

void TimeLineCells::paintFrameCursorOnCurrentLayer(QPainter &painter, int recTop, int recWidth, int recHeight) const
{
    int recLeft = getFrameX(mFramePosMoveX) - recWidth;

    painter.save();
    const QPalette palette = QApplication::palette();
    // Don't fill
    painter.setBrush(Qt::NoBrush);
    // paint border
    QColor penColor = palette.color(QPalette::WindowText);
    penColor.setAlpha(127);
    painter.setPen(penColor);
    painter.drawRect(recLeft, recTop, recWidth, recHeight);
    painter.restore();
}

void TimeLineCells::paintHighlightedFrame(QPainter& painter, int framePos, int recTop, int recWidth, int recHeight) const
{
    int recLeft = getFrameX(framePos) - recWidth;

    painter.save();
    const QPalette palette = QApplication::palette();
    painter.setBrush(palette.color(QPalette::Window));
    painter.setPen(palette.color(QPalette::WindowText));

    // Draw a rect slighly smaller than the frame
    painter.drawRect(recLeft+1, recTop+1, recWidth-1, recHeight-1);
    painter.restore();
}

void TimeLineCells::paintSelectedFrames(QPainter& painter, const Layer* layer, const int layerIndex) const
{
    int mouseX = mMouseMoveX;
    int posUnderCursor = getFrameNumber(mMousePressX);
    int standardWidth = mFrameSize - 2;
    int recWidth = standardWidth;
    int recHeight = mLayerHeight - 4;
    int recTop = getLayerY(layerIndex) + 1;

    painter.save();
    for (int framePos : layer->getSelectedFramesByPos()) {

        KeyFrame* key = layer->getKeyFrameAt(framePos);
        if (key->length() > 1)
        {
            // This is a special case for sound clip.
            // Sound clip is the only type of KeyFrame that has variable frame length.
            recWidth = mFrameSize * key->length();
        }

        painter.setBrush(QColor(60, 60, 60));
        painter.setPen(QPen(QBrush(QColor(40, 40, 40)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        int frameX = getFrameX(framePos);
        if (mGesture.isMoving()) {
            int offset = (framePos - posUnderCursor) + mFrameOffset;
            int newFrameX = getFrameX(getFrameNumber(getFrameX(offset)+mouseX)) - standardWidth;
            // Paint as frames are hovering
            painter.drawRect(newFrameX, recTop-4, recWidth, recHeight);

        } else {
            int currentFrameX = frameX - standardWidth;
            painter.drawRect(currentFrameX, recTop, recWidth, recHeight);
            paintMark(painter, key->mark(), QRect(currentFrameX, recTop, standardWidth, recHeight),
                      palette().color(QPalette::HighlightedText));
            if (layer->isLinked(key)) { paintLink(painter, QRect(currentFrameX, recTop, standardWidth, recHeight), palette().color(QPalette::Highlight)); }
        }
    }
    painter.restore();
}

void TimeLineCells::paintMark(QPainter& painter, Mark mark, const QRect& cell, const QColor& colour)
{
    if (mark == Mark::None) { return; }
    const qreal r = std::min(cell.width(), cell.height()) * 0.3;
    const QPointF c = QRectF(cell).center();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    switch (mark)
    {
    case Mark::Key:
        painter.drawPolygon(QPolygonF({ c + QPointF(0, -r), c + QPointF(r, 0), c + QPointF(0, r), c + QPointF(-r, 0) }));
        break;
    case Mark::Breakdown:
        painter.drawPolygon(QPolygonF({ c + QPointF(0, -r), c + QPointF(r, r * 0.8), c + QPointF(-r, r * 0.8) }));
        break;
    default:
        painter.drawEllipse(c, r * 0.5, r * 0.5);
        break;
    }
    painter.restore();
}

void TimeLineCells::paintLink(QPainter& painter, const QRect& cell, const QColor& colour)
{
    painter.fillRect(cell.left() + 1, cell.bottom() - 2, cell.width() - 1, 3, colour);
}

void TimeLineCells::paintLabel(QPainter& painter, const Layer* layer,
                       int x, int y, int width, int height,
                       bool selected, LayerVisibility layerVisibility) const
{
    const QPalette palette = QApplication::palette();

    if (selected)
    {
        painter.setBrush(palette.color(QPalette::Highlight));
    }
    else
    {
        painter.setBrush(palette.color(QPalette::Base));
    }
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, y - 1, width, height); // empty rectangle by default

    if (!layer->visible())
    {
        painter.setBrush(palette.color(QPalette::Base));
    }
    else
    {
        if ((layerVisibility == LayerVisibility::ALL) || selected)
        {
            painter.setBrush(palette.color(QPalette::Text));
        }
        else if (layerVisibility == LayerVisibility::CURRENTONLY)
        {
            painter.setBrush(palette.color(QPalette::Base));
        }
        else if (layerVisibility == LayerVisibility::RELATED)
        {
            QColor color = palette.color(QPalette::Text);
            color.setAlpha(128);
            painter.setBrush(color);
        }
    }
    if (selected)
    {
        painter.setPen(palette.color(QPalette::HighlightedText));
    }
    else
    {
        painter.setPen(palette.color(QPalette::Text));
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawEllipse(x + 6, y + 4, 9, 9);
    painter.setRenderHint(QPainter::Antialiasing, false);

    if (layer->type() != Layer::SOUND)
    {
        static const QIcon onionIcon = Theme::icon(":/icons/tabler/stack-2.svg");
        const QIcon::Mode mode = layer->onionSkin() ? (selected ? QIcon::Selected : QIcon::Normal) : QIcon::Disabled;
        onionIcon.paint(&painter, QRect(kOnionToggleLeft + 1, y, 16, 16), Qt::AlignCenter, mode, layer->onionSkin() ? QIcon::On : QIcon::Off);
    }

    static const QIcon bitmapIcon = Theme::icon(":/icons/tabler/photo.svg");
    static const QIcon vectorIcon = Theme::icon(":/icons/tabler/vector.svg");
    static const QIcon soundIcon = Theme::icon(":/icons/tabler/volume.svg");
    static const QIcon cameraIcon = Theme::icon(":/icons/tabler/video.svg");
    const QIcon* typeIcon = nullptr;
    if (layer->type() == Layer::BITMAP) typeIcon = &bitmapIcon;
    if (layer->type() == Layer::VECTOR) typeIcon = &vectorIcon;
    if (layer->type() == Layer::SOUND) typeIcon = &soundIcon;
    if (layer->type() == Layer::CAMERA) typeIcon = &cameraIcon;
    if (typeIcon)
        typeIcon->paint(&painter, QRect(kOnionToggleRight + 2, y - 1, 18, 18), Qt::AlignCenter, selected ? QIcon::Selected : QIcon::Normal);

    auto paintToggle = [&](const QIcon& icon, int left, bool on) {
        icon.paint(&painter, QRect(left + 1, y, 16, 16), Qt::AlignCenter, on ? (selected ? QIcon::Selected : QIcon::Normal) : QIcon::Disabled);
    };
    if (layer->type() == Layer::BITMAP)
    {
        static const QIcon lockAlphaIcon = Theme::icon(":/icons/tabler/lock-square.svg");
        paintToggle(lockAlphaIcon, kLockAlphaLeft, static_cast<const LayerBitmap*>(layer)->lockAlpha());
    }
    static const QIcon lockAllIcon = Theme::icon(":/icons/tabler/lock.svg");
    paintToggle(lockAllIcon, kLockAllLeft, layer->lockAll());

    if (selected)
    {
        painter.setPen(palette.color(QPalette::HighlightedText));
    }
    else
    {
        painter.setPen(palette.color(QPalette::Text));
    }
    QString name = layer->name();
    const Object* object = mEditor->object();
    if (const LayerGroup* group = object->findLayerGroup(layer->groupId()))
    {
        QString groupName = group->name;
        if (!group->visible) { groupName += tr(" (hidden)"); }
        else if (group->opacity < 1.0) { groupName += QString(" %1%").arg(qRound(group->opacity * 100)); }
        if (object->getLayer(object->layerGroupRange(group->id).second) != layer) { name = "    " + name; }
        else if (group->collapsed) { name = QChar(0x25B8) + QString(" ") + groupName; }
        else { name = QChar(0x25BE) + QString(" ") + groupName + " / " + name; }
    }
    painter.drawText(QPoint(kLayerNameLeft, y + (2 * height) / 3), name);
    if (layer->isRough())
    {
        QFont tagFont = painter.font();
        tagFont.setPointSizeF(tagFont.pointSizeF() * 0.75);
        tagFont.setBold(true);
        const int tagLeft = kLayerNameLeft + painter.fontMetrics().horizontalAdvance(name) + 6;
        painter.save();
        painter.setFont(tagFont);
        painter.drawText(QPoint(tagLeft, y + (2 * height) / 3), tr("ROUGH"));
        painter.restore();
    }
}

void TimeLineCells::paintSelection(QPainter& painter, int x, int y, int width, int height) const
{
    QLinearGradient linearGrad(QPointF(0, y), QPointF(0, y + height));
    linearGrad.setColorAt(0, QColor(0, 0, 0, 255));
    linearGrad.setColorAt(1, QColor(255, 255, 255, 0));
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Overlay);
    painter.setBrush(linearGrad);
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, y, width, height - 1);
    painter.restore();
}

void TimeLineCells::paintLayerGutter(QPainter& painter) const
{
    painter.setPen(QApplication::palette().color(QPalette::Mid));
    if (mMouseMoveY > mLayerDetachThreshold)
    {
        painter.drawRect(0, getLayerY(getInbetweenLayerNumber(mEndY))+mLayerHeight, width(), 2);
    }
    else
    {
        painter.drawRect(0, getLayerY(getInbetweenLayerNumber(mEndY)), width(), 2);
    }
}

void TimeLineCells::paintOnionSkin(QPainter& painter) const
{
    Layer* layer = mEditor->layers()->currentLayer();
    if (layer == nullptr || !layer->onionSkin()) { return; }

    int frameNumber = mEditor->currentFrame();

    int prevOnionSkinCount = mEditor->preference()->get<SETTING::ONION_PREV_FRAMES_NUM>();
    int nextOnionSkinCount = mEditor->preference()->get<SETTING::ONION_NEXT_FRAMES_NUM>();

    const bool isAbsolute = mEditor->preference()->isOnionAbsolute();
    const MarkFilter filter = mEditor->preference()->markFilter();

    if (mEditor->preference()->get<SETTING::PREV_ONION>() && prevOnionSkinCount > 0)
    {
        int onionFrameNumber = frameNumber;
        if (isAbsolute)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber+1, true);
        }
        onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute, filter);
        int onionPosition = 0;

        while (onionPosition < prevOnionSkinCount && onionFrameNumber > 0)
        {
            painter.setBrush(QColor(128, 128, 128, 128));
            painter.setPen(Qt::NoPen);
            QRect onionRect;
            onionRect.setTopLeft(QPoint(getFrameX(onionFrameNumber - 1), 0));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), height()));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), 19));
            painter.drawRect(onionRect);

            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute, filter);
            onionPosition++;
        }
    }

    if (mEditor->preference()->get<SETTING::NEXT_ONION>() && nextOnionSkinCount > 0) {

        int onionFrameNumber = layer->getNextFrameNumber(frameNumber, isAbsolute, filter);
        int onionPosition = 0;

        while (onionPosition < nextOnionSkinCount && onionFrameNumber > 0)
        {
            painter.setBrush(QColor(128, 128, 128, 128));
            painter.setPen(Qt::NoPen);
            QRect onionRect;
            onionRect.setTopLeft(QPoint(getFrameX(onionFrameNumber - 1), 0));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), height()));
            onionRect.setBottomRight(QPoint(getFrameX(onionFrameNumber), 19));
            painter.drawRect(onionRect);

            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, isAbsolute, filter);
            onionPosition++;
        }
    }
}

void TimeLineCells::paintEvent(QPaintEvent*)
{
    const QPalette palette = QApplication::palette();
    QPainter painter(this);

    bool isPlaying = mEditor->playback()->isPlaying();
    if (mCache == nullptr || mRedrawContent || trackScrubber())
    {
        drawContent();
    }
    if (mCache)
    {
        painter.drawPixmap(QPoint(0, 0), *mCache);
    }

    const Object* object = mEditor->object();
    for (int i = 0; mFlashTicks % 2 == 1 && i < object->getLayerCount(); ++i)
    {
        const Layer* layer = object->getLayer(i);
        if (layer->id() != mFlashLayerId || object->isLayerFolded(i)) { continue; }
        // A hidden layer flashes its eye; a locked one its lock.
        const int left = layer->visible() ? kLockAllLeft : 2;
        QColor accent = palette.color(QPalette::Highlight);
        accent.setAlpha(160);
        painter.fillRect(QRect(left, getLayerY(i), 18, mLayerHeight - 2), accent);
    }

    if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        if (!isPlaying) {
            paintOnionSkin(painter);
        }

        int currentFrame = mEditor->currentFrame();
        Layer* currentLayer = mEditor->layers()->currentLayer();
        KeyFrame* keyFrame = currentLayer->getKeyFrameWhichCovers(currentFrame);
        if (keyFrame != nullptr)
        {
            int recWidth = keyFrame->length() == 1 ? mFrameSize - 2 : mFrameSize * keyFrame->length();
            int recLeft = getFrameX(keyFrame->pos()) - (mFrameSize - 2);
            paintCurrentFrameBorder(painter, recLeft, getLayerY(mEditor->currentLayerIndex()) + 1, recWidth, mLayerHeight - 4);
        }

        if (!mGesture.isMoving() && mLayerPosMoveY != -1 && mLayerPosMoveY == mEditor->currentLayerIndex())
        {
            // This is terrible but well...
            int recTop = getLayerY(mLayerPosMoveY) + 1;
            int standardWidth = mFrameSize - 2;
            int recHeight = mLayerHeight - 4;

            if (mHighlightFrameEnabled)
            {
                paintHighlightedFrame(painter, mHighlightedFrame, recTop, standardWidth, recHeight);
            }
            if (currentLayer->visible())
            {
                paintFrameCursorOnCurrentLayer(painter, recTop, standardWidth, recHeight);
            }
        }

        if (mGesture.hit() == FrameHit::ExposureEdge)
        {
            // The new end of the Exposure being dragged.
            const KeyFrame* key = currentLayer->getLastKeyFrameAtPosition(mGesture.startFrame());
            const int x = getFrameX(qMax(key ? key->pos() : 1, mGesture.lastFrame()));
            painter.fillRect(x - 1, getLayerY(mEditor->currentLayerIndex()), 3, mLayerHeight, palette.color(QPalette::Highlight));
        }

        // --- draw the position of the current frame
        if (currentFrame > mFrameOffset)
        {
            QColor scrubColor = palette.color(QPalette::Highlight);
            scrubColor.setAlpha(160);
            painter.setBrush(scrubColor);
            painter.setPen(Qt::NoPen);

            int currentFrameStartX = getFrameX(currentFrame - 1) + 1;
            int currentFrameEndX = getFrameX(currentFrame);
            QRect scrubRect;
            scrubRect.setTopLeft(QPoint(currentFrameStartX, 0));
            scrubRect.setBottomRight(QPoint(currentFrameEndX, height()));
            if (mbShortScrub)
            {
                scrubRect.setBottomRight(QPoint(currentFrameEndX, 19));
            }
            painter.save();

            bool mouseUnderScrubber = currentFrame == mFramePosMoveX;
            if (mouseUnderScrubber) {
                QRect smallScrub = QRect(QPoint(currentFrameStartX, 0), QPoint(currentFrameEndX,19));
                QPen pen = scrubColor;
                pen.setWidth(2);
                painter.setPen(pen);
                painter.drawRect(smallScrub);
                painter.setBrush(Qt::NoBrush);
            }
            painter.drawRect(scrubRect);
            painter.restore();

            painter.setPen(palette.color(QPalette::HighlightedText));
            int incr = (currentFrame < 10) ? 4 : 0;
            painter.drawText(QPoint(currentFrameStartX + incr, 15),
                             QString::number(currentFrame));
        }
    }
}

void TimeLineCells::resizeEvent(QResizeEvent* event)
{
    clearCache();
    updateContent();
    event->accept();
    emit lengthChanged(getFrameLength());
}

bool TimeLineCells::event(QEvent* event)
{
    if (event->type() == QEvent::Leave) {
        onDidLeaveWidget();
    }

    return QWidget::event(event);
}

// Same keys as the canvas: Left/Right move the frame, Up/Down the layer.
void TimeLineCells::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Right: mEditor->scrubForward(); break;
    case Qt::Key_Left: mEditor->scrubBackward(); break;
    case Qt::Key_Up: mEditor->layers()->gotoNextLayer(); break;
    case Qt::Key_Down: mEditor->layers()->gotoPreviouslayer(); break;
    case Qt::Key_F2:
        if (mType != TIMELINE_CELL_TYPE::Layers) { QWidget::keyPressEvent(event); return; }
        renameCurrentLayer();
        break;
    default: QWidget::keyPressEvent(event); return;
    }
    event->accept();
}

void TimeLineCells::mousePressEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());
    mCurrentLayerNumber = layerNumber;

    mMousePressX = event->pos().x();
    mFromLayer = mToLayer = layerNumber;

    mStartY = event->pos().y();
    mStartLayerNumber = layerNumber;
    mEndY = event->pos().y();

    mLastFrameNumber = frameNumber;
    mGesture.press(-1, frameNumber, FrameHit::Empty, Qt::NoModifier);

    primaryButton = event->button();

    const int exposureEdge = event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier ? exposureEdgeAt(event->pos()) : 0;
    if (exposureEdge > 0)
    {
        mGesture.press(layerNumber, exposureEdge, FrameHit::ExposureEdge, Qt::NoModifier);
        return;
    }

    switch (mType)
    {
    case TIMELINE_CELL_TYPE::Layers:
        if (layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
        {
            const Layer* clicked = mEditor->object()->getLayer(layerNumber);
            const LayerGroup* clickedGroup = mEditor->object()->findLayerGroup(clicked->groupId());
            if (event->pos().x() < 15 && event->button() == Qt::LeftButton && event->modifiers() == Qt::AltModifier)
            {
                mEditor->layers()->toggleSolo(layerNumber);
            }
            else if (event->pos().x() < 15)
            {
                mEditor->switchVisibilityOfLayer(layerNumber);
            }
            else if (event->button() == Qt::LeftButton && event->pos().x() >= kLockAlphaLeft && event->pos().x() < kLockAllLeft &&
                     clicked->type() == Layer::BITMAP)
            {
                Layer* layer = mEditor->object()->getLayer(layerNumber);
                mEditor->layers()->setAttribute(layer, AttributeId::LockAlpha, !static_cast<LayerBitmap*>(layer)->lockAlpha());
            }
            else if (event->button() == Qt::LeftButton && event->pos().x() >= kLockAllLeft && event->pos().x() < kLayerNameLeft)
            {
                Layer* layer = mEditor->object()->getLayer(layerNumber);
                mEditor->layers()->setAttribute(layer, AttributeId::LockAll, !layer->lockAll());
            }
            else if (event->button() == Qt::LeftButton && event->pos().x() >= kOnionToggleLeft && event->pos().x() < kOnionToggleRight &&
                     clicked->type() != Layer::SOUND)
            {
                mEditor->switchOnionSkinOfLayer(layerNumber);
            }
            else if (clickedGroup && event->button() == Qt::LeftButton && event->pos().x() < kLayerNameLeft + 12 && event->pos().x() >= kLayerNameLeft &&
                     mEditor->object()->layerGroupRange(clickedGroup->id).second == layerNumber)
            {
                // The arrow in front of the group name
                mEditor->layers()->setLayerGroupCollapsed(clickedGroup->id, !clickedGroup->collapsed);
            }
            else if (mEditor->currentLayerIndex() != layerNumber)
            {
                mEditor->layers()->setCurrentLayer(layerNumber);
                LayerManager::deselectFrames(mEditor->layers()->currentLayer());
            }
            if (event->pos().x() >= 15 && event->button() == Qt::RightButton)
            {
                CellMenus::showLayerMenu(mEditor, mEditor->object()->getLayer(layerNumber), event->globalPosition().toPoint());
            }
        }
        if (layerNumber == -1)
        {
            if (event->pos().x() < 15)
            {
                if (event->button() == Qt::LeftButton) {
                    mEditor->increaseLayerVisibilityIndex();
                } else if (event->button() == Qt::RightButton) {
                    mEditor->decreaseLayerVisibilityIndex();
                }
            }
        }
        break;
    case TIMELINE_CELL_TYPE::Tracks:
        if (event->button() == Qt::RightButton && mStartY < 20)
        {
            CellMenus::showMarkerMenu(mEditor, 0, mEditor->currentFrame(), event->globalPosition().toPoint());
            updateContent();
            return;
        }
        if (event->button() == Qt::LeftButton && event->modifiers() == Qt::ShiftModifier && mStartY < 20 && frameNumber > 0)
        {
            mRangeDragStart = frameNumber;
            return;
        }
        if (event->button() == Qt::MiddleButton)
        {
            mLastFrameNumber = getFrameNumber(event->pos().x());
        }
        else
        {
            if (frameNumber == mEditor->currentFrame() && mStartY < 20)
            {
                if (mEditor->playback()->isPlaying())
                {
                    mEditor->playback()->stop();
                }
                mTimeLine->scrubbing = true;
            }
            else
            {
                if ((layerNumber != -1) && layerNumber < mEditor->object()->getLayerCount())
                {
                    Layer *currentLayer = mEditor->object()->getLayer(layerNumber);

                    // Alt on a Drawing drags a Linked Drawing of it
                    if (event->modifiers() == Qt::AltModifier && event->button() == Qt::LeftButton &&
                        currentLayer->isPaintable() && currentLayer->keyExists(frameNumber))
                    {
                        mGesture.press(layerNumber, frameNumber, FrameHit::Link, event->modifiers());
                    }
                    else if (event->button() == Qt::LeftButton)
                    {
                        mGesture.press(layerNumber, frameNumber, FrameHit::Cell, event->modifiers());
                    }
                    else if (event->button() == Qt::RightButton)
                    {
                        if (mEditor->layers()->currentLayerIndex() != layerNumber && !currentLayer->isFrameSelected(frameNumber))
                        {
                            mEditor->layers()->setCurrentLayer(layerNumber);
                            emit mEditor->selectedFramesChanged();
                        }
                        showFrameMenu(event->pos());
                    }

                    if (currentLayer->hasAnySelectedFrames()) {
                        emit selectionChanged();
                    }

                    if (mTimeLine) { mTimeLine->updateContent(); }
                }
                else
                {
                    if (frameNumber > 0)
                    {
                        if (mEditor->playback()->isPlaying())
                        {
                            mEditor->playback()->stop();
                        }
                        if (mEditor->playback()->getSoundScrubActive() && mLastScrubFrame != frameNumber)
                        {
                            mEditor->playback()->playScrub(frameNumber);
                            mLastScrubFrame = frameNumber;
                        }

                        mEditor->scrubTo(frameNumber);

                        mTimeLine->scrubbing = true;
                        qDebug("Scrub to %d frame", frameNumber);
                    }
                }
            }
        }
        break;
    }
}

void TimeLineCells::mouseMoveEvent(QMouseEvent* event)
{
    mMouseMoveX = event->pos().x();
    mFramePosMoveX = getFrameNumber(mMouseMoveX);
    mLayerPosMoveY = getLayerNumber(event->pos().y());

    if (mType == TIMELINE_CELL_TYPE::Layers)
    {
        if (event->buttons() & Qt::LeftButton ) {
            mEndY = event->pos().y();
            emit mouseMovedY(mEndY - mStartY);
        }
    }
    else if (mType == TIMELINE_CELL_TYPE::Tracks)
    {
        if (primaryButton == Qt::MiddleButton)
        {
            mFrameOffset = qMin(qMax(0, mFrameLength - width() / getFrameSize()), qMax(0, mFrameOffset + mLastFrameNumber - mFramePosMoveX));
            update();
            emit offsetChanged(mFrameOffset);
        }
        else if (mGesture.hit() == FrameHit::ExposureEdge)
        {
            mGesture.drag(mCurrentLayerNumber, mFramePosMoveX);
            update();
        }
        else if (mRangeDragStart > 0)
        {
            const int frame = qMax(1, mFramePosMoveX);
            if (frame != mRangeDragStart)
            {
                mEditor->playback()->setLoopRange(mRangeDragStart, frame);
                mTimeLine->updateUI();
            }
        }
        else
        {
            if (event->buttons() == Qt::NoButton)
            {
                if (exposureEdgeAt(event->pos()) > 0) { setCursor(Qt::SizeHorCursor); } else { unsetCursor(); }
            }
            if (mTimeLine->scrubbing)
            {
                if (mEditor->playback()->getSoundScrubActive() && mLastScrubFrame != mFramePosMoveX)
                {
                    mEditor->playback()->playScrub(mFramePosMoveX);
                    mLastScrubFrame = mFramePosMoveX;
                }
                mEditor->scrubTo(mFramePosMoveX);
            }
            else
            {
                if (event->buttons() & Qt::LeftButton) {
                    if (mStartLayerNumber != -1 && mStartLayerNumber < mEditor->object()->getLayerCount())
                    {
                        // Above the tracks (-1) is the top layer.
                        mGesture.drag(mLayerPosMoveY == -1 ? mEditor->layers()->count() - 1 : mLayerPosMoveY, mFramePosMoveX);
                        mLastFrameNumber = mFramePosMoveX;
                        updateContent();
                    }
                }
                update();
            }
        }
    }
}

void TimeLineCells::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != primaryButton) return;

    int frameNumber = getFrameNumber(event->pos().x());
    if (frameNumber < 1) frameNumber = 1;
    int layerNumber = getLayerNumber(event->pos().y());

    if (mType == TIMELINE_CELL_TYPE::Tracks && mCurrentLayerNumber != -1 && primaryButton != Qt::MiddleButton)
    {
        mGesture.release();
        updateContent();
    }
    if (mType == TIMELINE_CELL_TYPE::Layers && !mScrollingVertically && layerNumber != mStartLayerNumber && mStartLayerNumber != -1 && layerNumber != -1)
    {
        mToLayer = getInbetweenLayerNumber(event->pos().y());
        if (mToLayer != mFromLayer && mToLayer > -1 && mToLayer < mEditor->layers()->count())
        {
            mEditor->layers()->moveLayer(mFromLayer, mToLayer);
        }
    }

    if (mType == TIMELINE_CELL_TYPE::Layers && event->button() == Qt::LeftButton)
    {
        emit mouseMovedY(0);
    }

    primaryButton = Qt::NoButton;
    mEndY = mStartY;
    mTimeLine->scrubbing = false;
    mRangeDragStart = 0;
}

void TimeLineCells::mouseDoubleClickEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());

    // -- short scrub --
    if (event->pos().y() < 20 && (mType != TIMELINE_CELL_TYPE::Layers || event->pos().x() >= 15))
    {
        mPrefs->set<SETTING::SHORT_SCRUB>(!mbShortScrub);
    }

    // -- layer --
    Layer* layer = mEditor->object()->getLayer(layerNumber);
    if (layer && event->buttons() & Qt::LeftButton)
    {
        if (mType == TIMELINE_CELL_TYPE::Tracks && (layerNumber != -1) && (frameNumber > 0) && layerNumber < mEditor->object()->getLayerCount())
        {
            if (!layer->keyExistsWhichCovers(frameNumber))
            {
                mEditor->scrubTo(frameNumber);
            }

            // The release event will toggle the frame on again, so we make sure it gets
            // deselected now instead.
            LayerManager::setFrameSelected(layer, frameNumber, true);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers && event->pos().x() >= kLayerNameLeft)
        {
            startRename(layer);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers && event->pos().x() >= kOnionToggleRight && event->pos().x() < kLockAlphaLeft &&
                 layer->type() == Layer::CAMERA)
        {
            editLayerProperties(static_cast<LayerCamera*>(layer));
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimeLineCells::editLayerProperties(LayerCamera* cameraLayer) const
{
    QRegularExpression regex("([\\x{FFEF}-\\x{FFFF}])+");

    CameraPropertiesDialog dialog(cameraLayer->name(), cameraLayer->getViewRect().width(),
                                  cameraLayer->getViewRect().height());
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    QString name = dialog.getName().replace(regex, "");

    if (!name.isEmpty())
    {
        mEditor->layers()->renameLayer(cameraLayer, name);
    }
    QSettings& settings = pencilSettings();
    settings.setValue(SETTING_FIELD_W, dialog.getWidth());
    settings.setValue(SETTING_FIELD_H, dialog.getHeight());
    mEditor->layers()->setCameraSize(cameraLayer, QSize(dialog.getWidth(), dialog.getHeight()));
}

void TimeLineCells::renameCurrentLayer()
{
    startRename(mEditor->layers()->currentLayer());
}

void TimeLineCells::startRename(Layer* layer)
{
    if (!layer) { return; }
    finishRename(false);
    const Object* object = mEditor->object();
    int number = 0;
    while (number < object->getLayerCount() - 1 && object->getLayer(number) != layer) { ++number; }
    mRenameLayerId = layer->id();
    mRenameEdit = new QLineEdit(layer->name(), this);
    mRenameEdit->setGeometry(kLayerNameLeft, getLayerY(number), width() - kLayerNameLeft, mLayerHeight);
    mRenameEdit->installEventFilter(this);
    mRenameEdit->selectAll();
    mRenameEdit->show();
    mRenameEdit->setFocus();
}

void TimeLineCells::finishRename(bool accept)
{
    if (!mRenameEdit) { return; }
    QLineEdit* edit = mRenameEdit;
    mRenameEdit = nullptr;
    static const QRegularExpression regex("([\\x{FFEF}-\\x{FFFF}])+");
    QString name = edit->text();
    name.replace(regex, "");
    Layer* layer = mEditor->object()->findLayerById(mRenameLayerId);
    edit->removeEventFilter(this);
    edit->deleteLater();
    edit->hide();
    if (accept && layer && name != layer->name())
    {
        mEditor->layers()->renameLayer(layer, name);
    }
}

bool TimeLineCells::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == mRenameEdit)
    {
        if (event->type() == QEvent::KeyPress)
        {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Escape) { finishRename(false); return true; }
            if (key == Qt::Key_Return || key == Qt::Key_Enter)
            {
                // An empty name is refused: the editor stays open.
                if (!mRenameEdit->text().trimmed().isEmpty()) { finishRename(true); }
                return true;
            }
        }
        else if (event->type() == QEvent::FocusOut)
        {
            finishRename(!mRenameEdit->text().trimmed().isEmpty());
        }
    }
    return QWidget::eventFilter(watched, event);
}
void TimeLineCells::hScrollChange(int x)
{
    mFrameOffset = x;
    updateContent();
}

void TimeLineCells::vScrollChange(int x)
{
    mLayerOffset = x;
    mScrollingVertically = true;
    updateContent();
}

void TimeLineCells::onScrollingVerticallyStopped()
{
    mScrollingVertically = false;
}

void TimeLineCells::setMouseMoveY(int x)
{
    mMouseMoveY = x;
    updateContent();
}

bool TimeLineCells::trackScrubber()
{
    if (mType != TIMELINE_CELL_TYPE::Tracks ||
        (mPrevFrame == mEditor->currentFrame() && !mEditor->playback()->isPlaying()))
    {
        return false;
    }
    mPrevFrame = mEditor->currentFrame();

    if (mEditor->currentFrame() <= mFrameOffset)
    {
        // Move the timeline back if the scrubber is offscreen to the left
        mFrameOffset = mEditor->currentFrame() - 1;
        emit offsetChanged(mFrameOffset);
        return true;
    }
    else if (width() < (mEditor->currentFrame() - mFrameOffset + 1) * mFrameSize)
    {
        // Move timeline forward if the scrubber is offscreen to the right
        if (mEditor->playback()->isPlaying())
            mFrameOffset = mFrameOffset + ((mEditor->currentFrame() - mFrameOffset) / 2);
        else
            mFrameOffset = mEditor->currentFrame() - width() / mFrameSize;
        emit offsetChanged(mFrameOffset);
        return true;
    }
    return false;
}

void TimeLineCells::onDidLeaveWidget()
{
    // Reset last known frame pos to avoid wrong UI states when leaving the widget
    mFramePosMoveX = 0;
    update();
}
