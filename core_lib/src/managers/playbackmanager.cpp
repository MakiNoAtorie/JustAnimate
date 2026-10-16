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

#include "playbackmanager.h"

#include <cstdlib>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <QSettings>
#include "object.h"
#include "editor.h"
#include "layersound.h"
#include "layermanager.h"
#include "soundclip.h"
#include "toolmanager.h"
#include "pencilsettings.h"
#include "preferencemanager.h"


PlaybackManager::PlaybackManager(Editor* editor) : BaseManager(editor, __FUNCTION__)
{
}

PlaybackManager::~PlaybackManager()
{
    delete mElapsedTimer;
}

bool PlaybackManager::init()
{
    mTimer = new QTimer(this);
    mTimer->setTimerType(Qt::PreciseTimer);

    mFlipTimer = new QTimer(this);
    mFlipTimer->setTimerType(Qt::PreciseTimer);

    mScrubTimer = new QTimer(this);
    mScrubTimer->setTimerType(Qt::PreciseTimer);
    mScrubTimer->setSingleShot(true);
    connect(mScrubTimer, &QTimer::timeout, this, &PlaybackManager::stopScrubPlayback);
    mSoundclipsToPLay.clear();

    QSettings& settings = pencilSettings();
    mFps = settings.value(SETTING_FPS).toInt();
    mMsecSoundScrub = settings.value(SETTING_SOUND_SCRUB_MSEC).toInt();
    if (mMsecSoundScrub == 0) { mMsecSoundScrub = 100; }
    mSoundScrub = settings.value(SETTING_SOUND_SCRUB_ACTIVE).toBool();
    mPlayAroundFrames = qMax(1, settings.value(SETTING_PLAY_AROUND_FRAMES, 5).toInt());

    mElapsedTimer = new QElapsedTimer;
    connect(mTimer, &QTimer::timeout, this, &PlaybackManager::timerTick);
    connect(mFlipTimer, &QTimer::timeout, this, &PlaybackManager::flipTimerTick);
    return true;
}

Status PlaybackManager::load(Object* o)
{
    const ObjectData* data = o->data();

    mIsLooping = data->isLooping();
    mIsPingPong = data->isPingPong();
    mIsRangedPlayback = data->isRangedPlayback();
    mMarkInFrame = data->getMarkInFrameNumber();
    mMarkOutFrame = data->getMarkOutFrameNumber();
    mFps = data->getFrameRate();

    updateStartFrame();
    updateEndFrame();

    return Status::OK;
}

Status PlaybackManager::save(Object* o)
{
    ObjectData* data = o->data();
    data->setLooping(mIsLooping);
    data->setPingPong(mIsPingPong);
    data->setRangedPlayback(mIsRangedPlayback);
    data->setMarkInFrameNumber(mMarkInFrame);
    data->setMarkOutFrameNumber(mMarkOutFrame);
    data->setFrameRate(mFps);
    data->setCurrentFrame(editor()->currentFrame());
    return Status::OK;
}

bool PlaybackManager::isPlaying()
{
    return (mTimer->isActive() || mFlipTimer->isActive());
}

void PlaybackManager::play()
{
    updateStartFrame();
    updateEndFrame();

    int frame = editor()->currentFrame();
    if (frame >= mEndFrame || frame < mStartFrame)
    {
        editor()->scrubTo(mStartFrame);
        frame = editor()->currentFrame();
    }

    mListOfActiveSoundFrames.clear();
    // Check for any sounds we should start playing part-way through.
    mCheckForSoundsHalfway = true;
    playSounds(frame);

    mIsReversing = false;
    mTimer->setInterval(frameIntervalMs());
    mTimer->start();

    // for error correction, please ref skipFrame()
    mPlayingFrameCounter = 1;
    mElapsedTimer->start();

    emit playStateChanged(true);
}

void PlaybackManager::stop()
{
    mTimer->stop();
    stopSounds();
    if (mPlayAroundOrigin > 0)
    {
        const int origin = mPlayAroundOrigin;
        mPlayAroundOrigin = 0;
        updateStartFrame();
        updateEndFrame();
        editor()->scrubTo(origin);
    }
    emit playStateChanged(false);
}

void PlaybackManager::playAround()
{
    if (isPlaying()) { stop(); return; }
    mPlayAroundOrigin = editor()->currentFrame();
    updateStartFrame();
    editor()->scrubTo(mStartFrame);
    play();
}

void PlaybackManager::setPlayAroundFrames(int n)
{
    mPlayAroundFrames = qMax(1, n);
    pencilSettings().setValue(SETTING_PLAY_AROUND_FRAMES, mPlayAroundFrames);
}

void PlaybackManager::setLoopRange(int from, int to)
{
    mMarkInFrame = qMax(1, qMin(from, to));
    mMarkOutFrame = qMax(from, to);
    enableRangedPlayback(true);
    updateStartFrame();
    updateEndFrame();
    setLooping(true);
}

void PlaybackManager::playFlipRoll()
{
    if (isPlaying()) { return; }

    int start = editor()->currentFrame();
    int tmp = start;
    mFlipList.clear();
    QSettings& settings = pencilSettings();
    mFlipRollMax = settings.value(SETTING_FLIP_ROLL_DRAWINGS).toInt();
    for (int i = 0; i < mFlipRollMax; i++)
    {
        int prev = editor()->layers()->currentLayer()->getPreviousKeyFramePosition(tmp);
        if (prev < tmp)
        {
            mFlipList.prepend(prev);
            tmp = prev;
        }
    }
    if (mFlipList.isEmpty()) { return; }

    // run the roll...
    mFlipRollInterval = settings.value(SETTING_FLIP_ROLL_MSEC).toInt();
    mFlipList.append(start);
    mFlipTimer->setInterval(mFlipRollInterval);

    editor()->scrubTo(mFlipList[0]);
    mFlipTimer->start();
    emit playStateChanged(true);
}

void PlaybackManager::playFlipInBetween()
{
    if (isPlaying()) { return; }

    LayerManager* layerMgr = editor()->layers();
    int start = editor()->currentFrame();

    int prev = layerMgr->currentLayer()->getPreviousKeyFramePosition(start);
    int next = layerMgr->currentLayer()->getNextKeyFramePosition(start);

    if (prev < start && next > start &&
            layerMgr->currentLayer()->keyExists(prev) &&
            layerMgr->currentLayer()->keyExists(next))
    {
        mFlipList.clear();
        mFlipList.append(prev);
        mFlipList.append(prev);
        mFlipList.append(start);
        mFlipList.append(next);
        mFlipList.append(next);
        mFlipList.append(start);
    }
    else
    {
        return;
    }
    // run the flip in-between...
    QSettings& settings = pencilSettings();
    mFlipInbetweenInterval = settings.value(SETTING_FLIP_INBETWEEN_MSEC).toInt();

    mFlipTimer->setInterval(mFlipInbetweenInterval);
    editor()->scrubTo(mFlipList[0]);
    mFlipTimer->start();
    emit playStateChanged(true);
}

void PlaybackManager::beginPenFlip()
{
    if (isPlaying()) { return; }
    mPenFlipOrigin = editor()->currentFrame();
    mPenFlipDragFrame = mPenFlipOrigin;
}

void PlaybackManager::penFlipPress(qreal x)
{
    mPenFlipDragX = x;
    mPenFlipDragFrame = editor()->currentFrame();
}

void PlaybackManager::penFlipMove(qreal x)
{
    if (!isPenFlipping()) { return; }
    const int steps = static_cast<int>((x - mPenFlipDragX) / kPenFlipPixels);
    const Layer* layer = editor()->layers()->currentLayer();
    const MarkFilter filter = editor()->preference()->markFilter();
    int frame = mPenFlipDragFrame;
    for (int i = 0; i < std::abs(steps); i++)
    {
        const int f = (steps > 0) ? layer->getNextFrameNumber(frame, true, filter)
                                  : layer->getPreviousFrameNumber(frame, true, filter);
        if (f < 0) { break; }
        frame = f;
    }
    if (frame != editor()->currentFrame()) { editor()->scrubTo(frame); }
}

void PlaybackManager::endPenFlip()
{
    if (!isPenFlipping()) { return; }
    const int origin = mPenFlipOrigin;
    mPenFlipOrigin = 0;
    if (origin != editor()->currentFrame()) { editor()->scrubTo(origin); }
}

void PlaybackManager::playScrub(int frame)
{
    if (!mSoundScrub) { return; }

    // A new frame restarts the snippet there, so a fast drag does not skip audio.
    QVector<SoundClip*> clips;
    auto layerMan = editor()->layers();
    for (int i = 0; i < layerMan->count(); i++)
    {
        Layer* layer = layerMan->getLayer(i);
        if (layer->type() == Layer::SOUND && layer->visible())
        {
            KeyFrame* key = layer->getKeyFrameWhichCovers(frame);
            if (key != nullptr)
            {
                SoundClip* clip = static_cast<SoundClip*>(key);
                clips.append(clip);
            }
        }
    }

    for (SoundClip* clip : std::as_const(mSoundclipsToPLay))
    {
        if (!clips.contains(clip)) { clip->pause(); }
    }
    mSoundclipsToPLay = clips;
    if (clips.isEmpty()) { mScrubTimer->stop(); return; }

    mScrubTimer->start(mMsecSoundScrub);
    for (SoundClip* clip : clips)
    {
        clip->playFromPosition(frame, mFps);
    }
}

void PlaybackManager::setFps(int fps)
{
    if (mFps != fps)
    {
        mFps = fps;
        QSettings& settings = pencilSettings();
        settings.setValue(SETTING_FPS, fps);
        emit fpsChanged(mFps);

        // Update key-frame lengths of sound layers,
        // since the length depends on fps.
        for (int i = 0; i < object()->getLayerCount(); ++i)
        {
            Layer* layer = object()->getLayer(i);
            if (layer->type() == Layer::SOUND)
            {
                auto soundLayer = dynamic_cast<LayerSound *>(layer);
                soundLayer->updateFrameLengths(mFps);
            }
        }
    }
}

void PlaybackManager::playSounds(int frame)
{
    // If sound is turned off, don't play anything.
    if (!mIsPlaySound)
    {
        return;
    }

    std::vector< LayerSound* > kSoundLayers;
    for (int i = 0; i < object()->getLayerCount(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (layer->type() == Layer::SOUND)
        {
            kSoundLayers.push_back(static_cast<LayerSound*>(layer));
        }
    }

    for (LayerSound* layer : kSoundLayers)
    {
        KeyFrame* key = layer->getLastKeyFrameAtPosition(frame);

        if (!layer->visible())
        {
            continue;
        }

        if (key != nullptr)
        {
            // add keyframe position to list
            if (key->pos() + key->length() >= frame)
            {
                if (!mListOfActiveSoundFrames.contains(key->pos()))
                {
                    mListOfActiveSoundFrames.append(key->pos());
                }
            }
        }

        if (mCheckForSoundsHalfway)
        {
            // Check for sounds which we should start playing from part-way through.
            for (int i = 0; i < mListOfActiveSoundFrames.count(); i++)
            {
                int listPosition = mListOfActiveSoundFrames.at(i);
                if (layer->keyExistsWhichCovers(listPosition))
                {
                    key = layer->getKeyFrameWhichCovers(listPosition);
                    SoundClip* clip = static_cast<SoundClip*>(key);
                    clip->playFromPosition(frame, mFps);
                }
            }
        }
        else if (layer->keyExists(frame))
        {
            key = layer->getKeyFrameAt(frame);
            SoundClip* clip = static_cast<SoundClip*>(key);

            clip->play();

            // save the position of our active sound frame
            mActiveSoundFrame = frame;
        }

        if (frame >= mEndFrame)
        {
            if (layer->keyExists(mActiveSoundFrame))
            {
                key = layer->getKeyFrameWhichCovers(mActiveSoundFrame);
                SoundClip* clip = static_cast<SoundClip*>(key);
                clip->stop();

                // make sure list is cleared on end
                if (!mListOfActiveSoundFrames.isEmpty())
                    mListOfActiveSoundFrames.clear();
            }
        }
    }

    // Set flag to false, since this check should only be done when
    // starting play-back, or when looping.
    mCheckForSoundsHalfway = false;
}

/**
 * @brief PlaybackManager::skipFrame()
 * Small errors accumulate while playing animation
 * If the error is greater than a frame interval, skip a frame
 */
bool PlaybackManager::skipFrame()
{
    // uncomment these debug outputs to see what happens
    //float expectedTime = (mPlayingFrameCounter) * (1000.f / mFps);
    //qDebug("Expected:  %.2f ms", expectedTime);
    //qDebug("Actual:    %d   ms", mElapsedTimer->elapsed());

    int t = qRound((mPlayingFrameCounter - 1) * (1000.0 / (mFps * mSpeed)));
    if (mElapsedTimer->elapsed() < t)
    {
        qDebug() << "skip";
        return true;
    }

    ++mPlayingFrameCounter;
    return false;
}

void PlaybackManager::stopSounds()
{
    std::vector<LayerSound*> kSoundLayers;

    for (int i = 0; i < object()->getLayerCount(); ++i)
    {
        Layer* layer = object()->getLayer(i);
        if (layer->type() == Layer::SOUND)
        {
            kSoundLayers.push_back(static_cast<LayerSound*>(layer));
        }
    }

    for (LayerSound* layer : kSoundLayers)
    {
        layer->foreachKeyFrame([](KeyFrame* key)
        {
            SoundClip* clip = static_cast<SoundClip*>(key);
            clip->stop();
        });
    }
}

void PlaybackManager::stopScrubPlayback()
{
    for (int i = 0; i < mSoundclipsToPLay.count(); i++)
    {
        mSoundclipsToPLay.at(i)->pause();
    }
    mSoundclipsToPLay.clear();
}

void PlaybackManager::timerTick()
{
    if (skipFrame())
        return;

    advance();
}

int PlaybackManager::frameIntervalMs() const
{
    return qRound(1000.0 / (mFps * mSpeed));
}

// One playback step: forward, or backward while ping-pong is reversing; at an end it reverses, loops or stops.
void PlaybackManager::advance()
{
    const int currentFrame = editor()->currentFrame();

    if (mIsReversing && currentFrame <= mStartFrame)
    {
        mIsReversing = false;
    }

    if (!mIsReversing && currentFrame >= mEndFrame)
    {
        if (mIsPingPong && mEndFrame > mStartFrame)
        {
            mIsReversing = true;
        }
        else if (mIsLooping || mPlayAroundOrigin > 0)
        {
            editor()->scrubTo(mStartFrame);
            mCheckForSoundsHalfway = true;
            return;
        }
        else
        {
            stop();
            return;
        }
    }

    if (mIsReversing)
    {
        editor()->scrubBackward();
        return;
    }

    editor()->scrubForward();
    playSounds(editor()->currentFrame());
}

void PlaybackManager::flipTimerTick()
{
    if (mFlipList.count() < 2 || editor()->currentFrame() != mFlipList[0])
    {
        mFlipTimer->stop();
        editor()->scrubTo(mFlipList.last());
        emit playStateChanged(false);
    }
    else
    {
        editor()->scrubTo(mFlipList[1]);
        mFlipList.removeFirst();
    }
}

void PlaybackManager::setLooping(bool isLoop)
{
    if (mIsLooping != isLoop)
    {
        mIsLooping = isLoop;
        emit loopStateChanged(mIsLooping);
    }
}

void PlaybackManager::setPingPong(bool b)
{
    if (mIsPingPong != b)
    {
        mIsPingPong = b;
        mIsReversing = false;
        emit pingPongChanged(mIsPingPong);
    }
}

void PlaybackManager::setSpeed(double speed)
{
    if (speed <= 0 || mSpeed == speed) { return; }
    mSpeed = speed;
    if (mTimer->isActive())
    {
        // restart the drift correction so the new rate starts from now
        mTimer->setInterval(frameIntervalMs());
        mPlayingFrameCounter = 1;
        mElapsedTimer->restart();
    }
    emit speedChanged(mSpeed);
}

void PlaybackManager::enableRangedPlayback(bool b)
{
    if (mIsRangedPlayback != b)
    {
        mIsRangedPlayback = b;

        updateStartFrame();
        updateEndFrame();

        emit rangedPlaybackStateChanged(mIsRangedPlayback);
    }
}

void PlaybackManager::setRangedStartFrame(int frame)
{
    mMarkInFrame = frame;
    updateStartFrame();
}

void PlaybackManager::setRangedEndFrame(int frame)
{
    mMarkOutFrame = frame;
    updateEndFrame();
}

void PlaybackManager::updateStartFrame()
{
    if (mPlayAroundOrigin > 0) { mStartFrame = qMax(1, mPlayAroundOrigin - mPlayAroundFrames); return; }
    mStartFrame = (mIsRangedPlayback) ? mMarkInFrame : 1;
}

void PlaybackManager::updateEndFrame()
{
    int projectLength = editor()->layers()->animationLength();
    if (mPlayAroundOrigin > 0)
    {
        mEndFrame = qMax(mPlayAroundOrigin, qMin(mPlayAroundOrigin + mPlayAroundFrames, projectLength));
        return;
    }
    mEndFrame = (mIsRangedPlayback) ? mMarkOutFrame : projectLength;
}

void PlaybackManager::enableSound(bool b)
{
    mIsPlaySound = b;

    if (!mIsPlaySound)
    {
        stopSounds();

        // If, during playback, the sound is turned on again,
        // check for sounds partway through.
        mCheckForSoundsHalfway = true;
    }
}
