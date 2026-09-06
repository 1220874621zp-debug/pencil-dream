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

#include "debuglog.h"

#include <QDateTime>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

namespace
{
    const int kMaxLines = 4000;

    QMutex gMutex;
    QList<QString> gLines;
    QtMessageHandler gPrevHandler = nullptr;

    void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
    {
        {
            QMutexLocker lock(&gMutex);
            QString tag;
            switch (type)
            {
            case QtWarningMsg:   tag = "W "; break;
            case QtCriticalMsg:  tag = "C "; break;
            case QtFatalMsg:     tag = "F "; break;
            case QtInfoMsg:      tag = "I "; break;
            default:             tag = "D "; break;
            }
            gLines.append(QDateTime::currentDateTime().toString("hh:mm:ss.zzz ") + tag + msg);
            if (gLines.size() > kMaxLines)
                gLines.removeAt(0);
        }
        if (gPrevHandler)
            gPrevHandler(type, context, msg);
    }
}

namespace DebugLog
{
    void install()
    {
        gPrevHandler = qInstallMessageHandler(messageHandler);
    }

    QString dump()
    {
        QMutexLocker lock(&gMutex);
        return gLines.join('\n');
    }
}
