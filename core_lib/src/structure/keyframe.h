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

/// A key's drawing state: translate, rotate (degrees) and uniform scale about the canvas origin, and opacity.
struct TweenState
{
    qreal tx = 0;
    qreal ty = 0;
    qreal rot = 0;
    qreal scale = 1;
    qreal opacity = 1;

    QTransform transform() const { return QTransform().translate(tx, ty).rotate(rot).scale(scale, scale); }
    /// True when the transform is identity; opacity is not part of it.
    bool isIdentity() const { return tx == 0 && ty == 0 && rot == 0 && scale == 1; }
};


/// The Panel fields of a storyboard panel (GLOSSARY.md), saved on its <image> element.
struct PanelFields
{
    QString scene;
    QString cut;
    QString notes;
    QString audio;

    bool operator==(const PanelFields& o) const { return scene == o.scene && cut == o.cut && notes == o.notes && audio == o.audio; }
    bool operator!=(const PanelFields& o) const { return !(*this == o); }
};

/// A Drawing's role in the motion (GLOSSARY.md, Mark).
enum class Mark { None, Key, Breakdown, Inbetween };

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
    /** The key's image opacity, saved as the <image> opacity attribute. */
    qreal getOpacity() const { return mTweenState.opacity; }
    void setOpacity(qreal opacity) { mTweenState.opacity = opacity; }

    Mark mark() const { return mMark; }
    void setMark(Mark mark) { mMark = mark; }

    /// Keys of one layer with the same non-zero link id are one Linked Drawing (GLOSSARY.md).
    int linkId() const { return mLinkId; }
    void setLinkId(int id) { mLinkId = id; }

    PanelFields panelFields() const { return mPanelFields; }
    void setPanelFields(const PanelFields& fields) { mPanelFields = fields; }
    /// Takes the image of `source`, a key of the same type, and keeps this key's position and attributes.
    virtual void copyImageFrom(KeyFrame* source) { Q_UNUSED(source) }

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
    Mark mMark = Mark::None;
    int mLinkId = 0;
    PanelFields mPanelFields;
    QString mAttachedFileName;

    std::vector<KeyFrameEventListener*> mEventListeners;
};

/// A set of Marks that onion skin and Flip follow. A default filter passes every Drawing;
/// a filter with a set of Marks, even an empty set, passes only Drawings with a Mark in it.
class MarkFilter
{
public:
    MarkFilter() = default;
    MarkFilter(std::initializer_list<Mark> marks) : mAll(false) { for (Mark m : marks) { set(m, true); } }
    static MarkFilter fromInt(int bits) { MarkFilter f; f.mAll = false; f.mBits = static_cast<unsigned>(bits); return f; }
    int toInt() const { return static_cast<int>(mBits); }

    void set(Mark mark, bool on) { mAll = false; mBits = on ? (mBits | bit(mark)) : (mBits & ~bit(mark)); }
    bool contains(Mark mark) const { return mBits & bit(mark); }
    bool passesAll() const { return mAll; }
    bool passes(const KeyFrame* key) const { return mAll || contains(key->mark()); }

private:
    static unsigned bit(Mark mark) { return 1u << static_cast<unsigned>(mark); }
    unsigned mBits = 0;
    bool mAll = true;
};

class KeyFrameEventListener
{
public:
    virtual void onKeyFrameDestroy(KeyFrame*) = 0;
};

#endif // KeyFrame_H
