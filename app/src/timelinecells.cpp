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

#include <QActionGroup>
#include <QApplication>
#include <QResizeEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QDebug>

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

#include "cameracontextmenu.h"
#include "pencilsettings.h"

TimeLineCells::TimeLineCells(TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE type) : QWidget(parent)
{
    mTimeLine = parent;
    mEditor = editor;
    mPrefs = editor->preference();
    mType = type;

    mFrameLength = mPrefs->getInt(SETTING::TIMELINE_SIZE);
    mFontSize = mPrefs->getInt(SETTING::LABEL_FONT_SIZE);
    mFrameSize = mPrefs->getInt(SETTING::FRAME_SIZE);
    mbShortScrub = mPrefs->isOn(SETTING::SHORT_SCRUB);
    mDrawFrameNumber = mPrefs->isOn(SETTING::DRAW_LABEL);

    setMinimumSize(500, 4 * mLayerHeight);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    // Tab focus only: a click must leave keyboard focus on the canvas.
    setFocusPolicy(Qt::TabFocus);

    connect(mPrefs, &PreferenceManager::optionChanged, this, &TimeLineCells::loadSetting);
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
        mFrameLength = mPrefs->getInt(SETTING::TIMELINE_SIZE);
        mTimeLine->updateLength();
        break;
    case SETTING::LABEL_FONT_SIZE:
        mFontSize = mPrefs->getInt(SETTING::LABEL_FONT_SIZE);
        break;
    case SETTING::FRAME_SIZE:
        mFrameSize = mPrefs->getInt(SETTING::FRAME_SIZE);
        mTimeLine->updateLength();
        break;
    case SETTING::SHORT_SCRUB:
        mbShortScrub = mPrefs->isOn(SETTING::SHORT_SCRUB);
        break;
    case SETTING::DRAW_LABEL:
        mDrawFrameNumber = mPrefs->isOn(SETTING::DRAW_LABEL);
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

void TimeLineCells::setFrameSize(int size)
{
    mFrameSize = size;
    mPrefs->set(SETTING::FRAME_SIZE, mFrameSize);
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
    int frameNumber = getFrameNumber(pos.x());

    const Layer* curLayer = mEditor->layers()->currentLayer();
    Q_ASSERT(curLayer);

    if ((curLayer->type() == Layer::BITMAP || curLayer->type() == Layer::VECTOR) && curLayer->keyExists(frameNumber))
    {
        showTweenMenu(frameNumber, mapToGlobal(pos));
        return;
    }

    if (curLayer->type() == Layer::SOUND)
    {
        showMarkerMenu(frameNumber, curLayer->id(), mapToGlobal(pos));
        return;
    }

    // only show menu if on camera layer and key exists
    if (curLayer->type() != Layer::CAMERA || !curLayer->keyExists(frameNumber))
    {
        return;
    }

    mHighlightFrameEnabled = true;
    mHighlightedFrame = frameNumber;

    CameraContextMenu menu(frameNumber, static_cast<const LayerCamera*>(curLayer));

    menu.connect(&menu, &CameraContextMenu::aboutToHide, this, [=] {
        mHighlightFrameEnabled = false;
        mHighlightedFrame = -1;
        update();

        KeyFrame* key = curLayer->getKeyFrameAt(frameNumber);
        if (key->isModified()) {
            emit mEditor->frameModified(frameNumber);
        }
    });

    // Update needs to happen before executing menu, otherwise paint event might be postponed
    update();

    menu.exec(mapToGlobal(pos));
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

void TimeLineCells::showMarkerMenu(int frame, int layerId, const QPoint& globalPos)
{
    ObjectData* data = mEditor->object()->data();
    const bool has = data->markers(layerId).contains(frame);

    QMenu menu;
    QAction* add = menu.addAction(has ? tr("Rename Marker...") : tr("Add Marker..."));
    QAction* color = has ? menu.addAction(tr("Change Marker Color...")) : nullptr;
    QAction* remove = has ? menu.addAction(tr("Remove Marker")) : nullptr;
    QAction* chosen = menu.exec(globalPos);
    if (chosen == nullptr) return;

    FrameMarker m = data->markers(layerId).value(frame, FrameMarker{ QString(), QColor(255, 140, 0) });
    if (chosen == add)
    {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Marker"), tr("Name:"), QLineEdit::Normal, m.name, &ok);
        if (!ok) return;
        m.name = name;
        data->setMarker(frame, m, layerId);
    }
    else if (chosen == color)
    {
        const QColor c = QColorDialog::getColor(m.color, this);
        if (!c.isValid()) return;
        m.color = c;
        data->setMarker(frame, m, layerId);
    }
    else if (chosen == remove)
    {
        data->removeMarker(frame, layerId);
    }
    mEditor->object()->modification();
    updateContent();
}

void TimeLineCells::showTweenMenu(int frame, const QPoint& globalPos)
{
    const KeyFrame* key = mEditor->layers()->currentLayer()->getKeyFrameAt(frame);
    bool tween = key->tween();
    CameraEasingType easing = key->tweenEasing();
    TweenState state = key->tweenState();

    QMenu menu;
    QAction* toggle = menu.addAction(tr("Tween to Next Key"));
    toggle->setCheckable(true);
    toggle->setChecked(tween);
    QMenu* easingMenu = menu.addMenu(tr("Tween Easing"));
    for (int i = 0; i <= static_cast<int>(CameraEasingType::OUTINBOUNCE); ++i)
    {
        QAction* action = easingMenu->addAction(getInterpolationText(static_cast<CameraEasingType>(i)));
        action->setCheckable(true);
        action->setChecked(static_cast<int>(easing) == i);
        action->setData(i);
    }
    QAction* transform = menu.addAction(tr("Key Transform..."));
    QAction* chosen = menu.exec(globalPos);
    if (chosen == nullptr) { return; }

    if (chosen == toggle)
    {
        tween = !tween;
    }
    else if (chosen == transform)
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Key Transform"));
        QFormLayout* form = new QFormLayout(&dialog);
        auto spin = [&](const QString& label, qreal value, qreal min, qreal max) {
            QDoubleSpinBox* box = new QDoubleSpinBox(&dialog);
            box->setRange(min, max);
            box->setDecimals(2);
            box->setValue(value);
            form->addRow(label, box);
            return box;
        };
        QDoubleSpinBox* tx = spin(tr("Move X:"), state.tx, -100000, 100000);
        QDoubleSpinBox* ty = spin(tr("Move Y:"), state.ty, -100000, 100000);
        QDoubleSpinBox* rot = spin(tr("Rotation:"), state.rot, -3600, 3600);
        QDoubleSpinBox* scale = spin(tr("Scale:"), state.scale, 0.01, 100);
        QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted) { return; }
        state = TweenState{ tx->value(), ty->value(), rot->value(), scale->value(), state.opacity };
    }
    else if (chosen->data().isValid())
    {
        easing = static_cast<CameraEasingType>(chosen->data().toInt());
        tween = true;
    }
    mEditor->layers()->setKeyTween(frame, tween, easing, state);
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
        if (mMovingFrames) {
            int offset = (framePos - posUnderCursor) + mFrameOffset;
            int newFrameX = getFrameX(getFrameNumber(getFrameX(offset)+mouseX)) - standardWidth;
            // Paint as frames are hovering
            painter.drawRect(newFrameX, recTop-4, recWidth, recHeight);

        } else {
            int currentFrameX = frameX - standardWidth;
            painter.drawRect(currentFrameX, recTop, recWidth, recHeight);
        }
    }
    painter.restore();
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

    if (layer->type() == Layer::BITMAP) painter.drawPixmap(QPoint(22, y - 1), QPixmap(":icons/themes/playful/timeline/cell-bitmap.svg"));
    if (layer->type() == Layer::VECTOR) painter.drawPixmap(QPoint(22, y - 1), QPixmap(":icons/themes/playful/timeline/cell-vector.svg"));
    if (layer->type() == Layer::SOUND) painter.drawPixmap(QPoint(22, y - 1), QPixmap(":icons/themes/playful/timeline/cell-sound.svg"));
    if (layer->type() == Layer::CAMERA) painter.drawPixmap(QPoint(22, y - 1), QPixmap(":icons/themes/playful/timeline/cell-camera.svg"));

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
    painter.drawText(QPoint(45, y + (2 * height) / 3), name);
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

    int prevOnionSkinCount = mEditor->preference()->getInt(SETTING::ONION_PREV_FRAMES_NUM);
    int nextOnionSkinCount = mEditor->preference()->getInt(SETTING::ONION_NEXT_FRAMES_NUM);

    bool isAbsolute = (mEditor->preference()->getString(SETTING::ONION_TYPE) == "absolute");

    if (mEditor->preference()->isOn(SETTING::PREV_ONION) && prevOnionSkinCount > 0)
    {
        int onionFrameNumber = frameNumber;
        if (isAbsolute)
        {
            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber+1, true);
        }
        onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute);
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

            onionFrameNumber = layer->getPreviousFrameNumber(onionFrameNumber, isAbsolute);
            onionPosition++;
        }
    }

    if (mEditor->preference()->isOn(SETTING::NEXT_ONION) && nextOnionSkinCount > 0) {

        int onionFrameNumber = layer->getNextFrameNumber(frameNumber, isAbsolute);
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

            onionFrameNumber = layer->getNextFrameNumber(onionFrameNumber, isAbsolute);
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

        if (!mMovingFrames && mLayerPosMoveY != -1 && mLayerPosMoveY == mEditor->currentLayerIndex())
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

    mStartFrameNumber = frameNumber;
    mLastFrameNumber = mStartFrameNumber;

    mCanMoveFrame = false;
    mMovingFrames = false;

    mCanBoxSelect = false;
    mBoxSelecting = false;

    mClickSelecting = false;

    primaryButton = event->button();

    switch (mType)
    {
    case TIMELINE_CELL_TYPE::Layers:
        if (layerNumber != -1 && layerNumber < mEditor->object()->getLayerCount())
        {
            const Layer* clicked = mEditor->object()->getLayer(layerNumber);
            const LayerGroup* clickedGroup = mEditor->object()->findLayerGroup(clicked->groupId());
            if (event->pos().x() < 15)
            {
                mEditor->switchVisibilityOfLayer(layerNumber);
            }
            else if (clickedGroup && event->button() == Qt::LeftButton && event->pos().x() < 57 && event->pos().x() >= 45 &&
                     mEditor->object()->layerGroupRange(clickedGroup->id).second == layerNumber)
            {
                // The arrow in front of the group name
                mEditor->layers()->setLayerGroupCollapsed(clickedGroup->id, !clickedGroup->collapsed);
            }
            else if (mEditor->currentLayerIndex() != layerNumber)
            {
                mEditor->layers()->setCurrentLayer(layerNumber);
                mEditor->layers()->currentLayer()->deselectAll();
            }
            if (event->pos().x() >= 15 && event->button() == Qt::RightButton)
            {
                Layer* layer = mEditor->object()->getLayer(layerNumber);
                QMenu menu;
                QAction* onion = menu.addAction(tr("Onion Skin"));
                onion->setCheckable(true);
                onion->setChecked(layer->onionSkin());
                QAction* lockAlpha = nullptr;
                if (layer->type() == Layer::BITMAP)
                {
                    lockAlpha = menu.addAction(tr("Lock Transparency"));
                    lockAlpha->setCheckable(true);
                    lockAlpha->setChecked(static_cast<LayerBitmap*>(layer)->lockAlpha());
                }
                if (layer->type() == Layer::BITMAP || layer->type() == Layer::VECTOR)
                {
                    QMenu* blend = menu.addMenu(tr("Blend Mode"));
                    QActionGroup* group = new QActionGroup(blend);
                    for (int mode = 0; mode < kBlendModeCount; ++mode)
                    {
                        QAction* action = blend->addAction(QCoreApplication::translate("BlendMode", kBlendModeNames[mode]));
                        action->setCheckable(true);
                        action->setChecked(static_cast<int>(layer->blendMode()) == mode);
                        action->setData(mode);
                        group->addAction(action);
                    }
                }
                menu.addSeparator();
                QAction* newGroup = nullptr;
                QAction* ungroup = nullptr;
                QAction* collapse = nullptr;
                QAction* groupVisible = nullptr;
                QAction* groupOpacity = nullptr;
                if (clickedGroup == nullptr) { newGroup = menu.addAction(tr("New Group")); }
                QMenu* addTo = menu.addMenu(tr("Add to Group"));
                for (const LayerGroup& g : mEditor->object()->layerGroups())
                {
                    if (g.id != layer->groupId() && mEditor->object()->layerGroupRange(g.id).first >= 0)
                    {
                        addTo->addAction(g.name)->setData(g.id);
                    }
                }
                addTo->setEnabled(!addTo->isEmpty());
                if (clickedGroup)
                {
                    ungroup = menu.addAction(tr("Remove from Group"));
                    collapse = menu.addAction(clickedGroup->collapsed ? tr("Expand Group") : tr("Collapse Group"));
                    groupVisible = menu.addAction(tr("Show Group"));
                    groupVisible->setCheckable(true);
                    groupVisible->setChecked(clickedGroup->visible);
                    groupOpacity = menu.addAction(tr("Group Opacity..."));
                }
                QAction* chosen = menu.exec(event->globalPosition().toPoint());
                if (chosen == onion)
                {
                    mEditor->switchOnionSkinOfLayer(layerNumber);
                }
                else if (chosen && chosen == lockAlpha)
                {
                    static_cast<LayerBitmap*>(layer)->setLockAlpha(lockAlpha->isChecked());
                    mEditor->object()->modification();
                }
                else if (chosen && chosen == newGroup)
                {
                    mEditor->layers()->createLayerGroup(layer);
                }
                else if (chosen && chosen == ungroup)
                {
                    mEditor->layers()->removeLayerFromGroup(layer);
                }
                else if (chosen && chosen == collapse)
                {
                    mEditor->layers()->setLayerGroupCollapsed(clickedGroup->id, !clickedGroup->collapsed);
                }
                else if (chosen && chosen == groupVisible)
                {
                    mEditor->layers()->setLayerGroupVisible(clickedGroup->id, groupVisible->isChecked());
                }
                else if (chosen && chosen == groupOpacity)
                {
                    bool ok = false;
                    const int percent = QInputDialog::getInt(this, tr("Group Opacity"), tr("Opacity (%):"),
                                                             qRound(clickedGroup->opacity * 100), 0, 100, 1, &ok);
                    if (ok) { mEditor->layers()->setLayerGroupOpacity(clickedGroup->id, percent / 100.0); }
                }
                else if (chosen && chosen->parent() == addTo)
                {
                    mEditor->layers()->addLayerToGroup(layer, chosen->data().toInt());
                }
                else if (chosen && chosen->data().isValid())
                {
                    mEditor->layers()->setBlendMode(layer, static_cast<BlendMode>(chosen->data().toInt()));
                }
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
            showMarkerMenu(mEditor->currentFrame(), 0, event->globalPosition().toPoint());
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

                    // Switching layers deselects all frames: keep a box selection to drag it.
                    if (mEditor->layers()->currentLayerIndex() != layerNumber && !currentLayer->isFrameSelected(frameNumber))
                    {
                        mEditor->layers()->setCurrentLayer(layerNumber);
                        emit mEditor->selectedFramesChanged();
                    }

                    // Check if we are using the alt key
                    if (event->modifiers() == Qt::AltModifier)
                    {
                        // If it is the case, we select everything that is after the selected frame
                        mClickSelecting = true;
                        mCanMoveFrame = true;

                        currentLayer->selectAllFramesAfter(frameNumber);
                        emit mEditor->selectedFramesChanged();
                    }
                    // Check if we are clicking on a non selected frame
                    else if (!currentLayer->isFrameSelected(frameNumber))
                    {
                        // If it is the case, we select it if it is the left button...
                        mCanBoxSelect = true;
                        mClickSelecting = true;
                        if (event->button() == Qt::LeftButton)
                        {

                            if (event->modifiers() == Qt::ControlModifier)
                            {
                                // Add/remove from already selected
                                currentLayer->toggleFrameSelected(frameNumber, true);
                                emit mEditor->selectedFramesChanged();
                            }
                            else if (event->modifiers() == Qt::ShiftModifier)
                            {
                                // Select a range from the last selected
                                currentLayer->extendSelectionTo(frameNumber);
                                emit mEditor->selectedFramesChanged();
                            }
                            else
                            {
                                // Only select if left button clicked
                                mEditor->layers()->selectFrames(layerNumber, layerNumber, frameNumber, frameNumber);
                                emit mEditor->selectedFramesChanged();
                            }
                        }

                        // ... or we show the frame context menu, if it is the right button
                        if (event->button() == Qt::RightButton)
                        {
                            showFrameMenu(event->pos());
                        }

                    }
                    else
                    {
                        // If selected they can also be interpolated
                        if (event->button() == Qt::RightButton)
                        {
                            showFrameMenu(event->pos());
                        }
                        // We clicked on a selected frame, we can move it
                        mCanMoveFrame = true;
                    }

                    if (currentLayer->hasAnySelectedFrames()) {
                        emit selectionChanged();
                    }

                    mTimeLine->updateContent();
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
        else
        {
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
                        // Check if the frame we clicked was selected
                        if (mCanMoveFrame) {

                            // If it is the case, we move the selected frames in the layer
                            mMovingFrames = true;
                        }
                        else if (mCanBoxSelect)
                        {
                            // Otherwise, we do a box select
                            mBoxSelecting = true;

                            // Above the tracks (-1) is the top layer.
                            const int toLayer = mLayerPosMoveY == -1 ? mEditor->layers()->count() - 1 : mLayerPosMoveY;
                            mEditor->layers()->selectFrames(mStartLayerNumber, toLayer, mStartFrameNumber, mFramePosMoveX);
                            emit mEditor->selectedFramesChanged();
                        }
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
        // We should affect the current layer based on what's selected, not where the mouse currently is.
        Layer* currentLayer = mEditor->layers()->getLayer(mCurrentLayerNumber);
        Q_ASSERT(currentLayer);

        if (mMovingFrames)
        {
            int posUnderCursor = getFrameNumber(mMousePressX);
            mEditor->layers()->moveSelectedFrames(frameNumber - posUnderCursor);
            updateContent();
        }
        else if (!mTimeLine->scrubbing && !mMovingFrames && !mClickSelecting && !mBoxSelecting)
        {
            // De-selecting if we didn't move, scrub nor select anything
            bool multipleSelection = (event->modifiers() == Qt::ControlModifier);

            // Add/remove from already selected
            const bool wasSelected = currentLayer->isFrameSelected(frameNumber);
            if (!multipleSelection) { mEditor->layers()->deselectAllFrames(); }
            currentLayer->setFrameSelected(frameNumber, !wasSelected);
            emit mEditor->selectedFramesChanged();
            updateContent();
        }
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
    mMovingFrames = false;
}

void TimeLineCells::mouseDoubleClickEvent(QMouseEvent* event)
{
    int frameNumber = getFrameNumber(event->pos().x());
    int layerNumber = getLayerNumber(event->pos().y());

    // -- short scrub --
    if (event->pos().y() < 20 && (mType != TIMELINE_CELL_TYPE::Layers || event->pos().x() >= 15))
    {
        mPrefs->set(SETTING::SHORT_SCRUB, !mbShortScrub);
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
            layer->setFrameSelected(frameNumber, true);
        }
        else if (mType == TIMELINE_CELL_TYPE::Layers && event->pos().x() >= 15)
        {
            editLayerProperties(layer);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimeLineCells::editLayerProperties(Layer *layer) const
{
    if (layer->type() != Layer::CAMERA)
    {
        editLayerName(layer);
        return;
    }

    auto cameraLayer = dynamic_cast<LayerCamera*>(layer);
    Q_ASSERT(cameraLayer);
    editLayerProperties(cameraLayer);
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
    cameraLayer->setViewRect(QRect(-dialog.getWidth() / 2, -dialog.getHeight() / 2, dialog.getWidth(), dialog.getHeight()));
    mEditor->view()->forceUpdateViewTransform();
}

void TimeLineCells::editLayerName(Layer* layer) const
{
    QRegularExpression regex("([\\x{FFEF}-\\x{FFFF}])+");

    bool ok;
    QString name = QInputDialog::getText(nullptr, tr("Layer Properties"),
                                         tr("Layer name:"), QLineEdit::Normal,
                                         layer->name(), &ok);
    name.replace(regex, "");
    if (!ok || name.isEmpty())
    {
        return;
    }

    mEditor->layers()->renameLayer(layer, name);
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
