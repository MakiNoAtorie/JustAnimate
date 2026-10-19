/*

JustAnimate - Traditional Animation Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "screenshots.h"

#include "catch.hpp"

#include <cmath>
#include <random>

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainterPath>
#include <QPixmap>
#include <QTemporaryDir>
#include <QtMath>

#include "beziercurve.h"
#include "bitmapimage.h"
#include "camera.h"
#include "defaultlayout.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "layersound.h"
#include "layervector.h"
#include "object.h"
#include "soundclip.h"
#include "theme.h"
#include "vectorimage.h"

namespace
{
constexpr unsigned kSeed = 296;
constexpr int kFps = 12;
constexpr int kSampleRate = 8000;

// Ball centre per Drawing of the bounce: a drop from the top left to the floor and up to the right.
struct BallPose { int frame; Mark mark; QPointF centre; qreal squash; };
const BallPose kBounce[] = {
    { 1, Mark::Key, { -260, -150 }, 1.0 },
    { 4, Mark::Inbetween, { -150, -20 }, 1.0 },
    { 7, Mark::Breakdown, { -40, 95 }, 0.7 },
    { 10, Mark::Inbetween, { 70, -10 }, 1.0 },
    { 13, Mark::Key, { 180, -120 }, 1.0 },
};
constexpr qreal kBallRadius = 45;

QPainterPath ellipse(QPointF centre, qreal squash)
{
    QPainterPath path;
    path.addEllipse(centre, kBallRadius / squash, kBallRadius * squash);
    return path;
}

// A rough sketch line: several loose passes around the ball.
void drawRoughBall(BitmapImage* image, const BallPose& pose, std::mt19937& rng)
{
    std::uniform_real_distribution<qreal> jitter(-4, 4);
    const QPen pen(QColor(70, 90, 160), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (int pass = 0; pass < 3; ++pass)
    {
        const QPointF centre = pose.centre + QPointF(jitter(rng), jitter(rng));
        image->drawPath(ellipse(centre, pose.squash), pen, Qt::NoBrush, QPainter::CompositionMode_SourceOver, true);
    }
}

QList<QPointF> ellipsePoints(const BallPose& pose)
{
    QList<QPointF> points;
    for (int i = 0; i <= 24; ++i)
    {
        const qreal a = 2 * M_PI * i / 24;
        points << pose.centre + QPointF(std::cos(a) * kBallRadius / pose.squash, std::sin(a) * kBallRadius * pose.squash);
    }
    return points;
}

void drawBackground(BitmapImage* image)
{
    const QPen line(QColor(60, 60, 60), 3.0, Qt::SolidLine, Qt::RoundCap);
    image->drawLine({ -380, 140 }, { 380, 140 }, line, QPainter::CompositionMode_SourceOver, true);
    QPainterPath hills;
    hills.moveTo(-380, 140);
    hills.cubicTo(-250, 40, -120, 60, 0, 140);
    hills.cubicTo(120, 20, 260, 30, 380, 140);
    image->drawPath(hills, QPen(QColor(90, 140, 90), 3.0), QBrush(QColor(200, 230, 200)), QPainter::CompositionMode_SourceOver, true);
}

// Two seconds of mono speech-like bursts, as normalized samples.
QVector<float> voiceSamples(std::mt19937& rng)
{
    std::uniform_real_distribution<float> noise(-1, 1);
    QVector<float> samples(2 * kSampleRate);
    for (int i = 0; i < samples.size(); ++i)
    {
        const double t = double(i) / kSampleRate;
        const double syllable = std::max(0.0, std::sin(2 * M_PI * 2.5 * t)); // five syllables
        const double tone = std::sin(2 * M_PI * 180 * t) * 0.7 + noise(rng) * 0.3;
        samples[i] = float(syllable * tone * 0.8);
    }
    return samples;
}

bool writeWav(const QString& path, const QVector<float>& samples)
{
    QFile file(path);
    if (!file.open(QFile::WriteOnly)) { return false; }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    const quint32 dataBytes = quint32(samples.size() * 2);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataBytes);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(kSampleRate) << quint32(kSampleRate * 2)
        << quint16(2) << quint16(16);
    out.writeRawData("data", 4);
    out << dataBytes;
    for (float s : samples) { out << qint16(std::lround(s * 32767)); }
    return out.status() == QDataStream::Ok;
}
} // namespace

Object* makeDemoProject()
{
    std::mt19937 rng(kSeed);
    auto* object = new Object;
    object->init();
    object->data()->setFrameRate(kFps);

    LayerCamera* camera = object->addNewCameraLayer();
    camera->setName("Camera");
    camera->addNewKeyFrameAt(24);
    Camera* end = camera->getCameraAtFrame(24);
    end->translate(120, 40);
    end->scale(1.25);

    LayerBitmap* background = object->addNewBitmapLayer();
    background->setName("Background");
    drawBackground(background->getBitmapImageAtFrame(1));

    const int ballGroup = 1;
    object->addLayerGroup(LayerGroup{ ballGroup, "Ball", false, true, 1.0 });

    LayerBitmap* rough = object->addNewBitmapLayer();
    rough->setName("Rough");
    const int roughIndex = object->getLayerCount() - 1;
    rough->setGroupId(ballGroup);
    for (const BallPose& pose : kBounce)
    {
        if (!rough->keyExists(pose.frame)) { rough->addNewKeyFrameAt(pose.frame); }
        rough->getKeyFrameAt(pose.frame)->setMark(pose.mark);
        drawRoughBall(rough->getBitmapImageAtFrame(pose.frame), pose, rng);
    }

    LayerVector* line = object->addNewVectorLayer();
    line->setName("Line");
    line->setGroupId(ballGroup);
    for (const BallPose& pose : kBounce)
    {
        if (pose.mark == Mark::Inbetween) { continue; }
        if (!line->keyExists(pose.frame)) { line->addNewKeyFrameAt(pose.frame); }
        line->getKeyFrameAt(pose.frame)->setMark(pose.mark);
        BezierCurve curve(ellipsePoints(pose));
        curve.setWidth(3);
        curve.setColorNumber(0);
        line->getVectorImageAtFrame(pose.frame)->addCurve(curve, 1.0, false);
    }

    LayerSound* voice = object->addNewSoundLayer();
    voice->setName("Voice");
    const QVector<float> samples = voiceSamples(rng);
    const QString wavPath = QDir(object->dataDir()).filePath("voice.wav");
    if (writeWav(wavPath, samples))
    {
        auto* clip = new SoundClip;
        clip->setFileName(wavPath);
        clip->setSoundClipName("voice.wav");
        clip->setSamples(samples, 1, kSampleRate);
        clip->setDuration(samples.size() * 1000 / kSampleRate);
        clip->updateLength(kFps);
        voice->addKeyFrame(1, clip);
    }

    object->data()->setMarker(1, FrameMarker{ "Start", QColor(60, 130, 220) });
    object->data()->setMarker(13, FrameMarker{ "Hit", QColor(220, 90, 60) });
    object->data()->setMarker(5, FrameMarker{ "A", QColor(120, 170, 60) }, voice->id());

    object->setSceneLength(24);
    object->data()->setCurrentFrame(1);
    object->data()->setCurrentLayer(roughIndex);
    return object;
}

QSize shotSize() { return { 1280, 800 }; }

ShotWindow::ShotWindow()
    : guard({ SETTING_THEME, SETTING_ACCENT, SETTING_WINDOW_STATE, SETTING_WINDOW_GEOMETRY, SETTING_LAYOUT_VERSION,
              SETTING_WORKSPACE_CURRENT, "Workspace/State/Animate" })
{
    QSettings& settings = pencilSettings();
    for (const char* key : { SETTING_WINDOW_STATE, SETTING_WINDOW_GEOMETRY, SETTING_WORKSPACE_CURRENT, "Workspace/State/Animate" })
    {
        settings.remove(key);
    }
    Theme::save(Theme::Light);
    Theme::saveAccent(Theme::Blue);
    Theme::apply(Theme::Light);

    editor = window.mEditor;
    editor->setObject(makeDemoProject());
    window.resize(shotSize());
    window.show();
    QApplication::processEvents();
}

ShotWindow::~ShotWindow()
{
    Theme::apply(Theme::System);
}

QString saveShot(QWidget* widget, const QString& name, QString dir)
{
    if (dir.isEmpty()) { dir = qEnvironmentVariable("JUSTANIMATE_SHOT_DIR"); }
    INFO("Run the screenshots target; it sets JUSTANIMATE_SHOT_DIR");
    REQUIRE(!dir.isEmpty());
    REQUIRE(QDir().mkpath(dir));
    QApplication::processEvents();
    const QString path = QDir(dir).filePath(name + ".png");
    REQUIRE(widget->grab().save(path));
    return path;
}

TEST_CASE("A screenshot of the main window is a full-size picture", "[ScreenshotsSmoke]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    ShotWindow shot;
    const QImage image(saveShot(&shot.window, "main-window", dir.path()));

    REQUIRE(!image.isNull());
    CHECK(image.size() == shotSize() * shot.window.devicePixelRatio());
    const QRgb first = image.pixel(0, 0);
    bool varied = false;
    for (int y = 0; y < image.height() && !varied; y += 7)
    {
        for (int x = 0; x < image.width() && !varied; x += 7) { varied = image.pixel(x, y) != first; }
    }
    CHECK(varied);
}
