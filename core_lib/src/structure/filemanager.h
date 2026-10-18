/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef OBJECTSAVELOADER_H
#define OBJECTSAVELOADER_H


#include <functional>
#include <future>
#include <memory>
#include <QObject>
#include <QString>
#include <QDomElement>
#include "log.h"
#include "pencildef.h"
#include "pencilerror.h"
#include "colorref.h"
#include "layer.h"

class Object;
class ObjectData;
class QDir;


class FileManager : public QObject
{
    Q_OBJECT

public:
    FileManager(QObject* parent = 0);

    Object* load(const QString& sFilenNme);
    Status  save(const Object*, const QString& sFileName);
    struct PendingArchive;
    /// A save that a worker finishes. finish() waits for it, then marks the Drawings it wrote as saved.
    class BackgroundSave
    {
    public:
        bool valid() const { return mResult.valid(); }
        bool ready() const;
        void wait() const;
        /// On the thread that started the save, with the Object it saved.
        Status finish(const Object* object);

    private:
        friend class FileManager;
        std::future<Status> mResult;
        std::shared_ptr<PendingArchive> mArchive;
    };
    // Snapshots the modified Drawings and main.xml, moves frames and writes the palette now; the worker pool
    // encodes the Drawings, writes main.xml, zips and renames. done() runs on the worker before the result is
    // ready. The Object is read again only by finish().
    BackgroundSave saveInBackground(const Object*, const QString& sFileName, const std::function<void()>& done);
    Status  writeToWorkingFolder(const Object*);

    Status error() const { return mError; }
    // Sound clips the last save wrote without audio because their file was missing.
    QStringList skippedSoundClips() const { return mSkippedSoundClips; }
    Status verifyObject(Object* obj);

    QStringList searchForUnsavedProjects();
    Object* recoverUnsavedProject(QString projectIntermediatePath);

signals:
    void progressChanged(int progress);
    void progressRangeChanged(int maxValue);

private:
    Status copyDir(const QDir src, const QDir dst);
    Status unzip(const QString& strZipFile, const QString& strUnzipTarget);

    bool loadObject(Object*, const QDomElement& root);
    bool loadObjectOldWay(Object*, const QDomElement& root);
    bool isArchiveFormat(const QString& fileName) const;
    bool loadPalette(Object*);
    // framesWritten gets every frame file this save wrote or moved.
    Status writeKeyFrameFiles(const Object* obj, const QString& dataFolder, QStringList& filesWritten,
                              QStringList& framesWritten, std::vector<DrawingWrite>& drawings);
    QByteArray mainXmlBytes(const Object* obj);
    static Status writeMainXml(const QString& mainXmlPath, const QByteArray& xml);
    Status writePalette(const Object* obj, const QString& dataFolder, QStringList& filesWritten);

    ObjectData loadProjectData(const QDomElement& element);
    QDomElement saveProjectData(const Object*, QDomDocument& xmlDoc);

    void extractProjectData(const QDomElement& element, ObjectData& data);
    void handleOpenProjectError(Status::ErrorCode, const DebugDetails&);

    void progressForward();
    // Checks the path and writes the working folder; on OK, archive zips it into the target.
    Status writeProjectFiles(const Object*, const QString& sFileName, std::shared_ptr<PendingArchive>& archive);
    // Runs job(progress) on a worker thread and emits progress here until it is done.
    Status runArchiveJob(const std::function<Status(const std::function<void(int, int)>&)>& job);

private: // Project recovery
    bool isProjectRecoverable(const QString& projectFolder);
    Status recoverObject(Object* object);
    Status rebuildMainXML(Object* object);
    Status rebuildLayerXmlTag(QDomDocument& doc, QDomElement& elemObject,
                              const int layerIndex, const QStringList& frames);
    QString recoverLayerName(Layer::LAYER_TYPE, int index);
    int layerIndexFromFilename(const QString& filename);
    int framePosFromFilename(const QString& filename);

private:
    Status mError = Status::OK;
    QString mstrLastTempFolder;
    QStringList mSkippedSoundClips;

    int mCurrentProgress = 0;
    int mMaxProgressValue = 100;
};

#endif // OBJECTSAVELOADER_H
