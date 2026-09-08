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
#include "brushpresetstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QRegularExpression>
#include <QStandardPaths>

#include "brushengine.h"
#include "pencildef.h"

namespace
{

// 文件名里的非法字符替换成下划线（预设名仍保留原文）
QString sanitizedFileName(const QString& name)
{
    static const QRegularExpression illegalChars("[\\\\/:*?\"<>|]");
    QString safe = name;
    safe.replace(illegalChars, "_");
    return safe;
}

} // namespace

const QString BrushPresetStore::kFileExtension = ".pbp";

QString BrushPresetStore::userPresetDir()
{
    // Windows 上 QSettings(PENCIL2D, PENCIL2D) 走注册表，fileName() 返回的不是
    // 文件系统路径（QFileInfo 解析会得到无效目录），改用 AppDataLocation
    // （%APPDATA%/Pencil2D/Pencil2D，与单实例锁同根）
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/brushes";
}

void BrushPresetStore::load()
{
    mPresets.clear();

    // ---- 内置笔刷 ----
    struct BuiltinDef
    {
        const char* name;
        BrushSettings settings;
    };

    QList<BrushPreset> builtIns;
    auto addBuiltin = [&builtIns](const QString& name, const BrushSettings& settings)
    {
        BrushPreset preset;
        preset.name = name;
        preset.settings = settings;
        preset.builtIn = true;
        builtIns.append(preset);
    };

    {
        BrushSettings s; // 圆头笔：默认主力笔
        s.tipShape = BrushSettings::TipShape::Circle;
        s.diameter = 24.0;
        s.hardness = 0.9;
        s.opacity = 1.0;
        s.pressureSize = true;
        s.pressureOpacity = false;
        addBuiltin(QStringLiteral("圆头笔"), s);
    }
    {
        BrushSettings s; // 铅笔：细、带压感淡出
        s.tipShape = BrushSettings::TipShape::Circle;
        s.diameter = 6.0;
        s.hardness = 0.5;
        s.opacity = 0.85;
        s.pressureSize = true;
        s.sizeCurve = BrushCurve::fromString("0,0.08;1,1;");
        s.pressureOpacity = true;
        s.opacityCurve = BrushCurve::fromString("0,0.1;0.6,1;1,0.55;");
        addBuiltin(QStringLiteral("铅笔"), s);
    }
    {
        BrushSettings s; // 软笔：大而虚，适合铺色
        s.tipShape = BrushSettings::TipShape::Circle;
        s.diameter = 42.0;
        s.hardness = 0.06;
        s.opacity = 0.8;
        s.pressureSize = true;
        addBuiltin(QStringLiteral("软笔"), s);
    }
    {
        BrushSettings s; // 方头笔
        s.tipShape = BrushSettings::TipShape::Rectangle;
        s.diameter = 32.0;
        s.ratio = 1.0;
        s.hardness = 0.95;
        s.pressureSize = false;
        addBuiltin(QStringLiteral("方头笔"), s);
    }
    {
        BrushSettings s; // 马克笔：斜切扁笔尖
        s.tipShape = BrushSettings::TipShape::Rectangle;
        s.diameter = 40.0;
        s.ratio = 0.45;
        s.angle = 42.0;
        s.hardness = 0.85;
        s.opacity = 0.9;
        s.pressureSize = false;
        addBuiltin(QStringLiteral("马克笔"), s);
    }
    {
        BrushSettings s; // 细线：上色描边用
        s.tipShape = BrushSettings::TipShape::Circle;
        s.diameter = 3.0;
        s.hardness = 1.0;
        s.pressureSize = false;
        addBuiltin(QStringLiteral("细线"), s);
    }
    {
        BrushSettings s; // 粗涂：铺大色块
        s.tipShape = BrushSettings::TipShape::Circle;
        s.diameter = 90.0;
        s.hardness = 0.35;
        s.opacity = 0.6;
        s.pressureSize = false;
        addBuiltin(QStringLiteral("粗涂"), s);
    }
    {
        BrushSettings s; // 压感淡出：两头尖、中间实
        s.tipShape = BrushSettings::TipShape::Circle;
        s.diameter = 18.0;
        s.hardness = 0.8;
        s.pressureSize = true;
        s.sizeCurve = BrushCurve::fromString("0,0.1;0.5,1;1,0.1;");
        s.pressureOpacity = true;
        s.opacityCurve = BrushCurve::fromString("0,0;0.5,1;1,0;");
        addBuiltin(QStringLiteral("压感淡出"), s);
    }
    {
        BrushSettings s; // 软橡皮：dab alpha 经 DestinationOut 变成擦除量
        s.tipShape = BrushSettings::TipShape::Circle;
        s.diameter = 40.0;
        s.hardness = 0.5;
        s.opacity = 1.0;
        s.pressureSize = true;
        s.eraser = true;
        addBuiltin(QStringLiteral("软橡皮"), s);
    }

    // ---- 用户预设目录 ----
    QList<BrushPreset> userPresets;
    QDir dir(userPresetDir());
    if (dir.exists()) {
        const QFileInfoList entries = dir.entryInfoList({ "*" + kFileExtension },
                                                        QDir::Files, QDir::Name);
        for (const QFileInfo& entry : entries) {
            BrushSettings settings;
            QImage thumbnail;
            if (readPresetFile(entry.absoluteFilePath(), settings, thumbnail)) {
                BrushPreset preset;
                preset.name = settings.name.isEmpty() ? entry.completeBaseName() : settings.name;
                preset.settings = settings;
                preset.settings.name = preset.name;
                preset.thumbnail = thumbnail;
                preset.builtIn = false;
                preset.fileName = entry.absoluteFilePath();
                userPresets.append(preset);
            }
        }
    }

    mPresets = builtIns + userPresets;

    // 统一渲染缩略图（内置 + 读档时没带上缩略图的用户预设）
    for (BrushPreset& preset : mPresets) {
        if (preset.thumbnail.isNull()) {
            preset.thumbnail = BrushEngine::renderStrokePreview(preset.settings, QSize(128, 96));
        }
    }
}

int BrushPresetStore::indexOf(const QString& name) const
{
    for (int i = 0; i < mPresets.size(); ++i) {
        if (mPresets[i].name == name) {
            return i;
        }
    }
    return -1;
}

bool BrushPresetStore::saveUserPreset(const QString& name, const BrushSettings& settings)
{
    QDir dir(userPresetDir());
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    BrushSettings toSave = settings;
    toSave.name = name;

    const QString filePath = dir.filePath(sanitizedFileName(name) + kFileExtension);
    if (!writePresetFile(filePath, toSave)) {
        return false;
    }

    const int existing = indexOf(name);
    BrushPreset preset;
    preset.name = name;
    preset.settings = toSave;
    preset.builtIn = false;
    preset.fileName = filePath;
    preset.thumbnail = BrushEngine::renderStrokePreview(toSave, QSize(128, 96));
    if (existing >= 0 && !mPresets[existing].builtIn) {
        mPresets[existing] = preset;
    } else if (existing >= 0) {
        // 内置名被占用：不允许覆盖内置笔刷
        return false;
    } else {
        mPresets.append(preset);
    }
    return true;
}

bool BrushPresetStore::deleteUserPreset(const QString& name)
{
    const int index = indexOf(name);
    if (index < 0 || mPresets[index].builtIn) {
        return false;
    }
    QFile::remove(mPresets[index].fileName);
    mPresets.removeAt(index);
    return true;
}

bool BrushPresetStore::importPreset(const QString& filePath)
{
    BrushSettings settings;
    QImage thumbnail;
    if (!readPresetFile(filePath, settings, thumbnail)) {
        return false;
    }
    QString name = settings.name;
    if (name.isEmpty()) {
        name = QFileInfo(filePath).completeBaseName();
    }
    // 重名则加序号
    QString finalName = name;
    int counter = 2;
    while (indexOf(finalName) >= 0) {
        finalName = QString("%1 %2").arg(name).arg(counter++);
    }

    QDir dir(userPresetDir());
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }
    settings.name = finalName;
    const QString destPath = dir.filePath(sanitizedFileName(finalName) + kFileExtension);
    if (QFileInfo(filePath).absoluteFilePath() != QFileInfo(destPath).absoluteFilePath()) {
        if (!QFile::copy(filePath, destPath)) {
            return false;
        }
        // 复制过去后改写名字再存一遍，保证文件内名字一致
        writePresetFile(destPath, settings);
    } else {
        writePresetFile(destPath, settings);
    }

    BrushPreset preset;
    preset.name = finalName;
    preset.settings = settings;
    preset.builtIn = false;
    preset.fileName = destPath;
    if (thumbnail.isNull()) {
        thumbnail = BrushEngine::renderStrokePreview(settings, QSize(128, 96));
    }
    preset.thumbnail = thumbnail;
    mPresets.append(preset);
    return true;
}

bool BrushPresetStore::exportPreset(const QString& name, const QString& filePath)
{
    const int index = indexOf(name);
    if (index < 0) {
        return false;
    }
    BrushSettings settings = mPresets[index].settings;
    settings.name = name;
    return writePresetFile(filePath, settings);
}

bool BrushPresetStore::writePresetFile(const QString& filePath, const BrushSettings& settings)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QImage thumbnail = BrushEngine::renderStrokePreview(settings, QSize(128, 96));

    QImageWriter writer(&file, QByteArray("png"));
    writer.setText("preset", settings.toXMLString());
    writer.setText("version", "1");
    return writer.write(thumbnail);
}

bool BrushPresetStore::readPresetFile(const QString& filePath, BrushSettings& outSettings, QImage& outThumbnail)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    // 注意：Qt6 的 PNG handler 把文本块放进 QImage 的元数据，
    // QImageReader::text() 拿不到，必须从读出的图像上取
    QImageReader reader(&file, QByteArray("png"));
    QImage thumbnail = reader.read();
    if (thumbnail.isNull()) {
        return false;
    }
    const QString xml = thumbnail.text("preset");
    if (xml.isEmpty()) {
        return false;
    }
    if (!BrushSettings::fromXMLString(xml, outSettings)) {
        return false;
    }
    outThumbnail = thumbnail;
    return true;
}
