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

#include "mcpdispatcher.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPainter>

#include "bitmapimage.h"
#include "editor.h"
#include "filemanager.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "preferencemanager.h"
#include "structure/object.h"
#include "undoredomanager.h"
#include "util/pencildef.h"

namespace
{

QString layerTypeName(Layer* layer)
{
    switch (layer->type())
    {
    case Layer::BITMAP:   return QStringLiteral("bitmap");
    case Layer::COLORIZE: return QStringLiteral("colorize");
    case Layer::VECTOR:   return QStringLiteral("vector");
    case Layer::SOUND:    return QStringLiteral("sound");
    case Layer::CAMERA:   return QStringLiteral("camera");
    default:              return QStringLiteral("undefined");
    }
}

Layer::LAYER_TYPE layerTypeFromName(const QString& name)
{
    if (name == "bitmap") return Layer::BITMAP;
    if (name == "colorize") return Layer::COLORIZE;
    if (name == "camera") return Layer::CAMERA;
    if (name == "sound") return Layer::SOUND;
    if (name == "vector") return Layer::VECTOR;
    return Layer::UNDEFINED;
}

// 构建 {name, description, inputSchema:{...}} 工具声明
QJsonObject makeTool(const QString& name, const QString& desc, const QJsonObject& props, const QStringList& required)
{
    QJsonObject schema;
    schema.insert("type", "object");
    schema.insert("properties", props);

    QJsonArray req;
    for (const QString& r : required)
        req.append(r);
    if (!req.isEmpty())
        schema.insert("required", req);

    QJsonObject tool;
    tool.insert("name", name);
    tool.insert("description", desc);
    tool.insert("inputSchema", schema);
    return tool;
}

QJsonObject prop(const QString& type, const QString& desc)
{
    QJsonObject p;
    p.insert("type", type);
    p.insert("description", desc);
    return p;
}

} // namespace

McpDispatcher::McpDispatcher(Editor* editor, QObject* parent) :
    QObject(parent),
    mEditor(editor)
{
}

QJsonArray McpDispatcher::toolsSchema() const
{
    QJsonArray tools;

    // —— 感知 ——
    {
        QJsonObject props;
        tools.append(makeTool("get_scene_status",
            QStringLiteral("获取工程总览：全部图层（行号/id/名称/类型/可见性/关键帧分布）、当前帧、帧率、动画长度、画布尺寸。先调用它了解工程结构。"),
            props, {}));
    }
    {
        QJsonObject props;
        props.insert("frame", prop("number", QStringLiteral("帧号（默认当前帧）")));
        props.insert("layer", prop("any", QStringLiteral("可选：只渲染单层。图层引用=数字(优先id，其次1-based行号)或名称字符串；缺省=全部图层合成")));
        props.insert("background", prop("boolean", QStringLiteral("是否铺白底（默认true；false=透明底）")));
        props.insert("max_size", prop("number", QStringLiteral("返回图最长边像素数，默认1024；传0=原始尺寸")));
        tools.append(makeTool("get_frame_image",
            QStringLiteral("渲染指定帧并返回PNG图像（用于查看画面做决策）。坐标约定：图像左上角为(0,0)，x向右y向下，原图即画布坐标，与draw_stroke等绘制工具一致。不含相机变换。"),
            props, {}));
    }
    {
        QJsonObject props;
        tools.append(makeTool("get_palette",
            QStringLiteral("读取当前色卡（索引/名称/十六进制颜色）。填色或画笔画时优先使用色卡已有颜色。"),
            props, {}));
    }

    // —— 图层 ——
    {
        QJsonObject props;
        props.insert("type", prop("string", QStringLiteral("图层类型：bitmap（位图/默认）、colorize（智能填色层）、camera、sound")));
        props.insert("name", prop("string", QStringLiteral("图层名称（可选）")));
        tools.append(makeTool("create_layer", QStringLiteral("新建图层，返回新图层信息。"), props, {}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("图层引用：数字(优先id，其次1-based行号)或名称字符串")));
        tools.append(makeTool("delete_layer", QStringLiteral("删除图层（可撤销）。"), props, {"layer"}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("图层引用")));
        props.insert("visible", prop("boolean", QStringLiteral("true=显示 false=隐藏")));
        tools.append(makeTool("set_layer_visibility", QStringLiteral("设置图层可见性。"), props, {"layer", "visible"}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("图层引用")));
        tools.append(makeTool("select_layer", QStringLiteral("切换当前编辑图层。"), props, {"layer"}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("图层引用")));
        props.insert("name", prop("string", QStringLiteral("新名称")));
        tools.append(makeTool("rename_layer", QStringLiteral("重命名图层。"), props, {"layer", "name"}));
    }

    // —— 帧 ——
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("目标图层（默认当前图层）")));
        props.insert("frame", prop("number", QStringLiteral("帧号（默认当前帧；该位置已有关键帧时报错）")));
        tools.append(makeTool("add_key_frame", QStringLiteral("在指定位图族图层上新建空白关键帧（可撤销）。"), props, {}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("源图层")));
        props.insert("frame", prop("number", QStringLiteral("源帧号（取该帧显示的关键帧内容）")));
        props.insert("to_frame", prop("number", QStringLiteral("目标帧号（不能与已有关键帧重叠）")));
        tools.append(makeTool("duplicate_frame", QStringLiteral("把某帧内容复制为新关键帧（可撤销）。"), props, {"layer", "frame", "to_frame"}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("目标图层")));
        props.insert("frame", prop("number", QStringLiteral("要删除的关键帧位置（须精确命中已有键）")));
        tools.append(makeTool("delete_frame", QStringLiteral("删除指定位置的关键帧（可撤销）。"), props, {"layer", "frame"}));
    }
    {
        QJsonObject props;
        props.insert("frame", prop("number", QStringLiteral("目标帧号（从1开始）")));
        tools.append(makeTool("scrub_to", QStringLiteral("把播放头跳到指定帧。"), props, {"frame"}));
    }

    // —— 项目 / 播放 / 撤销 ——
    {
        QJsonObject props;
        props.insert("path", prop("string", QStringLiteral("工程文件(.pclx)绝对路径")));
        tools.append(makeTool("open_project", QStringLiteral("打开工程文件（当前工程未保存的修改会丢失，请先确认）。"), props, {"path"}));
    }
    {
        QJsonObject props;
        props.insert("path", prop("string", QStringLiteral("保存路径（缺省=原路径；新建工程从未保存过则必填）")));
        tools.append(makeTool("save_project", QStringLiteral("保存工程（.pclx）。"), props, {}));
    }
    {
        QJsonObject props;
        props.insert("frame", prop("number", QStringLiteral("帧号（默认当前帧）")));
        props.insert("path", prop("string", QStringLiteral("输出图片绝对路径，扩展名决定格式（默认.png）")));
        props.insert("transparent", prop("boolean", QStringLiteral("透明底（默认false）")));
        tools.append(makeTool("export_frame", QStringLiteral("导出指定帧为图片文件。"), props, {"path"}));
    }
    tools.append(makeTool("play", QStringLiteral("开始播放动画（让用户观看效果）。"), QJsonObject(), {}));
    tools.append(makeTool("stop", QStringLiteral("停止播放。"), QJsonObject(), {}));
    tools.append(makeTool("undo", QStringLiteral("撤销上一步操作（等效Ctrl+Z）。"), QJsonObject(), {}));
    tools.append(makeTool("redo", QStringLiteral("重做。"), QJsonObject(), {}));

    return tools;
}

McpDispatcher::ToolResult McpDispatcher::dispatch(const QString& tool, const QJsonObject& args)
{
    if (mEditor.isNull() || mEditor->object() == nullptr)
        return fail(tr("编辑器尚未就绪"));

    if (tool == "get_scene_status")        return toolGetSceneStatus(args);
    if (tool == "get_frame_image")         return toolGetFrameImage(args);
    if (tool == "get_palette")             return toolGetPalette(args);
    if (tool == "create_layer")            return toolCreateLayer(args);
    if (tool == "delete_layer")            return toolDeleteLayer(args);
    if (tool == "set_layer_visibility")    return toolSetLayerVisibility(args);
    if (tool == "select_layer")            return toolSelectLayer(args);
    if (tool == "rename_layer")            return toolRenameLayer(args);
    if (tool == "add_key_frame")           return toolAddKeyFrame(args);
    if (tool == "duplicate_frame")         return toolDuplicateFrame(args);
    if (tool == "delete_frame")            return toolDeleteFrame(args);
    if (tool == "scrub_to")                return toolScrubTo(args);
    if (tool == "open_project")            return toolOpenProject(args);
    if (tool == "save_project")            return toolSaveProject(args);
    if (tool == "export_frame")            return toolExportFrame(args);
    if (tool == "play")                    return toolPlay(args);
    if (tool == "stop")                    return toolStop(args);
    if (tool == "undo")                    return toolUndo(args);
    if (tool == "redo")                    return toolRedo(args);

    return fail(tr("未知工具: %1").arg(tool));
}

// ---------------------------------------------------------------- 感知 ---

McpDispatcher::ToolResult McpDispatcher::toolGetSceneStatus(const QJsonObject&)
{
    Object* object = mEditor->object();

    QJsonArray layers;
    const int count = object->getLayerCount();
    for (int i = 0; i < count; ++i)
        layers.append(layerSummary(object->getLayer(i), i + 1));

    QJsonObject data;
    data.insert("currentFrame", mEditor->currentFrame());
    data.insert("fps", mEditor->fps());
    data.insert("animationLength", mEditor->layers()->animationLength());

    QJsonObject canvas;
    canvas.insert("width", canvasSize().width());
    canvas.insert("height", canvasSize().height());
    data.insert("canvas", canvas);
    data.insert("layers", layers);
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolGetFrameImage(const QJsonObject& args)
{
    const int frame = args.contains("frame") ? qMax(1, args.value("frame").toInt()) : mEditor->currentFrame();
    const bool background = !args.contains("background") || args.value("background").toBool();
    const int maxSize = args.contains("max_size") ? args.value("max_size").toInt() : 1024;

    Layer* singleLayer = nullptr;
    if (args.contains("layer"))
    {
        QString err;
        singleLayer = resolveLayer(args, &err);
        if (singleLayer == nullptr)
            return fail(err);
    }

    QImage image = renderFrame(frame, singleLayer, background);
    if (maxSize > 0 && (image.width() > maxSize || image.height() > maxSize))
        image = image.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QJsonObject data;
    data.insert("frame", frame);
    data.insert("width", image.width());
    data.insert("height", image.height());
    data.insert("format", "png");
    if (singleLayer != nullptr)
    {
        data.insert("layer", QJsonObject{{"row", mEditor->object()->getIndex(singleLayer) + 1},
                                         {"id", singleLayer->id()},
                                         {"name", singleLayer->name()}});
    }
    return okWithImage(data, image);
}

McpDispatcher::ToolResult McpDispatcher::toolGetPalette(const QJsonObject&)
{
    Object* object = mEditor->object();

    QJsonArray colors;
    const int count = object->getColorCount();
    for (int i = 0; i < count; ++i)
    {
        const ColorRef ref = object->getColor(i);
        QJsonObject c;
        c.insert("index", i);
        c.insert("name", ref.name);
        c.insert("hex", ref.color.name());
        colors.append(c);
    }

    QJsonObject data;
    data.insert("colors", colors);
    return ok(data);
}

// ---------------------------------------------------------------- 图层 ---

McpDispatcher::ToolResult McpDispatcher::toolCreateLayer(const QJsonObject& args)
{
    const QString typeName = args.value("type").toString("bitmap");
    const Layer::LAYER_TYPE type = layerTypeFromName(typeName);
    if (type == Layer::UNDEFINED)
        return fail(tr("未知图层类型: %1（支持 bitmap / colorize / camera / sound）").arg(typeName));

    QString name = args.value("name").toString();
    if (name.isEmpty())
    {
        switch (type)
        {
        case Layer::COLORIZE: name = tr("填色层"); break;
        case Layer::CAMERA:   name = tr("相机层"); break;
        case Layer::SOUND:    name = tr("声音层"); break;
        default:              name = tr("位图层"); break;
        }
        name = mEditor->layers()->nameSuggestLayer(name);
    }

    Layer* layer = mEditor->layers()->createLayer(type, name);
    if (layer == nullptr)
        return fail(tr("创建图层失败"));

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolDeleteLayer(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    const int index = mEditor->object()->getIndex(layer);
    if (!mEditor->layers()->canDeleteLayer(index))
        return fail(tr("该图层不能删除（工程至少要保留一个图层）"));

    const Status st = mEditor->layers()->deleteLayer(index);
    if (!st.ok())
        return fail(tr("删除图层失败: %1").arg(st.msg()));

    QJsonObject data;
    data.insert("deleted", true);
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolSetLayerVisibility(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    layer->setVisible(args.value("visible").toBool());
    mEditor->layers()->notifyLayerChanged(layer);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolSelectLayer(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    mEditor->layers()->setCurrentLayer(layer);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolRenameLayer(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    const QString name = args.value("name").toString().trimmed();
    if (name.isEmpty())
        return fail(tr("图层名称不能为空"));

    mEditor->layers()->renameLayer(layer, name);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    return ok(data);
}

// ---------------------------------------------------------------- 帧 ---

McpDispatcher::ToolResult McpDispatcher::toolAddKeyFrame(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    if (!layer->isBitmapKind())
        return fail(tr("只能在位图/填色图层上创建关键帧（当前类型: %1）").arg(layerTypeName(layer)));
    if (!layer->visible())
        return fail(tr("图层 %1 当前处于隐藏状态，请先 set_layer_visibility 显示它").arg(layer->name()));

    const int frame = args.contains("frame") ? qMax(1, args.value("frame").toInt()) : mEditor->currentFrame();
    if (layer->keyExists(frame))
        return fail(tr("帧 %1 上已有关键帧").arg(frame));

    mEditor->beginLayerLayoutEdit(layer);
    const bool added = layer->addNewKeyFrameAt(frame);
    mEditor->endLayerLayoutEdit(tr("MCP：新增关键帧"));

    if (!added)
        return fail(tr("创建关键帧失败"));

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("frame", frame);
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolDuplicateFrame(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    LayerBitmap* bitmapLayer = dynamic_cast<LayerBitmap*>(layer);
    if (bitmapLayer == nullptr)
        return fail(tr("只能在位图族图层上复制关键帧（当前类型: %1）").arg(layerTypeName(layer)));

    const int fromFrame = qMax(1, args.value("frame").toInt());
    const int toFrame = qMax(1, args.value("to_frame").toInt());

    BitmapImage* source = bitmapLayer->getLastBitmapImageAtFrame(fromFrame);
    if (source == nullptr)
        return fail(tr("帧 %1 上没有可复制的内容").arg(fromFrame));
    if (layer->keyExists(toFrame))
        return fail(tr("帧 %1 上已有关键帧，无法放置副本").arg(toFrame));

    BitmapImage* copy = source->clone();
    copy->setPos(toFrame);

    mEditor->beginLayerLayoutEdit(layer);
    layer->addKeyFrame(toFrame, copy);
    mEditor->endLayerLayoutEdit(tr("MCP：复制关键帧"));

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("frame", toFrame);
    data.insert("copied_from", source->pos());
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolDeleteFrame(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    const int frame = qMax(1, args.value("frame").toInt());
    if (!layer->keyExists(frame))
        return fail(tr("帧 %1 上没有关键帧（只能删除精确命中的关键帧）").arg(frame));

    mEditor->beginLayerLayoutEdit(layer);
    mEditor->takeLayerKeyFrame(layer, frame);
    mEditor->endLayerLayoutEdit(tr("MCP：删除关键帧"));

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("deleted_frame", frame);
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolScrubTo(const QJsonObject& args)
{
    const int frame = qMax(1, args.value("frame").toInt());
    mEditor->scrubTo(frame);

    QJsonObject data;
    data.insert("currentFrame", mEditor->currentFrame());
    return ok(data);
}

// ------------------------------------------------------ 项目 / 播放 / 撤销 ---

McpDispatcher::ToolResult McpDispatcher::toolOpenProject(const QJsonObject& args)
{
    const QString path = args.value("path").toString();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return fail(tr("工程文件不存在: %1").arg(path));

    mEditor->playback()->stop();
    const Status st = mEditor->openObject(path, {}, {});
    if (!st.ok())
        return fail(tr("打开工程失败: %1").arg(st.msg()));

    QJsonObject data;
    data.insert("opened", path);
    data.insert("layers", mEditor->object()->getLayerCount());
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolSaveProject(const QJsonObject& args)
{
    Object* object = mEditor->object();

    QString path = args.value("path").toString();
    if (path.isEmpty())
        path = object->filePath();
    if (path.isEmpty())
        return fail(tr("工程尚未保存过，请提供保存路径"));

    QDir().mkpath(QFileInfo(path).absolutePath());
    FileManager fm;
    const Status st = fm.save(object, path);
    if (!st.ok())
        return fail(tr("保存工程失败: %1").arg(st.msg()));

    object->setModified(false);

    QJsonObject data;
    data.insert("saved", path);
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolExportFrame(const QJsonObject& args)
{
    const QString path = args.value("path").toString();
    if (path.isEmpty())
        return fail(tr("必须提供输出图片路径"));

    const int frame = args.contains("frame") ? qMax(1, args.value("frame").toInt()) : mEditor->currentFrame();
    const bool transparent = args.value("transparent").toBool(false);

    QString format = QFileInfo(path).suffix().toUpper();
    if (format.isEmpty())
        format = "PNG";

    QDir().mkpath(QFileInfo(path).absolutePath());
    const Status st = mEditor->object()->exportIm(frame, QTransform(), canvasSize(), canvasSize(),
                                                  path, format, true, transparent);
    if (!st.ok())
        return fail(tr("导出失败: %1").arg(st.msg()));

    QJsonObject data;
    data.insert("exported", path);
    data.insert("frame", frame);
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolPlay(const QJsonObject&)
{
    mEditor->playback()->play();
    return ok(QJsonObject{{"playing", true}});
}

McpDispatcher::ToolResult McpDispatcher::toolStop(const QJsonObject&)
{
    mEditor->playback()->stop();
    return ok(QJsonObject{{"playing", false}});
}

McpDispatcher::ToolResult McpDispatcher::toolUndo(const QJsonObject&)
{
    mEditor->undoRedo()->undo();
    return ok(QJsonObject{{"undone", true}});
}

McpDispatcher::ToolResult McpDispatcher::toolRedo(const QJsonObject&)
{
    mEditor->undoRedo()->redo();
    return ok(QJsonObject{{"redone", true}});
}

// ---------------------------------------------------------------- 帮助 ---

Layer* McpDispatcher::resolveLayer(const QJsonObject& args, QString* err) const
{
    const QJsonValue ref = args.contains("layer") ? args.value("layer") : args.value("layer_id");
    Object* object = mEditor->object();

    if (ref.isDouble())
    {
        const int number = static_cast<int>(ref.toDouble());
        Layer* byId = object->findLayerById(number);
        if (byId != nullptr)
            return byId;

        if (number >= 1 && number <= object->getLayerCount())
            return object->getLayer(number - 1);

        *err = tr("找不到图层（按 id=%1 或行号=%1 均未命中）。当前图层：%2")
                   .arg(number)
                   .arg(layerListHint());
        return nullptr;
    }

    if (ref.isString())
    {
        Layer* byName = object->findLayerByName(ref.toString());
        if (byName != nullptr)
            return byName;

        bool converted = false;
        const int number = ref.toString().toInt(&converted);
        if (converted)
        {
            Layer* layer = resolveLayer(QJsonObject{{"layer", number}}, err);
            if (layer != nullptr)
                return layer;
        }

        *err = tr("找不到名为「%1」的图层。当前图层：%2").arg(ref.toString()).arg(layerListHint());
        return nullptr;
    }

    *err = tr("缺少图层引用参数 layer（数字=id或1-based行号，字符串=名称）。当前图层：%1").arg(layerListHint());
    return nullptr;
}

QJsonObject McpDispatcher::layerSummary(Layer* layer, int rowIndex) const
{
    QJsonArray keys;
    int pos = layer->firstKeyFramePosition();
    while (pos > 0)
    {
        keys.append(pos);
        const int next = layer->getNextKeyFramePosition(pos);
        if (next <= pos)
            break;
        pos = next;
    }

    QJsonObject o;
    o.insert("row", rowIndex);
    o.insert("id", layer->id());
    o.insert("name", layer->name());
    o.insert("type", layerTypeName(layer));
    o.insert("visible", layer->visible());
    o.insert("locked", layer->locked());
    o.insert("opacity", layer->opacity());
    o.insert("groupId", layer->groupId());
    o.insert("keyFrames", keys);
    return o;
}

QString McpDispatcher::layerListHint() const
{
    Object* object = mEditor->object();
    QStringList hints;
    const int count = object->getLayerCount();
    for (int i = 0; i < count; ++i)
    {
        Layer* layer = object->getLayer(i);
        hints << QStringLiteral("行%1(id=%2,%3)「%4」")
                     .arg(i + 1)
                     .arg(layer->id())
                     .arg(layerTypeName(layer))
                     .arg(layer->name());
    }
    return hints.join(QStringLiteral("、"));
}

QImage McpDispatcher::renderFrame(int frame, Layer* singleLayer, bool background) const
{
    const QSize size = canvasSize();
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(background ? Qt::white : Qt::transparent);

    QPainter painter(&image);
    // 世界坐标原点在画布中心：平移到图像中心后各层内容落在画布内
    painter.translate(size.width() / 2.0, size.height() / 2.0);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (singleLayer != nullptr)
    {
        LayerBitmap* bitmapLayer = dynamic_cast<LayerBitmap*>(singleLayer);
        if (bitmapLayer != nullptr)
        {
            BitmapImage* bitmap = bitmapLayer->getLastBitmapImageAtFrame(frame);
            if (bitmap != nullptr)
                bitmap->paintImage(painter);
        }
    }
    else
    {
        mEditor->object()->paintImage(painter, frame, false, true);
    }

    painter.end();
    return image;
}

QSize McpDispatcher::canvasSize() const
{
    const int w = mEditor->preference()->getInt(SETTING::FIELD_W);
    const int h = mEditor->preference()->getInt(SETTING::FIELD_H);
    return QSize(w > 0 ? w : 1920, h > 0 ? h : 1080);
}

McpDispatcher::ToolResult McpDispatcher::ok(QJsonObject data)
{
    ToolResult r;
    r.data = data;
    return r;
}

McpDispatcher::ToolResult McpDispatcher::okWithImage(QJsonObject data, const QImage& image)
{
    ToolResult r;
    r.data = data;
    r.image = image;
    return r;
}

McpDispatcher::ToolResult McpDispatcher::fail(const QString& message)
{
    ToolResult r;
    r.isError = true;
    r.data = QJsonObject{{"error", message}};
    return r;
}
