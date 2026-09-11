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
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPainter>
#include <QTimer>

#include "bitmapimage.h"
#include "colorizeimage.h"
#include "colorizeupdatemanager.h"
#include "editor.h"
#include "filemanager.h"
#include "inbetween.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercolorize.h"
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

    // —— 绘制原语 ——
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("目标图层（位图或填色层；默认当前图层）")));
        props.insert("frame", prop("number", QStringLiteral("帧号（默认当前帧；无关键帧时自动创建）")));
        props.insert("points", prop("array", QStringLiteral("画布坐标点列表 [[x,y],...]；左上角为(0,0)，x向右y向下")));
        props.insert("color", prop("string", QStringLiteral("颜色，如 \"#336699\"；支持 #AARRGGBB 带透明")));
        props.insert("size", prop("number", QStringLiteral("笔宽像素（默认4）")));
        props.insert("opacity", prop("number", QStringLiteral("不透明度0-1（默认1）")));
        tools.append(makeTool("draw_stroke",
            QStringLiteral("在指定位图/填色图层指定帧上画笔画（圆头折线；单点=圆点）。可撤销。智能填色时用它点彩色种子。"),
            props, {"points", "color"}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("目标图层（默认当前图层）")));
        props.insert("frame", prop("number", QStringLiteral("帧号（默认当前帧）")));
        props.insert("polygon", prop("array", QStringLiteral("多边形顶点画布坐标 [[x,y],...]（至少3个）")));
        props.insert("color", prop("string", QStringLiteral("填充颜色，如 \"#ffcc00\"")));
        props.insert("opacity", prop("number", QStringLiteral("不透明度0-1（默认1）")));
        tools.append(makeTool("fill_region",
            QStringLiteral("用纯色填充多边形区域（可撤销）。适合大面积上色；细节填色优先用智能填色流程。"),
            props, {"polygon", "color"}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("目标图层")));
        props.insert("frame", prop("number", QStringLiteral("帧号（默认当前帧）")));
        tools.append(makeTool("clear_frame",
            QStringLiteral("清空指定帧的图像内容（关键帧保留为空白，可撤销）。"),
            props, {}));
    }

    // —— 智能填色 ——
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("填色图层（colorize 类型）")));
        props.insert("use_edge_detection", prop("boolean", QStringLiteral("LoG边缘检测加固线稿（默认false）")));
        props.insert("edge_detection_size", prop("number", QStringLiteral("边缘检测核尺寸1-20（默认4）")));
        props.insert("fuzzy_radius", prop("number", QStringLiteral("高斯闭缝半径0-10（默认0）")));
        props.insert("clean_up_amount", prop("number", QStringLiteral("清理强度0-1（默认0.7）")));
        props.insert("transparent_color", prop("string", QStringLiteral("透明标记色（线稿中该色区域保持透明）；传空字符串清除标记")));
        tools.append(makeTool("set_colorize_options",
            QStringLiteral("设置智能填色算法参数（对整层生效）。改动后需重新 request_colorize_update。"),
            props, {"layer"}));
    }
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("填色图层（colorize 类型）")));
        props.insert("frame", prop("number", QStringLiteral("帧号（默认当前帧）")));
        props.insert("max_size", prop("number", QStringLiteral("返回图最长边像素数（默认1024）")));
        tools.append(makeTool("request_colorize_update",
            QStringLiteral("触发指定帧的智能填色计算（线稿来自上方最近位图层），等待完成后返回填色结果图。自动填色流程：create_layer(type=colorize) → draw_stroke 点种子 → 本工具 → 看图复查。"),
            props, {"layer"}));
    }

    // —— 自动画中割 ——
    {
        QJsonObject props;
        props.insert("layer", prop("any", QStringLiteral("位图线稿图层")));
        props.insert("frame_a", prop("number", QStringLiteral("原画A帧号（取该帧显示的关键帧）")));
        props.insert("frame_b", prop("number", QStringLiteral("原画B帧号（须大于frame_a）")));
        props.insert("count", prop("number", QStringLiteral("中间帧数量（默认1，最大24）")));
        props.insert("epsilon", prop("number", QStringLiteral("线条半宽（默认1.2；越大线越粗）")));
        props.insert("blur_passes", prop("number", QStringLiteral("距离场平滑次数（默认1）")));
        props.insert("denoise_area", prop("number", QStringLiteral("孤立碎点面积阈值（默认6，0=不去噪）")));
        props.insert("stroke_color", prop("string", QStringLiteral("线条颜色（默认黑色 #000000）")));
        tools.append(makeTool("generate_inbetweens",
            QStringLiteral("在两张原画之间自动生成中间帧（距离场插值，确定性算法；适合平移/小幅形变，大幅旋转会收缩）。生成的帧一次性入撤销（单步可撤销）。建议先 get_frame_image 看两张原画再生成，完成后逐帧查看微调。"),
            props, {"layer", "frame_a", "frame_b"}));
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
    if (tool == "draw_stroke")             return toolDrawStroke(args);
    if (tool == "fill_region")             return toolFillRegion(args);
    if (tool == "clear_frame")             return toolClearFrame(args);
    if (tool == "set_colorize_options")    return toolSetColorizeOptions(args);
    if (tool == "request_colorize_update") return toolRequestColorizeUpdate(args);
    if (tool == "generate_inbetweens")     return toolGenerateInbetweens(args);
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

// -------------------------------------------------------------- 绘制原语 ---

McpDispatcher::ToolResult McpDispatcher::toolDrawStroke(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);
    if (!layer->isBitmapKind())
        return fail(tr("只能在位图/填色图层上绘制（当前类型: %1）").arg(layerTypeName(layer)));
    if (layer->locked())
        return fail(tr("图层 %1 已锁定，无法绘制").arg(layer->name()));
    if (!layer->visible())
        return fail(tr("图层 %1 当前隐藏，请先 set_layer_visibility 显示它").arg(layer->name()));

    const int frame = args.contains("frame") ? qMax(1, args.value("frame").toInt()) : mEditor->currentFrame();
    const qreal size = args.contains("size") ? qMax(1.0, args.value("size").toDouble()) : 4.0;
    const qreal opacity = args.contains("opacity") ? qBound(0.0, args.value("opacity").toDouble(), 1.0) : 1.0;
    const QColor color = parseColor(args.value("color"), opacity, &err);
    if (!color.isValid())
        return fail(err);

    const QJsonArray pointsJson = args.value("points").toArray();
    if (pointsJson.size() < 1)
        return fail(tr("points 至少要有一个 [x,y] 点"));

    BitmapImage* bitmap = ensureBitmapAtFrame(layer, frame, &err);
    if (bitmap == nullptr)
        return fail(err);

    QVector<QPointF> points;
    for (const QJsonValue& v : pointsJson)
    {
        const QJsonArray pair = v.toArray();
        if (pair.size() < 2)
            return fail(tr("points 中存在非 [x,y] 形式的项"));
        points.append(canvasToWorld(QPointF(pair[0].toDouble(), pair[1].toDouble())));
    }

    QPen pen(color, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const SAVESTATE_ID undoState = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);

    if (points.size() == 1)
    {
        const QPointF& p = points.first();
        bitmap->drawEllipse(QRectF(p.x() - size / 2, p.y() - size / 2, size, size),
                            QPen(Qt::NoPen), QBrush(color), QPainter::CompositionMode_SourceOver, true);
    }
    else
    {
        QPainterPath path;
        path.moveTo(points.first());
        for (int i = 1; i < points.size(); ++i)
            path.lineTo(points[i]);
        if (args.value("closed").toBool(false))
            path.closeSubpath();
        bitmap->drawPath(path, pen, QBrush(Qt::NoBrush), QPainter::CompositionMode_SourceOver, true);
    }

    mEditor->undoRedo()->record(undoState, tr("MCP：绘制笔画"));
    mEditor->setModified(mEditor->object()->getIndex(layer), frame);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("frame", frame);
    data.insert("points_drawn", points.size());
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolFillRegion(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);
    if (!layer->isBitmapKind())
        return fail(tr("只能在位图/填色图层上填充（当前类型: %1）").arg(layerTypeName(layer)));
    if (layer->locked())
        return fail(tr("图层 %1 已锁定，无法绘制").arg(layer->name()));
    if (!layer->visible())
        return fail(tr("图层 %1 当前隐藏，请先 set_layer_visibility 显示它").arg(layer->name()));

    const int frame = args.contains("frame") ? qMax(1, args.value("frame").toInt()) : mEditor->currentFrame();
    const qreal opacity = args.contains("opacity") ? qBound(0.0, args.value("opacity").toDouble(), 1.0) : 1.0;
    const QColor color = parseColor(args.value("color"), opacity, &err);
    if (!color.isValid())
        return fail(err);

    const QJsonArray polygonJson = args.value("polygon").toArray();
    if (polygonJson.size() < 3)
        return fail(tr("polygon 至少要三个顶点"));

    BitmapImage* bitmap = ensureBitmapAtFrame(layer, frame, &err);
    if (bitmap == nullptr)
        return fail(err);

    QPainterPath path;
    bool first = true;
    for (const QJsonValue& v : polygonJson)
    {
        const QJsonArray pair = v.toArray();
        if (pair.size() < 2)
            return fail(tr("polygon 中存在非 [x,y] 形式的项"));
        const QPointF p = canvasToWorld(QPointF(pair[0].toDouble(), pair[1].toDouble()));
        if (first)
        {
            path.moveTo(p);
            first = false;
        }
        else
        {
            path.lineTo(p);
        }
    }
    path.closeSubpath();

    const SAVESTATE_ID undoState = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    bitmap->drawPath(path, QPen(Qt::NoPen), QBrush(color), QPainter::CompositionMode_SourceOver, true);
    mEditor->undoRedo()->record(undoState, tr("MCP：填充区域"));
    mEditor->setModified(mEditor->object()->getIndex(layer), frame);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("frame", frame);
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolClearFrame(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);
    if (!layer->isBitmapKind())
        return fail(tr("只能在位图/填色图层上清空帧（当前类型: %1）").arg(layerTypeName(layer)));

    const int frame = args.contains("frame") ? qMax(1, args.value("frame").toInt()) : mEditor->currentFrame();
    LayerBitmap* bitmapLayer = dynamic_cast<LayerBitmap*>(layer);
    BitmapImage* bitmap = bitmapLayer ? bitmapLayer->getBitmapImageAtFrame(frame) : nullptr;
    if (bitmap == nullptr)
        return fail(tr("帧 %1 上没有关键帧，无需清空").arg(frame));

    const SAVESTATE_ID undoState = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    bitmap->clear();
    mEditor->undoRedo()->record(undoState, tr("MCP：清空帧"));
    mEditor->setModified(mEditor->object()->getIndex(layer), frame);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("cleared_frame", frame);
    return ok(data);
}

// -------------------------------------------------------------- 智能填色 ---

McpDispatcher::ToolResult McpDispatcher::toolSetColorizeOptions(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    LayerColorize* colorizeLayer = dynamic_cast<LayerColorize*>(layer);
    if (colorizeLayer == nullptr)
        return fail(tr("图层 %1 不是智能填色层（type=colorize）。当前图层：%2")
                        .arg(layer->name()).arg(layerListHint()));

    if (args.contains("use_edge_detection"))
        colorizeLayer->setUseEdgeDetection(args.value("use_edge_detection").toBool());
    if (args.contains("edge_detection_size"))
        colorizeLayer->setEdgeDetectionSize(qBound(1.0, args.value("edge_detection_size").toDouble(), 20.0));
    if (args.contains("fuzzy_radius"))
        colorizeLayer->setFuzzyRadius(qBound(0.0, args.value("fuzzy_radius").toDouble(), 10.0));
    if (args.contains("clean_up_amount"))
        colorizeLayer->setCleanUpAmount(qBound(0.0, args.value("clean_up_amount").toDouble(), 1.0));
    if (args.contains("transparent_color"))
    {
        const QString value = args.value("transparent_color").toString();
        if (value.isEmpty())
        {
            colorizeLayer->clearTransparentColor();
        }
        else
        {
            const QColor color(value);
            if (!color.isValid())
                return fail(tr("transparent_color 不是合法颜色: %1").arg(value));
            colorizeLayer->setTransparentColor(color.rgba());
        }
    }

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("note", QStringLiteral("参数已更新；需要重新 request_colorize_update 才会生效"));
    return ok(data);
}

McpDispatcher::ToolResult McpDispatcher::toolRequestColorizeUpdate(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    LayerColorize* colorizeLayer = dynamic_cast<LayerColorize*>(layer);
    if (colorizeLayer == nullptr)
        return fail(tr("图层 %1 不是智能填色层（type=colorize）。当前图层：%2")
                        .arg(layer->name()).arg(layerListHint()));

    const int frame = args.contains("frame") ? qMax(1, args.value("frame").toInt()) : mEditor->currentFrame();
    const int maxSize = args.contains("max_size") ? args.value("max_size").toInt() : 1024;

    // 填色源按当前帧解析：先把播放头对准目标帧
    mEditor->scrubTo(frame);

    const int layerIndex = mEditor->object()->getIndex(layer);
    if (mEditor->object()->getColorizeSourceLayer(layerIndex, frame) == nullptr)
        return fail(tr("找不到线稿源：填色层上/下方没有当前帧非空的位图图层。当前图层：%1").arg(layerListHint()));

    BitmapImage* keyBitmap = ensureBitmapAtFrame(layer, frame, &err);
    if (keyBitmap == nullptr)
        return fail(tr("填色层在帧 %1 上没有关键帧，请先 draw_stroke 落一颗种子").arg(frame));

    // requestUpdate 在后台线程池计算，完成后 frameUpdated 信号回主线程；
    // 受控等待（工具调用串行，mDispatching 已挡并发）
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    bool finished = false;
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(mEditor->colorizeUpdates(), &ColorizeUpdateManager::frameUpdated,
            &loop, [&](int layerId, int)
    {
        if (layerId == colorizeLayer->id())
        {
            finished = true;
            loop.quit();
        }
    });

    mEditor->colorizeUpdates()->requestUpdate(colorizeLayer, frame);
    timeoutTimer.start(30000);
    loop.exec();
    timeoutTimer.stop();

    if (!finished)
        return fail(tr("填色计算超时（30秒）；工程太大或算法参数过重"));

    ColorizeImage* colorizeKey = dynamic_cast<ColorizeImage*>(keyBitmap);
    if (colorizeKey == nullptr || colorizeKey->coloringImage().isNull())
        return fail(tr("填色结果为空：检查种子笔画是否落在封闭区域内"));

    // 把填色结果贴回画布坐标系输出
    const QSize size = canvasSize();
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.translate(size.width() / 2.0, size.height() / 2.0);
    painter.drawImage(colorizeKey->coloringBounds().topLeft(), colorizeKey->coloringImage());
    painter.end();

    if (maxSize > 0 && (image.width() > maxSize || image.height() > maxSize))
        image = image.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    data.insert("frame", frame);
    data.insert("width", image.width());
    data.insert("height", image.height());
    return okWithImage(data, image);
}

// -------------------------------------------------------------- 自动画中割 ---

McpDispatcher::ToolResult McpDispatcher::toolGenerateInbetweens(const QJsonObject& args)
{
    QString err;
    Layer* layer = resolveLayer(args, &err);
    if (layer == nullptr)
        return fail(err);

    LayerBitmap* bitmapLayer = dynamic_cast<LayerBitmap*>(layer);
    if (bitmapLayer == nullptr)
        return fail(tr("中割只能在位图族图层上生成（当前类型: %1）").arg(layerTypeName(layer)));

    const int frameA = qMax(1, args.value("frame_a").toInt());
    const int frameB = args.value("frame_b").toInt();
    if (frameB - frameA < 2)
        return fail(tr("frame_b 必须比 frame_a 至少大 2（中间才有空位放中割帧）"));

    const int count = qBound(1, args.contains("count") ? args.value("count").toInt() : 1, 24);

    BitmapImage* keyA = bitmapLayer->getLastBitmapImageAtFrame(frameA);
    BitmapImage* keyB = bitmapLayer->getLastBitmapImageAtFrame(frameB);
    if (keyA == nullptr || keyB == nullptr)
        return fail(tr("帧 %1 或 %2 上没有原画内容").arg(frameA).arg(frameB));
    keyA->loadFile();
    keyB->loadFile();

    // 目标位置：等分 A、B 区间；与已有关键帧冲突时整体报错（agent 可换 count 或先清帧）
    QList<int> positions;
    for (int k = 1; k <= count; ++k)
        positions.append(frameA + qRound(k * static_cast<qreal>(frameB - frameA) / (count + 1)));
    QStringList conflicts;
    for (int pos : positions)
        if (layer->keyExists(pos))
            conflicts.append(QString::number(pos));
    if (!conflicts.isEmpty())
        return fail(tr("目标位置上已有关键帧: %1；请先删除或换个 count").arg(conflicts.join(QStringLiteral("、"))));

    // 两张原画对齐到共同世界区域
    const QRect world = keyA->bounds().united(keyB->bounds());
    auto extract = [world](BitmapImage* key) -> QImage
    {
        QImage img(world.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        painter.drawImage(key->bounds().topLeft() - world.topLeft(), *key->image());
        painter.end();
        return img;
    };
    const QImage imgA = extract(keyA);
    const QImage imgB = extract(keyB);

    Inbetween::Options options;
    if (args.contains("epsilon"))
        options.epsilon = qBound(0.5, args.value("epsilon").toDouble(), 8.0);
    if (args.contains("blur_passes"))
        options.blurPasses = qBound(0, args.value("blur_passes").toInt(), 4);
    if (args.contains("denoise_area"))
        options.denoiseArea = qBound(0, args.value("denoise_area").toInt(), 100);
    if (args.contains("stroke_color"))
    {
        const QColor color = parseColor(args.value("stroke_color"), 1.0, &err);
        if (!color.isValid())
            return fail(err);
        options.strokeColor = color;
    }

    mEditor->beginLayerLayoutEdit(layer);
    QImage preview;
    for (int k = 1; k <= count; ++k)
    {
        const qreal t = k / static_cast<qreal>(count + 1);
        const QImage mid = Inbetween::interpolate(imgA, imgB, t, options);
        layer->addKeyFrame(positions[k - 1], new BitmapImage(world.topLeft(), mid));
        if (k == 1)
            preview = mid;
    }
    mEditor->endLayerLayoutEdit(tr("MCP：生成中割"));

    // 预览图贴回画布坐标
    const QSize canvas = canvasSize();
    QImage image(canvas, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.translate(canvas.width() / 2.0, canvas.height() / 2.0);
    painter.drawImage(world.topLeft(), preview);
    painter.end();
    image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QJsonObject data;
    data.insert("layer", layerSummary(layer, mEditor->object()->getIndex(layer) + 1));
    QJsonArray framesCreated;
    for (int pos : positions)
        framesCreated.append(pos);
    data.insert("frames_created", framesCreated);
    data.insert("preview_frame", positions.first());
    data.insert("note", QStringLiteral("距离场插值算法：平移/小形变效果稳定，大幅旋转会收缩；不满意的帧可用 delete_frame 后重画"));
    return okWithImage(data, image);
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

QPointF McpDispatcher::canvasToWorld(const QPointF& canvasPoint) const
{
    const QSize size = canvasSize();
    return QPointF(canvasPoint.x() - size.width() / 2.0, canvasPoint.y() - size.height() / 2.0);
}

BitmapImage* McpDispatcher::ensureBitmapAtFrame(Layer* layer, int frame, QString* err)
{
    LayerBitmap* bitmapLayer = dynamic_cast<LayerBitmap*>(layer);
    if (bitmapLayer == nullptr)
    {
        *err = tr("图层不是位图族类型");
        return nullptr;
    }

    if (!layer->keyExists(frame))
    {
        mEditor->beginLayerLayoutEdit(layer);
        const bool added = layer->addNewKeyFrameAt(frame);
        mEditor->endLayerLayoutEdit(tr("MCP：新增关键帧"));
        if (!added)
        {
            *err = tr("在帧 %1 上创建关键帧失败").arg(frame);
            return nullptr;
        }
    }

    BitmapImage* bitmap = bitmapLayer->getBitmapImageAtFrame(frame);
    if (bitmap == nullptr)
    {
        *err = tr("帧 %1 上取不到位图关键帧").arg(frame);
        return nullptr;
    }
    bitmap->loadFile();
    return bitmap;
}

QColor McpDispatcher::parseColor(const QJsonValue& value, qreal opacity, QString* err) const
{
    const QString text = value.toString().trimmed();
    QColor color(text);
    if (!color.isValid())
    {
        *err = tr("颜色格式不合法: 「%1」（应为 #RRGGBB 或 #AARRGGBB）").arg(text);
        return QColor();
    }
    color.setAlphaF(color.alphaF() * qBound(0.0, opacity, 1.0));
    return color;
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
