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
#include "qminiz.h"

#include <sstream>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDirIterator>
#include <QRegularExpression>
#include "util.h"


Status MiniZ::sanityCheck(const QString& sZipFilePath)
{
    mz_zip_archive* mz = new mz_zip_archive;
    OnScopeExit(delete mz);
    mz_zip_zero_struct(mz);
    QByteArray utf8Bytes = sZipFilePath.toUtf8();
    mz_bool readOk = mz_zip_reader_init_file(mz, utf8Bytes.constData(), 0);

    mz_zip_error read_err = mz_zip_get_last_error(mz);

    mz_bool closeOk = mz_zip_reader_end(mz);

    mz_zip_error close_err = mz_zip_get_last_error(mz);

    if (!readOk || !closeOk) {
        DebugDetails dd;

        dd << "\n[Miniz sanity check]\n";
        if (read_err != MZ_ZIP_NO_ERROR) {
            dd << QString("Found an error while reading the file. Error code: %2, reason: %3").arg(static_cast<int>(read_err)).arg(mz_zip_get_error_string(read_err));
        }
        if (close_err != MZ_ZIP_NO_ERROR) {
            dd << QString("Found an error while closing the file. Error code: %2, reason: %3").arg(static_cast<int>(close_err)).arg(mz_zip_get_error_string(close_err));
        }
        return Status(Status::ERROR_MINIZ_FAIL, dd);
    }


    return Status::OK;
}

size_t MiniZ::istreamReadCallback(void *pOpaque, mz_uint64 file_ofs, void * pBuf, size_t n)
{
    std::istream *stream = static_cast<std::istream*>(pOpaque);
    mz_int64 cur_ofs = stream->tellg();
    if ((mz_int64)file_ofs < 0 || (cur_ofs != (mz_int64)file_ofs && stream->seekg((mz_int64)file_ofs, std::ios_base::beg)))
        return 0;
    stream->read(static_cast<char*>(pBuf), n);
    return stream->gcount();
}

// ReSharper disable once CppInconsistentNaming
Status MiniZ::compressFolder(QString zipFilePath, QString srcFolderPath, const QStringList& fileList, QString mimetype,
                             const Progress& progress)
{
    DebugDetails dd;
    dd << "\n[Miniz COMPRESSION diagnostics]\n";
    dd << QString("Creating Zip %1 from folder %2").arg(zipFilePath, srcFolderPath);

    if (!srcFolderPath.endsWith("/"))
    {
        dd << "Adding / to path";
        srcFolderPath.append("/");
    }

    mz_zip_archive* mz = new mz_zip_archive;
    ScopeGuard mzScopeGuard([&] {
        delete mz;
    });

    mz_zip_zero_struct(mz);

    mz_bool ok = mz_zip_writer_init_file(mz, zipFilePath.toUtf8().data(), 0);

    if (!ok)
    {
        mz_zip_error err = mz_zip_get_last_error(mz);
        dd << QString("Error: Failed to init writer. Error code: %1, reason: %2").arg(static_cast<int>(err)).arg(mz_zip_get_error_string(err));
        return Status(Status::FAIL, dd);
    }
    ScopeGuard mzScopeGuard2([&] {
        mz_zip_writer_end(mz);
    });

    // Add special uncompressed mimetype file to help with the identification of projects
    {
        QByteArray mimeData = mimetype.toUtf8();
        std::stringstream mimeStream(mimeData.toStdString());
        ok = mz_zip_writer_add_read_buf_callback(mz, "mimetype", MiniZ::istreamReadCallback, &mimeStream, mimeData.length(),
                                    0, "", 0, MZ_NO_COMPRESSION, 0, 0,
                                    0, 0);
        if (!ok)
        {
            mz_zip_error err = mz_zip_get_last_error(mz);
            dd << QString("ERROR: Unable to add mimetype. Error code: %1, reason: %2").arg(static_cast<int>(err)).arg(mz_zip_get_error_string(err));
            return Status(Status::FAIL, dd);
        }
    }

    QString canonicalFolder = QFileInfo(srcFolderPath).canonicalFilePath();
    if (canonicalFolder.isEmpty())
    {
        dd << QString("Error: Source folder does not exist: %1").arg(srcFolderPath);
        return Status(Status::FAIL, dd);
    }
    QDir baseDir(canonicalFolder);
    int done = 0;
    for (const QString& filePath : fileList)
    {
        if (progress) progress(done++, fileList.size());
        QString canonicalFilePath = QFileInfo(filePath).canonicalFilePath();
        if (canonicalFilePath.isEmpty())
        {
            dd << QString("Error: File does not exist: %1").arg(filePath);
            return Status(Status::FAIL, dd);
        }
        QString sRelativePath = baseDir.relativeFilePath(canonicalFilePath);
        if (sRelativePath.startsWith("../") || sRelativePath == "..")
        {
            dd << QString("Error: File is outside the base folder: %1").arg(filePath);
            return Status(Status::FAIL, dd);
        }
        if (sRelativePath == "mimetype") continue;

        dd << QString("Add file to zip: ").append(sRelativePath);

        ok = mz_zip_writer_add_file(mz,
                                    sRelativePath.toUtf8().data(),
                                    filePath.toUtf8().data(),
                                    "", 0, MZ_BEST_SPEED);
        if (!ok)
        {
            mz_zip_error err = mz_zip_get_last_error(mz);
            dd << QString("Error: Unable to add file: %3. Error code: %1, reason: %2 - Aborting!").arg(static_cast<int>(err)).arg(mz_zip_get_error_string(err), sRelativePath);
            return Status(Status::FAIL, dd);
        }
    }
    ok &= mz_zip_writer_finalize_archive(mz);
    if (!ok)
    {
        mz_zip_error err = mz_zip_get_last_error(mz);
        dd << QString("Error: Failed to finalize archive. Error code %1, reason: %2").arg(static_cast<int>(err)).arg(mz_zip_get_error_string(err));
        return Status(Status::FAIL, dd);
    }

    return Status::OK;
}

Status MiniZ::uncompressFolder(QString zipFilePath, QString destPath, const Progress& progress)
{
    DebugDetails dd;
    dd << "\n[Miniz EXTRACTION diagnostics]\n";
    dd << QString("Unzip file %1 to folder %2").arg(zipFilePath, destPath);

    if (!QFile::exists(zipFilePath))
    {
        dd << QString("Error: Zip file does not exist.");
        return Status::FILE_NOT_FOUND;
    }

    QString sBaseDir = QFileInfo(destPath).absolutePath();
    QDir baseDir(sBaseDir);
    if (!baseDir.exists())
    {
        bool ok = baseDir.mkpath(".");
        Q_ASSERT(ok);
    }

    baseDir.makeAbsolute();

    mz_zip_archive* mz = new mz_zip_archive;
    ScopeGuard mzScopeGuard([&] {
        delete mz;
    });

    mz_zip_zero_struct(mz);

    mz_bool ok = mz_zip_reader_init_file(mz, zipFilePath.toUtf8().data(), 0);
    if (!ok) {
        mz_zip_error err = mz_zip_get_last_error(mz);
        dd << QString("Error: Failed to init reader. Error code: %1, reason: %2").arg(static_cast<int>(err)).arg(mz_zip_get_error_string(err));
        return Status(Status::FAIL, dd);
    }
    ScopeGuard mzScopeGuard2([&] {
        mz_zip_reader_end(mz);
    });

    int num = mz_zip_reader_get_num_files(mz);

    mz_zip_archive_file_stat* stat = new mz_zip_archive_file_stat;
    OnScopeExit(delete stat);

    // Zip-slip: reject the whole archive before writing anything.
    // Backslashes count as separators for the check only (old Windows archives).
    static const QRegularExpression driveLetter("^[A-Za-z]:");
    for (int i = 0; i < num; ++i)
    {
        if (!mz_zip_reader_file_stat(mz, i, stat))
        {
            dd << QString("Error: Entry %1 cannot be read.").arg(i);
            return Status(Status::FAIL, dd);
        }
        const QString name = QString::fromUtf8(stat->m_filename);
        const QString checked = QString(name).replace(QLatin1Char(0x5C), QLatin1Char('/'));
        // Checked by name only: extraction writes plain files, so an entry cannot plant a link,
        // and a lookup on disk per entry made a 5000-file project open 2 s slower.
        const QString clean = QDir::cleanPath(checked);
        if (checked.startsWith('/') || checked.contains(driveLetter) ||
            clean == QLatin1String("..") || clean.startsWith(QLatin1String("../")))
        {
            dd << QString("Error: Entry is outside the extraction folder: %1").arg(name);
            return Status(Status::FAIL, dd);
        }
    }

    for (int i = 0; i < num; ++i)
    {
        if (!mz_zip_reader_file_stat(mz, i, stat)) { return Status(Status::FAIL, dd); }

        if (stat->m_is_directory)
        {
            QString sFolderPath = QString::fromUtf8(stat->m_filename);
            dd << QString("Make Dir: ").append(sFolderPath);

            if (!baseDir.mkpath(sFolderPath))
            {
                dd << "Make Dir failed.";
                return Status(Status::FAIL, dd);
            }
        }
    }

    for (int i = 0; i < num; ++i)
    {
        if (progress) progress(i, num);
        if (!mz_zip_reader_file_stat(mz, i, stat)) { return Status(Status::FAIL, dd); }

        if (!stat->m_is_directory)
        {
            if (QString(stat->m_filename) == "mimetype") continue;
            QString sFullPath = baseDir.filePath(QString::fromUtf8(stat->m_filename));
            dd << QString("Unzip file: ").append(sFullPath);
            if (!QFileInfo(sFullPath).absoluteDir().mkpath("."))
            {
                dd << "Make Dir failed.";
                return Status(Status::FAIL, dd);
            }

            bool extractOK = mz_zip_reader_extract_to_file(mz, i, sFullPath.toUtf8(), 0);
            if (!extractOK)
            {
                ok = false;
                mz_zip_error err = mz_zip_get_last_error(mz);
                dd << QString("WARNING: Unable to extract file. Error code: %1, reason: %2").arg(static_cast<int>(err)).arg(mz_zip_get_error_string(err));
            }
        }
    }

    if (!ok)
    {
        return Status(Status::FAIL, dd);
    }
    return Status::OK;
}
