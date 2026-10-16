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
#include "catch.hpp"

#include <QDomDocument>
#include <QImage>
#include <QKeySequence>
#include <QPainter>

#include "bitmapimage.h"
#include "editor.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "onionskinpainteroptions.h"
#include "onionskinsubpainter.h"
#include "preferencemanager.h"
#include "testfixture.h"
#include "pencilsettings.h"
#include "scribblearea.h"
#include "toolmanager.h"

namespace
{
/// Paints onion skins around frame 5 of a layer with keys 1..9 and returns the opacity of each previous frame.
QVector<qreal> prevOpacities(LayerBitmap& layer, const OnionSkinPainterOptions& options)
{
    for (int i = 1; i <= 9; i++) { layer.addNewKeyFrameAt(i); }
    QImage image(4, 4, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    QVector<qreal> opacities;
    OnionSkinSubPainter().paint(painter, &layer, options, 5, [&](OnionSkinPaintState state, int) {
        if (state == OnionSkinPaintState::PREV) { opacities.append(painter.opacity()); }
    });
    return opacities;
}

OnionSkinPainterOptions fourPrevFrames(OnionSkinFalloff falloff)
{
    OnionSkinPainterOptions options;
    options.skinPrevFrames = true;
    options.framesToSkinPrev = 4;
    options.minOpacity = 20;
    options.maxOpacity = 60;
    options.falloff = falloff;
    return options;
}
}

TEST_CASE("Onion skin falloff sets the opacity of each frame", "[OnionSkin]")
{
    LayerBitmap layer(1);

    SECTION("Linear drops the same step per frame")
    {
        const QVector<qreal> o = prevOpacities(layer, fourPrevFrames(OnionSkinFalloff::Linear));
        REQUIRE(o.size() == 4);
        CHECK(o[0] == Approx(0.60));
        CHECK(o[1] == Approx(0.50));
        CHECK(o[2] == Approx(0.40));
        CHECK(o[3] == Approx(0.30));
    }
    SECTION("Gentle keeps near frames strong")
    {
        const QVector<qreal> o = prevOpacities(layer, fourPrevFrames(OnionSkinFalloff::Gentle));
        REQUIRE(o.size() == 4);
        CHECK(o[0] == Approx(0.600));
        CHECK(o[1] == Approx(0.575));
        CHECK(o[2] == Approx(0.500));
        CHECK(o[3] == Approx(0.375));
    }
    SECTION("Steep fades near frames fast")
    {
        const QVector<qreal> o = prevOpacities(layer, fourPrevFrames(OnionSkinFalloff::Steep));
        REQUIRE(o.size() == 4);
        CHECK(o[0] == Approx(0.600));
        CHECK(o[1] == Approx(0.400));
        CHECK(o[2] == Approx(0.317157).epsilon(0.001));
        CHECK(o[3] == Approx(0.253590).epsilon(0.001));
    }
}

TEST_CASE("A layer with onion skin off shows only its current frame", "[OnionSkin]")
{
    LayerBitmap layer(1);
    layer.setOnionSkin(false);
    OnionSkinPainterOptions options = fourPrevFrames(OnionSkinFalloff::Linear);
    options.skinNextFrames = true;
    options.framesToSkinNext = 2;

    for (int i = 1; i <= 9; i++) { layer.addNewKeyFrameAt(i); }
    QImage image(4, 4, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    QVector<OnionSkinPaintState> states;
    OnionSkinSubPainter().paint(painter, &layer, options, 5, [&](OnionSkinPaintState state, int) { states.append(state); });

    REQUIRE(states == QVector<OnionSkinPaintState>{ OnionSkinPaintState::CURRENT });
}

TEST_CASE("The layer onion skin flag is saved in the project", "[OnionSkin]")
{
    QDomDocument doc;

    SECTION("Off round-trips")
    {
        LayerBitmap saved(1);
        saved.setOnionSkin(false);
        LayerBitmap loaded(2);
        loaded.loadBaseDomElement(saved.createBaseDomElement(doc));
        REQUIRE_FALSE(loaded.onionSkin());
    }
    SECTION("On writes no attribute and old files load as on")
    {
        LayerBitmap saved(1);
        const QDomElement elem = saved.createBaseDomElement(doc);
        REQUIRE_FALSE(elem.hasAttribute("onion"));
        LayerBitmap loaded(2);
        loaded.setOnionSkin(false);
        loaded.loadBaseDomElement(elem);
        REQUIRE(loaded.onionSkin());
    }
}

TEST_CASE("Onion skin colours falloff and mode are remembered across restarts", "[OnionSkin]")
{
    const SettingsGuard guard({ SETTING_ONION_PREV_COLOR, SETTING_ONION_NEXT_COLOR, SETTING_ONION_FALLOFF, SETTING_ONION_TYPE, SETTING_ONION_MARKS });
    {
        TestSession session;
        session.object->addNewBitmapLayer(); // a mode change repaints the current layer
        REQUIRE(session.start());
        PreferenceManager* prefs = session.editor->preference();
        prefs->set(SETTING::ONION_PREV_COLOR, QString("#00ff00"));
        prefs->set(SETTING::ONION_NEXT_COLOR, QString("#ff00ff"));
        prefs->set(SETTING::ONION_FALLOFF, static_cast<int>(OnionSkinFalloff::Steep));
        prefs->set(SETTING::ONION_TYPE, QString("marked"));
        prefs->set(SETTING::ONION_MARKS, MarkFilter({ Mark::Inbetween }).toInt());
    }
    TestSession session;
    REQUIRE(session.start());
    PreferenceManager* prefs = session.editor->preference();
    REQUIRE(prefs->getString(SETTING::ONION_PREV_COLOR) == "#00ff00");
    REQUIRE(prefs->getString(SETTING::ONION_NEXT_COLOR) == "#ff00ff");
    REQUIRE(prefs->getInt(SETTING::ONION_FALLOFF) == static_cast<int>(OnionSkinFalloff::Steep));
    CHECK(prefs->markFilter().contains(Mark::Inbetween));
    CHECK_FALSE(prefs->markFilter().contains(Mark::Key));
}

TEST_CASE("Onion skin colours default to red and blue", "[OnionSkin]")
{
    const SettingsGuard guard({ SETTING_ONION_PREV_COLOR, SETTING_ONION_NEXT_COLOR, SETTING_ONION_FALLOFF });
    QSettings& settings = pencilSettings();
    settings.remove(SETTING_ONION_PREV_COLOR);
    settings.remove(SETTING_ONION_NEXT_COLOR);
    settings.remove(SETTING_ONION_FALLOFF);

    TestSession session;
    REQUIRE(session.start());
    PreferenceManager* prefs = session.editor->preference();
    REQUIRE(QColor(prefs->getString(SETTING::ONION_PREV_COLOR)) == QColor(Qt::red));
    REQUIRE(QColor(prefs->getString(SETTING::ONION_NEXT_COLOR)) == QColor(Qt::blue));
    REQUIRE(prefs->getInt(SETTING::ONION_FALLOFF) == static_cast<int>(OnionSkinFalloff::Linear));
}

TEST_CASE("Marked drawings onion skin shows only Drawings that pass the Mark filter", "[OnionSkin]")
{
    const SettingsGuard guard({ SETTING_PREV_ONION, SETTING_NEXT_ONION, SETTING_ONION_RED, SETTING_ONION_PREV_FRAMES_NUM,
                                SETTING_ONION_MAX_OPACITY, SETTING_ONION_TYPE, SETTING_ONION_MARKS });
    TestSession s;
    LayerBitmap* layer = s.object->addNewBitmapLayer();
    REQUIRE(s.start());
    Editor* editor = s.editor.get();
    ScribbleArea* area = s.area.get();
    area->init();
    area->resize(200, 200);
    area->show();
    editor->layers()->setCurrentLayer(layer);
    editor->tools()->setCurrentTool(HAND);

    PreferenceManager* prefs = editor->preference();
    prefs->set(SETTING::PREV_ONION, true);
    prefs->set(SETTING::NEXT_ONION, false);
    prefs->set(SETTING::ONION_RED, false);
    prefs->set(SETTING::ONION_PREV_FRAMES_NUM, 2);
    prefs->set(SETTING::ONION_MAX_OPACITY, 100);
    MarkFilter keysOnly;
    keysOnly.set(Mark::Key, true);
    prefs->set(SETTING::ONION_MARKS, keysOnly.toInt());
    prefs->set(SETTING::ONION_TYPE, QString("marked"));
    REQUIRE(prefs->markFilter().contains(Mark::Key));
    REQUIRE_FALSE(prefs->markFilter().contains(Mark::Inbetween));

    REQUIRE(layer->addNewKeyFrameAt(2));
    REQUIRE(layer->addNewKeyFrameAt(3));
    layer->getKeyFrameAt(1)->setMark(Mark::Key);
    layer->getKeyFrameAt(2)->setMark(Mark::Inbetween);
    layer->getBitmapImageAtFrame(1)->drawRect(QRectF(-60, -10, 20, 20), Qt::NoPen, QBrush(Qt::black), QPainter::CompositionMode_Source, false);
    layer->getBitmapImageAtFrame(2)->drawRect(QRectF(40, -10, 20, 20), Qt::NoPen, QBrush(Qt::black), QPainter::CompositionMode_Source, false);
    editor->scrubTo(3);
    area->onFrameModified(1);
    area->onFrameModified(2);

    QImage image = area->grab().toImage();
    const QColor empty = image.pixelColor(100, 100);
    CHECK(image.pixelColor(50, 100) != empty);  // the Key
    CHECK(image.pixelColor(150, 100) == empty); // the Inbetween

    prefs->set(SETTING::ONION_TYPE, QString("absolute"));
    REQUIRE(prefs->markFilter().passesAll());
    image = area->grab().toImage();
    CHECK(image.pixelColor(50, 100) != empty);
    CHECK(image.pixelColor(150, 100) != empty);
}

TEST_CASE("O toggles onion skin in both directions", "[OnionSkin]")
{
    const SettingsGuard guard({ SETTING_PREV_ONION, SETTING_NEXT_ONION });
    TestSession session;
    REQUIRE(session.start());
    PreferenceManager* prefs = session.editor->preference();

    prefs->set(SETTING::PREV_ONION, true);
    prefs->set(SETTING::NEXT_ONION, false);
    session.editor->toggleOnionSkin();
    CHECK_FALSE(prefs->isOn(SETTING::PREV_ONION));
    CHECK_FALSE(prefs->isOn(SETTING::NEXT_ONION));
    session.editor->toggleOnionSkin();
    CHECK(prefs->isOn(SETTING::PREV_ONION));
    CHECK(prefs->isOn(SETTING::NEXT_ONION));

    QSettings preset(":resources/kb.ini", QSettings::IniFormat);
    preset.beginGroup("shortcuts");
    CHECK(QKeySequence(preset.value("CmdOnionSkin").toString()) == QKeySequence(Qt::Key_O));
    CHECK(preset.value("CmdOnionSkinPrevious").toString().isEmpty());
    CHECK(preset.value("CmdOnionSkinNext").toString().isEmpty());
}

TEST_CASE("A saved O on previous-only onion skin moves to the onion skin toggle", "[OnionSkin]")
{
    const SettingsGuard guard({ "shortcuts/CmdOnionSkin", "shortcuts/CmdOnionSkinPrevious", "shortcuts/CmdOnionSkinNext" });
    QSettings& settings = pencilSettings();
    settings.remove("shortcuts/CmdOnionSkin");
    settings.setValue("shortcuts/CmdOnionSkinPrevious", "O");
    settings.setValue("shortcuts/CmdOnionSkinNext", "Alt+O");
    checkExistingShortcuts();
    CHECK(settings.value("shortcuts/CmdOnionSkin").toString() == "O");
    CHECK(settings.value("shortcuts/CmdOnionSkinPrevious").toString().isEmpty());
    CHECK(settings.value("shortcuts/CmdOnionSkinNext").toString().isEmpty());
}
