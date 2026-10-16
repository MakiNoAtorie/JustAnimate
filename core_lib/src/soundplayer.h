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

#ifndef SOUNDPLAYER_H
#define SOUNDPLAYER_H

#include <memory>
#include <QObject>
#include <QBuffer>
#include <QVector>
#include "pencilerror.h"
#include "keyframe.h"

class SoundClip;
class QMediaPlayer;

class SoundPlayer : public QObject, public KeyFrameEventListener
{
    Q_OBJECT
public:
    SoundPlayer();
    ~SoundPlayer() override;

    void init(SoundClip*);
    void onKeyFrameDestroy(KeyFrame*) override;
    bool isValid();

    void play();
    void pause();
    void stop();
    bool isPlaying() const;

    int64_t duration();
    SoundClip* clip() { return mSoundClip; }

    void setMediaPlayerPosition(qint64 pos);

signals:
    void durationChanged(SoundPlayer*, int64_t duration);
    void waveformReady(SoundPlayer*);

private:
    void makeConnections();
    void decodeWaveform();

    SoundClip* mSoundClip = nullptr;
    QMediaPlayer* mMediaPlayer = nullptr;
    // Set on play() at once: the media player stays stopped until it loads the file.
    bool mPlaying = false;
    QBuffer mBuffer;
    QVector<float> mDecoded;
    int mChannels = 1;
    int mSampleRate = 0;
};

#endif // SOUNDPLAYER_H
