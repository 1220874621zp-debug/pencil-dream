#include "catch.hpp"

#include "editor.h"
#include "scribblearea.h"
#include "object.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "bitmapimage.h"
#include "undoredocommand.h"
#include "undoredomanager.h"

#include <QPen>
#include <QDir>
#include <QFile>

// 与 ActionCommands::clearCurrentLayerCanvas 同核心：双快照 + clear + BitmapReplaceCommand
static void clearCurrentLayerCanvasCore(Editor* editor)
{
    Layer* layer = editor->layers()->currentLayer();
    auto bitmapLayer = static_cast<LayerBitmap*>(layer);
    BitmapImage* bitmap = static_cast<BitmapImage*>(
        bitmapLayer->getKeyFrameWhichCovers(bitmapLayer->displayFrameFor(editor->currentFrame())));
    REQUIRE(bitmap != nullptr);

    BitmapImage undoSnapshot = *bitmap;
    const int keyFramePos = bitmap->pos();
    bitmap->clear();
    BitmapImage redoSnapshot = *bitmap;
    editor->undoRedo()->pushUndoCommand(
        new BitmapReplaceCommand(&undoSnapshot, &redoSnapshot, layer->id(), "clear", editor));
    // 数据失效传关键帧 pos（精确查找），显示缓存失效传当前显示帧（endStroke 同款）
    editor->setModified(editor->layers()->currentLayerIndex(), keyFramePos);
    editor->getScribbleArea()->onFrameModified(editor->currentFrame());
}

static Editor* makeEditorWithStrokeLayer(BitmapImage** outFrame)
{
    Object* object = new Object;
    object->init();
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->setObject(object);
    scribbleArea->setEditor(editor);
    editor->init();
    scribbleArea->init();

    LayerBitmap* layer = editor->layers()->createBitmapLayer("clear");
    editor->layers()->setCurrentLayer(0);
    editor->scrubTo(1);

    // 关键帧1：在画布中心坐标系负区间落笔（模拟用户在画布左上区域画线）
    BitmapImage* frame = static_cast<BitmapImage*>(layer->getKeyFrameAt(1));
    REQUIRE(frame != nullptr);
    QPen pen(QColor(255, 0, 0, 255));
    pen.setWidth(5);
    frame->drawLine(QPointF(-100, -50), QPointF(-40, -20), pen, QPainter::CompositionMode_SourceOver, false);

    if (outFrame) { *outFrame = frame; }
    return editor;
}

TEST_CASE("ClearFrame double-snapshot undo roundtrip")
{
    BitmapImage* frame = nullptr;
    Editor* editor = makeEditorWithStrokeLayer(&frame);

    const QRect beforeBounds = frame->bounds();
    const QRgb sampleBefore = frame->constScanLine(-100, -50);
    REQUIRE(sampleBefore != 0);

    // 当前帧 = auto 块中部（关键帧1 延伸），与用户场景一致
    editor->scrubTo(3);
    REQUIRE(editor->currentFrame() == 3);

    SECTION("clear then undo restores pixels exactly in place")
    {
        clearCurrentLayerCanvasCore(editor);

        // 清空后：帧无内容，且关键帧 dirty 已标记（渲染缓存据此失效）
        REQUIRE(frame->bounds().isEmpty());
        REQUIRE(frame->isModified());

        // undo：bounds 与采样像素原位恢复
        editor->undoRedo()->undo();
        REQUIRE(frame->bounds() == beforeBounds);
        REQUIRE(frame->constScanLine(-100, -50) == sampleBefore);

        // redo：再次清空
        editor->undoRedo()->redo();
        REQUIRE(frame->bounds().isEmpty());
    }

    SECTION("cleared frame must not resurrect from stale file on render loadFile")
    {
        // 复现真实工程：保存过的帧挂有 PNG 文件名（惰性加载源）。
        // 清空后渲染层会调 loadFile()——若文件名未断开，旧图被加载回来
        //（像素复活 + topLeft 重置到画布中心 = 内容偏移）。
        const QString pngPath = QDir::temp().filePath("pencil_clearframe_stale.png");
        QImage stale(beforeBounds.size(), QImage::Format_ARGB32_Premultiplied);
        stale.fill(QColor(255, 0, 0, 255));
        REQUIRE(stale.save(pngPath));
        frame->setFileName(pngPath);

        clearCurrentLayerCanvasCore(editor);

        // 渲染路径的 loadFile：不得复活
        frame->loadFile();
        REQUIRE(frame->bounds().isEmpty());
        REQUIRE(frame->image()->isNull());

        QFile::remove(pngPath);
    }

    SECTION("stroke after clear lands at correct canvas position")
    {
        clearCurrentLayerCanvasCore(editor);

        // 清空后在画布另一处（正区间）落笔：像素应落在画布绝对坐标上
        QPen pen(QColor(0, 0, 255, 255));
        pen.setWidth(5);
        frame->drawLine(QPointF(200, 100), QPointF(260, 130), pen, QPainter::CompositionMode_SourceOver, false);
        REQUIRE(frame->constScanLine(200, 100) != 0);
        REQUIRE(frame->bounds().contains(200, 100));
        REQUIRE(frame->bounds().contains(260, 130));
    }

    SECTION("stroke after undo lands at correct canvas position")
    {
        clearCurrentLayerCanvasCore(editor);
        editor->undoRedo()->undo();
        REQUIRE(frame->constScanLine(-100, -50) == sampleBefore);

        // 撤销恢复后在原线旁边再画：旧像素不移动，新像素位置正确
        const QRect boundsAfterUndo = frame->bounds();
        QPen pen(QColor(0, 0, 255, 255));
        pen.setWidth(5);
        frame->drawLine(QPointF(300, 200), QPointF(340, 220), pen, QPainter::CompositionMode_SourceOver, false);

        REQUIRE(frame->constScanLine(-100, -50) == sampleBefore);      // 旧像素原位
        REQUIRE(frame->bounds().contains(boundsAfterUndo));            // bounds 只扩不移
        REQUIRE(frame->constScanLine(300, 200) != 0);                  // 新像素落位
    }

    delete editor;
}
