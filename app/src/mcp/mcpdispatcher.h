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

#ifndef MCPDISPATCHER_H
#define MCPDISPATCHER_H

#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <QPointer>

class Editor;
class Layer;
class LayerBitmap;

/**
 * MCP 工具分发层：工具 schema 清单 + 每工具一个实现函数。
 * 全部在主线程执行；像素修改入撤销栈，帧增删走布局事务。
 */
class McpDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit McpDispatcher(Editor* editor, QObject* parent = nullptr);

    QJsonArray toolsSchema() const;

    struct ToolResult
    {
        QJsonObject data;  // structuredContent 与 text 正文
        QImage image;      // 非空 → 附加 image content 返回
        bool isError = false;
    };

    ToolResult dispatch(const QString& tool, const QJsonObject& args);

private:
    // —— 感知 ——
    ToolResult toolGetSceneStatus(const QJsonObject& args);
    ToolResult toolGetFrameImage(const QJsonObject& args);
    ToolResult toolGetPalette(const QJsonObject& args);

    // —— 图层 ——
    ToolResult toolCreateLayer(const QJsonObject& args);
    ToolResult toolDeleteLayer(const QJsonObject& args);
    ToolResult toolSetLayerVisibility(const QJsonObject& args);
    ToolResult toolSelectLayer(const QJsonObject& args);
    ToolResult toolRenameLayer(const QJsonObject& args);

    // —— 帧 ——
    ToolResult toolAddKeyFrame(const QJsonObject& args);
    ToolResult toolDuplicateFrame(const QJsonObject& args);
    ToolResult toolDeleteFrame(const QJsonObject& args);
    ToolResult toolScrubTo(const QJsonObject& args);

    // —— 绘制原语 ——
    ToolResult toolDrawStroke(const QJsonObject& args);
    ToolResult toolFillRegion(const QJsonObject& args);
    ToolResult toolClearFrame(const QJsonObject& args);

    // —— 智能填色 ——
    ToolResult toolSetColorizeOptions(const QJsonObject& args);
    ToolResult toolRequestColorizeUpdate(const QJsonObject& args);

    // —— 项目 / 播放 / 撤销 ——
    ToolResult toolOpenProject(const QJsonObject& args);
    ToolResult toolSaveProject(const QJsonObject& args);
    ToolResult toolExportFrame(const QJsonObject& args);
    ToolResult toolPlay(const QJsonObject& args);
    ToolResult toolStop(const QJsonObject& args);
    ToolResult toolUndo(const QJsonObject& args);
    ToolResult toolRedo(const QJsonObject& args);

    // —— 帮助函数 ——
    Layer* resolveLayer(const QJsonObject& args, QString* err) const;
    QJsonObject layerSummary(Layer* layer, int rowIndex) const;
    QString layerListHint() const;
    QImage renderFrame(int frame, Layer* singleLayer, bool background) const;
    QSize canvasSize() const;
    QPointF canvasToWorld(const QPointF& canvasPoint) const;
    /** 取位图族图层在指定帧的位图；无关键帧时先建空关键帧（布局事务） */
    class BitmapImage* ensureBitmapAtFrame(Layer* layer, int frame, QString* err);
    QColor parseColor(const QJsonValue& value, qreal opacity, QString* err) const;
    static ToolResult ok(QJsonObject data);
    static ToolResult okWithImage(QJsonObject data, const QImage& image);
    static ToolResult fail(const QString& message);

    QPointer<Editor> mEditor;
};

#endif // MCPDISPATCHER_H
