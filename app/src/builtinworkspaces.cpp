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
    "AAAA/wAAAAD9AAAAAwAAAAAAAADrAAADUfwCAAAAAfwAAABJAAADUQAAAH8A/////AEA"
    "AAAC+wAAAA4AVABvAG8AbABCAG8AeAEAAAAAAAAAKAAAACgA////+wAAABgAQgByAHUA"
    "cwBoAFAAcgBlAHMAZQB0AHMBAAAAKwAAAMAAAADAAP///wAAAAEAAAH3AAADUfwCAAAA"
    "BPwAAABJAAABQgAAAP8A/////AEAAAAC+wAAABQATwBuAGkAbwBuACAAUwBrAGkAbgEA"
    "AAgJAAABCgAAAIwA/////AAACRYAAADqAAAAgQD////6AAAAAQEAAAAC+wAAAB4AQwBv"
    "AGwAbwByACAASQBuAHMAcABlAGMAdABvAHIBAAAAAP////8AAAB4AP////sAAAAUAEMA"
    "bwBsAG8AcgBXAGgAZQBlAGwBAAAJDQAAAPMAAAAHAP////wAAAGOAAACDAAAAJkA////"
    "/AEAAAAC+wAAABQAVABvAG8AbABPAHAAdABpAG8AbgEAAAgJAAABBgAAAIwA////+wAA"
    "ABgAQwBvAGwAbwByAFAAYQBsAGUAdAB0AGUBAAAJEgAAAO4AAACuAP////sAAAAaAFIA"
    "ZQBmAGUAcgBlAG4AYwBlAEMAYQByAGQCAAAGMwAAAVkAAAHOAAAB2/sAAAAaAEUAeABw"
    "AG8AcwB1AHIAZQBTAGgAZQBlAHQAAAAChAAAAKQAAAB8AP///wAAAAMAAAoAAAABlfwB"
    "AAAAAfwAAAAAAAAKAAAAAuoA////+gAAAAECAAAAAvsAAAAUAFMAdABvAHIAeQBiAG8A"
    "YQByAGQBAAAAAP////8AAAB8AP////sAAAAQAFQAaQBtAGUATABpAG4AZQEAAAOdAAAB"
    "lQAAAFgA////AAAHGAAAA1EAAAAEAAAABAAAAAgAAAAI/AAAAAEAAAACAAAAAwAAABgA"
    "bQBNAGEAaQBuAFQAbwBvAGwAYgBhAHIBAAAAAP////8AAAAAAAAAAAAAABgAbQBWAGkA"
    "ZQB3AFQAbwBvAGwAYgBhAHIBAAACvP////8AAAAAAAAAAAAAAB4AbQBPAHYAZQByAGwA"
    "YQB5AFQAbwBvAGwAYgBhAHIAAAABqv////8AAAAAAAAAAA==" },
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
