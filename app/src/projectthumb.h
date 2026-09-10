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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef PROJECTTHUMB_H
#define PROJECTTHUMB_H

#include <QImage>
#include <QString>

// 工程缩略图：从 .pclx 抽最上层位图类图层的首帧，letterbox 进 16:9 卡片。
// 结果按源文件 size+mtime 落盘缓存（应用数据目录 thumbs/），源变更即重生成。
namespace ProjectThumb
{
    // 16:9 卡片尺寸（显示尺寸的 2 倍，供高 DPI）
    inline constexpr int CARD_W = 480;
    inline constexpr int CARD_H = 270;

    // 含磁盘缓存的取图入口；文件缺失/损坏/无位图层返回空 QImage（不缓存失败）
    QImage load(const QString& pclxPath);
}

#endif // PROJECTTHUMB_H
