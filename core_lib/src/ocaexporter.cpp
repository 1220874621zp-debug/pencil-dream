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
#include <QPainter>

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

// 整画布渲染单层帧：透明画布 + 世界原点平移（结构清晰的 OCA 同构：帧图一律相机尺寸）
QImage renderLayerFrameFullCanvas(BitmapImage* bmp, int camW, int camH)
{
    QImage canvas(camW, camH, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.translate(camW / 2, camH / 2); // pencil 世界原点在画布中心
    painter.drawImage(bmp->topLeft(), *bmp->image());
    painter.end();
    return canvas;
}

struct OcaWriteContext
{
    QString ocaRootDir;    ///< <输出>/<工程名>.oca/
    QString frameSubDir;   ///< 帧图子目录名（<工程名>_oca）
    int camW = 1920;
    int camH = 1080;
    int startFrame = 1;
    int endFrame = 1;
    int exportEnd = 1;
};

// 单个位图族图层 → OCA paintlayer JSON（整画布帧图 <子目录>/<层名>.<帧号4位>.png）
QJsonObject exportBitmapLayer(LayerBitmap* layer, const OcaWriteContext& ctx)
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
    json.insert("position", QJsonArray{ ctx.camW / 2.0, ctx.camH / 2.0 });
    json.insert("width", ctx.camW);
    json.insert("height", ctx.camH);
    json.insert("animated", layer->keyFrameCount() > 1);

    const QString safeName = sanitizedFileName(layer->name());
    QJsonArray frames;
    layer->foreachKeyFrame([&](KeyFrame* key)
    {
        if (key->pos() < ctx.startFrame || key->pos() > ctx.endFrame)
        {
            return;
        }
        BitmapImage* bmp = static_cast<BitmapImage*>(key); // image()/topLeft() 非 const
        if (bmp->image() == nullptr || bmp->image()->isNull())
        {
            return; // 从未绘制的空帧
        }

        const QString fileName = QStringLiteral("%1/%2.%3.png")
            .arg(ctx.frameSubDir, safeName)
            .arg(key->pos(), 4, 10, QLatin1Char('0'));
        const QImage frameImg = renderLayerFrameFullCanvas(bmp, ctx.camW, ctx.camH);
        if (!frameImg.save(QDir(ctx.ocaRootDir).filePath(fileName), "PNG"))
        {
            return;
        }

        // 整幅帧：位置=画布中心，尺寸=相机尺寸（与参考实现同构，导入端无需逐帧对位）
        QJsonObject frame;
        frame.insert("name", QStringLiteral("%1.%2")
            .arg(layer->name()).arg(key->pos(), 4, 10, QLatin1Char('0')));
        frame.insert("fileName", fileName);
        frame.insert("frameNumber", key->pos());
        frame.insert("duration", keyDuration(layer, key, ctx.exportEnd));
        frame.insert("opacity", 1.0);
        frame.insert("position", QJsonArray{ ctx.camW / 2.0, ctx.camH / 2.0 });
        frame.insert("width", ctx.camW);
        frame.insert("height", ctx.camH);
        frames.append(frame);
    });

    json.insert("frames", frames);
    return json;
}

// 组 → OCA grouplayer（子层递归；组名取组表真实名称）
QJsonObject exportGroupLayer(const Object* obj,
                             const QList<LayerBitmap*>& members,
                             const OcaWriteContext& ctx)
{
    QJsonObject json;
    const int gid = members.first()->groupId();
    const LayerGroupInfo* info = obj->layerGroupInfo(gid);
    json.insert("name", info != nullptr ? info->name : QStringLiteral("组"));
    json.insert("type", QStringLiteral("grouplayer"));
    json.insert("blendingMode", QStringLiteral("normal"));
    json.insert("opacity", 1.0);
    json.insert("visible", info != nullptr ? info->visible : true);
    json.insert("passThrough", false);
    json.insert("inheritAlpha", false);
    json.insert("animated", false);
    json.insert("position", QJsonArray{ ctx.camW / 2.0, ctx.camH / 2.0 });
    json.insert("width", ctx.camW);
    json.insert("height", ctx.camH);

    // 子层按视觉序（pencil 索引大 = 屏幕上方）自上而下，与 OCA 数组序一致
    QJsonArray children;
    for (int i = members.size() - 1; i >= 0; --i)
    {
        children.append(exportBitmapLayer(members.at(i), ctx));
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

    // 目录布局（与参考实现同构）：
    //   <输出>/<工程名>.oca/             ← 根目录
    //   <输出>/<工程名>.oca/<工程名>.oca  ← JSON 清单
    //   <输出>/<工程名>.oca/<工程名>_oca/<层名>.<帧号4位>.png
    const QString projectName = [&obj]()
    {
        const QString base = QFileInfo(obj->filePath()).completeBaseName();
        return base.isEmpty() ? QStringLiteral("PencilDream") : sanitizedFileName(base);
    }();
    const QString ocaDir = QDir(desc.outputDir).filePath(projectName + QStringLiteral(".oca"));
    const QString frameSubDir = projectName + QStringLiteral("_oca");
    if (!QDir().mkpath(QDir(ocaDir).filePath(frameSubDir)))
    {
        return Status::FAIL;
    }

    OcaWriteContext ctx;
    ctx.ocaRootDir = ocaDir;
    ctx.frameSubDir = frameSubDir;
    ctx.camW = camera ? camera->getViewSize().width() : 1920;
    ctx.camH = camera ? camera->getViewSize().height() : 1080;
    ctx.startFrame = startFrame;
    ctx.endFrame = endFrame;
    ctx.exportEnd = endFrame;

    // 图层树：pencil 索引 0 = 栈顶 → OCA layers[] 首位即最上层
    QJsonArray layersJson;
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
                layersJson.append(exportGroupLayer(obj, members, ctx));
            }
        }
        else
        {
            layersJson.append(exportBitmapLayer(bmpLayer, ctx));
        }
    }

    if (majorProgress)
    {
        majorProgress(70);
    }

    // 文档清单
    QJsonObject doc;
    doc.insert("name", projectName);
    doc.insert("ocaVersion", QStringLiteral("1.1.0"));
    doc.insert("frameRate", desc.fps);
    doc.insert("width", ctx.camW);
    doc.insert("height", ctx.camH);
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
