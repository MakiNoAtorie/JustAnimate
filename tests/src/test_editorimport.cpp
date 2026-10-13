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

#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>

#include "editor.h"
#include "object.h"
#include "layermanager.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "scribblearea.h"
#include "vectorimage.h"
#include "importimageconfig.h"

namespace
{
// Closes every modal message box that opens while alive, counting the hidden-layer warnings.
class WarningCloser
{
public:
    WarningCloser()
    {
        QObject::connect(&mTimer, &QTimer::timeout, [this] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            {
                if (box->text().contains("hidden layer")) { mHiddenLayerWarnings++; }
                box->done(QMessageBox::Ok);
            }
        });
        mTimer.start(10);
    }
    int hiddenLayerWarnings() const { return mHiddenLayerWarnings; }

private:
    QTimer mTimer;
    int mHiddenLayerWarnings = 0;
};

// A single-frame 1x1 GIF; Qt's GIF reader reports it as supporting animation.
const QByteArray kGif = QByteArray::fromHex(
    "47494638396101000100800000000000ffffff21f90401000000002c00000000010001000002024401003b");
}

TEST_CASE("Editor - Importing into a hidden layer warns instead of crashing")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    Object* obj = new Object;
    obj->init();

    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(obj);
    editor->init();

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

        WarningCloser closer;
        ImportImageConfig config;
        config.positionType = ImportImageConfig::None;
        Status st = editor->importImage(path, config);

        REQUIRE(closer.hiddenLayerWarnings() == 1);
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

        WarningCloser closer;
        Status st = editor->importAnimatedImage(path, 1, [](int) {}, [] { return false; });

        REQUIRE(closer.hiddenLayerWarnings() == 1);
        REQUIRE(layer->keyFrameCount() == keys);
        REQUIRE_FALSE(st.code() == Status::FAIL);
    }

    delete scribbleArea;
    delete editor;
}
