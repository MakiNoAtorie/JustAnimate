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

#ifndef TIMELINECELLS_H
#define TIMELINECELLS_H

#include "frameeditgesture.h"
#include <functional>
#include <QString>
#include <QWidget>
#include "layercamera.h"

class Layer;
class SoundClip;
enum class LayerVisibility;
enum class Mark;
class TimeLine;
class QPaintEvent;
class QMouseEvent;
class QResizeEvent;
class Editor;
class PreferenceManager;
class QMenu;
class QLineEdit;
class QTimer;
class QAction;
enum class SETTING;

enum class TIMELINE_CELL_TYPE
{
    Layers,
    Tracks
};

class TimeLineCells : public QWidget
{
    Q_OBJECT

public:
    TimeLineCells( TimeLine* parent, Editor* editor, TIMELINE_CELL_TYPE );
    ~TimeLineCells() override;

    static int getOffsetX() { return mOffsetX; }
    static int getOffsetY() { return mOffsetY; }
    int getLayerHeight() const { return mLayerHeight; }
    /** Opens the inline name editor on the current layer. */
    void renameCurrentLayer();

    int getFrameLength() const { return mFrameLength; }
    int getFrameSize() const { return mFrameSize; }

    void setFrameLength(int n) { mFrameLength = n; }
    void setFrameSize(int size);
    void clearCache() { delete mCache; mCache = nullptr; }

    bool didDetachLayer() const;

    void showFrameMenu(QPoint pos);

    /** Draws a Drawing's Mark shape centred in its cell: Key diamond, Breakdown triangle, Inbetween dot. */
    static void paintMark(QPainter& painter, Mark mark, const QRect& cell, const QColor& colour);
    /** Draws the Linked Drawing bar along the bottom of a Drawing's cell. */
    static void paintLink(QPainter& painter, const QRect& cell, const QColor& colour);

signals:
    void mouseMovedY(int);
    void lengthChanged(int);
    void offsetChanged(int);
    void selectionChanged();
    void insertNewKeyFrame();

public slots:
    void updateContent();
    void updateFrame(int frameNumber);
    /** Redraws the cells only when a key in view came, went or changed; a pixel edit repaints one column. */
    void onFrameModified(int frameNumber);
    void hScrollChange(int);
    void vScrollChange(int);
    void onScrollingVerticallyStopped();
    void setMouseMoveY(int x);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void loadSetting(SETTING setting);

private:
    int getLayerNumber(int y) const;
    int getInbetweenLayerNumber(int y) const;
    int getLayerY(int layerNumber) const;
    int getFrameX(int frameNumber) const;
    int getFrameNumber(int x) const;
    /** The frame whose right edge is under pos and ends a Drawing's Exposure, or 0. */
    int exposureEdgeAt(const QPoint& pos) const;

    void onDidLeaveWidget();

    bool trackScrubber();
    void drawContent();
    /** Calls action on each key whose cell or tween line can reach the frames in view, last first. */
    void foreachVisibleKey(const Layer* layer, const std::function<void(KeyFrame*)>& action) const;
    size_t visibleKeysHash() const;
    void paintTicks(QPainter& painter, const QPalette& palette) const;
    void paintOnionSkin(QPainter& painter) const;
    void paintLayerGutter(QPainter& painter) const;
    void paintTrack(QPainter& painter, const Layer* layer, int x, int y, int width, int height, bool selected, int frameSize) const;
    void paintFrames(QPainter& painter, QColor trackCol, const Layer* layer, int y, int height, bool selected, int frameSize) const;
    void paintWaveform(QPainter& painter, const SoundClip* clip, int recTop, int recHeight, int frameSize) const;
    void paintCurrentFrameBorder(QPainter& painter, int recLeft, int recTop, int recWidth, int recHeight) const;
    void paintFrameCursorOnCurrentLayer(QPainter& painter, int recTop, int recWidth, int recHeight) const;
    void paintSelectedFrames(QPainter& painter, const Layer* layer, int layerY) const;
    void paintLabel(QPainter& painter, const Layer* layer, int x, int y, int height, int width, bool selected, LayerVisibility layerVisibility) const;
    void paintSelection(QPainter& painter, int x, int y, int width, int height) const;
    void paintHighlightedFrame(QPainter& painter, int framePos, int recTop, int recWidth, int recHeight) const;

    void editLayerProperties(LayerCamera *layer) const;
    void startRename(Layer* layer);
    void finishRename(bool accept);

    QLineEdit* mRenameEdit = nullptr;
    int mRenameLayerId = -1;

    TimeLine* mTimeLine;
    Editor* mEditor; // the editor for which this timeLine operates
    PreferenceManager* mPrefs;

    TIMELINE_CELL_TYPE mType;

    QPixmap* mCache = nullptr;
    bool mRedrawContent = false;
    size_t mDrawnKeysHash = 0; // visibleKeysHash() at the last drawContent
    bool mDrawFrameNumber = true;
    bool mbShortScrub = false;
    int mFrameLength = 1;
    int mFrameSize = 0;
    int mFontSize = 10;
    bool mScrubbing = false;
    bool mHighlightFrameEnabled = false;
    int mHighlightedFrame = -1;
    int mLayerHeight = 20;
    int mStartY = 0;
    int mEndY   = 0;

    int mCurrentLayerNumber = 0;
    int mLastScrubFrame = 0;

    int mFromLayer = 0;
    int mToLayer   = 1;
    int mStartLayerNumber = -1;
    int mLastFrameNumber = -1;

    // is used to move layers, don't use this to get mousePos;
    int mMouseMoveY = 0;
    int mPrevFrame = 0;
    int mFrameOffset = 0;
    int mLayerOffset = 0;
    Qt::MouseButton primaryButton = Qt::NoButton;

    bool mScrollingVertically = false;

    // A refused layer's eye or lock icon blinks while mFlashTicks counts down.
    void flashLayer(int layerId);
    QTimer* mFlashTimer = nullptr;
    int mFlashLayerId = 0;
    int mFlashTicks = 0;

    FrameEditGesture mGesture;
    int mRangeDragStart = 0; // ruler frame where a Shift-drag loop range started, or 0

    int mFramePosMoveX = 0;
    int mLayerPosMoveY = 0;

    int mMouseMoveX = 0;
    int mMousePressX = 0;

    const static int mOffsetX = 0;
    const static int mOffsetY = 20;
    const static int mLayerDetachThreshold = 5;

};

#endif // TIMELINECELLS_H
