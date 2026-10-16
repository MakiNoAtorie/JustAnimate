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

#include <QFile>
#include <QTemporaryDir>

#include "editor.h"
#include "object.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "scribblearea.h"
#include "testfixture.h"
#include "vectorimage.h"
#include "importimageconfig.h"
#include "importimageseqdialog.h"

namespace
{
// A single-frame 1x1 GIF; Qt's GIF reader reports it as supporting animation.
const QByteArray kGif = QByteArray::fromHex(
    "47494638396101000100800000000000ffffff21f90401000000002c00000000010001000002024401003b");
}

TEST_CASE("Editor - Importing into a hidden layer warns instead of crashing")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    TestSession session;
    Object* obj = session.object;

    session.start();
    Editor* editor = session.editor.get();
    ScribbleArea* scribbleArea = session.area.get();

    int hiddenLayerWarnings = 0;
    scribbleArea->setLayerNotVisibleWarning([&] { hiddenLayerWarnings++; });

    SECTION("Vector image")
    {
        const QString path = dir.filePath("image.vec");
        VectorImage source;
        REQUIRE(source.write(path, "VEC").ok());

        LayerVector* layer = obj->addNewVectorLayer();
        layer->setVisible(false);
        editor->layers()->setCurrentLayer(layer);
        editor->scrubTo(2);
        const int keys = layer->keyFrameCount();

        ImportImageConfig config;
        config.positionType = ImportImageConfig::None;
        Status st = editor->importImage(path, config);

        REQUIRE(hiddenLayerWarnings == 1);
        REQUIRE(layer->keyFrameCount() == keys);
        REQUIRE_FALSE(st.code() == Status::FAIL);
    }

    SECTION("Animated GIF")
    {
        const QString path = dir.filePath("anim.gif");
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write(kGif);
        file.close();

        LayerBitmap* layer = obj->addNewBitmapLayer();
        layer->setVisible(false);
        editor->layers()->setCurrentLayer(layer);
        editor->scrubTo(2);
        const int keys = layer->keyFrameCount();

        Status st = editor->importAnimatedImage(path, 1, [](int) {}, [] { return false; });

        REQUIRE(hiddenLayerWarnings == 1);
        REQUIRE(layer->keyFrameCount() == keys);
        REQUIRE_FALSE(st.code() == Status::FAIL);
    }
}

TEST_CASE("Image sequence file list is in natural order", "[ImportImageSeq]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QStringList names = {"a1.png", "a10.png", "a2.png", "b001.png", "b010.png", "b002.png"};
    for (const QString& n : names)
    {
        QFile f(dir.filePath(n));
        REQUIRE(f.open(QIODevice::WriteOnly));
    }

    const QStringList sorted = naturalSortedFileNames(QDir(dir.path()));

    // Prefixes stay grouped, numbers sort by value
    REQUIRE(sorted == QStringList({"a1.png", "a2.png", "a10.png", "b001.png", "b002.png", "b010.png"}));
}
