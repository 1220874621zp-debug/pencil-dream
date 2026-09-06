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

#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <QString>

/** In-memory ring buffer of qDebug/qWarning/qCritical output that the
 *  user can inspect and copy from a button in the status bar. */
namespace DebugLog
{
    void install();       // call once at startup (qInstallMessageHandler)
    QString dump();       // all buffered lines, oldest first
}

#endif
