/*

Pencil2D - Traditional Animation Software
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

#include "shadowengine.h"

#include <QColor>
#include <QImage>
#include <QLine>
#include <QPainter>
#include <QRect>

namespace
{

QImage makeCanvas(int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    return img;
}

// 线稿惯例：不透明白线只看 alpha
void drawSquareOutline(QImage& img, const QRect& r)
{
    QPainter p(&img);
    p.setPen(QPen(Qt::white, 4));
    p.drawRect(r);
    p.end();
}

void drawStrokeLine(QImage& img, const QLine& line, const QColor& color, int width = 3)
{
    QPainter p(&img);
    p.setPen(QPen(color, width));
    p.drawLine(line);
    p.end();
}

bool isColor(const QImage& img, int x, int y, QRgb expected)
{
    return img.pixel(x, y) == qPremultiply(expected);
}

bool isTransparent(const QImage& img, int x, int y)
{
    return qAlpha(img.pixel(x, y)) == 0;
}

struct FramePair
{
    QImage lineArt;
    QImage strokes;
};

// 正方形线稿 (10,10,180,180) + 一条分割线的标准画布
FramePair makeSquareWithLine(const QLine& divider, const QColor& color = Qt::red)
{
    FramePair fp;
    fp.lineArt = makeCanvas(200, 200);
    drawSquareOutline(fp.lineArt, QRect(10, 10, 180, 180));
    fp.strokes = makeCanvas(200, 200);
    drawStrokeLine(fp.strokes, divider, color);
    return fp;
}
} // namespace

TEST_CASE("ShadowFill_VerticalDividerPickLeft")
{
    FramePair fp = makeSquareWithLine(QLine(100, 8, 100, 192));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    params.fillColor = qRgb(0, 0, 0);
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    REQUIRE(result.warnings.isEmpty());
    CHECK(isColor(result.fill, 50, 100, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 150, 100));
    // 分割线像素贴阴影侧被接缝涂色，不残留透明细缝
    CHECK(isColor(result.fill, 100, 100, qRgb(0, 0, 0)));
    // 阴影严格止于分割线
    CHECK(isTransparent(result.fill, 102, 100));
}

TEST_CASE("ShadowFill_HorizontalDividerPickDown")
{
    FramePair fp = makeSquareWithLine(QLine(8, 100, 192, 100));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirDown;
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 100, 150, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 100, 50));
    CHECK(isColor(result.fill, 100, 100, qRgb(0, 0, 0)));
}

TEST_CASE("ShadowFill_PickUpMeansSmallerY")
{
    FramePair fp = makeSquareWithLine(QLine(8, 100, 192, 100));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirUp;
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 100, 50, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 100, 150));
}

TEST_CASE("ShadowFill_AmbiguousInheritsSpatialNeighbor")
{
    // 上方正方形横线分割（选"左"=歧义），下方正方形竖线分割（可信左）；
    // 上方正方形上下两半都继承下方左侧（阴影）区域的状态
    QImage lineArt = makeCanvas(200, 400);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    drawSquareOutline(lineArt, QRect(10, 210, 180, 180));
    QImage strokes = makeCanvas(200, 400);
    drawStrokeLine(strokes, QLine(8, 100, 192, 100), Qt::red);   // 上：横线 → 歧义
    drawStrokeLine(strokes, QLine(140, 208, 140, 392), Qt::red); // 下：竖线偏右 → 左侧为阴影

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 400), params);

    REQUIRE_FALSE(result.fill.isNull());
    // 下方正方形：可信判定，左阴影右不阴影
    CHECK(isColor(result.fill, 60, 300, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 170, 300));
    // 上方正方形：空间邻近继承 → 两半均为阴影
    CHECK(isColor(result.fill, 100, 50, qRgb(0, 0, 0)));
    CHECK(isColor(result.fill, 100, 150, qRgb(0, 0, 0)));
}

TEST_CASE("ShadowFill_UnclosedDividerWarning")
{
    // 悬空分割线（没搭到底边）→ 图形未切开 → 警告且不上阴影
    FramePair fp = makeSquareWithLine(QLine(100, 8, 100, 80));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    CHECK(result.fill.isNull());
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0].kind == ShadowFill::Warning::UnclosedDivider);
    CHECK(result.warnings[0].area.contains(100, 100));
}

TEST_CASE("ShadowFill_TransparentMarkerDisablesLine")
{
    // 红竖线（有效）+ 蓝横线（标透明=禁用）：图形只按红竖线切分，
    // 阴影跨过蓝线位置连续填充
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(60, 8, 60, 192), Qt::red);
    drawStrokeLine(strokes, QLine(8, 100, 192, 100), Qt::blue);

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    QVector<QRgb> disabled;
    disabled.append(qRgb(0, 0, 255));
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params, disabled);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 30, 50, qRgb(0, 0, 0)));
    CHECK(isColor(result.fill, 30, 100, qRgb(0, 0, 0))); // 蓝线位置照常填充
    CHECK(isColor(result.fill, 30, 150, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 120, 100));
}

TEST_CASE("ShadowFill_CrossDividersPickLeftFillsLeftColumn")
{
    // 十字切割出四个象限，选"左"：极端组规则把并列的整列一起判给
    // 所选方向——左列两个象限均为阴影（横线对左右无信息，不拖累判定）
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(60, 8, 60, 192), Qt::red);
    drawStrokeLine(strokes, QLine(8, 100, 192, 100), Qt::blue);

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(result.warnings.isEmpty());
    CHECK(isColor(result.fill, 30, 50, qRgb(0, 0, 0)));
    CHECK(isColor(result.fill, 30, 150, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 120, 50));
    CHECK(isTransparent(result.fill, 120, 150));
}

TEST_CASE("ShadowFill_UndecidableFallsBackDeterministic")
{
    // 全帧唯一图形且方向无信息（横线配"左"）、无可借鉴 → 兜底确定性
    // 选边（并列按次轴字典序，取质心 y 最小的上侧）必有填充，警告保留
    FramePair fp = makeSquareWithLine(QLine(8, 100, 192, 100));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0].kind == ShadowFill::Warning::UnresolvedAmbiguous);
    CHECK(isColor(result.fill, 100, 50, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 100, 150));
}

TEST_CASE("ShadowFill_ThreeStripsPickLeftmost")
{
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(60, 8, 60, 192), Qt::red);
    drawStrokeLine(strokes, QLine(140, 8, 140, 192), Qt::red);

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 30, 100, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 100, 100));
    CHECK(isTransparent(result.fill, 170, 100));
}

TEST_CASE("ShadowFill_NoDividerNoFill")
{
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);

    ShadowFill::Params params;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 0, 200, 200), params);

    CHECK(result.fill.isNull());
    CHECK(result.warnings.isEmpty());
}

TEST_CASE("ShadowFill_CustomFillColor")
{
    FramePair fp = makeSquareWithLine(QLine(100, 8, 100, 192));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    params.fillColor = qRgb(255, 0, 255);
    ShadowFill::Result result = ShadowFill::computeShadow(fp.lineArt, fp.strokes, QRect(0, 0, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    CHECK(isColor(result.fill, 50, 100, qRgb(255, 0, 255)));
}

TEST_CASE("ShadowFill_EmptyStrokeLayerNoFill")
{
    QImage lineArt = makeCanvas(200, 200);
    drawSquareOutline(lineArt, QRect(10, 10, 180, 180));
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(100, 8, 100, 192), Qt::red);

    // bounds 外的笔画被忽略：给一个不含分割线的 bounds
    ShadowFill::Params params;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(150, 150, 40, 40), params);

    CHECK(result.fill.isNull());
    CHECK(result.warnings.isEmpty());
}

// ============== 层级集成（LayerShadow 接线） ==============

#include "object.h"
#include "layershadow.h"
#include "shadowimage.h"
#include "layerbitmap.h"
#include <QDomDocument>
#include <QTemporaryDir>

namespace
{

// 位图层方形线稿 + 阴影层竖直分割线，跑一次 updateShadowAtFrame
void makeLayerFixture(Object* obj, LayerShadow** outShadow)
{
    auto* lineLayer = obj->addNewBitmapLayer();
    lineLayer->setName("line");
    QImage square = makeCanvas(200, 200);
    drawSquareOutline(square, QRect(10, 10, 180, 180));
    *lineLayer->getLastBitmapImageAtFrame(1) = BitmapImage(QPoint(0, 0), square);

    auto* shadowLayer = static_cast<LayerShadow*>(obj->addNewShadowLayer());
    shadowLayer->setName("shadow");
    QImage strokes = makeCanvas(200, 200);
    drawStrokeLine(strokes, QLine(100, 8, 100, 192), QColor(255, 0, 0));
    *static_cast<BitmapImage*>(shadowLayer->getLastShadowImageAtFrame(1)) = BitmapImage(QPoint(0, 0), strokes);

    *outShadow = shadowLayer;
}

} // namespace

TEST_CASE("LayerShadow_UpdateAtFrame")
{
    Object obj;
    LayerShadow* shadowLayer = nullptr;
    makeLayerFixture(&obj, &shadowLayer);
    REQUIRE(shadowLayer != nullptr);

    const quint32 gen = obj.layerStructureGeneration();
    LayerBitmap* source = obj.getColorizeSourceLayer(obj.getLayerCount() - 1, 1);
    REQUIRE(source != nullptr);

    REQUIRE(shadowLayer->updateShadowAtFrame(1, source, gen));
    ShadowImage* frame = shadowLayer->getShadowImageAtFrame(1);
    REQUIRE(frame != nullptr);
    REQUIRE_FALSE(frame->shadowImage().isNull());
    CHECK(frame->needsUpdate() == false);
    CHECK(frame->computedStructureGeneration() == gen);
    // 阴影填在左半（默认方向=左，层默认填充色=中灰）
    CHECK(isColor(frame->shadowImage(), 50, 100, qRgb(128, 128, 128)));
    CHECK(isTransparent(frame->shadowImage(), 150, 100));

    // 结构代数变化后缓存应视为过期
    obj.addNewBitmapLayer();
    CHECK(frame->computedStructureGeneration() != obj.layerStructureGeneration());
}

TEST_CASE("LayerShadow_XmlRoundtrip")
{
    Object obj;
    LayerShadow* shadowLayer = nullptr;
    makeLayerFixture(&obj, &shadowLayer);
    shadowLayer->setDirection(ShadowFill::DirRight);
    shadowLayer->setFillColor(qRgb(30, 40, 50));
    shadowLayer->setGapRadius(6.0);
    shadowLayer->setMarkerColor(1, qRgb(10, 200, 30));
    shadowLayer->setMarkerTransparent(2, true);
    shadowLayer->setEditLines(false);
    shadowLayer->setShowFill(false);

    QDomDocument doc;
    QDomElement elem = shadowLayer->createDomElement(doc);

    LayerShadow restored(999);
    restored.loadDomElement(elem, QDir::temp().absolutePath(), []() {});

    CHECK(restored.direction() == ShadowFill::DirRight);
    CHECK(restored.fillColor() == qRgb(30, 40, 50));
    CHECK(restored.gapRadius() == 6.0);
    CHECK(restored.markerColor(1) == qRgb(10, 200, 30));
    CHECK(restored.markerTransparent(2) == true);
    CHECK(restored.markerTransparent(0) == false);
    CHECK(restored.editLines() == false);
    CHECK(restored.showFill() == false);
    // 关键帧内容由 loadDomElement 从 dataDir 数据文件恢复，
    // 裸 DOM 单测无数据文件，帧内容回读由 FileManager 全链测试覆盖
}

TEST_CASE("ShadowFill_OffsetBoundsCanvasSizedImages")
{
    // 引擎契约：画布级图像 + bounds 子矩形（原点非零）。
    // 画布 200x400，方形在下半，bounds 只取 y>=200 的半幅
    QImage lineArt = makeCanvas(200, 400);
    drawSquareOutline(lineArt, QRect(10, 210, 180, 180));
    QImage strokes = makeCanvas(200, 400);
    drawStrokeLine(strokes, QLine(100, 208, 100, 392), QColor(255, 0, 0));

    ShadowFill::Params params;
    params.direction = ShadowFill::DirLeft;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(0, 200, 200, 200), params);

    REQUIRE_FALSE(result.fill.isNull());
    // fill 为 bounds 尺寸、本地坐标：方形内部在本地 (12..188)
    CHECK(isColor(result.fill, 50, 100, qRgb(0, 0, 0)));
    CHECK(isTransparent(result.fill, 150, 100));
}

TEST_CASE("ShadowFill_BoundsOutsideImageSafeEmpty")
{
    // 契约守卫：bounds 超出图像矩形 → 安全返回空，不越界崩溃
    QImage lineArt = makeCanvas(100, 100);
    QImage strokes = makeCanvas(100, 100);
    drawSquareOutline(lineArt, QRect(10, 10, 80, 80));
    drawStrokeLine(strokes, QLine(50, 8, 50, 92), QColor(255, 0, 0));

    ShadowFill::Params params;
    ShadowFill::Result result = ShadowFill::computeShadow(lineArt, strokes, QRect(50, 50, 100, 100), params);
    CHECK(result.fill.isNull());
}

TEST_CASE("LayerShadow_OffsetContentNoCrash")
{
    // 内容远离画布原点（真实工程常态）：曾因把未原点化的 bounds 传给
    // 引擎造成 compositeBarrier 越界写崩溃（合成测试贴原点没暴露）
    // 用带画布偏移的裁剪内容（关键帧 bounds 原点非零）——整幅大图灌入
    // 会令 bounds=整画布原点归零，踩不到崩溃路径
    Object obj;
    auto* lineLayer = obj.addNewBitmapLayer();
    QImage square = makeCanvas(185, 185);
    drawSquareOutline(square, QRect(2, 2, 180, 180));
    *lineLayer->getLastBitmapImageAtFrame(1) = BitmapImage(QPoint(98, 298), square);
    auto* shadowLayer = static_cast<LayerShadow*>(obj.addNewShadowLayer());
    QImage strokes = makeCanvas(3, 189);
    drawStrokeLine(strokes, QLine(1, 2, 1, 187), QColor(255, 0, 0));
    *static_cast<BitmapImage*>(shadowLayer->getLastShadowImageAtFrame(1)) = BitmapImage(QPoint(189, 296), strokes);

    LayerBitmap* source = obj.getColorizeSourceLayer(obj.getLayerCount() - 1, 1);
    REQUIRE(source != nullptr);
    REQUIRE(shadowLayer->updateShadowAtFrame(1, source, obj.layerStructureGeneration()));

    ShadowImage* frame = shadowLayer->getShadowImageAtFrame(1);
    REQUIRE(frame != nullptr);
    REQUIRE_FALSE(frame->shadowImage().isNull());
    // fill 为 bounds 本地坐标：分割线在本地 x≈92，左半为阴影
    CHECK(isColor(frame->shadowImage(), 50, 100, qRgb(128, 128, 128)));
    CHECK(isTransparent(frame->shadowImage(), 150, 100));
}
