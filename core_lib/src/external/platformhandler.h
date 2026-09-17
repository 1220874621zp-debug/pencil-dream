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
#ifndef PLATFORMHANDLER_H
#define PLATFORMHANDLER_H

#include <QtGlobal>
#include <QString>

namespace PlatformHandler
{

void configurePlatformSpecificSettings();
bool isDarkMode();
void initialise();

// 若 pid 进程在运行且可执行文件名与 exeName 一致（不区分大小写），
// 把它的所有可见顶层窗口恢复/置前后返回 true；进程不在、查询失败或
// 文件名对不上（PID 被其他程序复用）返回 false。调用方据此区分
// 「真有实例在跑」和「陈旧锁残留」
bool raiseWindowsOfProcessIfNamed(qint64 pid, const QString& exeName);

}

#endif // PLATFORMHANDLER_H
