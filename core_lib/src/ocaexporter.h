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

#ifndef OCAEXPORTER_H
#define OCAEXPORTER_H

#include <QString>
#include <functional>
#include "pencilerror.h"

class Object;

/** OCA（Open Animation Format）导出参数 */
struct OcaExportDesc
{
    QString outputDir;      ///< 导出根目录（内部创建 <工程名>.oca 文件夹）
    int startFrame = 1;
    int endFrame = -1;      ///< -1 = 自动取动画末帧
    QString cameraName;     ///< 空 = 第一个相机层
    int fps = 12;
};

/** 按 OCA 规范导出：JSON 清单 + 逐关键帧 PNG（内容裁剪 + 帧中心 position）。
 *  独立实现（仅参照 OCA 格式规范，不复用任何 GPLv3 参考插件代码）。 */
class OcaExporter
{
public:
    /** majorProgress(0..100)：帧级进度回调 */
    static Status run(const Object* obj,
                      const OcaExportDesc& desc,
                      std::function<void(int)> majorProgress = nullptr);
};

#endif // OCAEXPORTER_H
