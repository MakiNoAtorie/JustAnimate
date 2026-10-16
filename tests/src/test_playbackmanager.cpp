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

#include <memory>
#include <vector>
#include <QTemporaryDir>
#include "editor.h"
#include "filemanager.h"
#include "object.h"
#include "playbackmanager.h"
#include "testfixture.h"

namespace
{
// Plays range 2..5 and returns the frames shown after each step until playback stops or maxSteps.
std::vector<int> playRange(PlaybackManager* pb, Editor* editor, int maxSteps)
{
    pb->enableRangedPlayback(true);
    pb->setRangedStartFrame(2);
    pb->setRangedEndFrame(5);
    pb->play();
    std::vector<int> frames{ editor->currentFrame() };
    for (int i = 0; i < maxSteps && pb->isPlaying(); i++)
    {
        pb->advance();
        if (pb->isPlaying()) { frames.push_back(editor->currentFrame()); }
    }
    pb->stop();
    return frames;
}
}

TEST_CASE("PlaybackManager frame sequence")
{
    SettingsGuard guard({ SETTING_FPS });
    TestSession s;
    s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    PlaybackManager* pb = s.editor->playback();
    pb->setLooping(false);
    pb->setPingPong(false);

    SECTION("Plays once through the range and stops")
    {
        REQUIRE(playRange(pb, s.editor.get(), 20) == std::vector<int>{ 2, 3, 4, 5 });
    }

    SECTION("Loop restarts at the range start")
    {
        pb->setLooping(true);
        REQUIRE(playRange(pb, s.editor.get(), 6) == std::vector<int>{ 2, 3, 4, 5, 2, 3, 4 });
    }

    SECTION("Ping-pong reverses at both ends")
    {
        pb->setPingPong(true);
        REQUIRE(playRange(pb, s.editor.get(), 9) == std::vector<int>{ 2, 3, 4, 5, 4, 3, 2, 3, 4, 5 });
    }
}

TEST_CASE("PlaybackManager speed scales the frame interval")
{
    SettingsGuard guard({ SETTING_FPS });
    TestSession s;
    s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    PlaybackManager* pb = s.editor->playback();
    pb->setFps(10);

    pb->setSpeed(1.0);
    REQUIRE(pb->frameIntervalMs() == 100);
    pb->setSpeed(0.5);
    REQUIRE(pb->frameIntervalMs() == 200);
    pb->setSpeed(2.0);
    REQUIRE(pb->frameIntervalMs() == 50);
}

TEST_CASE("PlaybackManager ping-pong is saved and optional")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("pingpong.pclx");
    FileManager fm;
    TestSession s;
    s.object->addNewCameraLayer();
    REQUIRE(s.start());
    PlaybackManager* pb = s.editor->playback();

    SECTION("A file saved without ping-pong loads with it off")
    {
        pb->setPingPong(false);
        pb->save(s.object);
        REQUIRE(fm.save(s.object, path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        REQUIRE_FALSE(loaded->data()->isPingPong());
    }

    SECTION("Ping-pong survives save and load")
    {
        pb->setPingPong(true);
        pb->save(s.object);
        REQUIRE(fm.save(s.object, path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->data()->isPingPong());
        pb->setPingPong(false);
        REQUIRE(pb->load(loaded.get()).ok());
        REQUIRE(pb->isPingPong());
    }
}
