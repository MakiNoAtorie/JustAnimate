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
#ifndef EXPORTER_H
#define EXPORTER_H

#include <functional>
#include <QSize>
#include <QString>
#include "pencilerror.h"

class LayerCamera;
class Object;

struct ExportRequest
{
    enum class Kind { Image, Sequence, SpriteSheet };
    Kind kind = Kind::Image;
    QString cameraName;        // empty or unknown: the first camera
    int startFrame = 1;        // Image exports startFrame only
    int endFrame = 1;
    QSize size;
    QString format = "PNG";    // Image and Sequence; SpriteSheet is always PNG
    QString path;              // Sequence: the prefix before "0001.png"
    bool transparency = false; // JPG and BMP are always opaque
    bool keyframesOnly = false; // Sequence: only frames where keyLayerName has a key
    QString keyLayerName;
    int columns = 0;           // SpriteSheet; 0 means ceil(sqrt(n))
    // Asked once when a target file exists; empty means overwrite.
    std::function<bool(const QString& path)> confirmOverwrite;
};

namespace Exporter
{
// The camera layer called name, or the first camera.
const LayerCamera* camera(const Object& object, const QString& name);

// progress(done, total) returns false to cancel. A cancelled or failed run leaves no new
// files and keeps the files it would have replaced; a declined overwrite returns CANCELED.
Status run(const Object& object, const ExportRequest& request, const std::function<bool(int done, int total)>& progress = {});
}

#endif // EXPORTER_H
