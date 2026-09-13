/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License;
version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#ifndef BUILTINWORKSPACES_H
#define BUILTINWORKSPACES_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

/** 软件自带的工作区预设条目（名称 + 主窗口 state 快照的 base64） */
struct BuiltinWorkspace
{
    const char* name = nullptr;
    const char* stateBase64 = nullptr;
};

namespace BuiltinWorkspaces
{
    const QList<BuiltinWorkspace>& all();
    QStringList names();
    /** 内置工作区状态快照；名字不存在时返回空 QByteArray */
    QByteArray state(const QString& name);
}

#endif // BUILTINWORKSPACES_H
