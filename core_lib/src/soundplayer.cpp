/*

Pencil2D - Traditional Animation Software
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "soundplayer.h"
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QFile>
#include <QDebug>
#include "soundclip.h"
#include "util.h"

SoundPlayer::SoundPlayer()
{
}

SoundPlayer::~SoundPlayer()
{
    if (mMediaPlayer)
    {
        // Qt 6.4 GStreamer links outputs from its streaming thread while it loads; ~QMediaPlayer rewires them
        // without a lock. An empty source stops the pipeline and joins that thread first.
        mMediaPlayer->disconnect();
        mMediaPlayer->setSource(QUrl());
    }
    // Members die before child objects: delete the player while mBuffer, its source device, still exists.
    delete mMediaPlayer;
}

void SoundPlayer::init(SoundClip* clip)
{
    Q_ASSERT(clip != nullptr);
    mSoundClip = clip;

    mMediaPlayer = new QMediaPlayer(this);

    QFile file(clip->fileName());
    file.open(QIODevice::ReadOnly);

    mBuffer.setData(file.readAll());
    mBuffer.open(QBuffer::ReadOnly);

    mMediaPlayer->setAudioOutput(new QAudioOutput(this));
    mMediaPlayer->setSourceDevice(&mBuffer, QUrl::fromLocalFile(clip->fileName()));
    makeConnections();
    decodeWaveform();

    clip->attachPlayer(this);
}

void SoundPlayer::onKeyFrameDestroy(KeyFrame* keyFrame)
{
    Q_UNUSED(keyFrame)
}

bool SoundPlayer::isValid()
{
    if (mMediaPlayer)
    {
        return (mMediaPlayer->error() == QMediaPlayer::NoError);
    }
    return false;
}

void SoundPlayer::play()
{
    if (mMediaPlayer)
    {
        mMediaPlayer->play();
        mPlaying = true;
    }
}

void SoundPlayer::pause()
{
    if (mMediaPlayer)
    {
        mMediaPlayer->pause();
        mPlaying = false;
    }
}

void SoundPlayer::stop()
{
    if (mMediaPlayer)
    {
        mMediaPlayer->stop();
        mPlaying = false;
    }
}

bool SoundPlayer::isPlaying() const
{
    return mPlaying;
}

int64_t SoundPlayer::duration()
{
    if (mMediaPlayer)
    {
        return mMediaPlayer->duration();
    }
    return 0;
}

void SoundPlayer::setMediaPlayerPosition(qint64 pos)
{
    if (mMediaPlayer)
    {
        mMediaPlayer->setPosition(pos);
    }
}

void SoundPlayer::makeConnections()
{
    connect(mMediaPlayer, &QMediaPlayer::errorOccurred, this, [](QMediaPlayer::Error err, const QString&)
    {
        qDebug() << "MediaPlayer Error: " << err;
    });

    connect(mMediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state)
    {
        if (state == QMediaPlayer::StoppedState) { mPlaying = false; }
    });

    connect(mMediaPlayer, &QMediaPlayer::durationChanged, [this](qint64 duration)
    {
        qDebug() << "MediaPlayer durationChanged :" << duration;
        emit durationChanged(this, duration);
    });
}

void SoundPlayer::decodeWaveform()
{
    auto decoder = new QAudioDecoder(this);
    connect(decoder, &QAudioDecoder::bufferReady, this, [this, decoder]
    {
        const QAudioBuffer buffer = decoder->read();
        const QAudioFormat format = buffer.format();
        if (!format.isValid()) { return; }
        mChannels = format.channelCount();
        mSampleRate = format.sampleRate();
        const char* data = buffer.constData<char>();
        const qsizetype count = buffer.frameCount() * mChannels;
        const int step = format.bytesPerSample();
        mDecoded.reserve(mDecoded.size() + count);
        for (qsizetype i = 0; i < count; i++)
        {
            mDecoded.append(format.normalizedSampleValue(data + i * step));
        }
    });
    connect(decoder, &QAudioDecoder::finished, this, [this, decoder]
    {
        if (mSoundClip) { mSoundClip->setSamples(mDecoded, mChannels, mSampleRate); }
        mDecoded = {};
        decoder->deleteLater();
        emit waveformReady(this);
    });
    connect(decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), decoder, &QObject::deleteLater);
    decoder->setSource(QUrl::fromLocalFile(mSoundClip->fileName()));
    decoder->start();
}
