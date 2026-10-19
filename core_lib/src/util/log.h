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

#pragma once
#include<QLoggingCategory>
#include<QStringList>


/**
 * With Qt Category logging, it allows devs to turn on/off the debug log for certain modules
 * Uncomment the following #define DEBUG_LOG_ModuleName to enable the category logging
 */

//#define DEBUG_LOG_CANVASPAINTER
//#define DEBUG_LOG_FILEMANAGER

#ifdef DEBUG_LOG_CANVASPAINTER
  Q_DECLARE_LOGGING_CATEGORY(logCanvasPainter);
  #define CANVASPAINTER_LOG(...) qCDebug(logCanvasPainter, __VA_ARGS__)
#else
  #define CANVASPAINTER_LOG(...) ((void)0)
#endif

#ifdef DEBUG_LOG_FILEMANAGER
  Q_DECLARE_LOGGING_CATEGORY(logFileManager);
  #define FILEMANAGER_LOG(...) qCDebug(logFileManager, __VA_ARGS__)
#else
  #define FILEMANAGER_LOG(...) ((void)0)
#endif

void initCategoryLogging();

/** Write every Qt message to a new session log in `dir` as well as to the
 *  previous handler. `header` lines go first. Keeps the newest `keep` logs.
 *  Returns the log's path, or an empty string when it cannot be opened. */
QString startSessionLog(const QString& dir, const QStringList& header, int keep = 10);
/// Restore the previous handler and close the log.
void stopSessionLog();
/// The current session log, empty when none is open.
QString sessionLogPath();
/// Write the current log, then the previous session's, to `target`.
bool exportSessionLogs(const QString& target);
/// Append a "crashed" line. Only system calls: safe in a crash handler.
void writeCrashLine();
