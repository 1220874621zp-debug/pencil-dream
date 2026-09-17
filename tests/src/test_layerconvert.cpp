#include "catch.hpp"

#include "editor.h"
#include "scribblearea.h"
#include "object.h"
#include "layerbitmap.h"
#include "layercolorize.h"
#include "colorizeimage.h"
#include "layermanager.h"
#include "selectionmanager.h"
#include "bitmapimage.h"
#include "undoredomanager.h"

#include <QPen>

namespace
{

Editor* makeEditorWithLayers(LayerBitmap** outLine, LayerColorize** outColorize, LayerBitmap** outFrame)
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

    // 视觉栈序（索引大者在上）：底稿在填色层下方，线稿源在填色层上方
    editor->layers()->createBitmapLayer("底稿");
    LayerColorize* colorizeLayer = static_cast<LayerColorize*>(
        editor->layers()->createColorizeLayer("填色"));
    editor->layers()->createBitmapLayer("线稿");
    editor->scrubTo(1);

    if (outColorize) { *outColorize = colorizeLayer; }
    if (outLine)
    {
        Layer* top = editor->object()->getLayer(editor->object()->getLayerCount() - 1);
        *outLine = static_cast<LayerBitmap*>(top);
    }
    if (outFrame) { *outFrame = nullptr; }
    return editor;
}

} // namespace

TEST_CASE("ConvertColorizeLayerToBitmap swap and undo roundtrip")
{
    LayerColorize* colorizeLayer = nullptr;
    Editor* editor = makeEditorWithLayers(nullptr, &colorizeLayer, nullptr);
    editor->layers()->setCurrentLayer(editor->object()->getIndex(colorizeLayer));

    // 填色层关键帧1 画一笔（无着色结果——转换应保留曝光结构建空帧/烘焙空结果）
    auto* stroke = static_cast<ColorizeImage*>(colorizeLayer->getKeyFrameAt(1));
    REQUIRE(stroke != nullptr);
    QPen pen(QColor(255, 0, 0, 255));
    pen.setWidth(6);
    stroke->drawLine(QPointF(-50, -30), QPointF(30, 20), pen, QPainter::CompositionMode_SourceOver, false);
    const QRgb sampleStroke = stroke->constScanLine(-50, -30);
    REQUIRE(sampleStroke != 0);

    const int layerId = colorizeLayer->id();
    const int keyCount = colorizeLayer->keyFrameCount();

    // 转换：当前层须为填色层；换壳后类型/关键帧数一致、id 沿用
    REQUIRE(editor->convertColorizeLayerToBitmap());
    Layer* converted = editor->layers()->currentLayer();
    REQUIRE(converted->type() == Layer::BITMAP);
    REQUIRE(converted->id() == layerId);
    REQUIRE(converted->keyFrameCount() == keyCount);
    REQUIRE(converted->name() == QStringLiteral("填色"));

    // 非填色层调用被拒绝
    REQUIRE_FALSE(editor->convertColorizeLayerToBitmap());

    // undo：换回填色层，原笔画数据原样保留（同一层对象）
    editor->undoRedo()->undo();
    Layer* restored = editor->object()->findLayerById(layerId);
    REQUIRE(restored->type() == Layer::COLORIZE);
    auto* restoredStroke = static_cast<ColorizeImage*>(
        static_cast<LayerColorize*>(restored)->getKeyFrameAt(1));
    REQUIRE(restoredStroke->constScanLine(-50, -30) == sampleStroke);

    // redo：再次换壳成位图层
    editor->undoRedo()->redo();
    REQUIRE(editor->object()->findLayerById(layerId)->type() == Layer::BITMAP);

    delete editor;
}

TEST_CASE("DeleteSelection lasso polygon clears only selected pixels")
{
    LayerBitmap* lineLayer = nullptr;
    LayerColorize* unused = nullptr;
    Editor* editor = makeEditorWithLayers(&lineLayer, &unused, nullptr);

    LayerBitmap* frameLayer = static_cast<LayerBitmap*>(editor->object()->getLayer(1));
    editor->layers()->setCurrentLayer(editor->object()->getIndex(frameLayer));
    editor->scrubTo(1);
    BitmapImage* frame = static_cast<BitmapImage*>(frameLayer->getKeyFrameAt(1));
    REQUIRE(frame != nullptr);
    QPen pen(QColor(255, 0, 0, 255));
    pen.setWidth(5);
    frame->drawLine(QPointF(-100, -50), QPointF(-40, -20), pen, QPainter::CompositionMode_SourceOver, false);
    const QRgb inside = frame->constScanLine(-100, -50);
    const QRgb outside = frame->constScanLine(-45, -22);
    REQUIRE(inside != 0);
    REQUIRE(outside != 0);
    Q_UNUSED(lineLayer);

    // 套索多边形套住线稿左半段
    QPolygonF polygon;
    polygon << QPointF(-110, -60) << QPointF(-70, -60) << QPointF(-70, -10) << QPointF(-110, -10);
    editor->select()->setSelection(polygon);

    // 与 Key_Delete/Key_Backspace 同链的删除入口
    editor->getScribbleArea()->deleteSelection();
    REQUIRE(frame->constScanLine(-100, -50) == 0); // 选区内被删
    REQUIRE(frame->constScanLine(-45, -22) == outside); // 选区外原样保留

    delete editor;
}
