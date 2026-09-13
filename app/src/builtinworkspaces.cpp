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
#include "builtinworkspaces.h"

#include <QByteArray>

/** 软件自带的工作区预设：主窗口 QMainWindow::saveState() 快照（base64 存放）。
 *  与用户保存的同名工作区并存时，用户版本优先（applyWorkspace 先查设置）。 */
static const BuiltinWorkspace kBuiltinWorkspaces[] = {
    { "默认",
    "AAAA/wAAAAD9AAAAAwAAAAAAAADrAAADUfwCAAAAAfwAAABJAAADUQAAAH8A/////AEAAAAC+wAAAA4AVABvAG8AbABCAG8A"
    "eAEAAAAAAAAAKAAAACgA////+wAAABgAQgByAHUAcwBoAFAAcgBlAHMAZQB0AHMBAAAAKwAAAMAAAADAAP///wAAAAEAAAH3"
    "AAADUfwCAAAABPwAAABJAAABQgAAAP8A/////AEAAAAC+wAAABQATwBuAGkAbwBuACAAUwBrAGkAbgEAAAgJAAABCgAAAIwA"
    "/////AAACRYAAADqAAAAgQD////6AAAAAQEAAAAC+wAAAB4AQwBvAGwAbwByACAASQBuAHMAcABlAGMAdABvAHIBAAAAAP//"
    "//8AAAB4AP////sAAAAUAEMAbwBsAG8AcgBXAGgAZQBlAGwBAAAJDQAAAPMAAAAHAP////wAAAGOAAACDAAAAJkA/////AEA"
    "AAAC+wAAABQAVABvAG8AbABPAHAAdABpAG8AbgEAAAgJAAABBgAAAIwA////+wAAABgAQwBvAGwAbwByAFAAYQBsAGUAdAB0"
    "AGUBAAAJEgAAAO4AAACuAP////sAAAAaAFIAZQBmAGUAcgBlAG4AYwBlAEMAYQByAGQCAAAGMwAAAVkAAAHOAAAB2/sAAAAa"
    "AEUAeABwAG8AcwB1AHIAZQBTAGgAZQBlAHQAAAAChAAAAKQAAAB8AP///wAAAAMAAAoAAAABlfwBAAAAAfsAAAAQAFQAaQBt"
    "AGUATABpAG4AZQEAAAAAAAAKAAAAAuoA////AAAHGAAAA1EAAAAEAAAABAAAAAgAAAAI/AAAAAEAAAACAAAAAwAAABgAbQBN"
    "AGEAaQBuAFQAbwBvAGwAYgBhAHIBAAAAAP////8AAAAAAAAAAAAAABgAbQBWAGkAZQB3AFQAbwBvAGwAYgBhAHIBAAACmP//"
    "//8AAAAAAAAAAAAAAB4AbQBPAHYAZQByAGwAYQB5AFQAbwBvAGwAYgBhAHIAAAABqv////8AAAAAAAAAAA==" },
    { "摄影表",
    "AAAA/wAAAAD9AAAABAAAAAAAAAFAAAAEjvwCAAAAAfwAAABJAAAEjgAAATcA/////AEAAAAD+wAAAA4AVABvAG8AbABCAG8A"
    "eAEAAAAAAAAAOwAAACgA/////AAAAD4AAAECAAAArgD////8AgAAAAL8AAAASQAAAi4AAAC2AQAAHPoAAAAAAgAAAAL7AAAA"
    "GABDAG8AbABvAHIAUABhAGwAZQB0AHQAZQEAAAAA/////wAAAJkA////+wAAABQAVABvAG8AbABPAHAAdABpAG8AbgEAAABJ"
    "AAAEUgAAAFgA////+wAAABQATwBuAGkAbwBuACAAUwBrAGkAbgEAAAJ6AAACXQAAAH4A////+wAAABgAQgByAHUAcwBoAFAA"
    "cgBlAHMAZQB0AHMAAAAAKwAAAMAAAADAAP///wAAAAEAAAFWAAAEjvwCAAAAA/sAAAAaAEUAeABwAG8AcwB1AHIAZQBTAGgA"
    "ZQBlAHQBAAAASQAABI4AAAB8AP////sAAAAeAEMAbwBsAG8AcgAgAEkAbgBzAHAAZQBjAHQAbwByAgAAA4MAAALHAAAA3gAA"
    "AX37AAAAGgBSAGUAZgBlAHIAZQBuAGMAZQBDAGEAcgBkAgAABjMAAAFZAAABzgAAAdsAAAACAAAKAAAAAIL8AQAAAAH7AAAA"
    "FABDAG8AbABvAHIAVwBoAGUAZQBsAgAAAQcAAAEnAAABDQAAAQwAAAADAAAKAAAAAFj8AQAAAAH7AAAAEABUAGkAbQBlAEwA"
    "aQBuAGUBAAAAAAAACgAAAALqAP///wAAB2QAAASOAAAABAAAAAQAAAAIAAAACPwAAAABAAAAAgAAAAMAAAAYAG0ATQBhAGkA"
    "bgBUAG8AbwBsAGIAYQByAQAAAAD/////AAAAAAAAAAAAAAAYAG0AVgBpAGUAdwBUAG8AbwBsAGIAYQByAQAAApj/////AAAA"
    "AAAAAAAAAAAeAG0ATwB2AGUAcgBsAGEAeQBUAG8AbwBsAGIAYQByAAAAAar/////AAAAAAAAAAA=" },
};

namespace BuiltinWorkspaces
{

const QList<BuiltinWorkspace>& all()
{
    static const QList<BuiltinWorkspace> list = []()
    {
        QList<BuiltinWorkspace> l;
        for (const auto& w : kBuiltinWorkspaces)
        {
            l.append(w);
        }
        return l;
    }();
    return list;
}

QStringList names()
{
    QStringList result;
    for (const auto& w : kBuiltinWorkspaces)
    {
        result << QString::fromUtf8(w.name);
    }
    return result;
}

QByteArray state(const QString& name)
{
    for (const auto& w : kBuiltinWorkspaces)
    {
        if (name == QString::fromUtf8(w.name))
        {
            return QByteArray::fromBase64(w.stateBase64);
        }
    }
    return QByteArray();
}

} // namespace BuiltinWorkspaces
