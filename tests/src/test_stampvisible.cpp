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

#include "catch.hpp"

#include "editor.h"
#include "scribblearea.h"
#include "object.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "bitmapimage.h"
#include "undoredomanager.h"
#include "layerlayoutcommand.h"

#include <QImage>
#include <QPainter>
#include <QPen>

namespace
{

Editor* makeStampEditor(LayerBitmap** outBottom, LayerBitmap** outTop)
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

    // 栈序：0=相机（显式补建——Object::init 不建层，真实工程恒有相机层），1=底，2=顶
    editor->layers()->createCameraLayer("相机");
    LayerBitmap* bottomLayer = editor->layers()->createBitmapLayer("底");
    LayerBitmap* topLayer = editor->layers()->createBitmapLayer("顶");
    editor->scrubTo(1);

    if (outBottom) { *outBottom = bottomLayer; }
    if (outTop) { *outTop = topLayer; }
    return editor;
}

QImage compositeVisible(Object* object, const QRect& worldRect, int frame)
{
    QImage stamp(worldRect.size(), QImage::Format_ARGB32_Premultiplied);
    stamp.fill(Qt::transparent);
    QPainter stampPainter(&stamp);
    stampPainter.setTransform(QTransform::fromTranslate(-worldRect.left(), -worldRect.top()));
    object->paintImage(stampPainter, frame, false, true);
    return stamp;
}

} // namespace

/** 盖印可见图层（图层行右键菜单）核心链路回归：ActionCommands::stampVisibleLayers 的
 *  core_lib 侧镜像——相机取景、离线合成、置顶建层、SplitLayerCommand 单步撤销/重做。
 *  历史雷：getCameraLayerBelow 必须传 count-1（传 count 时 getLayer 越界返回 nullptr，
 *  getLayerBelow 里 Release 版 Q_ASSERT 空操作后 nullptr->type() 直接崩溃）。 */
TEST_CASE("StampVisible composite + top layer + undo roundtrip")
{
    LayerBitmap* bottom = nullptr;
    LayerBitmap* top = nullptr;
    Editor* editor = makeStampEditor(&bottom, &top);
    Object* object = editor->object();

    // 两层各画一条互不重叠的实色线（采样取线段中点，抗锯齿边缘不影响）
    QPen redPen(QColor(255, 0, 0, 255));
    redPen.setWidth(20);
    QPen bluePen(QColor(0, 0, 255, 255));
    bluePen.setWidth(20);
    auto* bottomFrame = static_cast<BitmapImage*>(bottom->getKeyFrameAt(1));
    bottomFrame->drawLine(QPointF(-100, -100), QPointF(-40, -100), redPen,
                          QPainter::CompositionMode_SourceOver, false);
    auto* topFrame = static_cast<BitmapImage*>(top->getKeyFrameAt(1));
    topFrame->drawLine(QPointF(50, 50), QPointF(110, 50), bluePen,
                       QPainter::CompositionMode_SourceOver, false);

    LayerCamera* camera = editor->layers()->getCameraLayerBelow(object->getLayerCount() - 1);
    REQUIRE(camera != nullptr);
    const QRect worldRect = camera->getViewRect();
    REQUIRE(worldRect.width() > 0);

    const QPoint redAt = QPoint(-70, -100) - worldRect.topLeft();
    const QPoint blueAt = QPoint(80, 50) - worldRect.topLeft();

    // 合成同时包含两层内容
    QImage stamp = compositeVisible(object, worldRect, 1);
    REQUIRE(stamp.pixel(redAt) == qRgb(255, 0, 0));
    REQUIRE(stamp.pixel(blueAt) == qRgb(0, 0, 255));

    // 隐藏上层后合成只剩下层
    top->setVisible(false);
    QImage stampHidden = compositeVisible(object, worldRect, 1);
    REQUIRE(stampHidden.pixel(redAt) == qRgb(255, 0, 0));
    REQUIRE(qAlpha(stampHidden.pixel(blueAt)) == 0);
    top->setVisible(true);

    // 置顶建层 + 帧格承载（与 ActionCommands::stampVisibleLayers 同式）
    const int layerCountBefore = object->getLayerCount();
    const LayerOrderCommand::GroupSnapshot undoGroups = LayerOrderCommand::captureGroups(object);
    Layer* prevCurrent = editor->layers()->currentLayer();

    auto* stampLayer = new LayerBitmap(object->getUniqueLayerID());
    stampLayer->setName("盖印 帧2");
    object->insertLayer(object->getLayerCount(), stampLayer);

    BitmapImage* stampImage = new BitmapImage(worldRect.topLeft(), stamp);
    stampImage->enableAutoCrop(true);
    stampLayer->addKeyFrame(1, stampImage);
    stampImage->setModified(true);
    editor->setModified(object->getIndex(stampLayer), 1);

    editor->undoRedo()->pushUndoCommand(
        new SplitLayerCommand(editor, QList<Layer*>() << stampLayer,
                              prevCurrent != nullptr ? prevCurrent->id() : -1,
                              false, undoGroups, "盖印可见图层"));

    REQUIRE(object->getLayerCount() == layerCountBefore + 1);
    REQUIRE(object->getLayer(object->getLayerCount() - 1) == stampLayer);
    REQUIRE(stampLayer->getKeyFrameAt(1) == stampImage);

    // 撤销：整层（连帧带像素）一起摘下
    editor->undoRedo()->undo();
    REQUIRE(object->getLayerCount() == layerCountBefore);
    REQUIRE(object->getLayer(object->getLayerCount() - 1) == top);

    // 重做：层原位挂回，帧内容仍在
    editor->undoRedo()->redo();
    REQUIRE(object->getLayer(object->getLayerCount() - 1) == stampLayer);
    REQUIRE(stampLayer->getKeyFrameAt(1) == stampImage);
}

/** Solo 独显（图层行 s 按钮）渲染语义：任一可见层 solo 激活时只合成 solo 层；
 *  多 solo 并存、眼关的 solo 不激活、全关恢复。判定点=Object::isLayerRenderable
 *  （画布/盖印/导出同源），XML 持久化属性 solo。 */
TEST_CASE("Solo visibility composite semantics")
{
    LayerBitmap* bottom = nullptr;
    LayerBitmap* top = nullptr;
    Editor* editor = makeStampEditor(&bottom, &top);
    Object* object = editor->object();

    QPen redPen(QColor(255, 0, 0, 255));
    redPen.setWidth(20);
    QPen bluePen(QColor(0, 0, 255, 255));
    bluePen.setWidth(20);
    auto* bottomFrame = static_cast<BitmapImage*>(bottom->getKeyFrameAt(1));
    bottomFrame->drawLine(QPointF(-100, -100), QPointF(-40, -100), redPen,
                          QPainter::CompositionMode_SourceOver, false);
    auto* topFrame = static_cast<BitmapImage*>(top->getKeyFrameAt(1));
    topFrame->drawLine(QPointF(50, 50), QPointF(110, 50), bluePen,
                       QPainter::CompositionMode_SourceOver, false);

    LayerCamera* camera = editor->layers()->getCameraLayerBelow(object->getLayerCount() - 1);
    REQUIRE(camera != nullptr);
    const QRect worldRect = camera->getViewRect();
    const QPoint redAt = QPoint(-70, -100) - worldRect.topLeft();
    const QPoint blueAt = QPoint(80, 50) - worldRect.topLeft();

    // solo 底层：合成只含底层
    bottom->setSolo(true);
    REQUIRE(object->anyLayerSolo());
    QImage soloOne = compositeVisible(object, worldRect, 1);
    REQUIRE(soloOne.pixel(redAt) == qRgb(255, 0, 0));
    REQUIRE(qAlpha(soloOne.pixel(blueAt)) == 0);

    // 第二层也 solo：多 solo 并存全显示
    top->setSolo(true);
    QImage soloBoth = compositeVisible(object, worldRect, 1);
    REQUIRE(soloBoth.pixel(redAt) == qRgb(255, 0, 0));
    REQUIRE(soloBoth.pixel(blueAt) == qRgb(0, 0, 255));

    // 全部关 solo：恢复全显
    bottom->setSolo(false);
    top->setSolo(false);
    REQUIRE_FALSE(object->anyLayerSolo());
    QImage soloOff = compositeVisible(object, worldRect, 1);
    REQUIRE(soloOff.pixel(redAt) == qRgb(255, 0, 0));
    REQUIRE(soloOff.pixel(blueAt) == qRgb(0, 0, 255));

    // 眼睛关闭的 solo 层不激活独显（否则全场景不可见）；眼关仍胜过 solo
    bottom->setSolo(true);
    bottom->setVisible(false);
    REQUIRE_FALSE(object->anyLayerSolo());
    QImage eyeOff = compositeVisible(object, worldRect, 1);
    REQUIRE(qAlpha(eyeOff.pixel(redAt)) == 0);
    REQUIRE(eyeOff.pixel(blueAt) == qRgb(0, 0, 255));
}
