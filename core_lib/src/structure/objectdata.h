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
};

/// The Paper template of a project and its Title block fields.
struct PaperTemplate
{
    bool on = false;
    QString title, id, name, state, date, notes;
    /// Last typed Timesheet export values that differ from the Title block; empty uses the Title block.
    QString sheetScene, sheetShot, sheetAnimator;

    bool operator==(const PaperTemplate& o) const
    {
        return on == o.on && title == o.title && id == o.id && name == o.name
            && state == o.state && date == o.date && notes == o.notes
            && sheetScene == o.sheetScene && sheetShot == o.sheetShot && sheetAnimator == o.sheetAnimator;
    }
    bool operator!=(const PaperTemplate& o) const { return !(*this == o); }
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

    // layerId: the sound layer of a lip-sync marker; 0 for a global marker.
    void setMarker(int frame, const FrameMarker& m, int layerId = 0) { mMarkers[layerId].insert(frame, m); }
    void removeMarker(int frame, int layerId = 0) { mMarkers[layerId].remove(frame); }
    QMap<int, FrameMarker> markers(int layerId = 0) const { return mMarkers.value(layerId); }
    // Markers by layer id, then by frame. Key 0 holds the global markers.
    const QMap<int, QMap<int, FrameMarker>>& allMarkers() const { return mMarkers; }
    void setMarkers(int layerId, const QMap<int, FrameMarker>& m)
    {
        if (m.isEmpty()) { mMarkers.remove(layerId); } else { mMarkers.insert(layerId, m); }
    }

    const PaperTemplate& paperTemplate() const { return mPaperTemplate; }
    void setPaperTemplate(const PaperTemplate& t) { mPaperTemplate = t; }

private:
    QMap<int, QMap<int, FrameMarker>> mMarkers;
    PaperTemplate mPaperTemplate;
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
