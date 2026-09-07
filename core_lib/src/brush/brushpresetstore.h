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
#ifndef BRUSHPRESETSTORE_H
#define BRUSHPRESETSTORE_H

#include <QImage>
#include <QList>
#include <QString>

#include "brushsettings.h"

/**
 * 笔刷预设存储（参考 Krita .kpp 思路）：
 *   预设文件 = 一张 PNG 缩略图 + 名为 "preset" 的 PNG 文本块（完整参数 XML）
 * 内置笔刷由代码生成；用户笔刷存放在设置目录旁的 brushes/ 文件夹。
 */
struct BrushPreset
{
    QString name;
    BrushSettings settings;
    QImage thumbnail;   // 已按 2x 渲染，使用时记得设置 devicePixelRatio
    bool builtIn = false;
    QString fileName;   // 用户预设的绝对路径
};

class BrushPresetStore
{
public:
    BrushPresetStore() = default;

    /** 载入内置笔刷 + 扫描用户预设目录 */
    void load();

    const QList<BrushPreset>& presets() const { return mPresets; }
    int indexOf(const QString& name) const;

    /** 保存/覆盖用户预设（渲染缩略图并写 .pbp 文件），成功返回 true */
    bool saveUserPreset(const QString& name, const BrushSettings& settings);
    bool deleteUserPreset(const QString& name);

    bool importPreset(const QString& filePath);
    bool exportPreset(const QString& name, const QString& filePath);

    static QString userPresetDir();

    /** 预设文件后缀（本质是带文本块的 PNG） */
    static const QString kFileExtension;

    /** 写一个 .pbp 预设文件（PNG 缩略图 + 参数 XML 文本块） */
    static bool writePresetFile(const QString& filePath, const BrushSettings& settings);
    /** 读一个 .pbp 预设文件，失败返回 false */
    static bool readPresetFile(const QString& filePath, BrushSettings& outSettings, QImage& outThumbnail);

private:
    QList<BrushPreset> mPresets;
};

#endif // BRUSHPRESETSTORE_H
