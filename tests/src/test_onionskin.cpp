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
#include <QPainter>

#include "layerbitmap.h"
#include "onionskinpainteroptions.h"
#include "onionskinsubpainter.h"
#include "preferencemanager.h"
#include "testfixture.h"

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

TEST_CASE("Onion skin colours and falloff are remembered across restarts", "[OnionSkin]")
{
    const SettingsGuard guard({ SETTING_ONION_PREV_COLOR, SETTING_ONION_NEXT_COLOR, SETTING_ONION_FALLOFF });
    {
        TestSession session;
        REQUIRE(session.start());
        PreferenceManager* prefs = session.editor->preference();
        prefs->set(SETTING::ONION_PREV_COLOR, QString("#00ff00"));
        prefs->set(SETTING::ONION_NEXT_COLOR, QString("#ff00ff"));
        prefs->set(SETTING::ONION_FALLOFF, static_cast<int>(OnionSkinFalloff::Steep));
    }
    TestSession session;
    REQUIRE(session.start());
    PreferenceManager* prefs = session.editor->preference();
    REQUIRE(prefs->getString(SETTING::ONION_PREV_COLOR) == "#00ff00");
    REQUIRE(prefs->getString(SETTING::ONION_NEXT_COLOR) == "#ff00ff");
    REQUIRE(prefs->getInt(SETTING::ONION_FALLOFF) == static_cast<int>(OnionSkinFalloff::Steep));
}

TEST_CASE("Onion skin colours default to red and blue", "[OnionSkin]")
{
    const SettingsGuard guard({ SETTING_ONION_PREV_COLOR, SETTING_ONION_NEXT_COLOR, SETTING_ONION_FALLOFF });
    QSettings settings(PENCIL2D, PENCIL2D);
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
