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

#ifndef KeyFrame_H
#define KeyFrame_H

#include <cstdint>
#include <vector>
#include <memory>
#include <QString>
#include <QTransform>
#include "pencilerror.h"
#include "cameraeasingtype.h"
class KeyFrameEventListener;

/// A key's drawing transform: translate, rotate (degrees) and uniform scale about the canvas origin.
struct TweenState
{
    qreal tx = 0;
    qreal ty = 0;
    qreal rot = 0;
    qreal scale = 1;

    QTransform transform() const { return QTransform().translate(tx, ty).rotate(rot).scale(scale, scale); }
    bool isIdentity() const { return tx == 0 && ty == 0 && rot == 0 && scale == 1; }
};


class KeyFrame
{
public:
    explicit KeyFrame();
    explicit KeyFrame(const KeyFrame& k2);
    virtual ~KeyFrame();

    KeyFrame& operator=(const KeyFrame& k2);

    int  pos() const { return mFrame; }
    void setPos(int position) { mFrame = position; }

    int length() const { return mLength; }
    void setLength(int len) { mLength = len; }

    void modification() { mIsModified = true; }
    void setModified(bool b) { mIsModified = b; }
    bool isModified() const { return mIsModified; }

    /** With tween on, the frames this key holds interpolate its state to the next key's. */
    bool tween() const { return mTween; }
    void setTween(bool tween) { mTween = tween; }
    CameraEasingType tweenEasing() const { return mTweenEasing; }
    void setTweenEasing(CameraEasingType easing) { mTweenEasing = easing; }
    TweenState tweenState() const { return mTweenState; }
    void setTweenState(const TweenState& state) { mTweenState = state; }

    QString fileName() const { return mAttachedFileName; }
    void    setFileName(QString strFileName) { mAttachedFileName = strFileName; }

    void addEventListener(KeyFrameEventListener*);
    void removeEventListner(KeyFrameEventListener*);

    virtual KeyFrame* clone() const { return nullptr; }
    /** Loads the backing file into memory if it exists, otherwise it does nothing */
    virtual void loadFile() {}

    /** Unloads the data from memory in order to save memory. This will only work if a backing file exists
     *  and the latest keyframe data has been saved to disk, eg. `isModified` is false */
    virtual void unloadFile() {}

    /** A check to ensure that the keyframe holds valid data.
     *  @return `true` if the keyframe holds valid data, otherwise false. */
    virtual bool isLoaded() const { return false; }

    virtual quint64 memoryUsage() { return 0; }

private:
    int mFrame = -1;
    int mLength = 1;
    bool mIsModified = true;
    bool mTween = false;
    CameraEasingType mTweenEasing = CameraEasingType::LINEAR;
    TweenState mTweenState;
    QString mAttachedFileName;

    std::vector<KeyFrameEventListener*> mEventListeners;
};

class KeyFrameEventListener
{
public:
    virtual void onKeyFrameDestroy(KeyFrame*) = 0;
};

#endif // KeyFrame_H
