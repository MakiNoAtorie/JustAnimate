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

#include <cstring>
#include <memory>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include "qminiz.h"
#include "beziercurve.h"
#include "blendmode.h"
#include "keyframe.h"
#include "objectdata.h"
#include "fileformat.h"
#include "filemanager.h"
#include "util.h"
#include "object.h"
#include "bitmapimage.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "vectorimage.h"
#include "layersound.h"
#include "layercamera.h"
#include "layermanager.h"
#include "camera.h"
#include "soundclip.h"
#include "playbackmanager.h"
#include "testfixture.h"
#include "stageprofiler.h"

#include <memory>
#include <random>


TEST_CASE("FileManager Initial Test")
{
    SECTION("Initial error code")
    {
        FileManager f;
        REQUIRE(f.error() == Status::OK);
    }
}

TEST_CASE("FileManager invalid operations")
{
    SECTION("Open a non-existing file")
    {
        FileManager fm;

        QString strDummyPath = "hahaha_blala.pcl";
        Object* obj = fm.load(strDummyPath);

        REQUIRE(obj == nullptr);
        REQUIRE(fm.error().code() == Status::FILE_NOT_FOUND);
    }

    SECTION("Bad XML")
    {
        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        QString strBadXMLPath = dir.filePath("bad.pcl");

        // make a fake xml.
        QFile badXMLFile(strBadXMLPath);
        badXMLFile.open(QIODevice::WriteOnly);

        QTextStream fout(&badXMLFile);
        fout << "%% haha, this is not a xml file.";
        badXMLFile.close();

        FileManager fm;
        Object* pObj = fm.load(strBadXMLPath);

        REQUIRE(pObj == nullptr);
        REQUIRE(fm.error().code() == Status::ERROR_INVALID_XML_FILE);
    }

    SECTION("A valid xml, but doesn't follow pencil2d's rule")
    {
        QTemporaryDir dir;
        REQUIRE(dir.isValid());
        QString strBadXMLPath = dir.filePath("bad.pcl");

        QFile badXMLFile(strBadXMLPath);
        badXMLFile.open(QIODevice::WriteOnly);

        QTextStream fout(&badXMLFile);
        fout << "<!DOCTYPE BlahBlahRoot><document></document>";
        badXMLFile.close();

        FileManager fm;
        Object* pObj = fm.load(strBadXMLPath);

        REQUIRE(pObj == nullptr);
        REQUIRE(fm.error().code() == Status::ERROR_INVALID_PENCIL_FILE);
    }
}

TEST_CASE("FileManager Loading XML Tests")
{
    SECTION("Minimal working xml")
    {
        QTemporaryFile minimalDoc;
        if (minimalDoc.open())
        {
            QFile minXML(minimalDoc.fileName());
            minXML.open(QIODevice::WriteOnly);

            QTextStream fout(&minXML);
            fout << "<!DOCTYPE PencilDocument><document>";
            fout << "  <object></object>";
            fout << "</document>";
            minXML.close();

            FileManager fm;
            Object* o = fm.load(minimalDoc.fileName());

            REQUIRE(o != nullptr);
            REQUIRE(fm.error().ok());
            REQUIRE(o->getLayerCount() >= 1); // have at least one cam layer

            delete o;
        }
    }

    SECTION("Xml with one bitmap layer")
    {
        QTemporaryFile tmpFile;
        if (!tmpFile.open())
        {
            REQUIRE(false);
        }
        QFile theXML(tmpFile.fileName());
        theXML.open(QIODevice::WriteOnly);

        QTextStream fout(&theXML);
        fout << "<!DOCTYPE PencilDocument><document>";
        fout << "  <object>";
        fout << "    <layer name='MyLayer' id='5' visibility='1' type='1'></layer>";
        fout << "  </object>";
        fout << "</document>";
        theXML.close();

        FileManager fm;
        Object* obj = fm.load(theXML.fileName());
        REQUIRE(obj->getLayerCount() == 2); // one bitmap layer and one default cam layer
        REQUIRE(obj->getLayer(0)->name() == "MyLayer");
        REQUIRE(obj->getLayer(0)->id() == 5);
        REQUIRE(obj->getLayer(0)->visible() == true);
        REQUIRE(obj->getLayer(0)->type() == Layer::BITMAP);

        delete obj;
    }

    SECTION("xml with one bitmap layer and one key")
    {
        QTemporaryFile tmpFile;
        if (!tmpFile.open())
        {
            REQUIRE(false);
        }
        QFile theXML(tmpFile.fileName());
        theXML.open(QIODevice::WriteOnly);

        QTextStream fout(&theXML);
        fout << "<!DOCTYPE PencilDocument><document>";
        fout << "  <object>";
        fout << "    <layer name='GoodLayer' id='5' visibility='0' type='1' >";
        fout << "      <image frame='1' topLeftY='0' src='003.001.png' topLeftX='0' />";
        fout << "    </layer>";
        fout << "  </object>";
        fout << "</document>";
        theXML.close();

        FileManager fm;
        Object* obj = fm.load(theXML.fileName());

        Layer* layer = obj->getLayer(0);
        REQUIRE(layer->name() == "GoodLayer");
        REQUIRE(layer->id() == 5);
        REQUIRE(layer->visible() == false);
        REQUIRE(layer->type() == Layer::BITMAP);

        REQUIRE(layer->getKeyFrameAt(1) != nullptr);
        REQUIRE(layer->keyFrameCount() == 1);

        delete obj;
    }

    SECTION("xml with 1 bitmap layer and 2 keys")
    {
        QTemporaryFile tmpFile;
        if (!tmpFile.open())
        {
            REQUIRE(false);
        }
        QFile theXML(tmpFile.fileName());
        theXML.open(QIODevice::WriteOnly);

        QTextStream fout(&theXML);
        fout << "<!DOCTYPE PencilDocument><document>";
        fout << "  <object>";
        fout << "    <layer name='MyLayer' id='5' visibility='1' type='1' >";
        fout << "      <image frame='1' topLeftY='0' src='003.001.png' topLeftX='0' />";
        fout << "      <image frame='2' topLeftY='0' src='003.001.png' topLeftX='0' />";
        fout << "    </layer>";
        fout << "  </object>";
        fout << "</document>";
        theXML.close();

        FileManager fm;
        Object* obj = fm.load(theXML.fileName());

        Layer* layer = obj->getLayer(0);
        REQUIRE(layer->name() == "MyLayer");
        REQUIRE(layer->id() == 5);
        REQUIRE(layer->visible() == true);
        REQUIRE(layer->type() == Layer::BITMAP);

        REQUIRE(layer->getKeyFrameAt(1) != nullptr);
        delete obj;
    }
}

static Object* loadXmlString(FileManager& fm, const QString& xml)
{
    QTemporaryFile tmpFile;
    REQUIRE(tmpFile.open());
    QTextStream fout(&tmpFile);
    fout << xml;
    fout.flush();
    tmpFile.close();
    return fm.load(tmpFile.fileName());
}

TEST_CASE("FileManager frame markers round-trip and are optional")
{
    QTemporaryDir testDir;
    REQUIRE(testDir.isValid());
    const QString path = testDir.filePath("markers.pclx");
    FileManager fm;
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    obj->addNewCameraLayer();

    SECTION("A file without markers loads with none")
    {
        REQUIRE(fm.save(obj.get(), path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->data()->markers().isEmpty());
    }

    SECTION("Markers survive save and load")
    {
        obj->data()->setMarker(3, { "key", QColor("#ff0000") });
        obj->data()->setMarker(7, { "breakdown", QColor("#00ff00") });
        REQUIRE(fm.save(obj.get(), path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        const auto& m = loaded->data()->markers();
        REQUIRE(m.size() == 2);
        REQUIRE(m.value(3).name == "key");
        REQUIRE(m.value(3).color == QColor("#ff0000"));
        REQUIRE(m.value(7).name == "breakdown");
    }

    SECTION("Lip-sync markers on a sound layer survive save and load next to a global marker")
    {
        const int soundId = obj->addNewSoundLayer()->id();
        obj->data()->setMarker(3, { "key", QColor("#ff0000") });
        obj->data()->setMarker(3, { "AI", QColor("#0000ff") }, soundId);
        obj->data()->setMarker(5, { "O", QColor("#00ff00") }, soundId);
        REQUIRE(fm.save(obj.get(), path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->data()->markers().size() == 1);
        REQUIRE(loaded->data()->markers().value(3).name == "key");
        const auto lip = loaded->data()->markers(soundId);
        REQUIRE(lip.size() == 2);
        REQUIRE(lip.value(3).name == "AI");
        REQUIRE(lip.value(5).color == QColor("#00ff00"));
    }

    SECTION("Markers of a deleted sound layer are not saved")
    {
        LayerSound* sound = obj->addNewSoundLayer();
        const int soundId = sound->id();
        obj->data()->setMarker(3, { "key", QColor("#ff0000") });
        obj->data()->setMarker(5, { "AI", QColor("#0000ff") }, soundId);
        obj->deleteLayer(sound);
        REQUIRE(fm.save(obj.get(), path).ok());
        std::unique_ptr<Object> loaded(fm.load(path));
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->data()->markers().keys() == QList<int>{ 3 });
        REQUIRE(loaded->data()->allMarkers().size() == 1);
    }
}

TEST_CASE("FileManager loads markers with an unknown or non-sound layer as global markers")
{
    FileManager fm;
    std::unique_ptr<Object> obj(loadXmlString(fm,
        "<!DOCTYPE PencilDocument><document>"
        "  <object><layer id='4' name='B' visibility='1' type='1'/></object>"
        "  <projectdata><markers>"
        "    <marker frame='2' name='unknown' color='#ff0000' layer='99'/>"
        "    <marker frame='6' name='bitmap' color='#ff0000' layer='4'/>"
        "    <marker frame='8' name='bad' color='#ff0000' layer='x'/>"
        "  </markers></projectdata>"
        "</document>"));
    REQUIRE(obj != nullptr);
    const auto m = obj->data()->markers();
    REQUIRE(m.size() == 3);
    REQUIRE(m.value(2).name == "unknown");
    REQUIRE(m.value(6).name == "bitmap");
    REQUIRE(m.value(8).name == "bad");
    REQUIRE(obj->data()->markers(4).isEmpty());
    REQUIRE(obj->data()->markers(99).isEmpty());
}

TEST_CASE("FileManager loads a Paper template with the old kind attribute and saves it without")
{
    FileManager fm;
    std::unique_ptr<Object> obj(loadXmlString(fm,
        "<!DOCTYPE PencilDocument><document><object></object>"
        "  <projectdata><paperTemplate on='1' kind='genga' title='Cut 12' id='A-012' name='Maki'"
        "    state='Genga' date='2026-10-17' notes='Hold'/></projectdata>"
        "</document>"));
    REQUIRE(obj != nullptr);
    PaperTemplate expected;
    expected.on = true;
    expected.title = "Cut 12";
    expected.id = "A-012";
    expected.name = "Maki";
    expected.state = "Genga";
    expected.date = "2026-10-17";
    expected.notes = "Hold";
    CHECK(obj->data()->paperTemplate() == expected);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    REQUIRE(fm.save(obj.get(), dir.filePath("paper.pclx")).ok());
    QFile mainXml(QDir(obj->workingDir()).filePath(PFF_XML_FILE_NAME));
    REQUIRE(mainXml.open(QIODevice::ReadOnly));
    const QByteArray xml = mainXml.readAll();
    REQUIRE(xml.contains("<paperTemplate"));
    CHECK_FALSE(xml.contains("kind="));
}

TEST_CASE("FileManager loads malformed project XML")
{
    SECTION("XML comment inside projectdata")
    {
        FileManager fm;
        std::unique_ptr<Object> obj(loadXmlString(fm,
            "<!DOCTYPE PencilDocument><document>"
            "  <projectdata>"
            "    <!-- a comment -->"
            "    <currentFrame value='3'/>"
            "  </projectdata>"
            "  <object></object>"
            "</document>"));

        REQUIRE(obj != nullptr);
        REQUIRE(obj->data()->getCurrentFrame() == 3);
    }

    SECTION("Unknown layer type is skipped")
    {
        FileManager fm;
        std::unique_ptr<Object> obj(loadXmlString(fm,
            "<!DOCTYPE PencilDocument><document><object>"
            "  <layer name='Weird' id='7' visibility='1' type='99'></layer>"
            "  <layer name='Movie' id='8' visibility='1' type='3'></layer>"
            "  <layer name='Good' id='5' visibility='1' type='1'></layer>"
            "</object></document>"));

        REQUIRE(obj != nullptr);
        REQUIRE(obj->findLayerByName("Weird") == nullptr);
        REQUIRE(obj->findLayerByName("Movie") == nullptr);
        REQUIRE(obj->findLayerByName("Good") != nullptr);
    }

    SECTION("Frames numbered zero or below are skipped")
    {
        FileManager fm;
        std::unique_ptr<Object> obj(loadXmlString(fm,
            "<!DOCTYPE PencilDocument><document><object>"
            "  <layer name='Vec' id='2' visibility='1' type='2'>"
            "    <image frame='0'/>"
            "    <image frame='-1'/>"
            "    <image frame='2'/>"
            "  </layer>"
            "  <layer name='Bmp' id='3' visibility='1' type='1'>"
            "    <image frame='0' topLeftX='0' topLeftY='0' src='003.001.png'/>"
            "    <image frame='1' topLeftX='0' topLeftY='0' src='003.001.png'/>"
            "  </layer>"
            "  <layer name='Cam' id='4' visibility='1' type='5' width='800' height='600'>"
            "    <camera frame='0'/>"
            "    <camera frame='1'/>"
            "  </layer>"
            "</object></document>"));

        REQUIRE(obj != nullptr);
        Layer* vec = obj->findLayerByName("Vec");
        REQUIRE(vec != nullptr);
        REQUIRE(vec->keyFrameCount() == 1);
        REQUIRE(vec->keyExists(2));

        Layer* bmp = obj->findLayerByName("Bmp");
        REQUIRE(bmp != nullptr);
        REQUIRE(bmp->keyFrameCount() == 1);
        REQUIRE(!bmp->keyExists(0));

        Layer* cam = obj->findLayerByName("Cam");
        REQUIRE(cam != nullptr);
        REQUIRE(cam->keyFrameCount() == 1);
        REQUIRE(!cam->keyExists(0));
    }

    SECTION("Camera scale zero or below loads as 1")
    {
        FileManager fm;
        std::unique_ptr<Object> obj(loadXmlString(fm,
            "<!DOCTYPE PencilDocument><document><object>"
            "  <layer name='Cam' id='4' visibility='1' type='5' width='800' height='600'>"
            "    <camera frame='1' s='0'/>"
            "    <camera frame='2' s='-2'/>"
            "  </layer>"
            "</object></document>"));

        REQUIRE(obj != nullptr);
        auto cam = static_cast<LayerCamera*>(obj->findLayerByName("Cam"));
        REQUIRE(static_cast<Camera*>(cam->getKeyFrameAt(1))->scaling() == 1.0);
        REQUIRE(static_cast<Camera*>(cam->getKeyFrameAt(2))->scaling() == 1.0);
    }

    SECTION("Camera easing out of range loads as linear")
    {
        FileManager fm;
        std::unique_ptr<Object> obj(loadXmlString(fm,
            "<!DOCTYPE PencilDocument><document><object>"
            "  <layer name='Cam' id='4' visibility='1' type='5' width='800' height='600'>"
            "    <camera frame='1' easing='-1'/>"
            "    <camera frame='2' easing='9999'/>"
            "    <camera frame='3' easing='2'/>"
            "  </layer>"
            "</object></document>"));

        REQUIRE(obj != nullptr);
        auto cam = static_cast<LayerCamera*>(obj->findLayerByName("Cam"));
        REQUIRE(static_cast<Camera*>(cam->getKeyFrameAt(1))->getEasingType() == CameraEasingType::LINEAR);
        REQUIRE(static_cast<Camera*>(cam->getKeyFrameAt(2))->getEasingType() == CameraEasingType::LINEAR);
        REQUIRE(static_cast<Camera*>(cam->getKeyFrameAt(3))->getEasingType() == CameraEasingType::OUTQUAD);
    }
    SECTION("Unknown element under document is skipped")
    {
        FileManager fm;
        std::unique_ptr<Object> obj(loadXmlString(fm,
            "<!DOCTYPE PencilDocument><document><object></object><x/></document>"));

        REQUIRE(obj != nullptr);
    }
}

// Turn a Qt resource file into an actual file on disk
QString QtResourceToFile(QString rscPath, QString filename, QTemporaryDir& tempDir)
{
    QFile fin(rscPath);
    if (!fin.open(QFile::ReadOnly))
    {
        qWarning() << __FUNCTION__ << "Cannot open" << rscPath;
        return "";
    }
    QByteArray content = fin.readAll();
    fin.close();

    QString filePathOnDisk = tempDir.filePath(filename);
    QFile fout(filePathOnDisk);
    if (!fout.open(QFile::WriteOnly))
    {
        qWarning() << __FUNCTION__ << "Cannot write to" << filePathOnDisk;
    }
    fout.write(content);
    fout.close();
    return filePathOnDisk;
}

TEST_CASE("FileManager Load PCLX")
{
    SECTION("Empty PCLX")
    {
        QTemporaryDir tempDir;

        FileManager fm;
        Object* o = fm.load(QtResourceToFile(":/empty.pclx", "empty.pclx", tempDir));
        REQUIRE(o != nullptr);
        if (o)
        {
            // file has 2 bitmap layers, 1 vector layers and 1 cam layers
            REQUIRE(o->getLayerCount() == 4);
        }
        delete o;
    }

    SECTION("Chinese Filename")
    {
        QTemporaryDir tempDir;

        FileManager fm;
        Object* o = fm.load(QtResourceToFile(":/cjk-test.pclx", "許功蓋.pclx", tempDir));
        REQUIRE(o != nullptr);
        if (o)
        {
            // file has 2 bitmap layers, 1 vector layers and 1 cam layers
            REQUIRE(o->getLayerCount() == 4);
        }
        delete o;
    }

    SECTION("Japanese Filename")
    {
        QTemporaryDir tempDir;

        FileManager fm;
        Object* o = fm.load(QtResourceToFile(":/cjk-test.pclx", "構わない.pclx", tempDir));
        REQUIRE(o != nullptr);
        if (o)
        {
            // file has 2 bitmap layers, 1 vector layers and 1 cam layers
            REQUIRE(o->getLayerCount() == 4);
        }
        delete o;
    }

    SECTION("Korean Filename")
    {
        QTemporaryDir tempDir;

        FileManager fm;
        Object* o = fm.load(QtResourceToFile(":/cjk-test.pclx", "대박이야.pclx", tempDir));
        REQUIRE(o != nullptr);
        if (o)
        {
            // file has 2 bitmap layers, 1 vector layers and 1 cam layers
            REQUIRE(o->getLayerCount() == 4);
        }
        delete o;
    }
}

TEST_CASE("FileManager File-saving")
{
    // https://github.com/pencil2d/pencil/issues/939
    SECTION("#939 Clear action not properly saved")
    {
        FileManager fm;

        // 1. create a animation with one red frame & save it
        Object* o1 = new Object;
        o1->init();
        o1->addNewCameraLayer();
        o1->addNewVectorLayer();
        o1->addNewBitmapLayer();

        LayerBitmap* layer = dynamic_cast<LayerBitmap*>(o1->getLayer(2));
        REQUIRE(layer->addNewKeyFrameAt(2));

        BitmapImage* b1 = layer->getBitmapImageAtFrame(2);
        b1->drawRect(QRectF(0, 0, 10, 10), QPen(QColor(255, 0, 0)), QBrush(Qt::red), QPainter::CompositionMode_SourceOver, false);

        QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
        QString animationPath = testDir.path() + "/abc.pclx";
        fm.save(o1, animationPath);
        delete o1;

        // 2. load the animation, and then clear the red frame, save it.
        Object* o2 = fm.load(animationPath);
        layer = dynamic_cast<LayerBitmap*>(o2->getLayer(2));

        BitmapImage* b2 = layer->getBitmapImageAtFrame(2);
        b2->clear();

        fm.save(o2, animationPath);
        delete o2;

        // 3. load the animation again, check whether it's an empty frame
        Object* o3 = fm.load(animationPath);
        layer = dynamic_cast<LayerBitmap*>(o3->getLayer(2));

        BitmapImage* b3 = layer->getBitmapImageAtFrame(2);
        REQUIRE(b3->bounds().isEmpty());

        delete o3;
    }

    //https://github.com/pencil2d/pencil/issues/966
    SECTION("#966 Moving more than 200 frames corrupts frames upon save")
    {
        FileManager fm;

        // 1. Create a animation with 150 frames & save it
        Object* o1 = new Object;
        o1->init();
        o1->addNewCameraLayer();
        o1->addNewVectorLayer();
        o1->addNewBitmapLayer();

        LayerBitmap* layer = dynamic_cast<LayerBitmap*>(o1->getLayer(2));
        for (int i = 100; i < 150; ++i)
        {
            layer->addNewKeyFrameAt(i);
            auto bitmap = layer->getBitmapImageAtFrame(i);
            bitmap->drawRect(QRectF(0, 0, 10, 10), QPen(QColor(255, 0, 0)), QBrush(Qt::red), QPainter::CompositionMode_SourceOver, false);
        }

        QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
        QString animationPath = testDir.path() + "/abc.pclx";
        fm.save(o1, animationPath);
        delete o1;

        // 2. Load the animation back and then make some frames unloaded by active frame pool
        Object* o2 = fm.load(animationPath);
        o2->setActiveFramePoolSize(20);

        layer = dynamic_cast<LayerBitmap*>(o2->getLayer(2));
        for (int i = 1; i < 150; ++i)
            o2->updateActiveFrames(i);

        // 3. Move those unloaded frames around
        for (int i = 100; i < 150; ++i)
            LayerManager::setFrameSelected(layer, i, true);

        LayerManager::moveSelectedFrames(layer, -55);
        fm.save(o2, animationPath);
        delete o2;

        // 4. Check no lost frames
        Object* o3 = fm.load(animationPath);
        layer = dynamic_cast<LayerBitmap*>(o3->getLayer(2));
        for (int i = 2; i < 150; ++i)
        {
            auto bitmap = layer->getBitmapImageAtFrame(i);
            if (bitmap)
            {
                REQUIRE(bitmap->image()->width() > 1);
                REQUIRE(bitmap->image()->height() > 1);
            }
        }
        delete o3;
    }
}

// Adds a sound clip whose copy in the data folder is deleted after the layers
// are saved, so MiniZ::compressFolder fails on a listed file on every OS.
static void addClipDeletedBeforeZip(FileManager& fm, Object* obj, const QString& srcDir)
{
    const QString src = QDir(srcDir).filePath("beep.wav");
    QFile f(src);
    REQUIRE(f.open(QFile::WriteOnly));
    f.write(QByteArray(64, '\0'));
    f.close();

    SoundClip* clip = new SoundClip;
    clip->setFileName(src);
    REQUIRE(obj->addNewSoundLayer()->addKeyFrame(1, clip));
    // Progress is emitted on the main thread between the layer saves and the zip job.
    QObject::connect(&fm, &FileManager::progressChanged, [clip, src] {
        if (clip->fileName() != src) QFile::remove(clip->fileName());
    });
}

TEST_CASE("FileManager atomic save", "[FileManager]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    const QString projectPath = testDir.filePath("shot.v2.pclx");

    auto readAll = [](const QString& path) {
        QFile f(path);
        REQUIRE(f.open(QFile::ReadOnly));
        return f.readAll();
    };
    auto dirEntries = [&] {
        return QDir(testDir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    };

    FileManager fm;
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    obj->addNewCameraLayer();
    obj->addNewBitmapLayer();
    REQUIRE(fm.save(obj.get(), projectPath).ok());

    SECTION("A successful save leaves only the project file, no .tmp or backup")
    {
        REQUIRE(fm.save(obj.get(), projectPath).ok());
        REQUIRE(dirEntries() == QStringList{ "shot.v2.pclx" });

        std::unique_ptr<Object> loaded(fm.load(projectPath));
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getLayerCount() == obj->getLayerCount());
    }

    SECTION("A save that fails part-way leaves the original byte-identical")
    {
        const QByteArray original = readAll(projectPath);

        // The zip writer fails after it has started writing the archive.
        QTemporaryDir srcDir;
        REQUIRE(srcDir.isValid());
        addClipDeletedBeforeZip(fm, obj.get(), srcDir.path());

        REQUIRE_FALSE(fm.save(obj.get(), projectPath).ok());
        const bool unchanged = (readAll(projectPath) == original);
        REQUIRE(unchanged);
        REQUIRE(dirEntries() == QStringList{ "shot.v2.pclx" });
    }
}

TEST_CASE("FileManager reports progress while it zips and unzips", "[FileManager][ZipThread]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    const QString projectPath = testDir.filePath("busy.pclx");

    std::unique_ptr<Object> obj(new Object);
    obj->init();
    LayerBitmap* layer = obj->addNewBitmapLayer();
    for (int f = 1; f <= 20; ++f)
    {
        if (f > 1) REQUIRE(layer->addNewKeyFrameAt(f)); // a new layer starts with frame 1
        layer->getBitmapImageAtFrame(f)->drawEllipse(QRectF(f, f, 20, 20), QPen(Qt::black, 2), QBrush(Qt::red),
                                                     QPainter::CompositionMode_SourceOver, true);
    }

    FileManager fm;
    int max = 0;
    int last = 0;
    bool reportedWhileZipping = false;
    QObject::connect(&fm, &FileManager::progressRangeChanged, [&](int m) { max = m; });
    QObject::connect(&fm, &FileManager::progressChanged, [&](int p) {
        last = p;
        // The UI repaints only from these signals, so they must keep coming while the archive is written.
        if (QFile::exists(projectPath + ".tmp")) reportedWhileZipping = true;
    });

    REQUIRE(fm.save(obj.get(), projectPath).ok());
    REQUIRE(reportedWhileZipping);
    REQUIRE(last <= max);

    SECTION("A save started from a progress signal fails and leaves the running save intact")
    {
        // The UI processes events on progress, so an autosave timer can fire mid-save.
        FileManager nested;
        bool nestedFailed = false;
        QObject::connect(&fm, &FileManager::progressChanged, [&] {
            if (QFile::exists(projectPath + ".tmp") && !nestedFailed)
                nestedFailed = !nested.save(obj.get(), testDir.filePath("autosave.pclx")).ok();
        });
        REQUIRE(fm.save(obj.get(), projectPath).ok());
        REQUIRE(nestedFailed);
        REQUIRE_FALSE(QFile::exists(testDir.filePath("autosave.pclx")));
    }

    SECTION("A zip that fails on the worker thread leaves the original and no .tmp")
    {
        QFile file(projectPath);
        REQUIRE(file.open(QFile::ReadOnly));
        const QByteArray original = file.readAll();
        file.close();

        QTemporaryDir srcDir;
        REQUIRE(srcDir.isValid());
        addClipDeletedBeforeZip(fm, obj.get(), srcDir.path());

        const Status st = fm.save(obj.get(), projectPath);
        REQUIRE(st.code() == Status::ERROR_MINIZ_FAIL);
        REQUIRE_FALSE(QFile::exists(projectPath + ".tmp"));
        REQUIRE(file.open(QFile::ReadOnly));
        const bool unchanged = file.readAll() == original;
        REQUIRE(unchanged);
    }

    FileManager loader;
    int loadMax = 0;
    int loadLast = 0;
    QObject::connect(&loader, &FileManager::progressRangeChanged, [&](int m) { loadMax = m; });
    QObject::connect(&loader, &FileManager::progressChanged, [&](int p) { loadLast = p; });
    std::unique_ptr<Object> loaded(loader.load(projectPath));
    REQUIRE(loaded != nullptr);
    int keys = 0;
    for (int i = 0; i < loaded->getLayerCount(); ++i)
        keys = std::max(keys, loaded->getLayer(i)->keyFrameCount());
    REQUIRE(keys == 20);
    // One step per archive entry while unzipping, then one per frame loaded; the range covers both.
    REQUIRE(loadLast >= 2 * 20);
    REQUIRE(loadLast <= loadMax);
}

TEST_CASE("FileManager saves a project whose sound clip file is missing", "[FileManager]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    const QString projectPath = testDir.filePath("shot.pclx");

    auto addClip = [&](LayerSound* layer, int frame, const QString& wavName) {
        QFile wav(testDir.filePath(wavName));
        REQUIRE(wav.open(QFile::WriteOnly));
        wav.write("RIFF");
        wav.close();
        SoundClip* clip = new SoundClip;
        clip->setFileName(wav.fileName());
        REQUIRE(layer->addKeyFrame(frame, clip));
        return clip;
    };

    FileManager fm;
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    obj->addNewCameraLayer();
    obj->addNewBitmapLayer();
    LayerSound* sound = obj->addNewSoundLayer();
    sound->setName("Voice");
    SoundClip* lost = addClip(sound, 1, "lost.wav");
    addClip(sound, 10, "kept.wav");

    REQUIRE(fm.save(obj.get(), projectPath).ok());
    REQUIRE(fm.skippedSoundClips().isEmpty());

    REQUIRE(QFile::remove(lost->fileName()));
    REQUIRE(fm.save(obj.get(), projectPath).ok());
    REQUIRE(fm.skippedSoundClips() == QStringList{ "Voice frame 1" });

    std::unique_ptr<Object> loaded(fm.load(projectPath));
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->getLayerCount() == obj->getLayerCount());
    LayerSound* loadedSound = loaded->getLayersByType<LayerSound>().front();
    REQUIRE(loadedSound->getKeyFrameAt(1) == nullptr);
    KeyFrame* kept = loadedSound->getKeyFrameAt(10);
    REQUIRE(kept != nullptr);
    REQUIRE(QFile::exists(kept->fileName()));

    // The file comes back: the next save finds it again.
    QFile back(lost->fileName());
    REQUIRE(back.open(QFile::WriteOnly));
    back.write("RIFF");
    back.close();
    REQUIRE(fm.save(obj.get(), projectPath).ok());
    REQUIRE(fm.skippedSoundClips().isEmpty());
    std::unique_ptr<Object> reloaded(fm.load(projectPath));
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->getLayersByType<LayerSound>().front()->getKeyFrameAt(1) != nullptr);
}

TEST_CASE("FileManager saves bitmap frames byte-identical to serial PNG encoding", "[FileManager][ParallelSave]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());

    std::unique_ptr<Object> obj(new Object);
    obj->init();
    LayerBitmap* layer = obj->addNewBitmapLayer();
    const int frames = 40;
    for (int f = 1; f <= frames; ++f)
    {
        if (f > 1) REQUIRE(layer->addNewKeyFrameAt(f)); // a new layer starts with frame 1
        BitmapImage* image = layer->getBitmapImageAtFrame(f);
        image->drawEllipse(QRectF(f * 3, f * 2, 20 + f, 30), QPen(Qt::black, 2), QBrush(QColor::fromHsv(f * 9, 200, 200)),
                           QPainter::CompositionMode_SourceOver, true);
        // Reference: the frame encoded alone, the same way BitmapImage::writeFile does.
        image->autoCrop();
        REQUIRE(QImageWriter(testDir.filePath(QString("serial%1.png").arg(f))).write(*image->image()));
    }

    FileManager fm;
    REQUIRE(fm.save(obj.get(), testDir.filePath("frames.pclx")).ok());

    for (int f = 1; f <= frames; ++f)
    {
        CAPTURE(f);
        QFile saved(layer->getBitmapImageAtFrame(f)->fileName());
        QFile serial(testDir.filePath(QString("serial%1.png").arg(f)));
        REQUIRE(saved.open(QFile::ReadOnly));
        REQUIRE(serial.open(QFile::ReadOnly));
        const bool identical = (saved.readAll() == serial.readAll());
        REQUIRE(identical);
    }
}

TEST_CASE("FileManager autosave and crash recovery", "[FileManager]")
{
    FileManager fm;
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    obj->addNewCameraLayer();
    LayerBitmap* layer = obj->addNewBitmapLayer();
    layer->setName("Ink");
    REQUIRE(layer->getBitmapImageAtFrame(1) != nullptr);
    BitmapImage red(QRect(0, 0, 8, 8), Qt::red);
    layer->getBitmapImageAtFrame(1)->paste(&red);

    // The time-based autosave runs once to completion.
    REQUIRE(fm.writeToWorkingFolder(obj.get()).ok());

    // Then the app is killed in the middle of the next autosave: main.xml is
    // half written, a frame file is cut short and the archive .tmp is partial.
    const QDir workingDir(obj->workingDir());
    auto truncateFile = [](const QString& path) {
        QFile f(path);
        REQUIRE(f.open(QFile::ReadWrite));
        REQUIRE(f.resize(f.size() / 2));
    };
    truncateFile(workingDir.filePath(PFF_XML_FILE_NAME));
    const QStringList frames = QDir(obj->dataDir()).entryList({ "*.png" }, QDir::Files);
    REQUIRE(frames.size() == 1);
    truncateFile(QDir(obj->dataDir()).filePath(frames.first()));
    {
        QFile partial(workingDir.filePath(QString(PFF_AUTOSAVE_FILE_NAME) + ".tmp"));
        REQUIRE(partial.open(QFile::WriteOnly));
        partial.write("PK\x03\x04 cut short");
    }

    // The next start offers the project, and recovering it gives back the
    // last completed autosave rather than the half-written files.
    QStringList recoverables;
    for (const QString& path : fm.searchForUnsavedProjects())
        recoverables << QDir(path).canonicalPath();
    REQUIRE(recoverables.contains(workingDir.canonicalPath()));

    std::unique_ptr<Object> recovered(fm.recoverUnsavedProject(workingDir.path()));
    REQUIRE(fm.error().ok());
    REQUIRE(recovered != nullptr);

    LayerBitmap* recoveredLayer = nullptr;
    for (int i = 0; i < recovered->getLayerCount(); ++i)
        if (recovered->getLayer(i)->type() == Layer::BITMAP)
            recoveredLayer = static_cast<LayerBitmap*>(recovered->getLayer(i));
    REQUIRE(recoveredLayer != nullptr);
    REQUIRE(recoveredLayer->name() == "Ink");

    BitmapImage* recoveredFrame = recoveredLayer->getBitmapImageAtFrame(1);
    REQUIRE(recoveredFrame != nullptr);
    REQUIRE(recoveredFrame->image()->pixel(4, 4) == qRgba(255, 0, 0, 255));
}

TEST_CASE("FileManager recovery never goes back behind a save or loses files", "[FileManager]")
{
    FileManager fm;
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    obj->addNewCameraLayer();
    LayerBitmap* layer = obj->addNewBitmapLayer();
    BitmapImage red(QRect(0, 0, 8, 8), Qt::red);
    layer->getBitmapImageAtFrame(1)->paste(&red);
    REQUIRE(fm.writeToWorkingFolder(obj.get()).ok());
    const QDir workingDir(obj->workingDir());

    auto recoveredPixel = [&] {
        std::unique_ptr<Object> recovered(fm.recoverUnsavedProject(workingDir.path()));
        REQUIRE(fm.error().ok());
        REQUIRE(recovered != nullptr);
        // Quitting a recovered project must prompt: it exists only in the temp folder.
        REQUIRE(recovered->isModified());
        for (int i = 0; i < recovered->getLayerCount(); ++i)
            if (recovered->getLayer(i)->type() == Layer::BITMAP)
                return static_cast<LayerBitmap*>(recovered->getLayer(i))->getBitmapImageAtFrame(1)->image()->pixel(4, 4);
        return QRgb(0);
    };

    SECTION("A manual save after the autosave is what recovery gives back")
    {
        BitmapImage blue(QRect(0, 0, 8, 8), Qt::blue);
        layer->getBitmapImageAtFrame(1)->paste(&blue);
        QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
        REQUIRE(fm.save(obj.get(), testDir.filePath("shot.pclx")).ok());

        REQUIRE(recoveredPixel() == qRgba(0, 0, 255, 255));
    }

    SECTION("An autosave archive that fails to extract keeps the working files")
    {
        // The central directory is whole, so the archive opens; the content
        // no longer matches its checksum, so extracting it fails.
        const QString archive = workingDir.filePath(PFF_AUTOSAVE_FILE_NAME);
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        REQUIRE(mz_zip_writer_init_file(&zip, archive.toUtf8().data(), 0));
        REQUIRE(mz_zip_writer_add_mem(&zip, PFF_XML_FILE_NAME, "AAAAAAAA", 8, MZ_NO_COMPRESSION));
        REQUIRE(mz_zip_writer_finalize_archive(&zip));
        mz_zip_writer_end(&zip);
        QFile zf(archive);
        REQUIRE(zf.open(QIODevice::ReadWrite));
        QByteArray bytes = zf.readAll();
        bytes.replace("AAAAAAAA", "BBBBBBBB");
        zf.seek(0);
        zf.write(bytes);
        zf.close();
        REQUIRE(MiniZ::sanityCheck(archive).ok());

        REQUIRE(recoveredPixel() == qRgba(255, 0, 0, 255));
        REQUIRE_FALSE(workingDir.exists("autosave.tmp"));
    }

    SECTION("A swap that fails part-way gives back the working files")
    {
        // The archive holds red; the working files hold blue.
        const QString archive = workingDir.filePath(PFF_AUTOSAVE_FILE_NAME);
        REQUIRE(QFile::copy(archive, archive + ".red"));
        BitmapImage blue(QRect(0, 0, 8, 8), Qt::blue);
        layer->getBitmapImageAtFrame(1)->paste(&blue);
        REQUIRE(fm.writeToWorkingFolder(obj.get()).ok());
        REQUIRE(QFile::remove(archive));
        REQUIRE(QFile::rename(archive + ".red", archive));

        // A file where the data folder is kept aside makes the swap fail after main.xml moved.
        {
            QFile blocker(workingDir.filePath(QString(PFF_DATA_DIR) + ".old"));
            REQUIRE(blocker.open(QFile::WriteOnly));
        }

        REQUIRE(recoveredPixel() == qRgba(0, 0, 255, 255));
        REQUIRE_FALSE(workingDir.exists(QString(PFF_XML_FILE_NAME) + ".old"));
        REQUIRE_FALSE(workingDir.exists("autosave.tmp"));
    }
}

TEST_CASE("FileManager write errors fail the save", "[FileManager]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    const QString projectPath = testDir.filePath("shot.pclx");

    auto readAll = [](const QString& path) {
        QFile f(path);
        REQUIRE(f.open(QFile::ReadOnly));
        return f.readAll();
    };
    auto findBitmapLayer = [](Object* o) -> LayerBitmap* {
        for (int i = 0; i < o->getLayerCount(); ++i)
            if (o->getLayer(i)->type() == Layer::BITMAP)
                return static_cast<LayerBitmap*>(o->getLayer(i));
        return nullptr;
    };

    FileManager fm;
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    obj->addNewCameraLayer();
    LayerVector* vectorLayer = obj->addNewVectorLayer();
    LayerBitmap* bitmapLayer = obj->addNewBitmapLayer();
    BitmapImage red(QRect(0, 0, 8, 8), Qt::red);
    bitmapLayer->getBitmapImageAtFrame(1)->paste(&red);
    REQUIRE(fm.save(obj.get(), projectPath).ok());
    const QByteArray original = readAll(projectPath);

    // Writing to /dev/full fails with "no space left on device", like a full
    // disk. The error has to be caught where the file is written; a failed
    // save then never replaces the archive (A4).
    const bool haveFullDevice = QFile::exists("/dev/full");
    auto replaceWithFullDevice = [](const QString& path) {
        REQUIRE(QFile::remove(path));
        REQUIRE(QFile::link("/dev/full", path));
    };

    SECTION("A write error in the project XML fails the save")
    {
        if (!haveFullDevice) { WARN("No /dev/full on this platform"); return; }

        // The XML is written to <name>.tmp first.
        REQUIRE(QFile::link("/dev/full", QDir(obj->workingDir()).filePath(QString(PFF_XML_FILE_NAME) + ".tmp")));

        REQUIRE_FALSE(fm.save(obj.get(), projectPath).ok());
        const bool unchanged = (readAll(projectPath) == original);
        REQUIRE(unchanged);
    }

    SECTION("Saving to legacy .pcl is refused and writes nothing")
    {
        const QString folderProjectPath = testDir.filePath("shot.pcl");
        REQUIRE_FALSE(fm.save(obj.get(), folderProjectPath).ok());
        REQUIRE_FALSE(QFile::exists(folderProjectPath));
        REQUIRE_FALSE(QFile::exists(folderProjectPath + "." + PFF_OLD_DATA_DIR));
    }

    SECTION("A write error in a vector frame fails the save")
    {
        if (!haveFullDevice) { WARN("No /dev/full on this platform"); return; }

        VectorImage* vec = vectorLayer->getVectorImageAtFrame(1);
        REQUIRE(vec != nullptr);
        vec->setModified(true);
        replaceWithFullDevice(QDir(obj->dataDir()).filePath(QString::asprintf("%03d.%03d.vec", vectorLayer->id(), 1)));

        REQUIRE_FALSE(fm.save(obj.get(), projectPath).ok());
        const bool unchanged = (readAll(projectPath) == original);
        REQUIRE(unchanged);
    }

    SECTION("A failed frame rename during Save As fails the save and keeps the frame")
    {
        // Reopen so the frame is on disk and not loaded, then move it without drawing.
        obj.reset(fm.load(projectPath));
        REQUIRE(obj != nullptr);
        bitmapLayer = findBitmapLayer(obj.get());
        REQUIRE(bitmapLayer != nullptr);
        REQUIRE(bitmapLayer->moveKeyFrame(1, 1));

        // Something in the way of the frame's temporary name makes the rename fail.
        const QString blocker = QDir(obj->dataDir()).filePath(QString::asprintf("t_%03d.%03d.png", bitmapLayer->id(), 2));
        REQUIRE(QDir().mkpath(blocker));

        const QString saveAsPath = testDir.filePath("shot-v2.pclx");
        REQUIRE_FALSE(fm.save(obj.get(), saveAsPath).ok());
        REQUIRE_FALSE(QFile::exists(saveAsPath));
        const bool unchanged = (readAll(projectPath) == original);
        REQUIRE(unchanged);

        // Once the obstacle is gone the same project saves with the frame intact.
        REQUIRE(QDir(blocker).removeRecursively());
        REQUIRE(fm.save(obj.get(), saveAsPath).ok());
        std::unique_ptr<Object> reloaded(fm.load(saveAsPath));
        REQUIRE(reloaded != nullptr);
        LayerBitmap* reloadedLayer = findBitmapLayer(reloaded.get());
        REQUIRE(reloadedLayer != nullptr);
        BitmapImage* frame = reloadedLayer->getBitmapImageAtFrame(2);
        REQUIRE(frame != nullptr);
        REQUIRE(frame->image()->pixel(4, 4) == qRgba(255, 0, 0, 255));
    }
}

namespace
{
struct ZipEntry { bool found = false; int method = -1; QByteArray data; };

ZipEntry readZipEntry(const QString& zipPath, const QString& name)
{
    ZipEntry e;
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!mz_zip_reader_init_file(&zip, zipPath.toUtf8().constData(), 0)) return e;
    const int index = mz_zip_reader_locate_file(&zip, name.toUtf8().constData(), nullptr, 0);
    mz_zip_archive_file_stat stat;
    if (index >= 0 && mz_zip_reader_file_stat(&zip, index, &stat))
    {
        size_t size = 0;
        void* p = mz_zip_reader_extract_to_heap(&zip, index, &size, 0);
        e = { p != nullptr, stat.m_method, QByteArray(static_cast<const char*>(p), static_cast<int>(size)) };
        mz_free(p);
    }
    mz_zip_reader_end(&zip);
    return e;
}

QMap<QString, QByteArray> unzipAll(const QString& zipPath, const QString& dir)
{
    REQUIRE(MiniZ::uncompressFolder(zipPath, dir + "/").ok());
    QMap<QString, QByteArray> files;
    for (QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories); it.hasNext();)
    {
        QFile f(it.next());
        REQUIRE(f.open(QFile::ReadOnly));
        files[QDir(dir).relativeFilePath(f.fileName())] = f.readAll();
    }
    return files;
}

// Bitmap layer with frames 1..count, each an 8x8 fill, so every PNG has the same size.
LayerBitmap* addFilledFrames(Object* obj, int count, QRgb (*color)(int))
{
    LayerBitmap* layer = obj->addNewBitmapLayer();
    for (int f = 1; f <= count; ++f)
    {
        if (f > 1) REQUIRE(layer->addNewKeyFrameAt(f));
        BitmapImage fill(QRect(0, 0, 8, 8), QColor::fromRgba(color(f)));
        layer->getBitmapImageAtFrame(f)->paste(&fill);
    }
    return layer;
}

QString zipName(const Object* obj, const QString& file)
{
    return QDir(obj->workingDir()).relativeFilePath(file);
}
}

TEST_CASE("FileManager stores PNG entries without compression", "[FileManager][SaveCopy]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    LayerBitmap* layer = addFilledFrames(obj.get(), 2, [](int f) { return qRgba(f * 90, 20, 200, 255); });
    FileManager fm;
    const QString path = testDir.filePath("stored.pclx");
    REQUIRE(fm.save(obj.get(), path).ok());

    const ZipEntry png = readZipEntry(path, zipName(obj.get(), layer->getBitmapImageAtFrame(1)->fileName()));
    REQUIRE(png.found);
    REQUIRE(png.method == 0);
    REQUIRE(readZipEntry(path, PFF_XML_FILE_NAME).method == MZ_DEFLATED);
}

TEST_CASE("FileManager copies unchanged entries from the previous archive", "[FileManager][SaveCopy]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    auto colorAt = [](int f) { return qRgba(f * 60, 255 - f * 40, 30, 255); };
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    LayerBitmap* layer = addFilledFrames(obj.get(), 3, colorAt);
    FileManager fm;
    const QString path = testDir.filePath("copy.pclx");
    REQUIRE(fm.save(obj.get(), path).ok());

    SECTION("An unchanged frame comes from the previous archive, not from the working folder")
    {
        // Same size and time, other bytes: only a copy from the archive keeps the saved bytes.
        const QString file1 = layer->getBitmapImageAtFrame(1)->fileName();
        const QByteArray saved1 = readZipEntry(path, zipName(obj.get(), file1)).data;
        REQUIRE_FALSE(saved1.isEmpty());
        const QDateTime time1 = QFileInfo(file1).lastModified();
        {
            QFile f(file1);
            REQUIRE(f.open(QFile::WriteOnly));
            f.write(QByteArray(saved1.size(), 'x'));
            REQUIRE(f.flush()); // a write flushed after setFileTime sets a new time on Unix
            REQUIRE(f.setFileTime(time1, QFileDevice::FileModificationTime));
        }
        BitmapImage red(QRect(0, 0, 8, 8), QColor(Qt::red));
        layer->getBitmapImageAtFrame(2)->paste(&red);
        REQUIRE(fm.save(obj.get(), path).ok());

        REQUIRE(readZipEntry(path, zipName(obj.get(), file1)).data == saved1);
        QFile file2(layer->getBitmapImageAtFrame(2)->fileName());
        REQUIRE(file2.open(QFile::ReadOnly));
        REQUIRE(readZipEntry(path, zipName(obj.get(), file2.fileName())).data == file2.readAll());

        SECTION("Another archive is written from the working folder")
        {
            const QString other = testDir.filePath("other.pclx");
            REQUIRE(fm.save(obj.get(), other).ok());
            REQUIRE(readZipEntry(other, zipName(obj.get(), file1)).data == QByteArray(saved1.size(), 'x'));
        }
    }

    SECTION("Swapped frames with the same size and time are not copied")
    {
        const QString file1 = layer->getBitmapImageAtFrame(1)->fileName();
        const QString file2 = layer->getBitmapImageAtFrame(2)->fileName();
        REQUIRE(QFileInfo(file1).size() == QFileInfo(file2).size());
        const QDateTime time = QFileInfo(file1).lastModified();
        for (const QString& file : { file1, file2 })
        {
            QFile f(file);
            REQUIRE(f.open(QFile::Append));
            REQUIRE(f.setFileTime(time, QFileDevice::FileModificationTime));
        }
        REQUIRE(fm.save(obj.get(), path).ok()); // the baseline now holds equal sizes and times
        REQUIRE(layer->swapKeyFrames(1, 2));
        REQUIRE(fm.save(obj.get(), path).ok());

        FileManager reader;
        std::unique_ptr<Object> reloaded(reader.load(path));
        REQUIRE(reloaded);
        auto loaded = static_cast<LayerBitmap*>(reloaded->findLayerById(layer->id()));
        REQUIRE(loaded->getBitmapImageAtFrame(1)->image()->pixel(4, 4) == colorAt(2));
        REQUIRE(loaded->getBitmapImageAtFrame(2)->image()->pixel(4, 4) == colorAt(1));
    }

    SECTION("An archive built by copying opens identical to a full write")
    {
        BitmapImage red(QRect(0, 0, 8, 8), QColor(Qt::red));
        layer->getBitmapImageAtFrame(3)->paste(&red);
        REQUIRE(fm.save(obj.get(), path).ok());
        const QString full = testDir.filePath("full.pclx");
        REQUIRE(fm.save(obj.get(), full).ok());
        REQUIRE(unzipAll(path, testDir.filePath("a")) == unzipAll(full, testDir.filePath("b")));

        FileManager reader;
        std::unique_ptr<Object> reloaded(reader.load(path));
        REQUIRE(reloaded);
        auto loaded = static_cast<LayerBitmap*>(reloaded->findLayerById(layer->id()));
        REQUIRE(loaded->getBitmapImageAtFrame(1)->image()->pixel(4, 4) == colorAt(1));
        REQUIRE(loaded->getBitmapImageAtFrame(2)->image()->pixel(4, 4) == colorAt(2));
        REQUIRE(loaded->getBitmapImageAtFrame(3)->image()->pixel(4, 4) == qRgba(255, 0, 0, 255));
    }
}

TEST_CASE("Empty Sound Frames")
{
    SECTION("Invalid src value")
    {

        QTemporaryFile soundFrameDoc;
        if (soundFrameDoc.open())
        {
            QFile newXML(soundFrameDoc.fileName());
            newXML.open(QIODevice::WriteOnly);

            QTextStream fout(&newXML);
            fout << "<!DOCTYPE PencilDocument><document>";
            fout << "  <object>";
            fout << "       <layer type='4' id='5' name='GoodLayer' visibility='1'>";
            fout << "           <sound frame='1' name='' src=''/>";
            fout << "       </layer>";
            fout << "  </object>";
            fout << "</document>";
            newXML.close();


            FileManager fm;
            Object* newObj = fm.load(soundFrameDoc.fileName());

            REQUIRE(newObj != nullptr);
            REQUIRE(fm.error().ok());
            REQUIRE(newObj->getLayerCount() == 2);
            REQUIRE(newObj->getLayer(0)->type() == 4);
            REQUIRE(newObj->getLayer(0)->id() == 5);
            REQUIRE(newObj->getLayer(0)->name() == "GoodLayer");
            REQUIRE(newObj->getLayer(0)->visible() == true);
            REQUIRE(newObj->getLayer(0)->getKeyFrameAt(1) == nullptr);

            delete newObj;
        }
    }
}

TEST_CASE("Frame pool unloads far-away keyframes and keeps their pixels", "[FileManager]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    const QString path1 = testDir.path() + "/a.pclx";
    const QString path2 = testDir.path() + "/b.pclx";
    const int frames = 200;
    const QRgb red = qRgb(255, 0, 0);
    const QRgb blue = qRgb(0, 0, 255);
    FileManager fm;

    {
        std::unique_ptr<Object> o(new Object);
        o->init();
        o->addNewCameraLayer();
        LayerBitmap* layer = o->addNewBitmapLayer();
        for (int f = 1; f <= frames; ++f)
        {
            layer->addNewKeyFrameAt(f);
            // Antialiased ellipse: its bounds keep a transparent margin, like a drawn stroke.
            layer->getBitmapImageAtFrame(f)->drawEllipse(QRectF(0, 0, 20, 20), QPen(Qt::red, 4), QBrush(Qt::red),
                                                         QPainter::CompositionMode_SourceOver, true);
        }
        REQUIRE(fm.save(o.get(), path1).ok());
    }

    // The default pool budget (1 GB) holds the whole project: only distance unloads frame 1.
    std::unique_ptr<Object> o(fm.load(path1));
    REQUIRE(o);
    auto layer = static_cast<LayerBitmap*>(o->getLayer(1));
    BitmapImage* first = layer->getBitmapImageAtFrame(1);
    o->updateActiveFrames(1);
    first->bounds(); // the canvas reads the bounds when it paints the frame
    REQUIRE(first->isLoaded());
    REQUIRE_FALSE(first->isModified()); // viewing a frame is no edit
    o->updateActiveFrames(frames);
    REQUIRE_FALSE(first->isLoaded());

    // Edit after unload: the frame reloads, takes the edit and keeps it while far away.
    first->drawRect(QRectF(8, 4, 4, 4), QPen(Qt::blue), QBrush(Qt::blue), QPainter::CompositionMode_SourceOver, false);
    o->updateActiveFrames(1);
    o->updateActiveFrames(frames);
    REQUIRE(first->isLoaded());
    REQUIRE(first->pixel(10, 6) == blue);

    // Save after unload, to a new file: unloaded frames and the edit are all written.
    REQUIRE_FALSE(layer->getBitmapImageAtFrame(2)->isLoaded());
    REQUIRE(fm.save(o.get(), path2).ok());
    o->updateActiveFrames(1);
    o->updateActiveFrames(frames);
    REQUIRE_FALSE(first->isLoaded()); // saved, so it can unload now
    REQUIRE(first->pixel(10, 6) == blue);
    o.reset();

    std::unique_ptr<Object> o2(fm.load(path2));
    REQUIRE(o2);
    auto layer2 = static_cast<LayerBitmap*>(o2->getLayer(1));
    // A frame's bounds are known only once loaded, so load before reading pixels.
    auto pixelAt = [layer2](int f, int x, int y) { BitmapImage* b = layer2->getBitmapImageAtFrame(f); b->image(); return b->pixel(x, y); };
    REQUIRE(pixelAt(1, 10, 6) == blue);
    REQUIRE(pixelAt(1, 10, 16) == red);
    for (int f = 2; f <= frames; ++f)
        REQUIRE(pixelAt(f, 10, 16) == red);
}

TEST_CASE("Playback loads frames ahead of the playhead and keeps edits on the next loop", "[PlaybackCache]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    const QString path = testDir.path() + "/a.pclx";
    const int frames = 80;
    FileManager fm;
    {
        std::unique_ptr<Object> o(new Object);
        o->init();
        o->addNewCameraLayer();
        LayerBitmap* layer = o->addNewBitmapLayer();
        for (int f = 1; f <= frames; ++f)
        {
            layer->addNewKeyFrameAt(f);
            layer->getBitmapImageAtFrame(f)->drawRect(QRectF(0, 0, 20, 20), QPen(Qt::red), QBrush(Qt::red),
                                                      QPainter::CompositionMode_SourceOver, false);
        }
        REQUIRE(fm.save(o.get(), path).ok());
    }

    TestSession s(fm.load(path));
    REQUIRE(s.object);
    REQUIRE(s.start());
    auto layer = static_cast<LayerBitmap*>(s.object->getLayer(1));
    s.editor->playback()->play();
    s.editor->scrubTo(1);
    // Frame 12 is past the pool's own window of frame + 3: it is read on a worker, scrubTo does not wait for it.
    BitmapImage* ahead = layer->getBitmapImageAtFrame(12);
    REQUIRE_FALSE(ahead->isLoaded());
    REQUIRE_FALSE(layer->getBitmapImageAtFrame(40)->isLoaded());

    // Edit a preloaded frame while playing: the next loop shows the edit, not the file on disk.
    BitmapImage* edited = layer->getBitmapImageAtFrame(8);
    edited->drawRect(QRectF(4, 4, 4, 4), QPen(Qt::blue), QBrush(Qt::blue), QPainter::CompositionMode_SourceOver, false);
    for (int f = 2; f <= 12; ++f)
        s.editor->scrubTo(f);
    REQUIRE(ahead->isLoaded()); // shown: it takes the worker's result
    REQUIRE(ahead->pixel(5, 5) == qRgb(255, 0, 0));
    for (int f = 10; f <= frames; ++f)
        s.editor->scrubTo(f);
    REQUIRE_FALSE(layer->getBitmapImageAtFrame(9)->isLoaded()); // unloaded, far from the playhead
    s.editor->scrubTo(1);
    REQUIRE(edited->pixel(5, 5) == qRgb(0, 0, 255));
    REQUIRE(layer->getBitmapImageAtFrame(9)->pixel(5, 5) == qRgb(255, 0, 0)); // takes the worker's read
    s.editor->playback()->stop();
}

// Load a project and read every keyframe's file. A crash or sanitizer report is the failure.
static void loadEverything(const QString& path)
{
    FileManager fm;
    std::unique_ptr<Object> obj(fm.load(path));
    if (!obj) return;
    for (int i = 0; i < obj->getLayerCount(); ++i)
        obj->getLayer(i)->foreachKeyFrame([](KeyFrame* k) { k->loadFile(); });
}

static QByteArray mutate(QByteArray data, std::mt19937& rng)
{
    static const char* const tokens[] = { "-1", "0", "2147483648", "-2147483649", "99999", "nan", "1e308", "\"",
                                          "<", ">", "/>", "</layer>", "<layer type='1'>", "<image frame='1'/>",
                                          "&#0;", "../", "<object>", "<curve>" };
    const int ops = 1 + static_cast<int>(rng() % 4);
    for (int op = 0; op < ops && !data.isEmpty(); ++op)
    {
        const int pos = static_cast<int>(rng() % data.size());
        const int len = 1 + static_cast<int>(rng() % 16);
        switch (rng() % 4)
        {
        case 0: data[pos] = static_cast<char>(rng()); break;
        case 1: data.remove(pos, len); break;
        case 2: data.insert(pos, data.mid(pos, len)); break;
        default: data.insert(pos, tokens[rng() % (sizeof(tokens) / sizeof(tokens[0]))]); break;
        }
    }
    return data;
}

static void writeBytes(const QString& path, const QByteArray& data)
{
    QFile f(path);
    REQUIRE(f.open(QFile::WriteOnly | QFile::Truncate));
    f.write(data);
}

TEST_CASE("Fuzzed project files load without crashing", "[FileManager][Fuzz]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString seedPath = dir.filePath("seed.pclx");
    {
        std::unique_ptr<Object> o(new Object);
        o->init();
        o->addNewCameraLayer()->addNewKeyFrameAt(3);
        LayerBitmap* bmp = o->addNewBitmapLayer();
        bmp->addNewKeyFrameAt(1);
        bmp->getBitmapImageAtFrame(1)->drawRect(QRectF(0, 0, 8, 8), QPen(Qt::red), QBrush(Qt::red),
                                                QPainter::CompositionMode_SourceOver, false);
        LayerVector* vec = o->addNewVectorLayer();
        vec->addNewKeyFrameAt(2);
        BezierCurve curve({ QPointF(0, 0), QPointF(9, 0), QPointF(9, 9) });
        vec->getVectorImageAtFrame(2)->addCurve(curve, 1.0);
        o->addNewSoundLayer();
        o->addLayerGroup({ 1, "G" });
        vec->setGroupId(1);
        o->data()->setMarker(2, { "m", QColor("#ff0000") });
        FileManager fm;
        REQUIRE(fm.save(o.get(), seedPath).ok());
    }
    QFile seedFile(seedPath);
    REQUIRE(seedFile.open(QFile::ReadOnly));
    const QByteArray seedZip = seedFile.readAll();

    // Unpacked seed as a legacy .pcl: main XML plus its "<file>.data" folder.
    const QString unpacked = dir.filePath("unpacked") + "/";
    REQUIRE(MiniZ::uncompressFolder(seedPath, unpacked).ok());
    const QString pclPath = dir.filePath("fuzz.pcl");
    REQUIRE(QDir().rename(unpacked + "data", pclPath + ".data"));
    QFile xmlFile(unpacked + PFF_XML_FILE_NAME);
    REQUIRE(xmlFile.open(QFile::ReadOnly));
    const QByteArray seedXml = xmlFile.readAll();

    // Fixed seed keeps CI deterministic. Raise the counts or change the seed locally to search further.
    std::mt19937 rng(118);
    for (int i = 0; i < 2000; ++i)
    {
        INFO("xml iteration " << i);
        writeBytes(pclPath, mutate(seedXml, rng));
        loadEverything(pclPath);
    }
    const QString pclxPath = dir.filePath("fuzz.pclx");
    for (int i = 0; i < 500; ++i)
    {
        INFO("zip iteration " << i);
        writeBytes(pclxPath, mutate(seedZip, rng));
        loadEverything(pclxPath);
    }
}

TEST_CASE("FileManager saves after its temp working folder was removed", "[FileManager]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());

    FileManager fm;
    std::unique_ptr<Object> obj(new Object);
    obj->init();
    obj->addNewBitmapLayer();
    REQUIRE(QDir(obj->workingDir()).removeRecursively());
    REQUIRE_FALSE(removePFFTmpDirectory(obj->workingDir()));

    REQUIRE(fm.save(obj.get(), testDir.filePath("shot.pclx")).ok());
}

TEST_CASE("FileManager keeps every frame when it saves or autosaves right after an open", "[FileManager][ParallelOpen]")
{
    QTemporaryDir testDir("PENCIL_TEST_XXXXXXXX");
    REQUIRE(testDir.isValid());
    const int layers = 3;
    const int frames = 30;
    auto colorAt = [](int l, int f) { return qRgba(l * 60, f * 6, 200, 255); };

    // Enough entries that every extract worker gets some.
    const QString source = testDir.filePath("source.pclx");
    {
        std::unique_ptr<Object> obj(new Object);
        obj->init();
        for (int l = 0; l < layers; ++l)
        {
            LayerBitmap* layer = obj->addNewBitmapLayer();
            for (int f = 1; f <= frames; ++f)
            {
                if (f > 1) REQUIRE(layer->addNewKeyFrameAt(f)); // a new layer starts with frame 1
                BitmapImage fill(QRect(0, 0, 8, 8), QColor::fromRgba(colorAt(l, f)));
                layer->getBitmapImageAtFrame(f)->paste(&fill);
            }
        }
        FileManager fm;
        REQUIRE(fm.save(obj.get(), source).ok());
    }

    auto requireEveryFrame = [&](Object* obj) {
        REQUIRE(obj != nullptr);
        int l = 0;
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            if (obj->getLayer(i)->type() != Layer::BITMAP) continue;
            auto layer = static_cast<LayerBitmap*>(obj->getLayer(i));
            REQUIRE(layer->keyFrameCount() == frames);
            for (int f = 1; f <= frames; ++f)
            {
                CAPTURE(l, f);
                REQUIRE(layer->getBitmapImageAtFrame(f)->image()->pixel(4, 4) == colorAt(l, f));
            }
            ++l;
        }
        REQUIRE(l == layers);
    };

    FileManager fm;
    std::unique_ptr<Object> opened(fm.load(source));
    REQUIRE(fm.error().ok());

    SECTION("Save as a new file")
    {
        const QString copy = testDir.filePath("copy.pclx");
        REQUIRE(fm.save(opened.get(), copy).ok());
        FileManager reader;
        std::unique_ptr<Object> reloaded(reader.load(copy));
        requireEveryFrame(reloaded.get());
    }

    SECTION("Save over the opened file")
    {
        REQUIRE(fm.save(opened.get(), source).ok());
        FileManager reader;
        std::unique_ptr<Object> reloaded(reader.load(source));
        requireEveryFrame(reloaded.get());
    }

    SECTION("Autosave, then recover after a crash")
    {
        REQUIRE(fm.writeToWorkingFolder(opened.get()).ok());
        FileManager recovery;
        std::unique_ptr<Object> recovered(recovery.recoverUnsavedProject(opened->workingDir()));
        REQUIRE(recovery.error().ok());
        requireEveryFrame(recovered.get());
    }
}

namespace
{
// One line per node, attributes sorted: Qt 6.4 and 6.5 QDom write attributes in hash order.
void canonicalXml(const QDomNode& parent, int depth, QString& out)
{
    for (QDomNode n = parent.firstChild(); !n.isNull(); n = n.nextSibling())
    {
        const QString indent(depth * 2, ' ');
        if (n.isElement())
        {
            const QDomElement e = n.toElement();
            QStringList attributes;
            const QDomNamedNodeMap map = e.attributes();
            for (int i = 0; i < map.count(); ++i)
            {
                const QDomAttr a = map.item(i).toAttr();
                attributes << QString("%1=\"%2\"").arg(a.name(), a.value());
            }
            attributes.sort();
            out += indent + "<" + e.tagName() + (attributes.isEmpty() ? "" : " " + attributes.join(' ')) + ">\n";
            // The app version changes on every release, not with the format.
            if (e.tagName() == "version" && depth == 1) { continue; }
            canonicalXml(n, depth + 1, out);
        }
        else if (n.isText())
        {
            out += indent + "\"" + n.nodeValue() + "\"\n";
        }
    }
}
} // namespace

TEST_CASE("A saved fixture project writes the main.xml of the format before v1.7", "[FileManager]")
{
    std::unique_ptr<Object> object(new Object);
    object->init();

    LayerCamera* camera = object->addNewCameraLayer();
    camera->addNewKeyFrameAt(5);
    static_cast<Camera*>(camera->getKeyFrameAt(5))->translate(12, -7);

    LayerBitmap* ink = object->addNewBitmapLayer();
    ink->setName("Ink");
    ink->addNewKeyFrameAt(1);
    ink->addNewKeyFrameAt(3);
    QImage pixels(4, 4, QImage::Format_ARGB32_Premultiplied);
    pixels.fill(Qt::red);
    BitmapImage source(QPoint(2, 3), pixels);
    ink->getBitmapImageAtFrame(1)->paste(&source);
    KeyFrame* key = ink->getKeyFrameAt(1);
    key->setMark(Mark::Breakdown);
    key->setTween(true);
    key->setTweenEasing(CameraEasingType::INQUAD);
    key->setTweenState(TweenState{ 1.5, -2, 30, 0.25, 0.5 });
    key->setPanelFields({ "1", "12", "Pan & go", "\"Hi\"" });
    key->setLinkId(4);
    ink->getKeyFrameAt(3)->setLinkId(4);
    ink->setBlendMode(BlendMode::Multiply);
    ink->setTimesheetColumn(TimesheetColumn::InbetweenB);
    ink->setLockAlpha(true);
    ink->setOnionSkin(false);
    ink->setGroupId(3);
    object->addLayerGroup({ 3, "Group", true, false, 0.5 });

    LayerVector* lines = object->addNewVectorLayer();
    lines->setName("Lines");
    lines->addNewKeyFrameAt(2);
    BezierCurve curve(QList<QPointF>{ QPointF(0, 0), QPointF(10, 5), QPointF(20, 0) }, false);
    lines->getVectorImageAtFrame(2)->addCurve(curve, 1.0, false);
    lines->setRough(true);
    lines->setStoryboard(true);
    lines->setLockAll(true);
    lines->setVisible(false);

    ObjectData* data = object->data();
    data->setMarker(2, { "Hit", QColor(10, 20, 30) });
    data->setMarker(4, { "Ink", QColor(40, 50, 60) }, ink->id());
    PaperTemplate paper;
    paper.on = true;
    paper.title = "Title";
    paper.notes = "Notes";
    data->setPaperTemplate(paper);
    data->setCurrentFrame(3);
    data->setFrameRate(12); // the default comes from the user's settings
    object->setSceneLength(24);

    QTemporaryDir dir;
    const QString path = dir.filePath("fixture.pclx");
    FileManager fm;
    REQUIRE(fm.save(object.get(), path).ok());
    const QString unpacked = dir.filePath("unpacked") + "/";
    REQUIRE(MiniZ::uncompressFolder(path, unpacked).ok());
    // Every file by name; main.xml and the vector Drawings element by element.
    QString canonical;
    QDirIterator it(unpacked, QDir::Files, QDirIterator::Subdirectories);
    QStringList files;
    while (it.hasNext()) { files << QDir(unpacked).relativeFilePath(it.next()); }
    files.sort();
    for (const QString& name : files)
    {
        canonical += "== " + name + "\n";
        if (!name.endsWith(".xml") && !name.endsWith(".vec")) { continue; }
        QFile file(unpacked + name);
        REQUIRE(file.open(QFile::ReadOnly));
        QDomDocument doc;
        REQUIRE(doc.setContent(&file));
        canonicalXml(doc, 0, canonical);
    }

    INFO(canonical.toStdString());
    // Golden from master before v1.7 (5a1a584).
    CHECK(QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha1).toHex().toStdString() == "5360d849a3c02c451685bbf48fc969abe123e44c");
}

namespace
{
// Saves bitmap layers with a Drawing at each of `keys` and loads them back: every Drawing is unloaded, with a file.
Object* savedDrawings(const QString& path, int layers, const std::vector<int>& keys)
{
    FileManager fm;
    {
        Object object;
        object.init();
        object.addNewCameraLayer();
        for (int l = 0; l < layers; ++l)
        {
            LayerBitmap* layer = object.addNewBitmapLayer();
            for (int k : keys)
            {
                layer->addNewKeyFrameAt(k);
                layer->getBitmapImageAtFrame(k)->drawRect(QRectF(0, 0, 10, 10), Qt::NoPen, Qt::red,
                                                          QPainter::CompositionMode_SourceOver, false);
            }
        }
        REQUIRE(fm.save(&object, path).ok());
    }
    return fm.load(path);
}
}

TEST_CASE("Frame pool: a far jump decodes only the shown Drawing of each layer on the calling thread")
{
    QTemporaryDir dir;
    std::vector<int> keys;
    for (int f = 1; f <= 200; ++f) { keys.push_back(f); }
    TestSession s(savedDrawings(dir.filePath("jump.pclx"), 2, keys));
    REQUIRE(s.object);
    REQUIRE(s.start());

    const bool wasOn = StageProfiler::enabled();
    StageProfiler::enabled() = true;
    StageProfiler::reset();
    s.editor->scrubTo(150);
    const auto decodes = StageProfiler::calls(StageProfiler::DrawingLoad);
    StageProfiler::enabled() = wasOn;

    CHECK(decodes <= 2);
    CHECK(static_cast<LayerBitmap*>(s.object->getLayer(1))->getBitmapImageAtFrame(150)->isLoaded());
    CHECK(static_cast<LayerBitmap*>(s.object->getLayer(2))->getBitmapImageAtFrame(150)->isLoaded());
}

TEST_CASE("Frame pool: held and onion Drawings unload when far and the pool counts every loaded Drawing")
{
    QTemporaryDir dir;
    TestSession s(savedDrawings(dir.filePath("held.pclx"), 1, { 1, 100, 200, 300 }));
    REQUIRE(s.object);
    REQUIRE(s.start());
    auto* layer = static_cast<LayerBitmap*>(s.object->getLayer(1));

    for (int f : { 50, 150, 250 })
    {
        s.editor->scrubTo(f);
        // What the canvas painter loads: the held Drawing and the next onion Drawing.
        static_cast<BitmapImage*>(layer->getLastKeyFrameAtPosition(f))->image();
        layer->getBitmapImageAtFrame(layer->getNextKeyFramePosition(f))->image();
    }
    s.editor->scrubTo(400);

    quint64 loaded = 0;
    for (int k : { 1, 100, 200, 300 })
    {
        BitmapImage* b = layer->getBitmapImageAtFrame(k);
        INFO("Drawing " << k);
        CHECK(b->isLoaded() == (k == 300));
        loaded += b->memoryUsage();
    }
    CHECK(s.object->framePoolMemory() == loaded);
}
