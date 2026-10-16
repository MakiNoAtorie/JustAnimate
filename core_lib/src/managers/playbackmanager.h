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

#ifndef PLAYBACKMANAGER_H
#define PLAYBACKMANAGER_H

#include "basemanager.h"
#include <QVector>

class QTimer;
class QElapsedTimer;
class SoundClip;


class PlaybackManager : public BaseManager
{
    Q_OBJECT
public:
    explicit PlaybackManager(Editor* editor);
    ~PlaybackManager() override;

    bool init() override;
    Status load(Object*) override;
    Status save(Object*) override;

    bool isPlaying();
    bool isLooping() { return mIsLooping; }
    bool isPingPong() const { return mIsPingPong; }
    double speed() const { return mSpeed; }
    int frameIntervalMs() const;
    void advance();
    void setCheckForSoundsHalfway(bool newCheck) { mCheckForSoundsHalfway = newCheck; }

    void play();
    void stop();
    void playFlipRoll();
    void playFlipInBetween();
    void playScrub(int frame);
    void setSoundScrubMsec(int mSec) { mMsecSoundScrub = mSec; }
    void setSoundScrubActive(bool b) { mSoundScrub = b; }
    bool getSoundScrubActive() { return mSoundScrub; }

    int fps() { return mFps; }
    int startFrame() { return mStartFrame; }
    int endFrame() { return mEndFrame; }

    bool isRangedPlaybackOn() { return mIsRangedPlayback; }
    int markInFrame() { return mMarkInFrame; }
    int markOutFrame() { return mMarkOutFrame; }

    void setFps(int fps);
    void setLooping(bool isLoop);
    void setPingPong(bool b);
    void setSpeed(double speed);
    void enableRangedPlayback(bool b);
    void setRangedStartFrame(int frame);
    void setRangedEndFrame(int frame);
    void enableSound(bool b);

    void stopSounds();

    // Pen-drag Flip: while the Flip key is held, a horizontal pen drag steps through Drawings.
    static constexpr int kPenFlipPixels = 20; // viewport pixels per Drawing
    void beginPenFlip();
    void penFlipPress(qreal x);
    void penFlipMove(qreal x);
    void endPenFlip();
    bool isPenFlipping() const { return mPenFlipOrigin > 0; }

private slots:
    void stopScrubPlayback();

signals:
    void fpsChanged(int fps);
    void loopStateChanged(bool b);
    void pingPongChanged(bool b);
    void speedChanged(double speed);
    void rangedPlaybackStateChanged(bool b);
    void playStateChanged(bool isPlaying);

private:
    void timerTick();
    void flipTimerTick();
    void playSounds(int frame);
    bool skipFrame();

    int mStartFrame = 1;
    int mEndFrame = 60;

    void updateStartFrame();
    void updateEndFrame();

    bool mIsLooping = false;
    bool mIsPingPong = false;
    bool mIsReversing = false;
    double mSpeed = 1.0;
    bool mIsPlaySound = true;

    bool mIsRangedPlayback = false;
    int mMarkInFrame = 1;
    int mMarkOutFrame = 10;
    int mActiveSoundFrame = 0;

    int mFps = 12;

    int mFlipRollInterval = 100;
    int mFlipInbetweenInterval = 100;
    int mFlipRollMax = 5;
    int mMsecSoundScrub = 100;
    bool mSoundScrub = false;

    QTimer* mTimer = nullptr;
    QTimer* mFlipTimer = nullptr;
    QTimer* mScrubTimer = nullptr;
    QElapsedTimer* mElapsedTimer = nullptr;
    int mPlayingFrameCounter = 0; // how many frames has passed after pressing play

    bool mCheckForSoundsHalfway = false;
    QVector<int> mListOfActiveSoundFrames;
    QVector<SoundClip*> mSoundclipsToPLay;
    QVector<int> mFlipList;

    int mPenFlipOrigin = 0; // frame the flip started on; 0 = not flipping
    int mPenFlipDragFrame = 0;
    qreal mPenFlipDragX = 0;
};

#endif // PLAYBACKMANAGER_H
