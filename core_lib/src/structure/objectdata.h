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

#ifndef EDITORSTATE_H
#define EDITORSTATE_H

#include <QColor>
#include <QMap>
#include <QString>
#include <QTransform>
#include "pencildef.h"


struct FrameMarker
{
    QString name;
    QColor color;
    int layerId = 0; // Sound layer of a lip-sync marker; 0 for a global marker.
};

class ObjectData
{
public:
    ObjectData();
    ~ObjectData() = default;

    void setCurrentFrame(int n) { mCurrentFrame = n; }
    int  getCurrentFrame() const { return mCurrentFrame; }

    void   setCurrentColor(QColor c) { mCurrentColor = c; }
    QColor getCurrentColor() const { return mCurrentColor; }

    void setCurrentLayer(int n) { mCurrentLayer = n; }
    int  getCurrentLayer() const { return mCurrentLayer; }

    void setCurrentView(QTransform t) { mCurrentView = t; }
    QTransform getCurrentView() const { return mCurrentView; }

    void setFrameRate(int n) { mFps = n; }
    int  getFrameRate() const { return mFps; }

    void setLooping(bool b) { mIsLoop = b; }
    bool isLooping() const { return mIsLoop; }

    void setPingPong(bool b) { mIsPingPong = b; }
    bool isPingPong() const { return mIsPingPong; }

    void setRangedPlayback(bool b) { mIsRangedPlayback = b; }
    bool isRangedPlayback() const { return mIsRangedPlayback; }

    void setMarkInFrameNumber(int n) { mMarkInFrame = n; }
    int  getMarkInFrameNumber() const { return mMarkInFrame; }

    void setMarkOutFrameNumber(int n) { mMarkOutFrame = n; }
    int  getMarkOutFrameNumber() const { return mMarkOutFrame; }

    void setMarker(int frame, const FrameMarker& m) { mMarkers[m.layerId].insert(frame, m); }
    void removeMarker(int frame, int layerId = 0) { mMarkers[layerId].remove(frame); }
    QMap<int, FrameMarker> markers(int layerId = 0) const { return mMarkers.value(layerId); }
    // Markers by layer id, then by frame. Key 0 holds the global markers.
    const QMap<int, QMap<int, FrameMarker>>& allMarkers() const { return mMarkers; }
    void clearMarkers(int layerId) { mMarkers.remove(layerId); }

private:
    QMap<int, QMap<int, FrameMarker>> mMarkers;
    int mCurrentFrame = 1;
    QColor mCurrentColor{ 0, 0, 0, 255 };
    int mCurrentLayer = 0;
    // view manager
    QTransform mCurrentView;

    // playback manager
    int  mFps = 12;
    bool mIsLoop = false;
    bool mIsPingPong = false;
    bool mIsRangedPlayback = false;
    int  mMarkInFrame = 1;
    int  mMarkOutFrame = 10;
};

#endif // EDITORSTATE_H
