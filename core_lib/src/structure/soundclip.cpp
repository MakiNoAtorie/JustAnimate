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

#include "soundclip.h"

#include <QFile>
#include <QMediaPlayer>
#include <QtMath>
#include "soundplayer.h"

SoundClip::SoundClip()
{
}

SoundClip::SoundClip(const SoundClip& s2) : KeyFrame(s2)
{
    mOriginalSoundClipName = s2.mOriginalSoundClipName;
    setSamples(s2.mSamples, s2.mChannels, s2.mSampleRate);
}

SoundClip::~SoundClip()
{
    //QFile::remove( fileName() );
}

SoundClip& SoundClip::operator=(const SoundClip& a)
{
    if (this == &a)
    {
        return *this; // a self-assignment
    }

    KeyFrame::operator=(a);
    mOriginalSoundClipName = a.mOriginalSoundClipName;
    // An undo copy taken before decoding finished must not erase the waveform.
    if (a.hasSamples()) { setSamples(a.mSamples, a.mChannels, a.mSampleRate); }
    return *this;
}

SoundClip* SoundClip::clone() const
{
    // Question: need to copy the file?
    // The audio files are not allowed to be edited in Pencil2D, it should be file for now.
    return new SoundClip(*this);
}

Status SoundClip::init(const QString& strSoundFile)
{
    if (strSoundFile.isEmpty())
    {
        return Status::FAIL;
    }
    setFileName(strSoundFile);
    return Status::OK;
}

bool SoundClip::isValid() const
{
    if (fileName().isEmpty())
    {
        return false;
    }

    if (!mPlayer)
    {
        return false;
    }

    return true;
}

void SoundClip::attachPlayer(SoundPlayer* player)
{
    Q_ASSERT( player != nullptr );
    mPlayer.reset(player);
}

void SoundClip::play()
{
    if (mPlayer)
    {
        mPlayer->play();
    }
}

qint64 SoundClip::msAtFrame(int frameNumber, int fps) const
{
    return qRound64((frameNumber - pos()) * 1000.0 / fps);
}

void SoundClip::playFromPosition(int frameNumber, int fps)
{
    if (mPlayer)
    {
        mPlayer->setMediaPlayerPosition(msAtFrame(frameNumber, fps));
        mPlayer->play();
    }
}

void SoundClip::setSamples(const QVector<float>& samples, int channels, int sampleRate)
{
    mSamples = samples;
    mChannels = qMax(1, channels);
    mSampleRate = sampleRate;
    mPeaksFps = 0;
}

QVector<SoundClip::Peak> SoundClip::waveformPeaks(int fps) const
{
    if (fps <= 0 || mSampleRate <= 0) { return {}; }
    if (fps == mPeaksFps) { return mPeaks; }

    const qint64 frames = mSamples.size() / mChannels;
    const qint64 count = (frames * fps + mSampleRate - 1) / mSampleRate;
    QVector<Peak> peaks(count);
    for (qint64 i = 0; i < count; i++)
    {
        // Integer bounds keep frame i at sample i * rate / fps with no drift.
        const qint64 begin = i * mSampleRate / fps * mChannels;
        const qint64 end = qMin<qint64>((i + 1) * mSampleRate / fps * mChannels, mSamples.size());
        Peak& p = peaks[i];
        for (qint64 s = begin; s < end; s++)
        {
            p.min = qMin(p.min, mSamples[s]);
            p.max = qMax(p.max, mSamples[s]);
        }
    }
    mPeaks = peaks;
    mPeaksFps = fps;
    return peaks;
}

void SoundClip::pause()
{
    if (mPlayer)
    {
        mPlayer->pause();
    }
}

void SoundClip::stop()
{
    if (mPlayer)
    {
        mPlayer->stop();
    }
}

int64_t SoundClip::duration() const
{
    return mDuration;
}

void SoundClip::setDuration(const int64_t& duration)
{
    mDuration = duration;
}

void SoundClip::updateLength(int fps)
{
    setLength(qCeil(mDuration * fps / 1000.0));
}
