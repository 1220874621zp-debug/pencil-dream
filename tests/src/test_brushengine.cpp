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

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QTemporaryDir>
#include <QThread>

#include "brush/brushengine.h"
#include "brush/maskedstrokecompositor.h"
#include "brush/brushpresetstore.h"
#include "editor.h"
#include "interface/scribblearea.h"
#include "managers/toolmanager.h"
#include "object.h"
#include "pencildef.h"
#include "tool/brushtool.h"
#include "tool/erasertool.h"
#include "tool/smudgetool.h"
#include "graphics/bitmap/tile.h"
#include "graphics/bitmap/tiledbuffer.h"

TEST_CASE("BrushCurve")
{
    SECTION("identity")
    {
        BrushCurve curve;
        REQUIRE(curve.isIdentity());
        REQUIRE(curve.value(0.0) == Approx(0.0).margin(1e-4));
        REQUIRE(curve.value(0.5) == Approx(0.5).margin(1e-4));
        REQUIRE(curve.value(1.0) == Approx(1.0).margin(1e-4));
    }

    SECTION("string roundtrip")
    {
        BrushCurve curve = BrushCurve::fromString("0,0.1;0.4,0.9;1,1;");
        REQUIRE(curve.points().size() == 3);
        const QString serialized = curve.toString();
        BrushCurve copy = BrushCurve::fromString(serialized);
        REQUIRE(copy.value(0.2) == Approx(curve.value(0.2)).margin(1e-4));
        REQUIRE(copy.value(0.7) == Approx(curve.value(0.7)).margin(1e-4));
    }

    SECTION("monotone curve stays in bounds")
    {
        // 陡峭控制点下样条不能过冲出 0..1
        BrushCurve curve = BrushCurve::fromString("0,0;0.05,0.9;0.5,0.5;1,1;");
        for (int i = 0; i <= 100; ++i) {
            const qreal v = curve.value(i / 100.0);
            REQUIRE(v >= 0.0);
            REQUIRE(v <= 1.0);
        }
    }
}

TEST_CASE("BrushSettings XML roundtrip")
{
    BrushSettings settings;
    settings.name = "测试笔刷";
    settings.tipShape = BrushSettings::TipShape::Rectangle;
    settings.diameter = 33.0;
    settings.ratio = 0.45;
    settings.angle = 42.0;
    settings.hardness = 0.07;
    settings.opacity = 0.6;
    settings.spacingMode = BrushSettings::SpacingMode::Fixed;
    settings.spacing = 0.31;
    settings.pressureSize = false;
    settings.sizeCurve = BrushCurve::fromString("0,0.1;0.5,0.8;1,1;");
    settings.pressureOpacity = true;
    settings.opacityCurve = BrushCurve::fromString("0,0;0.3,1;1,0.2;");
    settings.eraser = true;
    settings.flow = 0.4;
    settings.scatter = 1.5;
    settings.paintingMode = BrushSettings::PaintingMode::Buildup;
    settings.blendMode = BrushSettings::BlendMode::Multiply;
    settings.mirrorX = true;
    settings.mirrorY = false;
    settings.airbrushEnabled = true;
    settings.airbrushRate = 60;

    const QString xml = settings.toXMLString();
    BrushSettings loaded;
    REQUIRE(BrushSettings::fromXMLString(xml, loaded));

    REQUIRE(loaded.name == settings.name);
    REQUIRE(loaded.tipShape == settings.tipShape);
    REQUIRE(loaded.diameter == Approx(settings.diameter));
    REQUIRE(loaded.ratio == Approx(settings.ratio));
    REQUIRE(loaded.angle == Approx(settings.angle));
    REQUIRE(loaded.hardness == Approx(settings.hardness));
    REQUIRE(loaded.opacity == Approx(settings.opacity));
    REQUIRE(loaded.spacingMode == settings.spacingMode);
    REQUIRE(loaded.spacing == Approx(settings.spacing));
    REQUIRE(loaded.pressureSize == settings.pressureSize);
    REQUIRE(loaded.pressureOpacity == settings.pressureOpacity);
    REQUIRE(loaded.eraser == true);
    REQUIRE(loaded.flow == Approx(0.4));
    REQUIRE(loaded.scatter == Approx(1.5));
    REQUIRE(loaded.paintingMode == BrushSettings::PaintingMode::Buildup);
    REQUIRE(loaded.blendMode == BrushSettings::BlendMode::Multiply);
    REQUIRE(loaded.mirrorX == true);
    REQUIRE(loaded.mirrorY == false);
    REQUIRE(loaded.airbrushEnabled == true);
    REQUIRE(loaded.airbrushRate == 60);
    REQUIRE(loaded.sizeCurve.value(0.25) == Approx(settings.sizeCurve.value(0.25)).margin(1e-4));
    REQUIRE(loaded.opacityCurve.value(0.8) == Approx(settings.opacityCurve.value(0.8)).margin(1e-4));
}

TEST_CASE("BrushPreset file roundtrip")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    BrushSettings settings;
    settings.name = "导出测试";
    settings.diameter = 18.0;
    settings.hardness = 0.42;
    settings.pressureOpacity = true;

    const QString filePath = tempDir.path() + "/test" + BrushPresetStore::kFileExtension;
    REQUIRE(BrushPresetStore::writePresetFile(filePath, settings));

    BrushSettings loaded;
    QImage thumbnail;
    REQUIRE(BrushPresetStore::readPresetFile(filePath, loaded, thumbnail));
    REQUIRE_FALSE(thumbnail.isNull());
    REQUIRE(loaded.diameter == Approx(18.0));
    REQUIRE(loaded.hardness == Approx(0.42));
    REQUIRE(loaded.pressureOpacity == true);
    REQUIRE(loaded.name == QStringLiteral("导出测试"));
}

TEST_CASE("BrushEngine stroke dabbing")
{
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.pressureSize = false;
    settings.spacingMode = BrushSettings::SpacingMode::Fixed;
    settings.spacing = 0.25; // 5px 步进

    BrushEngine engine;
    engine.setSettings(settings);

    int dabCount = 0;
    QPointF lastDabCenter;
    auto painter = [&](const BrushEngine::DabRequest& dab) {
        ++dabCount;
        // 子像素烤进掩码后 topLeft 是整数，掩码中心 = topLeft + 图心
        lastDabCenter = QPointF(dab.topLeft)
                        + QPointF(dab.dab.width(), dab.dab.height()) * 0.5;
        REQUIRE(dab.opacity > 0.0);
        REQUIRE_FALSE(dab.dab.isNull());
    };

    engine.beginStroke(QPointF(0, 0), 1.0, QColor(0, 0, 0), painter);
    const int firstDabs = dabCount;
    REQUIRE(firstDabs == 1); // 起笔即落点

    engine.strokeTo(QPointF(100, 0), 1.0, painter);
    // 100px / 5px ≈ 20 个 dab（首点已画）
    REQUIRE(dabCount >= 18);
    REQUIRE(dabCount <= 24);
    REQUIRE(lastDabCenter.x() == Approx(100.0).margin(5.0));

    engine.endStroke();
    REQUIRE_FALSE(engine.isStrokeActive());
}

TEST_CASE("BrushEngine sub-pixel snapping and quantized dab cache")
{
    BrushSettings settings;
    settings.diameter = 40.0;
    settings.pressureSize = true;

    BrushEngine engine;
    engine.setSettings(settings);

    // 半像素网格吸附：10.24 → 10.0，10.76 → 11.0，整数落点差一格
    QPoint topLeftA, topLeftB;
    QImage dabA;
    engine.dabAt(QPointF(10.24, 10.24), 1.0, [&](const BrushEngine::DabRequest& r) {
        topLeftA = r.topLeft;
        dabA = r.dab;
    });
    engine.dabAt(QPointF(10.76, 10.76), 1.0, [&](const BrushEngine::DabRequest& r) {
        topLeftB = r.topLeft;
    });
    REQUIRE((topLeftB - topLeftA) == QPoint(1, 1));
    // 吸附点即掩码中心：topLeft + 图心 = 吸附后的落点
    REQUIRE(topLeftA + QPoint(dabA.width() / 2, dabA.height() / 2) == QPoint(10, 10));

    // 直径量化：4% 步长内的压感变化复用同一张缓存 dab，跨步长才重生成
    qint64 cacheKeySmall = 0, cacheKeyBig = 0;
    const QPointF p(50, 50);
    engine.dabAt(p, 0.50, [&](const BrushEngine::DabRequest& r) { cacheKeySmall = r.dab.cacheKey(); });
    qint64 cacheKeyNearby = 0;
    engine.dabAt(p, 0.51, [&](const BrushEngine::DabRequest& r) { cacheKeyNearby = r.dab.cacheKey(); });
    engine.dabAt(p, 0.90, [&](const BrushEngine::DabRequest& r) { cacheKeyBig = r.dab.cacheKey(); });
    REQUIRE(cacheKeySmall == cacheKeyNearby);
    REQUIRE(cacheKeySmall != cacheKeyBig);
}

TEST_CASE("BrushEngine mirror and airbrush")
{
    BrushSettings settings;
    settings.diameter = 10.0;
    settings.pressureSize = false;

    // 镜像：开启 mirrorX 后每枚 dab 发两份（原图 + 水平翻转），位置关于中心对称
    {
        settings.mirrorX = true;
        BrushEngine engine;
        engine.setSettings(settings);
        engine.setMirrorCenter(QPointF(200, 0));
        QImage dab;
        QPoint tl1, tl2; int count = 0;
        engine.dabAt(QPointF(100, 0), 1.0, [&](const BrushEngine::DabRequest& r) {
            if (count == 0) { tl1 = r.topLeft; dab = r.dab; }
            if (count == 1) tl2 = r.topLeft;
            ++count;
        });
        REQUIRE(count == 2);
        REQUIRE(tl2.x() == qRound(2.0 * 200.0) - (tl1.x() + dab.width()));
        REQUIRE(tl1.y() == tl2.y());
    }

    // 喷枪：静止时按速率补 dab
    {
        settings.mirrorX = false;
        settings.airbrushEnabled = true;
        settings.airbrushRate = 100; // 10ms 一枚
        BrushEngine engine;
        engine.setSettings(settings);
        int count = 0;
        const auto painter = [&](const BrushEngine::DabRequest&) { ++count; };
        engine.beginStroke(QPointF(50, 50), 1.0, QColor(0, 0, 0), painter);
        REQUIRE(count == 1); // 起笔一枚
        QThread::msleep(30);
        engine.airbrushTick(painter);
        REQUIRE(count >= 2); // 静止补喷
        engine.endStroke();
    }
}

TEST_CASE("BrushEngine smudge mode skips first dab")
{
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.pressureSize = false;
    BrushEngine engine;
    engine.setSettings(settings);
    engine.setSmudgeMode(true);
    int count = 0;
    const auto painter = [&](const BrushEngine::DabRequest&) { ++count; };
    engine.beginStroke(QPointF(0, 0), 1.0, QColor(0, 0, 0), painter);
    REQUIRE(count == 0); // Krita smudge：起笔只定位不作画
    engine.strokeTo(QPointF(50, 0), 1.0, painter);
    REQUIRE(count >= 2); // 后续 dab 正常布点
    engine.endStroke();
}

TEST_CASE("smudgeBlendImage lerp semantics")
{
    // tile 全黑；canvas 左半红右半蓝；Δ=(10,0)、rate=1、全透掩码
    // → tile(x) 取 canvas(x−10)
    QImage tile(30, 10, QImage::Format_ARGB32_Premultiplied);
    tile.fill(QColor(0, 0, 0, 255));
    QImage canvas(30, 10, QImage::Format_ARGB32_Premultiplied);
    QPainter cp(&canvas);
    cp.fillRect(QRect(0, 0, 15, 10), QColor(255, 0, 0, 255));
    cp.fillRect(QRect(15, 0, 15, 10), QColor(0, 0, 255, 255));
    cp.end();
    QImage mask(10, 10, QImage::Format_ARGB32_Premultiplied);
    mask.fill(QColor(255, 255, 255, 255));

    smudgeBlendImage(tile, QPoint(0, 0), canvas, QPoint(0, 0),
                     mask, QPoint(10, 0), QPointF(10, 0), 1.0);
    // tile(10,5) ← canvas(0,5) 红
    REQUIRE(qRed(tile.pixel(10, 5)) == 255);
    REQUIRE(qBlue(tile.pixel(10, 5)) == 0);
    // 掩码挪到 20..29：tile(25,5) ← canvas(15,5) 蓝、tile(20,5) ← canvas(10,5) 红
    smudgeBlendImage(tile, QPoint(0, 0), canvas, QPoint(0, 0),
                     mask, QPoint(20, 0), QPointF(10, 0), 1.0);
    REQUIRE(qBlue(tile.pixel(25, 5)) == 255);
    REQUIRE(qRed(tile.pixel(25, 5)) == 0);
    REQUIRE(qRed(tile.pixel(20, 5)) == 255);

    // rate=0.5：黑红对半
    QImage tile2(30, 10, QImage::Format_ARGB32_Premultiplied);
    tile2.fill(QColor(0, 0, 0, 255));
    smudgeBlendImage(tile2, QPoint(0, 0), canvas, QPoint(0, 0),
                     mask, QPoint(10, 0), QPointF(10, 0), 0.5);
    REQUIRE(qRed(tile2.pixel(10, 5)) == Approx(127).margin(3));
    REQUIRE(qBlue(tile2.pixel(10, 5)) == 0);
}

TEST_CASE("SmudgeTool options integration")
{
    Object* object = new Object;
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->init();
    editor->setObject(object);

    SmudgeTool* tool = dynamic_cast<SmudgeTool*>(editor->tools()->getTool(SMUDGE));
    REQUIRE(tool != nullptr);
    const BrushSettings s = tool->currentBrushSettings();
    REQUIRE(s.flow == Approx(0.5));          // 混合速率默认 50%
    REQUIRE(s.spacingMode == BrushSettings::SpacingMode::Fixed);
    REQUIRE(s.spacing == Approx(0.1));       // Krita 混合预设 10% 间距
    REQUIRE(s.pressureSize == false);        // 压感控速率不控大小
    // 硬度由羽化映射，受本机已保存设置影响，不作定值断言
}

TEST_CASE("BrushEngine pressure dynamics")
{
    BrushSettings settings;
    settings.diameter = 40.0;
    settings.pressureSize = true;
    settings.pressureOpacity = false;

    BrushEngine engine;
    engine.setSettings(settings);

    REQUIRE(engine.dabDiameterAt(1.0) == Approx(40.0));
    REQUIRE(engine.dabDiameterAt(0.5) == Approx(20.0));
    REQUIRE(engine.dabDiameterAt(0.0) == Approx(1.0)); // 钳到最小 1

    settings.pressureSize = false;
    engine.setSettings(settings);
    REQUIRE(engine.dabDiameterAt(0.2) == Approx(40.0));

    settings.pressureOpacity = true;
    settings.opacity = 0.8;
    settings.opacityCurve = BrushCurve::fromString("0,0.5;1,1;");
    engine.setSettings(settings);
    REQUIRE(engine.dabOpacityAt(1.0) == Approx(0.8));
    REQUIRE(engine.dabOpacityAt(0.0) == Approx(0.4));
}

TEST_CASE("BrushEngine preview rendering")
{
    BrushSettings settings;
    settings.diameter = 24.0;
    const QImage preview = BrushEngine::renderStrokePreview(settings, QSize(128, 96));
    REQUIRE_FALSE(preview.isNull());
    REQUIRE(preview.size() == QSize(128, 96));

    // 描边应留下非背景像素（背景 #27272a）
    bool hasStrokePixels = false;
    for (int y = 0; y < preview.height() && !hasStrokePixels; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(preview.constScanLine(y));
        for (int x = 0; x < preview.width(); ++x) {
            if (qRed(line[x]) > 100) {
                hasStrokePixels = true;
                break;
            }
        }
    }
    REQUIRE(hasStrokePixels);
}

TEST_CASE("BrushTool preset application")
{
    // 完整 Editor 台架：init 创建全套管理器与工具，用生产同款 BrushTool 实例
    Object* object = new Object;
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->init();
    editor->setObject(object);

    BrushTool* tool = dynamic_cast<BrushTool*>(editor->tools()->getTool(BRUSH));
    REQUIRE(tool != nullptr);

    // 换两支差异大的笔，验证 applyBrushPreset → 引擎参数整链生效
    BrushSettings fine;
    fine.diameter = 3.0;
    fine.hardness = 1.0;
    fine.pressureSize = false;

    BrushSettings fat;
    fat.diameter = 90.0;
    fat.hardness = 0.35;
    fat.tipShape = BrushSettings::TipShape::Rectangle;
    fat.ratio = 0.45;
    fat.angle = 42.0;

    tool->applyBrushPreset(fine);
    {
        const BrushSettings current = tool->currentBrushSettings();
        REQUIRE(current.diameter == Approx(3.0));
        REQUIRE(current.hardness == Approx(1.0 - 1.0 / 100.0)); // 羽化钳到 1 → 硬度 0.99
        REQUIRE(current.tipShape == BrushSettings::TipShape::Circle);
        REQUIRE(current.pressureSize == false);
    }

    tool->applyBrushPreset(fat);
    {
        const BrushSettings current = tool->currentBrushSettings();
        REQUIRE(current.diameter == Approx(90.0));
        REQUIRE(current.hardness == Approx(1.0 - 65.0 / 100.0).epsilon(0.01));
        REQUIRE(current.tipShape == BrushSettings::TipShape::Rectangle);
        REQUIRE(current.ratio == Approx(0.45));
        REQUIRE(current.angle == Approx(42.0));
    }

    // 引擎侧的 dab 尺寸也要跟着预设走
    BrushEngine probeEngine;
    probeEngine.setSettings(tool->currentBrushSettings());
    REQUIRE(probeEngine.dabDiameterAt(1.0) == Approx(90.0));

    // 台架各部件与全局单例（PixmapCache/QSettings 等）有交叉引用，
    // 进程退出时统一回收，测试内不手动 delete（与其它 Editor 测试不同，
    // init() 过的 Editor 析构链在某些 manager 上不稳定）
}

TEST_CASE("TiledBuffer drawDab wash blending")
{
    // 硬笔 dab：中心全不透明
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.hardness = 1.0;

    BrushEngine engine;
    engine.setSettings(settings);
    QImage dabImage;
    engine.dabAt(QPointF(0, 0), 1.0, [&](const BrushEngine::DabRequest& request) {
        dabImage = request.dab;
    });
    REQUIRE_FALSE(dabImage.isNull());
    const QRgb center = dabImage.pixel(dabImage.width() / 2, dabImage.height() / 2);
    REQUIRE(qAlpha(center) == 255); // 硬笔中心全不透明

    // —— 对照组：SourceOver 与 wash 单次落点在透明底上等价 ——
    struct Variant { QPainter::CompositionMode mode; qreal opacity; int expectedAlpha; };
    const Variant variants[] = {
        { QPainter::CompositionMode_SourceOver, 1.0, 255 },
        { QPainter::CompositionMode_SourceOver, 0.5, 127 },
    };
    for (const Variant& v : variants) {
        QImage canvas(200, 200, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter p(&canvas);
        p.setCompositionMode(v.mode);
        p.setOpacity(v.opacity);
        p.drawImage(QPoint(89, 89), dabImage);
        p.end();
        REQUIRE(qAlpha(canvas.pixel(100, 100)) == Approx(v.expectedAlpha).margin(3));
    }

    TiledBuffer buffer;
    // 整数对齐落点：掩码中心对准画布 (100,100)
    const QPoint placeTopLeft(100 - dabImage.width() / 2, 100 - dabImage.height() / 2);
    DabPasteParams half;
    half.opacity = 0.5;
    buffer.drawDab(dabImage, placeTopLeft, half);
    buffer.drawDab(dabImage, placeTopLeft, half); // 同位置再落一笔

    // wash 语义：同处重复落 dab 不叠加不透明度
    Tile* tile = buffer.tiles().value(TileIndex{ 1, 1 }, nullptr);
    REQUIRE(tile != nullptr);
    const QImage tileImage = tile->pixmap().toImage();
    // tile 图像是 64x64 本地坐标，画布点 (100,100) 对应本地 (36,36)
    const QPoint local(100 - tile->pos().x(), 100 - tile->pos().y());
    const QRgb result = tileImage.pixel(local);
    const int resultAlpha = qAlpha(result);
    REQUIRE(resultAlpha > 0);
    REQUIRE(resultAlpha <= 150); // wash 语义：两次半透明 dab 不叠加变深（0.5*255≈128）
}

TEST_CASE("washBlendImage converge, buildup and erase semantics")
{
    // 硬 dab 掩码
    BrushSettings settings;
    settings.diameter = 20.0;
    settings.hardness = 1.0;
    BrushEngine engine;
    engine.setSettings(settings);
    QImage dab;
    engine.dabAt(QPointF(0, 0), 1.0, [&](const BrushEngine::DabRequest& r) { dab = r.dab; });
    REQUIRE(qAlpha(dab.pixel(dab.width() / 2, dab.height() / 2)) == 255);

    const QPoint at(50 - dab.width() / 2, 50 - dab.height() / 2);

    // 涂抹收敛：同处反复落半透明 dab，alpha 收敛到 opacity 封顶不加深
    {
        QImage canvas(100, 100, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        DabPasteParams p;
        p.opacity = 0.5;
        for (int i = 0; i < 5; ++i) washBlendImage(canvas, dab, at, p);
        const int a = qAlpha(canvas.pixel(50, 50));
        REQUIRE(a == Approx(128).margin(2));
    }
    // 叠加：并集累积，两枚后超过单枚的 128
    {
        QImage canvas(100, 100, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        DabPasteParams p;
        p.opacity = 0.5;
        p.buildup = true;
        washBlendImage(canvas, dab, at, p);
        const int a1 = qAlpha(canvas.pixel(50, 50));
        washBlendImage(canvas, dab, at, p);
        const int a2 = qAlpha(canvas.pixel(50, 50));
        REQUIRE(a1 == Approx(128).margin(2));
        REQUIRE(a2 == Approx(191).margin(3)); // 128 + 128*(1-128/255)
    }
    // 流量：涂抹模式 flow 在"并集"与"收敛"间插值。
    // 首枚 dab 落在空底上 aZero==aFull==srcA'（flow 无差别）；
    // 第二枚起 aZero(并集) 继续涨、aFull 收敛封顶，flow 决定落点
    {
        QImage canvas(100, 100, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        DabPasteParams p;
        p.opacity = 0.5;
        p.flow = 0.5;
        washBlendImage(canvas, dab, at, p);
        REQUIRE(qAlpha(canvas.pixel(50, 50)) == Approx(128).margin(2)); // 首枚
        washBlendImage(canvas, dab, at, p);
        // 第二枚：aZero=191、aFull=128，flow=0.5 → ≈160（flow=1 则封在 128）
        REQUIRE(qAlpha(canvas.pixel(50, 50)) == Approx(160).margin(3));
    }
    // 橡皮真实管线：dab 画进"空白缓冲"（普通累积），终局 DestinationOut 反转语义。
    // 曾因擦除分支写成"收缩已有 alpha"而空缓冲全跳过 → 橡皮无效果（回归测试）
    {
        QImage buffer(100, 100, QImage::Format_ARGB32_Premultiplied);
        buffer.fill(Qt::transparent);
        DabPasteParams erase;
        erase.opacity = 0.5;
        for (int i = 0; i < 5; ++i) washBlendImage(buffer, dab, at, erase);
        // 缓冲 alpha = 擦除量，涂抹模式封顶在 opacity（128），不越擦越深
        REQUIRE(qAlpha(buffer.pixel(50, 50)) == Approx(128).margin(2));
        // 终局 DestinationOut 合并到不透明图层：255·(1-128/255) ≈ 127
        QImage layer(100, 100, QImage::Format_ARGB32_Premultiplied);
        layer.fill(QColor(90, 60, 30, 255));
        const int keptA = qRound(255 * (1.0 - qAlpha(buffer.pixel(50, 50)) / 255.0));
        REQUIRE(keptA == Approx(127).margin(2));
    }
}

TEST_CASE("EraserTool preset application")
{
    // 完整 Editor 台架：橡皮与画笔共用笔刷引擎，预设整链生效
    Object* object = new Object;
    Editor* editor = new Editor;
    ScribbleArea* scribbleArea = new ScribbleArea(nullptr);
    editor->setScribbleArea(scribbleArea);
    editor->init();
    editor->setObject(object);

    EraserTool* tool = dynamic_cast<EraserTool*>(editor->tools()->getTool(ERASER));
    REQUIRE(tool != nullptr);

    BrushSettings soft;
    soft.eraser = true;
    soft.diameter = 50.0;
    soft.hardness = 0.25;
    soft.pressureSize = true;
    soft.pressureOpacity = false;

    tool->applyBrushPreset(soft);
    const BrushSettings current = tool->currentBrushSettings();
    REQUIRE(current.eraser == true);
    REQUIRE(current.diameter == Approx(50.0));
    REQUIRE(current.hardness == Approx(0.25).epsilon(0.01)); // 硬度↔羽化往返
    REQUIRE(current.pressureSize == true);
    REQUIRE(current.pressureOpacity == false);

    // 引擎侧的 dab 尺寸也要跟着预设走
    BrushEngine probeEngine;
    probeEngine.setSettings(tool->currentBrushSettings());
    REQUIRE(probeEngine.dabDiameterAt(1.0) == Approx(50.0));

    // 台架各部件与全局单例（PixmapCache/QSettings 等）有交叉引用，
    // 进程退出时统一回收，测试内不手动 delete
}

// ===================== v2 扩展：图像笔尖 / 双笔尖 / 纹理 / 图案颜色源 =====================

namespace
{

// 白底 + 中心黑竖条（书法笔尖的最小模型：黑=不透明）
QImage makeCalligraphyTip(int w, int h, int inkWidth)
{
    QImage tip(w, h, QImage::Format_ARGB32);
    tip.fill(Qt::white);
    QPainter p(&tip);
    p.setPen(QPen(QColor(0, 0, 0), inkWidth));
    p.drawLine(w / 2, 0, w / 2, h - 1);
    p.end();
    return tip;
}

QImage renderSingleDab(const BrushSettings& s, const QPointF& at, qreal pressure, QImage& layer)
{
    BrushEngine engine;
    engine.setSettings(s);
    const auto painter = [&layer](const BrushEngine::DabRequest& dab) {
        DabPasteParams params;
        params.opacity = dab.opacity;
        params.flow = dab.flow;
        params.buildup = dab.buildup;
        params.blendMode = dab.blendMode;
        params.perPixelColor = dab.perPixelColor;
        washBlendImage(layer, dab.dab, dab.topLeft, params);
    };
    engine.beginStroke(at, pressure, QColor(200, 30, 30), painter);
    engine.endStroke();
    return layer;
}

QRect nonZeroBounds(const QImage& image)
{
    int x0 = image.width(), x1 = -1, y0 = image.height(), y1 = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 8) {
                x0 = qMin(x0, x); x1 = qMax(x1, x);
                y0 = qMin(y0, y); y1 = qMax(y1, y);
            }
        }
    }
    return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
}

} // namespace

TEST_CASE("Image tip dab generation")
{
    SECTION("mask direction: black ink paints, white background stays transparent")
    {
        BrushSettings s;
        s.tipImage = makeCalligraphyTip(32, 64, 12);
        s.tipShape = BrushSettings::TipShape::Image;
        s.bakeTipMask();
        REQUIRE(!s.tipMask.isNull());
        // 烘焙：黑(0)→alpha 255，白(255)→alpha 0（Krita ALPHAMASK：(255-灰)×α/255）
        REQUIRE(s.tipMask.pixel(16, 32) != 0);
        REQUIRE(s.tipMask.pixel(2, 2) == 0);

        s.diameter = 64.0;
        s.pressureSize = false;
        s.opacity = 1.0;
        s.flow = 1.0;
        QImage layer(200, 200, QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        renderSingleDab(s, QPointF(100, 100), 1.0, layer);
        const QRect bbox = nonZeroBounds(layer);
        REQUIRE(!bbox.isEmpty());
        // 黑竖条被画出来（中心有墨），尺寸≈diameter
        REQUIRE(bbox.width() <= 40);
        REQUIRE(bbox.height() > 45);
        REQUIRE(qAlpha(layer.pixel(100, 100)) > 200);
        REQUIRE(qAlpha(layer.pixel(100, 30)) == 0); // 笔尖上端之外
    }

    SECTION("diameter scales the tip, angle rotates the bbox")
    {
        BrushSettings s;
        s.tipImage = makeCalligraphyTip(16, 64, 6); // 纵长笔尖
        s.tipShape = BrushSettings::TipShape::Image;
        s.bakeTipMask();
        s.pressureSize = false;
        s.opacity = 1.0;
        s.flow = 1.0;
        s.diameter = 60.0;

        QImage a(200, 200, QImage::Format_ARGB32_Premultiplied);
        a.fill(Qt::transparent);
        renderSingleDab(s, QPointF(100, 100), 1.0, a);
        const QRect bbox0 = nonZeroBounds(a);
        REQUIRE(bbox0.height() > bbox0.width() * 2);

        s.angle = 90.0;
        QImage b(200, 200, QImage::Format_ARGB32_Premultiplied);
        b.fill(Qt::transparent);
        renderSingleDab(s, QPointF(100, 100), 1.0, b);
        const QRect bbox90 = nonZeroBounds(b);
        REQUIRE(bbox90.width() > bbox90.height() * 2);
    }

    SECTION("ratio squeezes vertically")
    {
        BrushSettings s;
        s.tipImage = makeCalligraphyTip(32, 64, 10);
        s.tipShape = BrushSettings::TipShape::Image;
        s.bakeTipMask();
        s.pressureSize = false;
        s.diameter = 64.0;
        s.ratio = 0.5;
        QImage layer(200, 200, QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        renderSingleDab(s, QPointF(100, 100), 1.0, layer);
        const QRect bbox = nonZeroBounds(layer);
        REQUIRE(bbox.height() < 35); // 纵向被压缩到一半
    }
}

TEST_CASE("BrushSettings v2 XML roundtrip with embedded images")
{
    BrushSettings s;
    s.name = "墨沁T";
    s.tipImage = makeCalligraphyTip(20, 50, 8);
    s.tipShape = BrushSettings::TipShape::Image;
    s.bakeTipMask();
    s.diameter = 118.0;
    s.texture.pattern = QImage(16, 16, QImage::Format_ARGB32);
    s.texture.pattern.fill(QColor(120, 130, 140));
    s.texture.mode = 15;
    s.texture.strength = 0.19;
    s.texture.contrast = 0.83;
    s.texture.bake();
    s.texture.enabled = true;
    s.colorSource = BrushSettings::ColorSource::Pattern;
    s.mask.enabled = true;
    s.mask.sizeCoeff = 0.325;
    s.mask.mode = BrushMaskSettings::Mode::Burn;
    s.mask.sub = std::make_unique<BrushSettings>();
    s.mask.sub->diameter = 17.0;
    s.mask.sub->spacing = 0.08;
    s.mask.sub->tipShape = BrushSettings::TipShape::Circle;

    const QString xml = s.toXMLString();
    BrushSettings out;
    REQUIRE(BrushSettings::fromXMLString(xml, out));

    REQUIRE(out.tipShape == BrushSettings::TipShape::Image);
    REQUIRE(out.tipImage.size() == s.tipImage.size());
    REQUIRE(out.tipMask.size() == s.tipMask.size());
    REQUIRE(out.tipMask.pixel(10, 25) == s.tipMask.pixel(10, 25));
    REQUIRE(out.diameter == Approx(118.0));
    REQUIRE(out.texture.enabled == true);
    REQUIRE(out.texture.mode == 15);
    REQUIRE(out.texture.strength == Approx(0.19));
    REQUIRE(!out.texture.bakedMask.isNull());
    REQUIRE(out.colorSource == BrushSettings::ColorSource::Pattern);
    REQUIRE(out.mask.enabled == true);
    REQUIRE(out.mask.sizeCoeff == Approx(0.325));
    REQUIRE(out.mask.mode == BrushMaskSettings::Mode::Burn);
    REQUIRE(out.mask.sub != nullptr);
    REQUIRE(out.mask.sub->diameter == Approx(17.0));
    REQUIRE(out.mask.sub->spacing == Approx(0.08));

    // v1 预设（无新元素）向后兼容
    BrushSettings legacy;
    legacy.name = "旧笔刷";
    legacy.diameter = 30.0;
    BrushSettings legacyOut;
    REQUIRE(BrushSettings::fromXMLString(legacy.toXMLString(), legacyOut));
    REQUIRE(legacyOut.tipShape == BrushSettings::TipShape::Circle);
    REQUIRE(legacyOut.mask.enabled == false);
    REQUIRE(legacyOut.texture.enabled == false);
}

TEST_CASE("MaskedStrokeCompositor formulas")
{
    using Mode = BrushMaskSettings::Mode;
    SECTION("burn alpha semantics")
    {
        // dst=1 是稳定点；src=0 → 0；src=1 → dst
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::Burn, 1.0, 1.0) == Approx(1.0));
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::Burn, 0.0, 0.5) == Approx(0.0));
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::Burn, 1.0, 0.5) == Approx(0.5));
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::Burn, 0.5, 0.75) == Approx(0.5));
    }
    SECTION("hard mix")
    {
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::HardMix, 0.6, 0.5) == Approx(1.0));
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::HardMix, 0.4, 0.5) == Approx(0.0));
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::HardMix, 0.0, 1.0) == Approx(0.0));
    }
    SECTION("hard mix softer is the antialiased variant")
    {
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::HardMixSofter, 0.5, 0.5) == Approx(0.5));
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::HardMixSofter, 1.0, 0.5) == Approx(1.0));
        REQUIRE(MaskedStrokeCompositor::maskOp(Mode::HardMixSofter, 0.0, 0.5) == Approx(0.0).margin(0.02));
    }
    SECTION("texture ops (strength variants)")
    {
        // LINEAR_HEIGHT_PHOTOSHOP(15)：m=dst*10*s，max((1-src)*m, m-src)
        REQUIRE(MaskedStrokeCompositor::textureOp(15, 0.8, 0.5, 0.19) == Approx(0.19).epsilon(0.02));
        REQUIRE(MaskedStrokeCompositor::textureOp(15, 0.5, 0.6, 1.0) == Approx(1.0));
        // HARD_MIX_SOFTER(11)：clamp(3*dst*s - 2*(1-src))
        REQUIRE(MaskedStrokeCompositor::textureOp(11, 0.5, 0.5, 1.0) == Approx(0.5));
        REQUIRE(MaskedStrokeCompositor::textureOp(11, 0.2, 0.5, 0.5) == Approx(0.0).margin(0.02));
        // HEIGHT(12)：s'=0.99s；dst/(1-s') - (src+(1-s'))
        REQUIRE(MaskedStrokeCompositor::textureOp(12, 1.0, 0.001, 1.0) == Approx(0.0).margin(0.02));
        REQUIRE(MaskedStrokeCompositor::textureOp(12, 0.5, 0.5, 1.0) == Approx(1.0));
    }
}

TEST_CASE("MaskedStrokeCompositor stroke integration")
{
    // 10x10 主 dab alpha≈128；6x6 副 dab（alpha=255 白，仅 (0,0) 角透明
    // ——extent 含整个 dab 矩形，角上属于"范围内未覆盖"）
    QImage mainDab(10, 10, QImage::Format_ARGB32_Premultiplied);
    mainDab.fill(qPremultiply(qRgba(200, 30, 30, 128)));
    QImage maskDabImg(6, 6, QImage::Format_ARGB32_Premultiplied);
    maskDabImg.fill(qPremultiply(qRgba(255, 255, 255, 255)));
    maskDabImg.setPixel(0, 0, 0);

    DabPasteParams params; // 默认 wash/满流量

    SECTION("main pixels outside mask extent survive; inside extent uncovered get burned away")
    {
        MaskedStrokeCompositor compositor;
        compositor.begin(BrushMaskSettings::Mode::Burn);
        compositor.mainDab(mainDab, QPoint(0, 0), params);
        // 副笔尖尚未出现：合成 = 原样主笔迹
        QPoint origin;
        QImage region = compositor.composedRegion(QRect(0, 0, 10, 10), origin);
        REQUIRE(origin == QPoint(0, 0));
        REQUIRE(qAlpha(region.pixel(1, 1)) > 100);

        // 副笔尖盖在中间 (2,2)-(7,7)：范围外保留，范围内未覆盖处被 burn 清零
        compositor.maskDab(maskDabImg, QPoint(2, 2));
        region = compositor.composedRegion(QRect(0, 0, 10, 10), origin);
        REQUIRE(qAlpha(region.pixel(1, 1)) > 100);   // 范围外：主笔迹原样
        REQUIRE(qAlpha(region.pixel(4, 4)) > 100);   // 覆盖处：burn(1, dst)=dst
        REQUIRE(qAlpha(region.pixel(2, 2)) < 15);    // 范围内未覆盖（dab 边角 alpha 低）→ burn(src≈0,dst)≈0
    }

    SECTION("hard mix keeps only covered pixels")
    {
        MaskedStrokeCompositor compositor;
        compositor.begin(BrushMaskSettings::Mode::HardMix);
        compositor.mainDab(mainDab, QPoint(0, 0), params);
        compositor.maskDab(maskDabImg, QPoint(0, 0));
        QPoint origin;
        const QImage region = compositor.composedRegion(QRect(0, 0, 10, 10), origin);
        // 覆盖满的 (3,3)：src=1 → hard mix(1, 0.5)=1
        REQUIRE(qAlpha(region.pixel(3, 3)) > 240);
    }

    SECTION("accumulation is functional: later main dabs recover erased pixels")
    {
        MaskedStrokeCompositor compositor;
        compositor.begin(BrushMaskSettings::Mode::Burn);
        compositor.maskDab(maskDabImg, QPoint(0, 0));
        compositor.mainDab(mainDab, QPoint(0, 0), params);
        compositor.mainDab(mainDab, QPoint(0, 0), params); // 同位置第二枚（wash 收敛封顶）
        QPoint origin;
        const QImage region = compositor.composedRegion(QRect(0, 0, 10, 10), origin);
        REQUIRE(qAlpha(region.pixel(3, 3)) > 100); // 函数式合成：始终从累积态重算
    }
}

TEST_CASE("Pattern color source recolors dabs by canvas position")
{
    // 4x4 图案：左半红右半蓝
    QImage pattern(4, 4, QImage::Format_ARGB32);
    pattern.fill(Qt::red);
    for (int y = 0; y < 4; ++y) {
        for (int x = 2; x < 4; ++x) {
            pattern.setPixel(x, y, qRgb(0, 0, 255));
        }
    }

    BrushSettings s;
    s.tipShape = BrushSettings::TipShape::Circle;
    s.diameter = 20.0;
    s.pressureSize = false;
    s.opacity = 1.0;
    s.flow = 1.0;
    s.texture.pattern = pattern;
    s.texture.bake();
    s.colorSource = BrushSettings::ColorSource::Pattern;

    QImage layer(200, 200, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    BrushEngine engine;
    engine.setSettings(s);
    const auto painter = [&layer](const BrushEngine::DabRequest& dab) {
        DabPasteParams params;
        params.opacity = dab.opacity;
        params.flow = dab.flow;
        params.buildup = dab.buildup;
        params.blendMode = dab.blendMode;
        params.perPixelColor = dab.perPixelColor;
        washBlendImage(layer, dab.dab, dab.topLeft, params);
    };
    engine.beginStroke(QPointF(100, 100), 1.0, QColor(255, 255, 255), painter);
    engine.endStroke();

    const QRgb center = layer.pixel(100, 100);
    REQUIRE(qAlpha(center) > 200);
    // 100 % 4 = 0 → 红侧
    REQUIRE(qRed(center) > 180);
    REQUIRE(qBlue(center) < 80);
    // 102 % 4 = 2 → 蓝侧
    const QRgb blueSide = layer.pixel(102, 100);
    REQUIRE(qBlue(blueSide) > 150);
    REQUIRE(qRed(blueSide) < 100);
}

TEST_CASE("Texture modulates dab alpha by canvas-anchored pattern")
{
    BrushSettings s;
    s.tipShape = BrushSettings::TipShape::Circle;
    s.diameter = 30.0;
    s.pressureSize = false;
    s.opacity = 1.0;
    s.flow = 1.0;

    // 2x2 棋盘纹理：黑(1)白(0)交替（bake 后 mask ≈ 255/0 交替）
    QImage pattern(2, 2, QImage::Format_ARGB32);
    pattern.setPixel(0, 0, qRgb(0, 0, 0));
    pattern.setPixel(1, 1, qRgb(0, 0, 0));
    pattern.setPixel(1, 0, qRgb(255, 255, 255));
    pattern.setPixel(0, 1, qRgb(255, 255, 255));
    s.texture.pattern = pattern;
    s.texture.mode = 12; // HEIGHT：strength=1 时 dst/(0.01) - (src+0.01) → src=1 处≈1，src=0 处=0
    s.texture.strength = 1.0;
    s.texture.bake();
    s.texture.enabled = true;

    QImage layer(200, 200, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    renderSingleDab(s, QPointF(100, 100), 1.0, layer);
    const QRect bbox = nonZeroBounds(layer);
    REQUIRE(!bbox.isEmpty());
    // HEIGHT 公式对 src=0 输出 0、src=1 输出 clamp(100·dst-1.01)=1 → 棋盘镂空
    int holes = 0, filled = 0;
    for (int y = bbox.top(); y <= bbox.bottom(); ++y) {
        for (int x = bbox.left(); x <= bbox.right(); ++x) {
            if ((x % 2 == 0) == (y % 2 == 0)) {
                if (qAlpha(layer.pixel(x, y)) == 0) ++holes;
            } else {
                if (qAlpha(layer.pixel(x, y)) > 100) ++filled;
            }
        }
    }
    REQUIRE(holes > 20);
    REQUIRE(filled > 20);
}

TEST_CASE("BrushEngine renderStrokePreview with masked brush")
{
    BrushSettings s;
    s.tipShape = BrushSettings::TipShape::Circle;
    s.diameter = 40.0;
    s.opacity = 1.0;
    s.flow = 1.0;
    s.mask.enabled = true;
    s.mask.mode = BrushMaskSettings::Mode::Burn;
    s.mask.sizeCoeff = 0.5;
    s.mask.sub = std::make_unique<BrushSettings>();
    s.mask.sub->diameter = 20.0;
    s.mask.sub->hardness = 1.0;
    s.mask.sub->spacing = 0.1;

    const QImage preview = BrushEngine::renderStrokePreview(s, QSize(128, 96));
    REQUIRE(!preview.isNull());
    // 有内容且不完全透明（合成路径没把整条笔迹烧没）
    int painted = 0;
    for (int y = 0; y < preview.height(); ++y) {
        for (int x = 0; x < preview.width(); ++x) {
            if (qAlpha(preview.pixel(x, y)) > 8) ++painted;
        }
    }
    REQUIRE(painted > 100);
}

// 手动验证台架：设置 BRUSH_PREVIEW_DIR=<目录> 后跑该用例，目录里每个 .pbp
// 都会用真实引擎渲染 256x192 预览并写出 <名字>_pencil.png（常规跑跳过）
TEST_CASE("Brush preset preview export (manual)")
{
    const QString dir = qEnvironmentVariable("BRUSH_PREVIEW_DIR");
    if (dir.isEmpty()) {
        return;
    }
    QDir d(dir);
    const auto entries = d.entryList(QStringList() << "*.pbp", QDir::Files);
    REQUIRE(entries.size() > 0);
    for (const QString& f : entries) {
        BrushSettings s;
        QImage thumb;
        REQUIRE(BrushPresetStore::readPresetFile(d.filePath(f), s, thumb));
        const QImage preview = BrushEngine::renderStrokePreview(s, QSize(256, 192));
        REQUIRE(!preview.isNull());
        const QString out = d.filePath(f).chopped(4) + "_pencil.png";
        REQUIRE(preview.save(out));
    }
}
