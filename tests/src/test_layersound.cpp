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
#include "catch.hpp"

#include "layersound.h"
#include "soundclip.h"
#include "util.h"

#include <memory>
#include <QDir>
#include <QDomElement>
#include <QTemporaryDir>

TEST_CASE("Load sound layer from XML")
{
    std::unique_ptr<Layer> soundLayer(new LayerSound(1));
    QTemporaryDir dataDir;
    REQUIRE(dataDir.isValid());
    QDomDocument doc;
    doc.setContent(QString("<layer id='1' name='Sound Layer' visibility='1'></layer>"));
    QDomElement layerElem = doc.documentElement();
    ProgressCallback nullCallback = []() {};

    auto createFrame = [&layerElem, &doc, &dataDir](QString src = "sound_001.wav", int frame = 1)
    {
        QDomElement clipElem = doc.createElement("sound");
        clipElem.setAttribute("src", src);
        clipElem.setAttribute("frame", frame);
        layerElem.appendChild(clipElem);
        if (QDir(src).isAbsolute()) {
            src = QDir(dataDir.path()).relativeFilePath(src);
        }
        QFile soundFile(dataDir.filePath(src));
        soundFile.open(QIODevice::WriteOnly);
        soundFile.close();
    };

    SECTION("No clips")
    {
        soundLayer->loadDomElement(layerElem, dataDir.path(), []() {});

        REQUIRE(soundLayer->keyFrameCount() == 0);
    }

    SECTION("Single clip")
    {
        createFrame("sound_001.wav", 1);

        soundLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(soundLayer->keyFrameCount() == 1);
        SoundClip* frame = static_cast<SoundClip*>(soundLayer->getKeyFrameAt(1));
        REQUIRE(frame != nullptr);
        REQUIRE(closestCanonicalPath(frame->fileName()) == closestCanonicalPath(dataDir.filePath("sound_001.wav")));
    }

    SECTION("Multiple clips")
    {
        createFrame("sound_001.wav", 1);
        createFrame("sound_002.wav", 2);

        soundLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(soundLayer->keyFrameCount() == 2);
        for (int i = 1; i <= 2; i++)
        {
            SoundClip* frame = static_cast<SoundClip*>(soundLayer->getKeyFrameAt(i));
            REQUIRE(frame != nullptr);
            REQUIRE(closestCanonicalPath(frame->fileName()) == closestCanonicalPath(dataDir.filePath(QString("sound_%1.wav").arg(QString::number(i), 3, QChar('0')))));
        }
    }

    SECTION("Clip with absolute src")
    {
        createFrame(QDir(dataDir.filePath("sound_001.wav")).absolutePath());

        soundLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(soundLayer->keyFrameCount() == 0);
    }

    SECTION("Clip src outside of data dir")
    {
        QTemporaryDir otherDir;
        createFrame(QDir(dataDir.path()).relativeFilePath(QDir(otherDir.filePath("sound_001.wav")).absolutePath()));

        soundLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(soundLayer->keyFrameCount() == 0);
    }

    SECTION("Clip src nested in data dir")
    {
        REQUIRE(QDir(dataDir.path()).mkdir("subdir"));
        createFrame("subdir/sound_001.wav");

        soundLayer->loadDomElement(layerElem, dataDir.path(), nullCallback);

        REQUIRE(soundLayer->keyFrameCount() == 1);
        SoundClip* frame = static_cast<SoundClip*>(soundLayer->getKeyFrameAt(1));
        REQUIRE(frame != nullptr);
        REQUIRE(closestCanonicalPath(frame->fileName()) == closestCanonicalPath(dataDir.filePath("subdir/sound_001.wav")));
    }
}

TEST_CASE("Sound clip waveform peaks line up with frames")
{
    SoundClip clip;
    clip.setPos(5);

    SECTION("One peak per frame at the project fps")
    {
        // 1 s of stereo at 1000 Hz, silent except one spike at 500 ms on the right channel.
        QVector<float> samples(2 * 1000, 0.f);
        samples[2 * 500 + 1] = 0.75f;
        samples[2 * 500 + 3] = -0.5f;
        clip.setSamples(samples, 2, 1000);

        const QVector<SoundClip::Peak> peaks24 = clip.waveformPeaks(24);
        REQUIRE(peaks24.size() == 24);
        for (int i = 0; i < peaks24.size(); i++)
        {
            INFO("frame " << i);
            const bool spike = (i == 12); // 0.5 s * 24 fps
            CHECK(peaks24[i].max == (spike ? 0.75f : 0.f));
            CHECK(peaks24[i].min == (spike ? -0.5f : 0.f));
        }

        const QVector<SoundClip::Peak> peaks10 = clip.waveformPeaks(10);
        REQUIRE(peaks10.size() == 10);
        CHECK(peaks10[5].max == 0.75f);
        CHECK(peaks10[4].max == 0.f);
    }

    SECTION("A partial last frame gets its own peak")
    {
        QVector<float> samples(1050, 0.f);
        samples[1049] = 1.f;
        clip.setSamples(samples, 1, 1000);

        const QVector<SoundClip::Peak> peaks = clip.waveformPeaks(10);
        REQUIRE(peaks.size() == 11);
        CHECK(peaks[10].max == 1.f);
    }

    SECTION("No samples gives no peaks")
    {
        CHECK(clip.waveformPeaks(24).isEmpty());
    }
}

TEST_CASE("Sound clip scrub position starts at the clip frame")
{
    SoundClip clip;
    clip.setPos(1);
    CHECK(clip.msAtFrame(1, 24) == 0);
    CHECK(clip.msAtFrame(13, 24) == 500);
    clip.setPos(11);
    CHECK(clip.msAtFrame(11, 10) == 0);
    CHECK(clip.msAtFrame(16, 10) == 500);
}
