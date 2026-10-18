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
#include <QDir>
#include <QApplication>
#include <QFile>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTabletEvent>
#include <QTemporaryDir>
#include "app_util.h"
#include "editor.h"
#include "fileformat.h"
#include "filemanager.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "layersound.h"
#include "movieexporter.h"
#include "object.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "scribblearea.h"
#include "soundclip.h"
#include "testfixture.h"
#include "toolmanager.h"

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

TEST_CASE("PlaybackManager starts at 12 fps on an empty settings store")
{
    SettingsGuard guard({ SETTING_FPS });
    pencilSettings().remove(SETTING_FPS);
    TestSession s;
    REQUIRE(s.start());
    REQUIRE(s.editor->playback()->fps() == 12);
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
        QFile mainXml(QDir(s.object->workingDir()).filePath(PFF_XML_FILE_NAME));
        REQUIRE(mainXml.open(QIODevice::ReadOnly));
        CHECK(mainXml.readAll().count("<pingPong") == 1);
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->data()->isPingPong());
        pb->setPingPong(false);
        REQUIRE(pb->load(loaded.get()).ok());
        REQUIRE(pb->isPingPong());
    }
}

TEST_CASE("A held frame step key flips at the project fps and single taps always flip", "[PlaybackManager]")
{
    // At 10 fps one flip lasts 100 ms.
    CHECK_FALSE(skipFrameStep(false, 10, 10)); // a tap right after a flip
    CHECK_FALSE(skipFrameStep(false, -1, 10));
    CHECK(skipFrameStep(true, 10, 10));       // a repeat inside the flip
    CHECK_FALSE(skipFrameStep(true, 100, 10));
    CHECK_FALSE(skipFrameStep(true, -1, 10)); // no flip yet
}

namespace
{
void flipKey(ScribbleArea* area, QEvent::Type type, ulong ms)
{
    QKeyEvent e(type, Qt::Key_F, Qt::NoModifier, "f");
    e.setTimestamp(ms);
    QApplication::sendEvent(area, &e);
}

void flipPen(ScribbleArea* area, QEvent::Type type, qreal x, ulong ms)
{
    const QPointF pos(x, 300);
    const Qt::MouseButtons buttons = (type == QEvent::TabletRelease) ? Qt::NoButton : Qt::LeftButton;
    QTabletEvent e(type, QPointingDevice::primaryPointingDevice(), pos, area->mapToGlobal(pos),
                   0.8, 0, 0, 0, 0, 0, Qt::NoModifier, Qt::LeftButton, buttons);
    e.setTimestamp(ms);
    QApplication::sendEvent(area, &e);
}
}

TEST_CASE("Pen-drag Flip steps through Drawings and returns on release", "[PlaybackManager]")
{
    const SettingsGuard guard({ "shortcuts/" CMD_FLIP_HOLD, SETTING_ONION_TYPE, SETTING_ONION_MARKS });
    pencilSettings().setValue("shortcuts/" CMD_FLIP_HOLD, "F");
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(800, 600);
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(PENCIL);
    for (int f : { 3, 5, 7 }) { REQUIRE(layer->addNewKeyFrameAt(f)); }
    editor->preference()->set<SETTING::ONION_TYPE>(QString("absolute"));
    editor->scrubTo(3);
    const int px = PlaybackManager::kPenFlipPixels;

    SECTION("Drag distance sets how many Drawings; release restores the start")
    {
        flipKey(area, QEvent::KeyPress, 1000);
        REQUIRE(editor->playback()->isPenFlipping());
        flipPen(area, QEvent::TabletPress, 400, 1010);
        flipPen(area, QEvent::TabletMove, 400 + 2 * px + 5, 1020);
        CHECK(editor->currentFrame() == 7);
        flipPen(area, QEvent::TabletMove, 400 + px / 2, 1030);
        CHECK(editor->currentFrame() == 3); // under one step: back at the start
        flipPen(area, QEvent::TabletMove, 400 - px - 1, 1040);
        CHECK(editor->currentFrame() == 1);
        flipPen(area, QEvent::TabletMove, 400 - 10 * px, 1050);
        CHECK(editor->currentFrame() == 1); // no Drawing before 1
        flipPen(area, QEvent::TabletRelease, 400 - 10 * px, 1060);
        CHECK(editor->currentFrame() == 1);
        flipKey(area, QEvent::KeyRelease, 1070);
        CHECK_FALSE(editor->playback()->isPenFlipping());
        CHECK(editor->currentFrame() == 3);
        for (int f : { 1, 3, 5, 7 })
        {
            CAPTURE(f);
            CHECK(layer->getBitmapImageAtFrame(f)->bounds().isEmpty()); // the pen did not draw
        }
        CHECK(layer->keyFrameCount() == 4);
    }

    SECTION("With a Mark filter only matching Drawings are visited")
    {
        layer->getKeyFrameAt(1)->setMark(Mark::Key);
        layer->getKeyFrameAt(7)->setMark(Mark::Key);
        editor->preference()->set<SETTING::ONION_TYPE>(QString("marked"));
        editor->preference()->set<SETTING::ONION_MARKS>(MarkFilter({ Mark::Key }).toInt());
        flipKey(area, QEvent::KeyPress, 2000);
        flipPen(area, QEvent::TabletPress, 400, 2010);
        flipPen(area, QEvent::TabletMove, 400 + px, 2020);
        CHECK(editor->currentFrame() == 7);
        flipPen(area, QEvent::TabletMove, 400 - px, 2030);
        CHECK(editor->currentFrame() == 1);
        flipPen(area, QEvent::TabletRelease, 400 - px, 2040);
        flipKey(area, QEvent::KeyRelease, 2050);
        CHECK(editor->currentFrame() == 3);
    }

    SECTION("Without the Flip key the pen draws")
    {
        flipPen(area, QEvent::TabletPress, 400, 3000);
        flipPen(area, QEvent::TabletMove, 450, 3010);
        flipPen(area, QEvent::TabletRelease, 450, 3020);
        CHECK(editor->currentFrame() == 3);
        CHECK_FALSE(layer->getBitmapImageAtFrame(3)->bounds().isEmpty());
    }
}

TEST_CASE("Play-around loops playhead - N to playhead + N and restores the playhead", "[PlaybackManager]")
{
    const SettingsGuard guard({ SETTING_FPS, SETTING_PLAY_AROUND_FRAMES });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    REQUIRE(layer->addNewKeyFrameAt(20));
    Editor* editor = s.editor.get();
    PlaybackManager* pb = editor->playback();
    pb->setLooping(false);
    pb->setPingPong(false);
    pb->setPlayAroundFrames(3);
    CHECK(pencilSettings().value(SETTING_PLAY_AROUND_FRAMES).toInt() == 3);

    auto playAround = [&](int from, int steps) {
        editor->scrubTo(from);
        pb->playAround();
        std::vector<int> frames{ editor->currentFrame() };
        for (int i = 0; i < steps; i++)
        {
            pb->advance();
            frames.push_back(editor->currentFrame());
        }
        REQUIRE(pb->isPlaying());
        pb->playAround(); // the key again stops
        CHECK_FALSE(pb->isPlaying());
        return frames;
    };

    SECTION("Loops the window even with Loop off; stopping restores the playhead")
    {
        CHECK(playAround(10, 8) == std::vector<int>{ 7, 8, 9, 10, 11, 12, 13, 7, 8 });
        CHECK(editor->currentFrame() == 10);
    }

    SECTION("The window is clamped to the project")
    {
        CHECK(playAround(2, 6) == std::vector<int>{ 1, 2, 3, 4, 5, 1, 2 });
        CHECK(editor->currentFrame() == 2);
        CHECK(playAround(19, 5) == std::vector<int>{ 16, 17, 18, 19, 20, 16 });
        CHECK(editor->currentFrame() == 19);
    }

    SECTION("Normal play afterwards uses the whole project again")
    {
        playAround(10, 2);
        pb->play();
        CHECK(pb->endFrame() == 20);
        CHECK(pb->startFrame() == 1);
        pb->stop();
    }
}

TEST_CASE("Setting a loop range turns on the range and Loop", "[PlaybackManager]")
{
    TestSession s;
    s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    PlaybackManager* pb = s.editor->playback();
    pb->setLooping(false);
    pb->enableRangedPlayback(false);
    pb->setLoopRange(9, 4); // dragged right to left
    CHECK(pb->isLooping());
    CHECK(pb->isRangedPlaybackOn());
    CHECK(pb->markInFrame() == 4);
    CHECK(pb->markOutFrame() == 9);
}

namespace
{
// Milliseconds into a clip that the exported movie plays at a frame, read from the audioMix ffmpeg args.
double exportedMsAtFrame(const QStringList& args, int frame, int startFrame, int fps)
{
    const QRegularExpression delay("adelay=(\\d+)S");
    const double delayMs = delay.match(args.join(' ')).captured(1).toDouble() * 1000 / 44100;
    const double trimMs = args.at(args.indexOf("-ss") + 1).toDouble() * 1000;
    return (frame - startFrame) * 1000.0 / fps + trimMs - delayMs;
}
}

TEST_CASE("Preview starts a sound at the offset the export plays at that frame", "[PlaybackManager]")
{
    SettingsGuard guard({ SETTING_FPS });
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("click.wav");
    {
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly));
    }
    TestSession s;
    s.object->addNewBitmapLayer();
    LayerSound* sound = s.object->addNewSoundLayer();
    REQUIRE(s.start());
    PlaybackManager* pb = s.editor->playback();
    pb->setFps(24);
    pb->setPingPong(false);
    SoundClip* clip = new SoundClip;
    clip->setFileName(path);
    clip->setLength(4); // covers frames 5..8
    REQUIRE(sound->addKeyFrame(5, clip));

    std::vector<std::pair<int, qint64>> starts; // frame shown, ms into the clip
    QObject::connect(pb, &PlaybackManager::soundStarted, [&](SoundClip* c, qint64 ms) {
        CHECK(c == clip);
        starts.emplace_back(s.editor->currentFrame(), ms);
    });
    auto play = [&](int from, int to, bool loop, int steps) {
        pb->setLooping(loop);
        pb->enableRangedPlayback(true);
        pb->setRangedStartFrame(from);
        pb->setRangedEndFrame(to);
        pb->play();
        for (int i = 0; i < steps && pb->isPlaying(); i++) { pb->advance(); }
        pb->stop();
        const QStringList args = MovieExporter::audioMix(s.editor->object(), from, to, 24, "out.wav").args;
        for (const auto& st : starts)
        {
            CHECK(std::abs(st.second - exportedMsAtFrame(args, st.first, from, 24)) <= 1.0);
        }
    };
    using Starts = std::vector<std::pair<int, qint64>>;

    SECTION("From the start the clip starts on its key at 0 ms")
    {
        play(1, 10, false, 20);
        REQUIRE(starts == Starts{ { 5, 0 } });
    }
    SECTION("Started part-way the clip seeks to the export offset")
    {
        s.editor->scrubTo(7);
        play(1, 10, false, 20);
        REQUIRE(starts == Starts{ { 7, 83 } });
    }
    SECTION("Each loop pass starts the clip on its key and never early")
    {
        play(3, 8, true, 9); // frames 3..8 then 3..6
        REQUIRE(starts == Starts{ { 5, 0 }, { 5, 0 } });
    }
    SECTION("A loop restart inside the clip seeks to the export offset")
    {
        play(6, 8, true, 3); // frames 6 7 8 6
        REQUIRE(starts == Starts{ { 6, 42 }, { 6, 42 } });
    }
}
