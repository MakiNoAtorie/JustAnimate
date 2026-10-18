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
#ifndef QMINIZ_H
#define QMINIZ_H

#include <QSet>
#include <QString>
#include <functional>
#include "miniz.h"
#include "pencilerror.h"

namespace MiniZ
{
    Status sanityCheck(const QString& sZipFilePath);
    size_t istreamReadCallback(void *pOpaque, mz_uint64 file_ofs, void * pBuf, size_t n);
    // progress(done, total) runs before each entry: compressFolder calls it on its own thread,
    // uncompressFolder on its extract workers, so it must be thread-safe.
    using Progress = std::function<void(int done, int total)>;
    // Entries named in copyFromPrevious (paths relative to srcFolderPath) are copied as they are from
    // previousZipPath; the others are read from their files. PNG entries are stored, not deflated.
    Status compressFolder(QString zipFilePath, QString srcFolderPath, const QStringList& fileList, QString mimetype,
                          const Progress& progress = {}, const QString& previousZipPath = {},
                          const QSet<QString>& copyFromPrevious = {});
    Status uncompressFolder(QString zipFilePath, QString destPath, const Progress& progress = {});
}
#endif
