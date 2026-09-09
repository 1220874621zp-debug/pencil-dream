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

#include "ocaexporter.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "bitmapimage.h"
#include "object.h"

namespace
{
// 文件名安全化：层名直接进文件名，剔掉非法字符
QString sanitizedFileName(const QString& name)
{
    QString out;
    out.reserve(name.size());
    for (const QChar& c : name)
    {
        if (c.isSpace() || QString::fromUtf8("\\/:*?\"<>|").contains(c))
        {
            out.append(QLatin1Char('_'));
        }
        else
        {
            out.append(c);
        }
    }
    return out.isEmpty() ? QStringLiteral("layer") : out;
}

// 关键帧块的曝光时长：显式块用 pos+len，开放末块撑到导出末帧
int keyDuration(const Layer* layer, KeyFrame* key, int exportEnd)
{
    const int end = layer->getBlockEnd(key);
    if (end < 0)
    {
        return qMax(1, exportEnd - key->pos() + 1); // 开放末块
    }
    return qMax(1, end - key->pos());
}

// 单个位图族图层 → OCA paintlayer JSON（含逐帧 PNG 落盘）
QJsonObject exportBitmapLayer(const LayerBitmap* layer,
                              const QString& frameDir,
                              int camW, int camH,
                              int startFrame, int endFrame,
                              int exportEnd,
                              const QString& projectKey,
                              int layerSeq,
                              int* savedCount)
{
    QJsonObject json;
    json.insert("name", layer->name());
    json.insert("type", QStringLiteral("paintlayer"));
    json.insert("fileType", QStringLiteral("png"));
    json.insert("blendingMode", QStringLiteral("normal"));
    json.insert("label", layer->colorIndex());
    json.insert("opacity", layer->opacity());
    json.insert("visible", layer->visible());
    json.insert("passThrough", false);
    json.insert("inheritAlpha", layer->clipMask());
    // 层级默认：画布中心 + 相机尺寸（OCA 语义：position 为图像中心点）
    json.insert("position", QJsonArray{ camW / 2.0, camH / 2.0 });
    json.insert("width", camW);
    json.insert("height", camH);
    json.insert("animated", layer->keyFrameCount() > 1);

    QJsonArray frames;
    const QString safeName = QStringLiteral("%1_%2").arg(sanitizedFileName(layer->name())).arg(layerSeq, 3, 10, QLatin1Char('0'));
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        if (key->pos() < startFrame || key->pos() > endFrame)
        {
            return;
        }
        BitmapImage* bmp = static_cast<BitmapImage*>(key); // image()/topLeft() 非 const
        QImage* img = bmp->image();
        if (img == nullptr || img->isNull())
        {
            return; // 从未绘制的空帧
        }

        const QString fileName = QStringLiteral("%1_%2.png")
            .arg(safeName)
            .arg(key->pos(), 5, 10, QLatin1Char('0'));
        const QString filePath = QDir(frameDir).filePath(fileName);
        if (!img->save(filePath, "PNG"))
        {
            return;
        }
        *savedCount += 1;

        // 帧级：内容裁剪尺寸 + 图像中心（pencil 世界原点在画布中心 → OCA 原点在左上）
        const QPoint topLeft = bmp->topLeft();
        QJsonObject frame;
        frame.insert("name", QStringLiteral("%1_%2").arg(layer->name()).arg(key->pos()));
        frame.insert("fileName", fileName);
        frame.insert("frameNumber", key->pos());
        frame.insert("duration", keyDuration(layer, key, exportEnd));
        frame.insert("opacity", 1.0);
        frame.insert("position", QJsonArray{ topLeft.x() + img->width() / 2.0 + camW / 2.0,
                                             topLeft.y() + img->height() / 2.0 + camH / 2.0 });
        frame.insert("width", img->width());
        frame.insert("height", img->height());
        frames.append(frame);
    });

    json.insert("frames", frames);
    Q_UNUSED(projectKey)
    return json;
}

// 组 → OCA grouplayer（childLayers 递归；pencil 组是扁平连续段，子层为位图族）
QJsonObject exportGroupLayer(const QList<LayerBitmap*>& members,
                             const QString& frameDir,
                             int camW, int camH,
                             int startFrame, int endFrame,
                             int exportEnd,
                             int* layerSeq,
                             int* savedCount)
{
    QJsonObject json;
    const int gid = members.first()->groupId();
    json.insert("name", QStringLiteral("group_%1").arg(gid));
    json.insert("type", QStringLiteral("grouplayer"));
    json.insert("blendingMode", QStringLiteral("normal"));
    json.insert("opacity", 1.0);
    json.insert("visible", true);
    json.insert("passThrough", false);
    json.insert("inheritAlpha", false);
    json.insert("animated", false);
    json.insert("position", QJsonArray{ camW / 2.0, camH / 2.0 });
    json.insert("width", camW);
    json.insert("height", camH);

    // 子层按视觉序（pencil 索引大 = 屏幕上方）自上而下排列，与 OCA 数组序一致
    QJsonArray children;
    for (int i = members.size() - 1; i >= 0; --i)
    {
        *layerSeq += 1;
        children.append(exportBitmapLayer(members.at(i), frameDir, camW, camH,
                                          startFrame, endFrame, exportEnd,
                                          QString(), *layerSeq, savedCount));
    }
    json.insert("childLayers", children);
    return json;
}
} // namespace

Status OcaExporter::run(const Object* obj,
                        const OcaExportDesc& desc,
                        std::function<void(int)> majorProgress)
{
    if (obj == nullptr || desc.outputDir.isEmpty())
    {
        return Status::FAIL;
    }
    if (majorProgress)
    {
        majorProgress(0);
    }

    // 导出相机：按名找，否则第一个相机层
    const LayerCamera* camera = nullptr;
    const auto cameras = obj->getLayersByType<LayerCamera>();
    for (const LayerCamera* cam : cameras)
    {
        if (desc.cameraName.isEmpty() || cam->name() == desc.cameraName)
        {
            camera = cam;
            break;
        }
    }
    const int camW = camera ? camera->getViewSize().width() : 1920;
    const int camH = camera ? camera->getViewSize().height() : 1080;

    // 末帧：未指定则取位图族层动画长度
    int endFrame = desc.endFrame;
    if (endFrame < 0)
    {
        endFrame = 1;
        for (int i = 0; i < obj->getLayerCount(); ++i)
        {
            Layer* layer = obj->getLayer(i);
            if (!layer->isBitmapKind())
            {
                continue;
            }
            layer->foreachKeyFrame([&](KeyFrame* key)
            {
                endFrame = qMax(endFrame, key->pos() + keyDuration(layer, key, key->pos() + 1) - 1);
            });
        }
    }
    const int startFrame = qMax(1, desc.startFrame);

    // 目录布局：<输出目录>/<工程名>.oca/<工程名>.oca(JSON) + 帧图同目录
    const QString projectName = [&obj]() {
        const QString base = QFileInfo(obj->filePath()).completeBaseName();
        return base.isEmpty() ? QStringLiteral("PencilDream") : sanitizedFileName(base);
    }();
    const QString ocaDir = QDir(desc.outputDir).filePath(projectName + QStringLiteral(".oca"));
    if (!QDir().mkpath(ocaDir))
    {
        return Status::FAIL;
    }

    // 图层树：pencil 索引 0 = 栈顶 → OCA layers[] 首位即最上层（Krita 同序）
    QJsonArray layersJson;
    int layerSeq = 0;
    int savedCount = 0;
    for (int i = 0; i < obj->getLayerCount(); ++i)
    {
        Layer* layer = obj->getLayer(i);
        if (layer->type() != Layer::BITMAP && layer->type() != Layer::COLORIZE)
        {
            continue; // 声音/相机层不进 OCA 图层树
        }
        auto* bmpLayer = static_cast<LayerBitmap*>(layer);
        if (bmpLayer->groupId() >= 0)
        {
            // 组：只在组块底端（栈序最小成员）处生成组节点，避免重复
            Layer* below = (i > 0) ? obj->getLayer(i - 1) : nullptr;
            const bool isGroupBottom = (below == nullptr || below->groupId() != bmpLayer->groupId());
            if (isGroupBottom)
            {
                QList<LayerBitmap*> members;
                for (int j = i; j < obj->getLayerCount()
                                 && obj->getLayer(j)->groupId() == bmpLayer->groupId(); ++j)
                {
                    members.append(static_cast<LayerBitmap*>(obj->getLayer(j)));
                }
                layersJson.append(exportGroupLayer(members, ocaDir, camW, camH,
                                                   startFrame, endFrame, endFrame,
                                                   &layerSeq, &savedCount));
            }
        }
        else
        {
            layerSeq += 1;
            layersJson.append(exportBitmapLayer(bmpLayer, ocaDir, camW, camH,
                                                startFrame, endFrame, endFrame,
                                                QString(), layerSeq, &savedCount));
        }
    }

    if (majorProgress)
    {
        majorProgress(70);
    }

    // 文档清单
    QJsonObject doc;
    doc.insert("name", projectName);
    doc.insert("frameRate", desc.fps);
    doc.insert("width", camW);
    doc.insert("height", camH);
    doc.insert("startTime", startFrame);
    doc.insert("endTime", endFrame);
    doc.insert("colorDepth", QStringLiteral("8ui"));
    doc.insert("backgroundColor", QJsonArray{ 1.0, 1.0, 1.0, 1.0 });
    doc.insert("originApp", QStringLiteral("Pencil Dream"));
    doc.insert("originAppVersion", QStringLiteral(APP_VERSION));
    doc.insert("layers", layersJson);

    QJsonDocument jsonDoc(doc);
    const QString manifestPath = QDir(ocaDir).filePath(projectName + QStringLiteral(".oca"));
    QFile manifest(manifestPath);
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return Status::FAIL;
    }
    manifest.write(jsonDoc.toJson(QJsonDocument::Indented));
    manifest.close();

    if (majorProgress)
    {
        majorProgress(100);
    }
    return Status::OK;
}
