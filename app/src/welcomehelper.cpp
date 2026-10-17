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
#include "welcomehelper.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>

#include "layercamera.h"
#include "object.h"

namespace Welcome
{
static const QSize THUMBNAIL_SIZE(256, 144);

const QList<Template>& templates()
{
    static const QList<Template> list = {
        { "Anime 1080p", QSize(1920, 1080), 24 },
        { "Anime 720p", QSize(1280, 720), 24 },
        { "HD 1080p", QSize(1920, 1080), 24 },
        { "HD 720p", QSize(1280, 720), 24 },
        { "4K UHD", QSize(3840, 2160), 24 },
        { "Vertical 1080 x 1920", QSize(1080, 1920), 30 },
        { "Square 1080", QSize(1080, 1080), 30 },
        { "GIF 480 x 480", QSize(480, 480), 12 },
    };
    return list;
}

int templateIndex(const QString& name)
{
    for (int i = 0; i < templates().size(); ++i)
    {
        if (templates().at(i).name == name) { return i; }
    }
    return -1;
}

Object* newObjectFromTemplate(int index)
{
    if (index < 0 || index >= templates().size())
    {
        return nullptr;
    }
    const Template& t = templates().at(index);

    auto* object = new Object();
    object->init();
    LayerCamera* camera = object->addNewCameraLayer();
    camera->setViewRect(QRect(QPoint(-t.size.width() / 2, -t.size.height() / 2), t.size));
    object->addNewBitmapLayer();
    object->data()->setCurrentLayer(1);
    object->data()->setFrameRate(t.fps);
    return object;
}

QString presetPath(int index)
{
    if (index == 0)
    {
        return QString();
    }

    const QString filename = QString("%1.pclx").arg(index);
    QDir dataDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (dataDir.cd("presets"))
    {
        return dataDir.filePath(filename);
    }
    return QString();
}

QString thumbnailPath(const QString& projectPath)
{
    QFileInfo info(projectPath);
    QString key = info.canonicalFilePath();
    if (key.isEmpty())
    {
        key = info.absoluteFilePath(); // file is missing
    }
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QString("thumbnails/%1.png").arg(QString::fromLatin1(hash)));
}

QList<RecentEntry> recentEntries(const QStringList& recentFiles)
{
    QList<RecentEntry> out;
    for (const QString& path : recentFiles)
    {
        RecentEntry e;
        e.path = path;
        e.name = QFileInfo(path).completeBaseName();
        e.missing = !QFileInfo::exists(path);
        const QString thumb = thumbnailPath(path);
        if (!e.missing && QFileInfo::exists(thumb))
        {
            e.thumbnail = thumb;
        }
        out.append(e);
    }
    return out;
}

bool saveThumbnail(const Object* object, const QString& projectPath, int frame)
{
    if (object == nullptr || projectPath.isEmpty())
    {
        return false;
    }
    auto cameras = object->getLayersByType<LayerCamera>();
    if (cameras.empty())
    {
        return false;
    }
    LayerCamera* camera = cameras.front();
    const QSize camSize = camera->getViewSize();
    if (camSize.isEmpty())
    {
        return false;
    }

    QImage image(THUMBNAIL_SIZE, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        const qreal s = qMin(qreal(THUMBNAIL_SIZE.width()) / camSize.width(),
                             qreal(THUMBNAIL_SIZE.height()) / camSize.height());
        QTransform fit;
        fit.translate(THUMBNAIL_SIZE.width() / 2.0, THUMBNAIL_SIZE.height() / 2.0);
        fit.scale(s, s);
        painter.setWorldTransform(camera->getViewAtFrame(frame) * fit);
        object->paintImage(painter, frame, false, true);
    }

    const QString file = thumbnailPath(projectPath);
    QDir().mkpath(QFileInfo(file).absolutePath());
    return image.save(file, "PNG");
}
}
